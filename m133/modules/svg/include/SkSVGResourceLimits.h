/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef SkSVGResourceLimits_DEFINED
#define SkSVGResourceLimits_DEFINED

#include "include/private/base/SkAPI.h"

#include <cstddef>

#ifdef SKIA_OHOS_SVG_PROTECTION

struct SK_API SkSVGResourceLimits {
    enum class Preset {
        kSmall,
        kMedium,
        kLarge,
    };

    // All fields must be set to positive, non-zero limits before calling a
    // protected SVG construction API. The zero-initialized state is invalid
    // and is rejected as an unsafe configuration.
    SkSVGResourceLimits();

    // Returns a fully configured limit set.
    static SkSVGResourceLimits MakePreset(Preset);

    int    fMaxSVGRecursionDepth;
    size_t fMaxDOMNodeCount;
    size_t fMaxArenaAllocBytes;

    size_t fMaxStyleTextLen;
    size_t fMaxTextUtf8Bytes;
    size_t fMaxCssStyleEntries;
    size_t fMaxCssDeclarationsPerRule;
    size_t fMaxClassFanOut;

    size_t fMaxListAttributeCount;
    size_t fMaxPointsCount;
    size_t fMaxPathSegmentCount;

    size_t fMaxLayerEffectPixels;
    size_t fMaxFilterFanIn;
};

#endif  // SKIA_OHOS_SVG_PROTECTION

#endif  // SkSVGResourceLimits_DEFINED
