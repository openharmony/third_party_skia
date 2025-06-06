/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/SkGr.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkData.h"
#include "include/core/SkPixelRef.h"
#include "include/effects/SkRuntimeEffect.h"
#include "include/gpu/GrRecordingContext.h"
#include "include/private/SkIDChangeListener.h"
#include "include/private/SkImageInfoPriv.h"
#include "include/private/SkTPin.h"
#include "include/private/SkTemplates.h"
#include "src/core/SkAutoMalloc.h"
#include "src/core/SkBlendModePriv.h"
#include "src/core/SkBlenderBase.h"
#include "src/core/SkColorFilterBase.h"
#include "src/core/SkColorSpacePriv.h"
#include "src/core/SkImagePriv.h"
#include "src/core/SkMaskFilterBase.h"
#include "src/core/SkMessageBus.h"
#include "src/core/SkMipmap.h"
#include "src/core/SkPaintPriv.h"
#include "src/core/SkResourceCache.h"
#include "src/core/SkRuntimeEffectPriv.h"
#include "src/core/SkTraceEvent.h"
#include "src/gpu/GrCaps.h"
#include "src/gpu/GrColorInfo.h"
#include "src/gpu/GrColorSpaceXform.h"
#include "src/gpu/GrGpuResourcePriv.h"
#include "src/gpu/GrPaint.h"
#include "src/gpu/GrProxyProvider.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrTextureProxy.h"
#include "src/gpu/GrXferProcessor.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/effects/GrBicubicEffect.h"
#include "src/gpu/effects/GrBlendFragmentProcessor.h"
#include "src/gpu/effects/GrPorterDuffXferProcessor.h"
#include "src/gpu/effects/GrSkSLFP.h"
#include "src/gpu/effects/GrTextureEffect.h"
#include "src/image/SkImage_Base.h"
#include "src/shaders/SkShaderBase.h"

void GrMakeKeyFromImageID(GrUniqueKey* key, uint32_t imageID, const SkIRect& imageBounds) {
    SkASSERT(key);
    SkASSERT(imageID);
    SkASSERT(!imageBounds.isEmpty());
    static const GrUniqueKey::Domain kImageIDDomain = GrUniqueKey::GenerateDomain();
    GrUniqueKey::Builder builder(key, kImageIDDomain, 5, "Image");
    builder[0] = imageID;
    builder[1] = imageBounds.fLeft;
    builder[2] = imageBounds.fTop;
    builder[3] = imageBounds.fRight;
    builder[4] = imageBounds.fBottom;
}

////////////////////////////////////////////////////////////////////////////////

sk_sp<SkIDChangeListener> GrMakeUniqueKeyInvalidationListener(GrUniqueKey* key,
                                                              uint32_t contextID) {
    class Listener : public SkIDChangeListener {
    public:
        Listener(const GrUniqueKey& key, uint32_t contextUniqueID) : fMsg(key, contextUniqueID) {}

        void changed() override {
            SkMessageBus<GrUniqueKeyInvalidatedMessage, uint32_t>::Post(fMsg);
        }

    private:
        GrUniqueKeyInvalidatedMessage fMsg;
    };

    auto listener = sk_make_sp<Listener>(*key, contextID);

    // We stick a SkData on the key that calls invalidateListener in its destructor.
    auto invalidateListener = [](const void* ptr, void* /*context*/) {
        auto listener = reinterpret_cast<const sk_sp<Listener>*>(ptr);
        (*listener)->markShouldDeregister();
        delete listener;
    };
    auto data = SkData::MakeWithProc(new sk_sp<Listener>(listener),
                                     sizeof(sk_sp<Listener>),
                                     invalidateListener,
                                     nullptr);
    SkASSERT(!key->getCustomData());
    key->setCustomData(std::move(data));
    return std::move(listener);
}

