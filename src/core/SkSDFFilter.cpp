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

#ifdef SKIA_OHOS
 
#include "src/core/SkSDFFilter.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/effects/GrBlendFragmentProcessor.h"

#ifdef SK_ENABLE_SDF_BLUR_SWITCH
#include <parameters.h>
#endif

namespace SDFBlur {

bool GetSDFBlurEnabled()
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

// Simple RRect use SDFShadow.
bool isSimpleRRectSDF(const SkRRect& srcRRect)
{
    if (srcRRect.isSimple() || srcRRect.isNinePatch()) {
        return true;
    }
    return false;
}

// Complex RRect use SDFShadow.
bool isComplexRRectSDF(const SkRRect& srcRRect)
{
    constexpr SkScalar tolerance = 0.001f;
    if (srcRRect.isComplex() &&
        SkScalarNearlyEqual(srcRRect.radii(SkRRect::Corner::kLowerLeft_Corner).x(),
                            srcRRect.radii(SkRRect::Corner::kLowerRight_Corner).x(), tolerance) &&
        SkScalarNearlyEqual(srcRRect.radii(SkRRect::Corner::kUpperLeft_Corner).x(),
                            srcRRect.radii(SkRRect::Corner::kUpperRight_Corner).x(), tolerance)) {
        return true;
    }
    return false;
}

} // SDFBlur
#endif // SKIA_OHOS
