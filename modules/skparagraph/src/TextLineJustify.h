/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.. All rights reserved.
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
#ifndef TEXTLINE_JUSTIFY_DEFINED
#define TEXTLINE_JUSTIFY_DEFINED

#ifdef OHOS_SUPPORT
#include "include/core/SkScalar.h"
#include "log.h"
#include "modules/skparagraph/src/TextLine.h"

namespace skia {
namespace textlayout {

class TextLineJustify {
public:
    TextLineJustify(TextLine& textLine) : textLineRef(textLine) {}
    bool justify(SkScalar maxWidth);

private:
    TextLine& textLineRef;

    struct HighLevelInfo {
        ClusterIndex clusterIndex{SIZE_MAX};
        bool isClusterPunct{false};
        SkScalar punctWidths{0.0f};
        SkScalar highLevelOffset{0.0f};
    };

    struct MiddleLevelInfo {
        ClusterIndex clusterIndex{SIZE_MAX};
        bool isPrevClusterSpace{true};
    };

    struct ClusterLevelsIndices {
        std::vector<HighLevelInfo> highLevelIndices;
        std::vector<MiddleLevelInfo> middleLevelIndices;
        std::vector<ClusterIndex> LowLevelIndices;
        SkScalar middleLevelOffset{0.0f};
        SkScalar lowLevelOffset{0.0f};

        bool empty()
        {
            return highLevelIndices.empty() && middleLevelIndices.empty() &&
                   LowLevelIndices.empty();
        }
    };

    enum class ShiftLevel {
        Undefined,
        HighLevel,    // include: Punctuation
        MiddleLevel,  // include: WhitespaceBreak, between ideographic and non-ideographic characters
        LowLevel      // include: Between ideographic characters
    };

    void allocateHighLevelOffsets(
        SkScalar ideographicMaxLen, ClusterLevelsIndices& clusterLevels, SkScalar& allocatedWidth);
    void allocateMiddleLevelOffsets(SkScalar ideographicMaxLen, size_t prevClusterNotSpaceCount,
        ClusterLevelsIndices& clusterLevels, SkScalar& allocatedWidth);
    void allocateLowLevelOffsets(
        SkScalar ideographicMaxLen, ClusterLevelsIndices& clusterLevels, SkScalar& allocatedWidth);
    void allocateRemainingWidth(
        SkScalar allocatedWidth, size_t prevClusterNotSpaceCount, ClusterLevelsIndices& clusterLevels);
    void distributeRemainingSpace(ClusterLevelsIndices& clusterLevels, SkScalar& middleLevelOffset,
        SkScalar& lowLevelOffset, SkScalar& allocatedWidth);
    SkScalar usingAutoSpaceWidth(const Cluster* cluster);
    ShiftLevel determineShiftLevelForIdeographic(const Cluster* prevCluster, MiddleLevelInfo& middleLevelInfo);
    ShiftLevel determineShiftLevelForPunctuation(
        const Cluster* cluster, const Cluster* prevCluster, HighLevelInfo& highLevelInfo);
    ShiftLevel determineShiftLevelForWhitespaceBreak(const Cluster* prevCluster);
    ShiftLevel determineShiftLevelForOtherCases(const Cluster* prevCluster, MiddleLevelInfo& middleLevelInfo);
    ShiftLevel determineShiftLevel(const Cluster* cluster, const Cluster* prevCluster, HighLevelInfo& highLevelInfo,
        MiddleLevelInfo& middleLevelInfo, SkScalar& ideographicMaxLen);
    SkScalar calculateClusterShift(
        const Cluster* cluster, ClusterIndex index, const ClusterLevelsIndices& clusterLevels);
    void justifyShiftCluster(const SkScalar maxWidth, SkScalar textLen, SkScalar ideographicMaxLen,
        size_t prevClusterNotSpaceCount, ClusterLevelsIndices& clusterLevels);
};
}  // namespace textlayout
}  // namespace skia

#endif  // OHOS_SUPPORT
#endif  // TEXTLINE_JUSTIFY_DEFINED