sk_sp<GrSurfaceProxy> GrCopyBaseMipMapToTextureProxy(GrRecordingContext* ctx,
                                                     sk_sp<GrSurfaceProxy> baseProxy,
                                                     GrSurfaceOrigin origin,
                                                     SkBudgeted budgeted) {
    SkASSERT(baseProxy);

    // We don't allow this for promise proxies i.e. if they need mips they need to give them
    // to us upfront.
    if (baseProxy->isPromiseProxy()) {
        return nullptr;
    }
    if (!ctx->priv().caps()->isFormatCopyable(baseProxy->backendFormat())) {
        return nullptr;
    }
    auto copy = GrSurfaceProxy::Copy(ctx, std::move(baseProxy), origin, GrMipmapped::kYes,
                                     SkBackingFit::kExact, budgeted);
    if (!copy) {
        return nullptr;
    }
    SkASSERT(copy->asTextureProxy());
    return copy;
}

GrSurfaceProxyView GrCopyBaseMipMapToView(GrRecordingContext* context,
                                          GrSurfaceProxyView src,
                                          SkBudgeted budgeted) {
    auto origin = src.origin();
    auto swizzle = src.swizzle();
    auto proxy = src.refProxy();
    return {GrCopyBaseMipMapToTextureProxy(context, proxy, origin, budgeted), origin, swizzle};
}

static GrMipmapped adjust_mipmapped(GrMipmapped mipmapped,
                                    const SkBitmap& bitmap,
                                    const GrCaps* caps) {
    if (!caps->mipmapSupport() || bitmap.dimensions().area() <= 1) {
        return GrMipmapped::kNo;
    }
    return mipmapped;
}

static GrColorType choose_bmp_texture_colortype(const GrCaps* caps, const SkBitmap& bitmap) {
    GrColorType ct = SkColorTypeToGrColorType(bitmap.info().colorType());
    if (caps->getDefaultBackendFormat(ct, GrRenderable::kNo).isValid()) {
        return ct;
    }
    return GrColorType::kRGBA_8888;
}

static sk_sp<GrTextureProxy> make_bmp_proxy(GrProxyProvider* proxyProvider,
                                            const SkBitmap& bitmap,
                                            GrColorType ct,
                                            GrMipmapped mipmapped,
                                            SkBackingFit fit,
                                            SkBudgeted budgeted) {
    SkBitmap bmpToUpload;
    if (ct != SkColorTypeToGrColorType(bitmap.info().colorType())) {
        SkColorType skCT = GrColorTypeToSkColorType(ct);
        if (!bmpToUpload.tryAllocPixels(bitmap.info().makeColorType(skCT)) ||
            !bitmap.readPixels(bmpToUpload.pixmap())) {
            return {};
        }
        bmpToUpload.setImmutable();
    } else {
        bmpToUpload = bitmap;
    }
    auto proxy = proxyProvider->createProxyFromBitmap(bmpToUpload, mipmapped, fit, budgeted);
    SkASSERT(!proxy || mipmapped == GrMipmapped::kNo || proxy->mipmapped() == GrMipmapped::kYes);
    return proxy;
}

