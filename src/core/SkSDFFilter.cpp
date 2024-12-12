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

#ifdef SK_ENABLE_SDF_BLUR_SWITCH
#include <parameters.h>
#endif

namespace SDFBlur {

static bool GetSDFBlurEnabled()
{
#ifdef SK_ENABLE_SDF_BLUR_SWITCH
    constexpr int enableFlag = 1;
    static bool enabled = std::atoi(
        (OHOS::system::GetParameter("persist.sys.graphic.SDFBlurEnabled", "1")).c_str()) == enableFlag;
    return enabled;
#else
    return false;
#endif
}

bool GetSDFBlurDebugTraceEnabled()
{
#ifdef SK_ENABLE_SDF_BLUR_SWITCH
    constexpr int enableFlag = 1;
    static bool enabled = std::atoi(
        (OHOS::system::GetParameter("persist.sys.graphic.SDFBlurDebugTraceEnabled", "0")).c_str()) == enableFlag;
    return enabled;
#else
    return false;
#endif
}

// Only the equal Radii RRect use SDFblur.
bool isSDFBlur(const GrStyledShape& shape)
{
    SkRRect srcRRect;
    bool inverted;
    if (!GetSDFBlurEnabled() || !shape.asRRect(&srcRRect, nullptr, nullptr, &inverted) || inverted ||
        (!(srcRRect.getType() == SkRRect::kSimple_Type) && !(srcRRect.getType() == SkRRect::kNinePatch_Type))) {
        return false;
    }
    return true;
}

bool drawMaskSDFBlur(GrRecordingContext* rContext, skgpu::v1::SurfaceDrawContext* sdc, const GrClip* clip,
    const SkMatrix& viewMatrix, const SkIRect& maskBounds, GrPaint&& paint, GrSurfaceProxyView mask,
    const SkMaskFilterBase* maskFilter, const GrStyledShape& shape)
{
    float noxFormedSigma3 = maskFilter->getNoxFormedSigma3();
    mask.concatSwizzle(GrSwizzle("aaaa"));
    SkRRect srcRRect;
    bool inverted;
    shape.asRRect(&srcRRect, nullptr, nullptr, &inverted);
    float r = srcRRect.getSimpleRadii().x();
    float areaLen = std::max(std::min({srcRRect.width(), srcRRect.height(), noxFormedSigma3}) * SK_ScalarHalf, r);

    // This vector represents the origin offset vector, not just half the width and height.
    SkV2 originOffset = {srcRRect.width() * SK_ScalarHalf - areaLen, srcRRect.height() * SK_ScalarHalf - areaLen};
    SkMatrix matrixTrans = SkMatrix::Translate(-SkIntToScalar(noxFormedSigma3), -SkIntToScalar(noxFormedSigma3));
    SkMatrix matrix;
    matrix.preConcat(viewMatrix);
    matrix.preConcat(matrixTrans);
    // add dither effect to reduce color discontinuity
    constexpr float ditherRange = 1.f / 255.f;
    auto inputFp = GrTextureEffect::Make(std::move(mask), kUnknown_SkAlphaType);
    SkPMColor4f origColor = paint.getColor4f();

    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader, R"(
        uniform shader fp;
        uniform half4 colorPaint;
        uniform vec2 originOffset;
        half4 main(float2 xy) {
            vec2 uv;
            vec2 pos = abs(xy) - originOffset;
            half4 colorMask;
            vec2 isCorner = step(0, pos);
            if (isCorner == vec2(0, 0)) {
                return colorPaint;
            }
            uv = pos * isCorner;
            colorMask = fp.eval(uv);
            return colorMask * colorPaint;
        }
    )");
    SkASSERT(SkRuntimeEffectPriv::SupportsConstantOutputForConstantInput(effect));
    auto inputFP = GrSkSLFP::Make(effect, "OverrideInput", nullptr,
        origColor.isOpaque() ? GrSkSLFP::OptFlags::kPreservesOpaqueInput : GrSkSLFP::OptFlags::kNone,
        "fp", std::move(inputFp), "colorPaint", origColor, "originOffset", originOffset);

    SkMatrix matrixOffset;
    matrixOffset.setTranslateX(-noxFormedSigma3 - srcRRect.rect().fLeft - srcRRect.width() * SK_ScalarHalf);
    matrixOffset.setTranslateY(-noxFormedSigma3 - srcRRect.rect().fTop - srcRRect.height() * SK_ScalarHalf);

    auto inputFPOffset = GrMatrixEffect::Make(matrixOffset, std::move(inputFP));

    auto paintFP = GrBlendFragmentProcessor::Make(std::move(inputFPOffset), nullptr, SkBlendMode::kSrc);
    
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
    const SkMatrix& viewMatrix, const SkRRect& srcRRect)
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

    SkScalar sx = viewMatrix.getScaleX();
    SkScalar sy = viewMatrix.getScaleY();
    sdc->drawPaint(nullptr, std::move(paint), SkMatrix::I().Scale(sx, sy));

    return sdc;
}

std::unique_ptr<skgpu::v1::SurfaceDrawContext> SDFBlur(GrRecordingContext* rContext,
    GrSurfaceProxyView srcView, GrColorType srcColorType, SkAlphaType srcAlphaType,
    sk_sp<SkColorSpace> colorSpace, SkIRect dstBounds, SkIRect srcBounds, float noxFormedSigma,
    SkTileMode mode, const SkMatrix& viewMatrix, const SkRRect& srcRRect, SkBackingFit fit)
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
                  std::move(colorSpace), fit,  viewMatrix, srcRRect);
}
} // SDFBlur
