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

#ifndef Sk_SDF_Filter_DEFINED
#define Sk_SDF_Filter_DEFINED

#include "src/core/SkMaskFilterBase.h"

#include "src/gpu/effects/GrSDFBlurEffect.h"
#include "src/gpu/effects/GrTextureEffect.h"
#include "src/gpu/geometry/GrStyledShape.h"
#include "src/gpu/v1/SurfaceDrawContext_v1.h"


namespace skgpu { namespace v1 { class SurfaceDrawContext; }}

namespace SDFBlur {

static constexpr auto kMaskOrigin = kTopLeft_GrSurfaceOrigin;

struct DrawRectData {
    SkIVector fOffset;
    SkISize   fSize;
};

bool isSDFBlur(const GrStyledShape& shape);

bool draw_mask_SDFBlur(skgpu::v1::SurfaceDrawContext* sdc, const GrClip* clip, const SkMatrix& viewMatrix,
    const SkIRect& maskBounds, GrPaint&& paint, GrSurfaceProxyView mask, const SkMaskFilterBase* maskFilter);

std::unique_ptr<skgpu::v1::SurfaceDrawContext> SDFBlur(GrRecordingContext* rContext,
    GrSurfaceProxyView srcView, GrColorType srcColorType, SkAlphaType srcAlphaType,
    sk_sp<SkColorSpace> colorSpace, SkIRect dstBounds, SkIRect srcBounds, float noxFormedSigma,
    SkTileMode mode, const SkRRect& srcRRect, SkBackingFit fit = SkBackingFit::kApprox);

} // SDFBlur
#endif
