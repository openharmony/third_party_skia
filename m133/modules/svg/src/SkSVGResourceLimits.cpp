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

#include "modules/svg/include/SkSVGResourceLimits.h"

#ifdef SKIA_OHOS_SVG_PROTECTION

SkSVGResourceLimits::SkSVGResourceLimits()
        : fMaxSVGRecursionDepth(0)
        , fMaxDOMNodeCount(0)
        , fMaxArenaAllocBytes(0)
        , fMaxStyleTextLen(0)
        , fMaxTextUtf8Bytes(0)
        , fMaxCssStyleEntries(0)
        , fMaxCssDeclarationsPerRule(0)
        , fMaxClassFanOut(0)
        , fMaxListAttributeBytes(0)
        , fMaxPointsCount(0)
        , fMaxPathSegmentCount(0)
        , fMaxLayerEffectPixels(0)
        , fMaxFilterFanIn(0) {}

SkSVGResourceLimits SkSVGResourceLimits::MakePreset(Preset preset) {
    SkSVGResourceLimits limits;

    switch (preset) {
        case Preset::kSmall:
        case Preset::kMedium:
        case Preset::kLarge:
            limits.fMaxSVGRecursionDepth = 256;
            limits.fMaxDOMNodeCount = 100'000;
            limits.fMaxArenaAllocBytes = 64 * 1024 * 1024;
            limits.fMaxStyleTextLen = 1024 * 1024;
            limits.fMaxTextUtf8Bytes = 1024 * 1024;
            limits.fMaxCssStyleEntries = 10'000;
            limits.fMaxCssDeclarationsPerRule = 1'000;
            limits.fMaxClassFanOut = 1'000'000;
            limits.fMaxListAttributeBytes = 1'000'000;
            limits.fMaxPointsCount = 100'000;
            limits.fMaxPathSegmentCount = 100'000;
            limits.fMaxLayerEffectPixels = 500'000'000;
            limits.fMaxFilterFanIn = 10;
            break;
    }

    return limits;
}

#endif  // SKIA_OHOS_SVG_PROTECTION