std::tuple<GrSurfaceProxyView, GrColorType>
GrMakeCachedBitmapProxyView(GrRecordingContext* rContext,
                            const SkBitmap& bitmap,
                            GrMipmapped mipmapped) {
    if (!bitmap.peekPixels(nullptr)) {
        return {};
    }

    GrProxyProvider* proxyProvider = rContext->priv().proxyProvider();
    const GrCaps* caps = rContext->priv().caps();

    GrUniqueKey key;
    SkIPoint origin = bitmap.pixelRefOrigin();
    SkIRect subset = SkIRect::MakePtSize(origin, bitmap.dimensions());
    GrMakeKeyFromImageID(&key, bitmap.pixelRef()->getGenerationID(), subset);

    mipmapped = adjust_mipmapped(mipmapped, bitmap, caps);
    GrColorType ct = choose_bmp_texture_colortype(caps, bitmap);

    auto installKey = [&](GrTextureProxy* proxy) {
        auto listener = GrMakeUniqueKeyInvalidationListener(&key, proxyProvider->contextID());
        bitmap.pixelRef()->addGenIDChangeListener(std::move(listener));
        proxyProvider->assignUniqueKeyToProxy(key, proxy);
    };

    sk_sp<GrTextureProxy> proxy = proxyProvider->findOrCreateProxyByUniqueKey(key);
    if (!proxy) {
        proxy = make_bmp_proxy(proxyProvider,
                               bitmap,
                               ct,
                               mipmapped,
                               SkBackingFit::kExact,
                               SkBudgeted::kYes);
        if (!proxy) {
            return {};
        }
        SkASSERT(mipmapped == GrMipmapped::kNo || proxy->mipmapped() == GrMipmapped::kYes);
        installKey(proxy.get());
    }

    GrSwizzle swizzle = caps->getReadSwizzle(proxy->backendFormat(), ct);
    if (mipmapped == GrMipmapped::kNo || proxy->mipmapped() == GrMipmapped::kYes) {
        return {{std::move(proxy), kTopLeft_GrSurfaceOrigin, swizzle}, ct};
    }

    // We need a mipped proxy, but we found a proxy earlier that wasn't mipped. Thus we generate
    // a new mipped surface and copy the original proxy into the base layer. We will then let
    // the gpu generate the rest of the mips.
    auto mippedProxy = GrCopyBaseMipMapToTextureProxy(rContext, proxy, kTopLeft_GrSurfaceOrigin);
    if (!mippedProxy) {
        // We failed to make a mipped proxy with the base copied into it. This could have
        // been from failure to make the proxy or failure to do the copy. Thus we will fall
        // back to just using the non mipped proxy; See skbug.com/7094.
        return {{std::move(proxy), kTopLeft_GrSurfaceOrigin, swizzle}, ct};
    }
    // In this case we are stealing the key from the original proxy which should only happen
    // when we have just generated mipmaps for an originally unmipped proxy/texture. This
    // means that all future uses of the key will access the mipmapped version. The texture
    // backing the unmipped version will remain in the resource cache until the last texture
    // proxy referencing it is deleted at which time it too will be deleted or recycled.
    SkASSERT(proxy->getUniqueKey() == key);
    proxyProvider->removeUniqueKeyFromProxy(proxy.get());
    installKey(mippedProxy->asTextureProxy());
    return {{std::move(mippedProxy), kTopLeft_GrSurfaceOrigin, swizzle}, ct};
}

std::tuple<GrSurfaceProxyView, GrColorType>
GrMakeUncachedBitmapProxyView(GrRecordingContext* rContext,
                              const SkBitmap& bitmap,
                              GrMipmapped mipmapped,
                              SkBackingFit fit,
                              SkBudgeted budgeted) {
    GrProxyProvider* proxyProvider = rContext->priv().proxyProvider();
    const GrCaps* caps = rContext->priv().caps();

    mipmapped = adjust_mipmapped(mipmapped, bitmap, caps);
    GrColorType ct = choose_bmp_texture_colortype(caps, bitmap);

    if (auto proxy = make_bmp_proxy(proxyProvider, bitmap, ct, mipmapped, fit, budgeted)) {
        GrSwizzle swizzle = caps->getReadSwizzle(proxy->backendFormat(), ct);
        SkASSERT(mipmapped == GrMipmapped::kNo || proxy->mipmapped() == GrMipmapped::kYes);
        return {{std::move(proxy), kTopLeft_GrSurfaceOrigin, swizzle}, ct};
    }
    return {};
}
///////////////////////////////////////////////////////////////////////////////

SkPMColor4f SkColorToPMColor4f(SkColor c, const GrColorInfo& colorInfo) {
    SkColor4f color = SkColor4f::FromColor(c);
    if (auto* xform = colorInfo.colorSpaceXformFromSRGB()) {
        color = xform->apply(color);
    }
    return color.premul();
}

SkColor4f SkColor4fPrepForDst(SkColor4f color, const GrColorInfo& colorInfo) {
    if (auto* xform = colorInfo.colorSpaceXformFromSRGB()) {
        color = xform->apply(color);
    }
    return color;
}

///////////////////////////////////////////////////////////////////////////////

static inline bool blend_requires_shader(const SkBlendMode mode) {
    return SkBlendMode::kDst != mode;
}

