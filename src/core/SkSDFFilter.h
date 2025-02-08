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

#ifndef SK_SDF_FILTER_DEFINED
#define SK_SDF_FILTER_DEFINED
#ifdef SKIA_OHOS

#include "src/core/SkMaskFilterBase.h"

#include "src/gpu/effects/GrTextureEffect.h"
#include "src/gpu/geometry/GrStyledShape.h"
#include "src/gpu/v1/SurfaceDrawContext_v1.h"
#include "include/gpu/GrRecordingContext.h"


namespace skgpu { namespace v1 { class SurfaceDrawContext; }}

namespace SDFBlur {

static constexpr auto kMaskOrigin = kTopLeft_GrSurfaceOrigin;

struct DrawRectData {
    SkIVector fOffset;
    SkISize   fSize;
};

bool GetSDFBlurEnabled();

bool isSimpleRRectSDF(const SkRRect& srcRRect);

bool isComplexRRectSDF(const SkRRect& srcRRect);

bool GetSDFBlurDebugTraceEnabled();


} // SDFBlur
#endif // SKIA_OHOS
#endif
