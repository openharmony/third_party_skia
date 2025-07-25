/*
 * Copyright 2011 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/effects/SkImageFilters.h"

#include "include/core/SkColorType.h"
#include "include/core/SkFlattenable.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSize.h"
#include "include/core/SkTileMode.h"
#include "include/core/SkTypes.h"
#include "include/private/base/SkFloatingPoint.h"
#include "include/private/base/SkTo.h"
#include "src/core/SkBlurEngine.h"
#include "src/core/SkImageFilterTypes.h"
#include "src/core/SkImageFilter_Base.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkSpecialImage.h"
#include "src/core/SkWriteBuffer.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace {

class SkBlurImageFilter final : public SkImageFilter_Base {
public:
    SkBlurImageFilter(SkSize sigma, sk_sp<SkImageFilter> input)
            : SkImageFilter_Base(&input, 1)
            , fSigma{sigma} {}

    SkBlurImageFilter(SkSize sigma, SkTileMode legacyTileMode, sk_sp<SkImageFilter> input)
            : SkImageFilter_Base(&input, 1)
            , fSigma(sigma)
            , fLegacyTileMode(legacyTileMode) {}

    SkRect computeFastBounds(const SkRect&) const override;

protected:
    void flatten(SkWriteBuffer&) const override;

private:
    friend void ::SkRegisterBlurImageFilterFlattenable();
    SK_FLATTENABLE_HOOKS(SkBlurImageFilter)

    skif::FilterResult onFilterImage(const skif::Context& context) const override;

    skif::LayerSpace<SkIRect> onGetInputLayerBounds(
            const skif::Mapping& mapping,
            const skif::LayerSpace<SkIRect>& desiredOutput,
            std::optional<skif::LayerSpace<SkIRect>> contentBounds) const override;

    std::optional<skif::LayerSpace<SkIRect>> onGetOutputLayerBounds(
            const skif::Mapping& mapping,
            std::optional<skif::LayerSpace<SkIRect>> contentBounds) const override;

    skif::LayerSpace<SkSize> mapSigma(const skif::Mapping& mapping, bool useBlurEngine) const;

    skif::LayerSpace<SkIRect> kernelBounds(const skif::Mapping& mapping,
                                           skif::LayerSpace<SkIRect> bounds,
                                           bool useBlurEngine) const {
        skif::LayerSpace<SkSize> sigma = this->mapSigma(mapping, useBlurEngine);
        bounds.outset(skif::LayerSpace<SkSize>({3 * sigma.width(), 3 * sigma.height()}).ceil());
        return bounds;
    }

    skif::ParameterSpace<SkSize> fSigma;
    // kDecal means no legacy tiling, it will be handled by SkCropImageFilter instead. Legacy
    // tiling occurs when there's no provided crop rect, and should be deleted once clients create
    // their filters with defined tiling geometry.
    SkTileMode fLegacyTileMode = SkTileMode::kDecal;
};

} // end namespace

sk_sp<SkImageFilter> SkImageFilters::Blur(
        SkScalar sigmaX, SkScalar sigmaY, SkTileMode tileMode, sk_sp<SkImageFilter> input,
        const CropRect& cropRect) {
    if (sigmaX < SK_ScalarNearlyZero && sigmaY < SK_ScalarNearlyZero && !cropRect) {
        return input;
    }

    if (!SkIsFinite(sigmaX, sigmaY) || sigmaX < 0.f || sigmaY < 0.f) {
        // Non-finite or negative sigmas are error conditions. We allow 0 sigma for X and/or Y
        // for 1D blurs; onFilterImage() will detect when no visible blurring would occur based on
        // the Context mapping.
        return nullptr;
    }

    // Temporarily allow tiling with no crop rect
    if (tileMode != SkTileMode::kDecal && !cropRect) {
        return sk_make_sp<SkBlurImageFilter>(SkSize{sigmaX, sigmaY}, tileMode, std::move(input));
    }

    // The 'tileMode' behavior is not well-defined if there is no crop. We only apply it if
    // there is a provided 'cropRect'.
    sk_sp<SkImageFilter> filter = std::move(input);
    if (tileMode != SkTileMode::kDecal && cropRect) {
        // Historically the input image was restricted to the cropRect when tiling was not
        // kDecal, so that the kernel evaluated the tiled edge conditions, while a kDecal crop
        // only affected the output.
        filter = SkImageFilters::Crop(*cropRect, tileMode, std::move(filter));
    }

    filter = sk_make_sp<SkBlurImageFilter>(SkSize{sigmaX, sigmaY}, std::move(filter));
    if (cropRect) {
        // But regardless of the tileMode, the output is always decal cropped
        filter = SkImageFilters::Crop(*cropRect, SkTileMode::kDecal, std::move(filter));
    }
    return filter;
}

void SkRegisterBlurImageFilterFlattenable() {
    SK_REGISTER_FLATTENABLE(SkBlurImageFilter);
    SkFlattenable::Register("SkBlurImageFilterImpl", SkBlurImageFilter::CreateProc);
}

sk_sp<SkFlattenable> SkBlurImageFilter::CreateProc(SkReadBuffer& buffer) {
    SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 1);
    SkScalar sigmaX = buffer.readScalar();
    SkScalar sigmaY = buffer.readScalar();
    SkTileMode tileMode = buffer.read32LE(SkTileMode::kLastTileMode);

    // NOTE: For new SKPs, 'tileMode' holds the "legacy" tile mode; any originally specified tile
    // mode with valid tiling geometry is handled in the SkCropImageFilters that wrap the blur.
    // In a new SKP, when 'tileMode' is not kDecal, common.cropRect() will be null and the blur
    // will automatically emulate the legacy tiling.
    //
    // In old SKPs, the 'tileMode' and common.cropRect() may not be null. ::Blur() automatically
    // detects when this is a legacy or valid tiling and constructs the DAG appropriately.
    return SkImageFilters::Blur(
          sigmaX, sigmaY, tileMode, common.getInput(0), common.cropRect());
}

void SkBlurImageFilter::flatten(SkWriteBuffer& buffer) const {
    this->SkImageFilter_Base::flatten(buffer);

    buffer.writeScalar(SkSize(fSigma).fWidth);
    buffer.writeScalar(SkSize(fSigma).fHeight);
    buffer.writeInt(static_cast<int>(fLegacyTileMode));
}

///////////////////////////////////////////////////////////////////////////////

namespace {

// This rather arbitrary-looking value results in a maximum box blur kernel size of 1000 pixels on
// the raster path, which matches the WebKit and Firefox implementations. Since the GPU path does
// not compute a box blur, putting the limit on sigma ensures consistent behaviour between the GPU
// and raster paths.
static constexpr SkScalar kMaxSigma = 532.f;

}  // namespace

skif::FilterResult SkBlurImageFilter::onFilterImage(const skif::Context& ctx) const {
    const bool useBlurEngine = SkToBool(ctx.backend()->getBlurEngine());

    skif::Context inputCtx = ctx.withNewDesiredOutput(
            this->kernelBounds(ctx.mapping(), ctx.desiredOutput(), useBlurEngine));

    skif::FilterResult childOutput = this->getChildOutput(0, inputCtx);
    skif::LayerSpace<SkSize> sigma = this->mapSigma(ctx.mapping(), useBlurEngine);
    if (sigma.width() == 0.f && sigma.height() == 0.f) {
        // No actual blur, so just return the input unmodified
        return childOutput;
    }

    SkASSERT(sigma.width() >= 0.f && sigma.width() <= kMaxSigma &&
             sigma.height() >= 0.f && sigma.height() <= kMaxSigma);

    // By default, FilterResult::blur() will calculate a more optimal output automatically, so
    // convey the original output to it.
    skif::LayerSpace<SkIRect> maxOutput = ctx.desiredOutput();
    if (!useBlurEngine || fLegacyTileMode != SkTileMode::kDecal) {
        maxOutput = this->kernelBounds(ctx.mapping(), childOutput.layerBounds(), useBlurEngine);
        if (!maxOutput.intersect(ctx.desiredOutput())) {
            return {};
        }
    }
    if (fLegacyTileMode != SkTileMode::kDecal) {
        // Legacy tiling applied to the input image when there was no explicit crop rect. Use the
        // child's output image's layer bounds as the crop rectangle to adjust the edge tile mode
        // without restricting the image.
        childOutput = childOutput.applyCrop(inputCtx,
                                            childOutput.layerBounds(),
                                            fLegacyTileMode);
    }

    // TODO(b/40039877): Once the CPU blur functions can handle tile modes and color types beyond
    // N32, there won't be any need to branch on how to apply the blur to the filter result.
    if (useBlurEngine) {
        // For non-legacy tiling, 'maxOutput' is equal to the desired output. For decal's it matches
        // what Builder::blur() calculates internally. For legacy tiling, however, it's dependent on
        // the original child output's bounds ignoring the tile mode's effect.
        skif::Context croppedOutput = ctx.withNewDesiredOutput(maxOutput);
        skif::FilterResult::Builder builder{croppedOutput};
        builder.add(childOutput);
        return builder.blur(sigma);
    }

    // The legacy CPU blur does not yet support tile modes so explicitly resolve it to a special
    // image that has the tiling rendered into the pixels.

    auto [resolvedChildOutput, origin] = childOutput.imageAndOffset(inputCtx);
    if (!resolvedChildOutput) {
        return {};
    }
    SkIRect srcRect = SkIRect::MakeSize(resolvedChildOutput->dimensions());
    SkIRect srcRelativeOutput = SkIRect(maxOutput).makeOffset(-origin.x(), -origin.y());

    // These parameters will always select the successive box-blur algorithm for 8888 data,
    // matching the legacy behavior when that code was defined inline here.
    const SkBlurEngine::Algorithm* legacyBlur = SkBlurEngine::GetRasterBlurEngine()->findAlgorithm(
            /*sigma=*/{kMaxSigma, kMaxSigma}, /*colorType=*/kN32_SkColorType);
    sk_sp<SkSpecialImage> blurResult = legacyBlur->blur(SkSize(sigma),
                                                        std::move(resolvedChildOutput),
                                                        srcRect,
                                                        SkTileMode::kDecal,
                                                        srcRelativeOutput);
    return skif::FilterResult{std::move(blurResult), maxOutput.topLeft()};
}