#ifndef SK_IGNORE_GPU_DITHER
static inline float dither_range_for_config(GrColorType dstColorType) {
    // We use 1 / (2^bitdepth-1) as the range since each channel can hold 2^bitdepth values
    switch (dstColorType) {
        // 4 bit
        case GrColorType::kABGR_4444:
        case GrColorType::kARGB_4444:
        case GrColorType::kBGRA_4444:
            return 1 / 15.f;
        // 6 bit
        case GrColorType::kBGR_565:
            return 1 / 63.f;
        // 8 bit
        case GrColorType::kUnknown:
        case GrColorType::kAlpha_8:
        case GrColorType::kAlpha_8xxx:
        case GrColorType::kGray_8:
        case GrColorType::kGrayAlpha_88:
        case GrColorType::kGray_8xxx:
        case GrColorType::kR_8:
        case GrColorType::kRG_88:
        case GrColorType::kRGB_888:
        case GrColorType::kRGB_888x:
        case GrColorType::kRGBA_8888:
        case GrColorType::kRGBA_8888_SRGB:
        case GrColorType::kBGRA_8888:
            return 1 / 255.f;
        // 10 bit
        case GrColorType::kRGBA_1010102:
        case GrColorType::kBGRA_1010102:
            return 1 / 1023.f;
        // 16 bit
        case GrColorType::kAlpha_16:
        case GrColorType::kR_16:
        case GrColorType::kRG_1616:
        case GrColorType::kRGBA_16161616:
            return 1 / 32767.f;
        // Half
        case GrColorType::kAlpha_F16:
        case GrColorType::kGray_F16:
        case GrColorType::kR_F16:
        case GrColorType::kRG_F16:
        case GrColorType::kRGBA_F16:
        case GrColorType::kRGBA_F16_Clamped:
        // Float
        case GrColorType::kAlpha_F32xxx:
        case GrColorType::kRGBA_F32:
            return 0.f; // no dithering
    }
    SkUNREACHABLE;
}

static SkBitmap make_dither_lut() {
    static constexpr struct DitherTable {
        constexpr DitherTable() : data() {
            for (int x = 0; x < 8; ++x) {
                for (int y = 0; y < 8; ++y) {
                    // The computation of 'm' and 'value' is lifted from CPU backend.
                    unsigned int m = (y & 1) << 5 | (x & 1) << 4 |
                                     (y & 2) << 2 | (x & 2) << 1 |
                                     (y & 4) >> 1 | (x & 4) >> 2;
                    float value = float(m) * 1.0 / 64.0 - 63.0 / 128.0;
                    // Bias by 0.5 to be in 0..1, mul by 255 and round to nearest int to make byte.
                    data[y * 8 + x] = (uint8_t)((value + 0.5) * 255.f + 0.5f);
                }
            }
        }
        uint8_t data[64];
    } gTable;
    SkBitmap bmp;
    bmp.setInfo(SkImageInfo::MakeA8(8, 8));
    bmp.setPixels(const_cast<uint8_t*>(gTable.data));
    bmp.setImmutable();
    return bmp;
}

