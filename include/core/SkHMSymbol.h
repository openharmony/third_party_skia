/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef LIB_RS_SRC_HM_SYMBOL_H_
#define LIB_RS_SRC_HM_SYMBOL_H_
#include <iostream>
#include <vector>
#include <string>
#include <map>

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMetrics.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkDiscretePathEffect.h"

enum AnimationType {
    INVALID_ANIMATION_TYPE = 0,
    SCALE_EFFECT = 1,
    VARIABLE_COLOR = 2,
};

enum AnimationSubType {
    INVALID_ANIMATION_SUB_TYPE = 0,
    UNIT = 1,
    VARIABLE_3_GROUP = 2,
    VARIABLE_4_GROUP = 3,
};

enum CurveType {
    INVALID_CURVE_TYPE = 0,
    SPRING = 1,
    LINEAR = 2,
};

using PiecewiseParameter = struct PiecewiseParameter {
    CurveType curveType_;
    std::map<std::string, double_t> curveArgs_;
    uint32_t duration_;
    uint32_t delay_;
    std::map<std::string, std::vector<double_t>> properties_;
};

using AnimationPara = struct AnimationPara {
    uint32_t animationMode_;
    AnimationSubType subType_;
    std::vector<std::vector<PiecewiseParameter>> groupParameters_;
};

using AnimationInfo = struct AnimationInfo {
    AnimationType animationType_;
    std::vector<AnimationPara> animationParas_;
};

using SColor = struct SColor {
    float a_ = 1;
    U8CPU r_ = 0;
    U8CPU g_ = 0;
    U8CPU b_ = 0;
};

using GroupInfo = struct GroupInfo {
    std::vector<size_t> layerIndexes_;
    std::vector<size_t> maskIndexes_;
};

using GroupSetting = struct GroupSetting {
    std::vector<GroupInfo> groupInfos_;
    uint32_t animationIndex_;
};

using AnimationSetting = struct AnimationSetting {
    AnimationType animationType_;
    AnimationSubType animationSubType_;
    uint32_t animationMode_;
    std::vector<GroupSetting> groupSettings_;
};

using RenderGroup = struct RenderGroup {
    std::vector<GroupInfo> groupInfos_;
    SColor color_;
};

using SymbolLayers = struct SymbolLayers {
    uint32_t symbolGlyphId_;
    std::vector<std::vector<size_t>> layers_;
    std::vector<RenderGroup> renderGroups_;
};

enum SymbolRenderingStrategy {
    INVALID_RENDERING_STRATEGY = 0,
    SINGLE = 1,
    MULTIPLE_COLOR = 2,
    MULTIPLE_OPACITY = 3,
};

enum EffectStrategy {
    INVALID_EFFECT_STRATEGY = 0,
    NONE = 1,
    SCALE = 2,
    HIERARCHICAL = 3,
};

using SymbolLayersGroups = struct SymbolLayersGroups {
    uint32_t symbolGlyphId_;
    std::vector<std::vector<size_t>> layers_;
    std::map<SymbolRenderingStrategy, std::vector<RenderGroup>> renderModeGroups_;
    std::vector<AnimationSetting> animationSettings_;
};

class SK_API HMSymbolData
{
public:
    SymbolLayers symbolInfo_;
    SkPath path_;
};

class SK_API HMSymbol
{
public:
    HMSymbol(){};

    ~HMSymbol(){};

    static void PathOutlineDecompose(const SkPath& path, std::vector<SkPath>& paths);

    static void MultilayerPath(const std::vector<std::vector<size_t>>& multMap,
        const std::vector<SkPath>& paths, std::vector<SkPath>& multPaths);
};

#endif