skif::LayerSpace<SkSize> SkBlurImageFilter::mapSigma(const skif::Mapping& mapping,
                                                     bool useBlurEngine) const {
    skif::LayerSpace<SkSize> sigma = mapping.paramToLayer(fSigma);
    // Clamp to the maximum sigma
    sigma = skif::LayerSpace<SkSize>({std::min(sigma.width(), kMaxSigma),
                                      std::min(sigma.height(), kMaxSigma)});

    // TODO(b/294575803) - The legacy CPU blur uses only the successive box-blur which is both
    // inaccurate for sigma < 2 and unable to represent blurring when its window is <= 1 (sigma
    // below ~0.8). Until everything is using the blur engine, support both notions of identity.

    // Disable bluring on axes that are not finite, or that are small enough that the blur is
    // effectively an identity.
    if (!SkIsFinite(sigma.width()) ||
        (!useBlurEngine && SkBlurEngine::BoxBlurWindow(sigma.width()) <= 1) ||
        (useBlurEngine && SkBlurEngine::IsEffectivelyIdentity(sigma.width()))) {
        sigma = skif::LayerSpace<SkSize>({0.f, sigma.height()});
    }

    if (!SkIsFinite(sigma.height()) ||
        (!useBlurEngine && SkBlurEngine::BoxBlurWindow(sigma.height()) <= 1) ||
        (useBlurEngine && SkBlurEngine::IsEffectivelyIdentity(sigma.height()))) {
        sigma = skif::LayerSpace<SkSize>({sigma.width(), 0.f});
    }

    return sigma;
}