#ifdef SKIA_OHOS
std::unique_ptr<GrFragmentProcessor> make_dither_effect(
    GrRecordingContext* rContext,
    std::unique_ptr<GrFragmentProcessor> inputFP,
    float range,
    const GrCaps* caps) {
#else
static std::unique_ptr<GrFragmentProcessor> make_dither_effect(
        GrRecordingContext* rContext,
        std::unique_ptr<GrFragmentProcessor> inputFP,
        float range,
        const GrCaps* caps) {
#endif // SKIA_OHOS
    if (range == 0 || inputFP == nullptr) {
        return inputFP;
    }

    if (caps->avoidDithering()) {
        return inputFP;
    }

    // We used to use integer math on sk_FragCoord, when supported, and a fallback using floating
    // point (on a 4x4 rather than 8x8 grid). Now we precompute a 8x8 table in a texture because
    // it was shown to be significantly faster on several devices. Test was done with the following
    // running in viewer with the stats layer enabled and looking at total frame time:
    //      SkRandom r;
    //      for (int i = 0; i < N; ++i) {
    //          SkColor c[2] = {r.nextU(), r.nextU()};
    //          SkPoint pts[2] = {{r.nextRangeScalar(0, 500), r.nextRangeScalar(0, 500)},
    //                            {r.nextRangeScalar(0, 500), r.nextRangeScalar(0, 500)}};
    //          SkPaint p;
    //          p.setDither(true);
    //          p.setShader(SkGradientShader::MakeLinear(pts, c, nullptr, 2, SkTileMode::kRepeat));
    //          canvas->drawPaint(p);
    //      }
    // Device            GPU             N      no dither    int math dither   table dither
    // Linux desktop     QuadroP1000     5000   304ms        400ms (1.31x)     383ms (1.26x)
    // TecnoSpark3Pro    PowerVRGE8320   200    299ms        820ms (2.74x)     592ms (1.98x)
    // Pixel 4           Adreno640       500    110ms        221ms (2.01x)     214ms (1.95x)
    // Galaxy S20 FE     Mali-G77 MP11   600    165ms        360ms (2.18x)     260ms (1.58x)
    static const SkBitmap gLUT = make_dither_lut();
    auto [tex, ct] = GrMakeCachedBitmapProxyView(rContext, gLUT, GrMipmapped::kNo);
    if (!tex) {
        return inputFP;
    }
    SkASSERT(ct == GrColorType::kAlpha_8);
    GrSamplerState sampler(GrSamplerState::WrapMode::kRepeat, SkFilterMode::kNearest);
    auto te = GrTextureEffect::Make(
            std::move(tex), kPremul_SkAlphaType, SkMatrix::I(), sampler, *caps);
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader, R"(
        uniform half range;
        uniform shader table;
        half4 main(float2 xy, half4 color) {
            half value = table.eval(sk_FragCoord.xy).a - 0.5; // undo the bias in the table
            // For each color channel, add the random offset to the channel value and then clamp
            // between 0 and alpha to keep the color premultiplied.
            return half4(clamp(color.rgb + value * range, 0.0, color.a), color.a);
        }
    )", SkRuntimeEffectPriv::ES3Options());
    return GrSkSLFP::Make(effect,
                          "Dither",
                          std::move(inputFP),
                          GrSkSLFP::OptFlags::kPreservesOpaqueInput,
                          "range",
                          range,
                          "table",
                          std::move(te));
}
#endif

