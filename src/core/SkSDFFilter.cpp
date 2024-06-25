/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/core/SkSDFFilter.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/effects/GrBlendFragmentProcessor.h"

namespace SDFBlur {

// Only the equal Radii RRect use SDFblur.
bool isSDFBlur(const GrStyledShape& shape)
{
    SkRRect srcRRect;
    bool inverted;
    if (!shape.asRRect(&srcRRect, nullptr, nullptr, &inverted) || inverted ||
        (!(srcRRect.getType() == SkRRect::kSimple_Type) && !(srcRRect.getType() == SkRRect::kNinePatch_Type))) {
        return false;
    }
    return true;
}

void GetSDFBlurScaleFactor(const SkRRect srcRRect, SkScalar& sx, SkScalar& sy)
{
    constexpr float minScaleFactor = 1.f;
    constexpr float maxScaleFactor = 3.f;
    constexpr float sizeThreshold = 500.f;
    int srcRRectW = srcRRect.rect().width();
    int srcRRectH = srcRRect.rect().height();
    // When the input size is greater than the threshold, it needs to be scaled. scale factor will be clamped in [1.0, 3.0].
    int scaleX = std::max(minScaleFactor, std::min(std::ceil(srcRRectW / sizeThreshold), maxScaleFactor));
    int scaleY = std::max(minScaleFactor, std::min(std::ceil(srcRRectH / sizeThreshold), maxScaleFactor));
    sx = SK_Scalar1 / scaleX;
    sy = SK_Scalar1 / scaleY;
}

bool drawMaskSDFBlur(GrRecordingContext* rContext, skgpu::v1::SurfaceDrawContext* sdc, const GrClip* clip, const SkMatrix& viewMatrix,
    const SkIRect& maskBounds, GrPaint&& paint, GrSurfaceProxyView mask, const SkMaskFilterBase* maskFilter,
    const SkScalar sx, const SkScalar sy)
{
    float noxFormedSigma3 = maskFilter->getNoxFormedSigma3();
    mask.concatSwizzle(GrSwizzle("aaaa"));

    SkMatrix matrixTrans =
            SkMatrix::Translate(-SkIntToScalar(noxFormedSigma3), -SkIntToScalar(noxFormedSigma3));
    SkMatrix matrixInverseScale =SkMatrix::Scale(1 / sx, 1 / sy);
    SkMatrix matrix;
    matrix.preConcat(viewMatrix);
    matrix.preConcat(matrixTrans);
    matrix.preConcat(matrixInverseScale);
    // add dither effect to reduce color discontinuity
    constexpr float ditherRange = 1.0 / 255.0;
    auto inputFp = GrTextureEffect::Make(std::move(mask), kUnknown_SkAlphaType);
    SkPMColor4f origColor = paint.getColor4f();

    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader, R"(
        uniform shader fp;
        uniform half4 colorPaint;
        half4 main(float2 pos) {

            half4 colorMask = fp.eval(pos);
            return colorMask * colorPaint;
        }
    )");
    SkASSERT(SkRuntimeEffectPriv::SupportsConstantOutputForConstantInput(effect));
    auto inputFP = GrSkSLFP::Make(effect, "OverrideInput", nullptr, 
        origColor.isOpaque() ? GrSkSLFP::OptFlags::kPreservesOpaqueInput : GrSkSLFP::OptFlags::kNone,
        "fp", std::move(inputFp), "colorPaint", origColor);

    auto paintFP = GrBlendFragmentProcessor::Make(std::move(inputFP),
                                                /*dst=*/nullptr,
                                                SkBlendMode::kSrc);
#ifndef SK_IGNORE_GPU_DITHER
    paintFP = make_dither_effect(rContext, std::move(paintFP), ditherRange, rContext->priv().caps());
#endif
    paint.setColorFragmentProcessor(std::move(paintFP));
    sdc->drawRect(clip, std::move(paint), GrAA::kYes, matrix,
                  SkRect::MakeXYWH(0, 0, maskBounds.width(), maskBounds.height()));
    return true;
}

static std::unique_ptr<skgpu::v1::SurfaceDrawContext> sdf_2d(GrRecordingContext* rContext,
    GrSurfaceProxyView srcView, GrColorType srcColorType, const SkIRect& srcBounds, const SkIRect& dstBounds,
    float noxFormedSigma, SkTileMode mode, sk_sp<SkColorSpace> finalCS, SkBackingFit dstFit,
    const SkRRect& srcRRect)
{
    auto sdc = skgpu::v1::SurfaceDrawContext::Make(
        rContext, srcColorType, std::move(finalCS), dstFit, dstBounds.size(), SkSurfaceProps(),
        1, GrMipmapped::kNo, srcView.proxy()->isProtected(), srcView.origin());
    if (!sdc) {
        return nullptr;
    }

    GrPaint paint;
    auto sdfFp = GrSDFBlurEffect::Make(rContext, noxFormedSigma, srcRRect);

    if (!sdfFp) {
        return nullptr;
    }

    paint.setColorFragmentProcessor(std::move(sdfFp));
    paint.setPorterDuffXPFactory(SkBlendMode::kSrc);

    SkScalar sx = 1.0;
    SkScalar sy = 1.0;
    SDFBlur::GetSDFBlurScaleFactor(srcRRect, sx, sy);
    sdc->drawPaint(nullptr, std::move(paint), SkMatrix::I().Scale(sx, sy));

    return sdc;
}

std::unique_ptr<skgpu::v1::SurfaceDrawContext> SDFBlur(GrRecordingContext* rContext,
    GrSurfaceProxyView srcView, GrColorType srcColorType, SkAlphaType srcAlphaType,
    sk_sp<SkColorSpace> colorSpace, SkIRect dstBounds, SkIRect srcBounds, float noxFormedSigma,
    SkTileMode mode, const SkRRect& srcRRect, SkBackingFit fit)
{
    SkASSERT(rContext);
    TRACE_EVENT0("skia.gpu", "SDFBlur");

    if (!srcView.asTextureProxy()) {
        return nullptr;
    }

    int maxRenderTargetSize = rContext->priv().caps()->maxRenderTargetSize();
    if (dstBounds.width() > maxRenderTargetSize || dstBounds.height() > maxRenderTargetSize) {
        return nullptr;
    }

    return sdf_2d(rContext, std::move(srcView), srcColorType, srcBounds, dstBounds, noxFormedSigma, mode,
                  std::move(colorSpace), fit, srcRRect);
}
} // SDFBlur