skif::LayerSpace<SkIRect> SkBlurImageFilter::onGetInputLayerBounds(
        const skif::Mapping& mapping,
        const skif::LayerSpace<SkIRect>& desiredOutput,
        std::optional<skif::LayerSpace<SkIRect>> contentBounds) const {
    // Use useBlurEngine=true since that has a more sensitive kernel, ensuring any layer input
    // bounds will be sufficient for both blur engine and legacy CPU evaluations.
    skif::LayerSpace<SkIRect> requiredInput =
            this->kernelBounds(mapping, desiredOutput, /*useBlurEngine=*/true);
    return this->getChildInputLayerBounds(0, mapping, requiredInput, contentBounds);
}

std::optional<skif::LayerSpace<SkIRect>> SkBlurImageFilter::onGetOutputLayerBounds(
        const skif::Mapping& mapping,
        std::optional<skif::LayerSpace<SkIRect>> contentBounds) const {
    auto childOutput = this->getChildOutputLayerBounds(0, mapping, contentBounds);
    if (childOutput) {
        // Use useBlurEngine=true since it will ensure output bounds are conservative; legacy
        // CPU-based blurs may produce 1px inset from this for very small sigmas.
        return this->kernelBounds(mapping, *childOutput, /*useBlurEngine=*/true);
    } else {
        return skif::LayerSpace<SkIRect>::Unbounded();
    }
}

SkRect SkBlurImageFilter::computeFastBounds(const SkRect& src) const {
    SkRect bounds = this->getInput(0) ? this->getInput(0)->computeFastBounds(src) : src;
    bounds.outset(SkSize(fSigma).width() * 3, SkSize(fSigma).height() * 3);
    return bounds;
}