static inline bool skpaint_to_grpaint_impl(GrRecordingContext* context,
                                           const GrColorInfo& dstColorInfo,
                                           const SkPaint& skPaint,
                                           const SkMatrixProvider& matrixProvider,
                                           std::unique_ptr<GrFragmentProcessor>* shaderProcessor,
                                           SkBlendMode* primColorMode,
                                           GrPaint* grPaint) {
    // Convert SkPaint color to 4f format in the destination color space
    SkColor4f origColor = SkColor4fPrepForDst(skPaint.getColor4f(), dstColorInfo);

    GrFPArgs fpArgs(context, matrixProvider, &dstColorInfo);

    // Setup the initial color considering the shader, the SkPaint color, and the presence or not
    // of per-vertex colors.
    std::unique_ptr<GrFragmentProcessor> paintFP;
    if (!primColorMode || blend_requires_shader(*primColorMode)) {
        if (shaderProcessor) {
            paintFP = std::move(*shaderProcessor);
        } else {
            if (const SkShaderBase* shader = as_SB(skPaint.getShader())) {
                paintFP = shader->asFragmentProcessor(fpArgs);
                if (paintFP == nullptr) {
                    return false;
                }
            }
        }
    }

    // Set this in below cases if the output of the shader/paint-color/paint-alpha/primXfermode is
    // a known constant value. In that case we can simply apply a color filter during this
    // conversion without converting the color filter to a GrFragmentProcessor.
    bool applyColorFilterToPaintColor = false;
    if (paintFP) {
        if (primColorMode) {
            // There is a blend between the primitive color and the shader color. The shader sees
            // the opaque paint color. The shader's output is blended using the provided mode by
            // the primitive color. The blended color is then modulated by the paint's alpha.

            // The geometry processor will insert the primitive color to start the color chain, so
            // the GrPaint color will be ignored.

            SkPMColor4f shaderInput = origColor.makeOpaque().premul();
            paintFP = GrFragmentProcessor::OverrideInput(std::move(paintFP), shaderInput);
            paintFP = GrBlendFragmentProcessor::Make(std::move(paintFP),
                                                     /*dst=*/nullptr,
                                                     *primColorMode);

            // We can ignore origColor here - alpha is unchanged by gamma
            float paintAlpha = skPaint.getColor4f().fA;
            if (1.0f != paintAlpha) {
                // No gamut conversion - paintAlpha is a (linear) alpha value, splatted to all
                // color channels. It's value should be treated as the same in ANY color space.
                paintFP = GrFragmentProcessor::ModulateRGBA(
                        std::move(paintFP), {paintAlpha, paintAlpha, paintAlpha, paintAlpha});
            }
        } else {
            float paintAlpha = skPaint.getColor4f().fA;
            if (paintAlpha != 1.0f) {
                // This invokes the shader's FP tree with an opaque version of the paint color,
                // then multiplies the final result by the incoming (paint) alpha.
                // We're actually putting the *unpremul* paint color on the GrPaint. This is okay,
                // because the shader is supposed to see the original (opaque) RGB from the paint.
                // ApplyPaintAlpha then creates a valid premul color by applying the paint alpha.
                // Think of this as equivalent to (but faster than) putting origColor.premul() on
                // the GrPaint, and ApplyPaintAlpha unpremuling it before passing it to the child.
                paintFP = GrFragmentProcessor::ApplyPaintAlpha(std::move(paintFP));
                grPaint->setColor4f({origColor.fR, origColor.fG, origColor.fB, origColor.fA});
            } else {
                // paintFP will ignore its input color, so we must disable coverage-as-alpha.
                // TODO(skbug:11942): The alternative would be to always use ApplyPaintAlpha, but
                // we'd need to measure the cost of that shader math against the CAA benefit.
                paintFP = GrFragmentProcessor::DisableCoverageAsAlpha(std::move(paintFP));
                grPaint->setColor4f(origColor.premul());
            }
        }
    } else {
        if (primColorMode) {
            // Examining all of the SkPaintToGrPaintFoo methods and their uses, it turns out that
            // we can only encounter this code path when we're *just* using the primitive color.
            // Literally no code path cares about blending the primitive color with the paint
            // color. This makes sense, if you think about the SkCanvas draw calls that use
            // primitive color - none of them are specified to do anything with paint color.
            SkASSERT(*primColorMode == SkBlendMode::kDst);

            // There is no "blend" - the output of that blend is just the primitive color.
            // We still put the opaque paint color on the GrPaint.
            // TODO: Is this even necessary? It seems entirely superfluous. Any op that uses this
            // code path should be ignoring the paint color, right?
            grPaint->setColor4f(origColor.makeOpaque().premul());

            // The paint's *alpha* is applied to the primitive color:

            // We can ignore origColor here - alpha is unchanged by gamma
            float paintAlpha = skPaint.getColor4f().fA;
            if (1.0f != paintAlpha) {
                // No gamut conversion - paintAlpha is a (linear) alpha value, splatted to all
                // color channels. It's value should be treated as the same in ANY color space.
                paintFP = GrFragmentProcessor::ModulateRGBA(
                        std::move(paintFP), {paintAlpha, paintAlpha, paintAlpha, paintAlpha});
            }
        } else {
            // No shader, no primitive color.
            grPaint->setColor4f(origColor.premul());
            applyColorFilterToPaintColor = true;
        }
    }

    SkColorFilter* colorFilter = skPaint.getColorFilter();
    if (colorFilter) {
        if (applyColorFilterToPaintColor) {
            SkColorSpace* dstCS = dstColorInfo.colorSpace();
            grPaint->setColor4f(colorFilter->filterColor4f(origColor, dstCS, dstCS).premul());
        } else {
            auto [success, fp] = as_CFB(colorFilter)->asFragmentProcessor(std::move(paintFP),
                                                                          context, dstColorInfo);
            if (!success) {
                return false;
            }
            paintFP = std::move(fp);
        }
    }

    SkMaskFilterBase* maskFilter = as_MFB(skPaint.getMaskFilter());
    if (maskFilter) {
        if (auto mfFP = maskFilter->asFragmentProcessor(fpArgs)) {
            grPaint->setCoverageFragmentProcessor(std::move(mfFP));
        }
    }

#ifndef SK_IGNORE_GPU_DITHER
    GrColorType ct = dstColorInfo.colorType();
    if (SkPaintPriv::ShouldDither(skPaint, GrColorTypeToSkColorType(ct)) && paintFP != nullptr) {
        float ditherRange = dither_range_for_config(ct);
        paintFP = make_dither_effect(
                context, std::move(paintFP), ditherRange, context->priv().caps());
    }
#endif

    if (auto bm = skPaint.asBlendMode()) {
        // When the xfermode is null on the SkPaint (meaning kSrcOver) we need the XPFactory field
        // on the GrPaint to also be null (also kSrcOver).
        SkASSERT(!grPaint->getXPFactory());
        if (bm.value() != SkBlendMode::kSrcOver) {
            grPaint->setXPFactory(SkBlendMode_AsXPFactory(bm.value()));
        }
    } else {
        // Apply a custom blend against the surface color, and force the XP to kSrc so that the
        // computed result is applied directly to the canvas while still honoring the alpha.
        paintFP = as_BB(skPaint.getBlender())->asFragmentProcessor(
                std::move(paintFP),
                GrFragmentProcessor::SurfaceColor(),
                fpArgs);
        grPaint->setXPFactory(SkBlendMode_AsXPFactory(SkBlendMode::kSrc));
    }

    if (GrColorTypeClampType(dstColorInfo.colorType()) == GrClampType::kManual) {
        if (paintFP != nullptr) {
            paintFP = GrFragmentProcessor::ClampOutput(std::move(paintFP));
        } else {
            auto color = grPaint->getColor4f();
            grPaint->setColor4f({SkTPin(color.fR, 0.f, 1.f),
                                 SkTPin(color.fG, 0.f, 1.f),
                                 SkTPin(color.fB, 0.f, 1.f),
                                 SkTPin(color.fA, 0.f, 1.f)});
        }
    }

    if (paintFP) {
        grPaint->setColorFragmentProcessor(std::move(paintFP));
    }

    return true;
}

bool SkPaintToGrPaint(GrRecordingContext* context,
                      const GrColorInfo& dstColorInfo,
                      const SkPaint& skPaint,
                      const SkMatrixProvider& matrixProvider,
                      GrPaint* grPaint) {
    return skpaint_to_grpaint_impl(context, dstColorInfo, skPaint, matrixProvider,
                                   /*shaderProcessor=*/nullptr, /*primColorMode=*/nullptr, grPaint);
}

/** Replaces the SkShader (if any) on skPaint with the passed in GrFragmentProcessor. */
bool SkPaintToGrPaintReplaceShader(GrRecordingContext* context,
                                   const GrColorInfo& dstColorInfo,
                                   const SkPaint& skPaint,
                                   const SkMatrixProvider& matrixProvider,
                                   std::unique_ptr<GrFragmentProcessor> shaderFP,
                                   GrPaint* grPaint) {
    if (!shaderFP) {
        return false;
    }
    return skpaint_to_grpaint_impl(context, dstColorInfo, skPaint, matrixProvider, &shaderFP,
                                   /*primColorMode=*/nullptr, grPaint);
}

/** Blends the SkPaint's shader (or color if no shader) with a per-primitive color which must
    be setup as a vertex attribute using the specified SkBlendMode. */
bool SkPaintToGrPaintWithBlend(GrRecordingContext* context,
                               const GrColorInfo& dstColorInfo,
                               const SkPaint& skPaint,
                               const SkMatrixProvider& matrixProvider,
                               SkBlendMode primColorMode,
                               GrPaint* grPaint) {
    return skpaint_to_grpaint_impl(context, dstColorInfo, skPaint, matrixProvider,
                                   /*shaderProcessor=*/nullptr, &primColorMode, grPaint);
}
