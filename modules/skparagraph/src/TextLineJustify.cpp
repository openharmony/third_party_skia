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

#include "TextLineJustify.h"
#include <algorithm>
#include <cstddef>
#include <numeric>

namespace skia {
namespace textlayout {
void TextLineJustify::allocateHighLevelOffsets(
    SkScalar ideographicMaxLen, ClusterLevelsIndices& clusterLevels, SkScalar& allocatedWidth)
{
    // High level allocation: punctuation
    if (allocatedWidth < 0 || nearlyZero(allocatedWidth)) {
        return;
    }
    // Pre-calculate the punctuation width to obtain the maximum width increment of each punctuation character.
    SkScalar lastPunctStretch = 0.0f;   // Extrusion width to the left of the previous punctuation.
    constexpr size_t scaleFactor = 6;  // Defines the maximum width of 1 / 6 ideographs.
    for (auto& index : clusterLevels.highLevelIndices) {
        if (index.isClusterPunct) {
            SkScalar curPunctWidth = index.punctWidths;
            SkScalar stretchWidth = std::max(0.0f, (ideographicMaxLen - curPunctWidth) / scaleFactor);
            index.highLevelOffset = stretchWidth + lastPunctStretch;
            lastPunctStretch = stretchWidth;
        } else {
            index.highLevelOffset = lastPunctStretch;
            lastPunctStretch = 0.0f;
        }
    }
    SkScalar highLevelMaxWidth =
        std::accumulate(clusterLevels.highLevelIndices.begin(), clusterLevels.highLevelIndices.end(), 0.0f,
            [](const SkScalar& a, const HighLevelInfo& b) { return a + b.highLevelOffset; });
    if (highLevelMaxWidth > allocatedWidth) {
        std::for_each(clusterLevels.highLevelIndices.begin(), clusterLevels.highLevelIndices.end(),
            [highLevelMaxWidth, allocatedWidth](HighLevelInfo& val) {
                val.highLevelOffset = allocatedWidth * val.highLevelOffset / highLevelMaxWidth;
            });
        allocatedWidth = 0;
    } else {
        allocatedWidth -= highLevelMaxWidth;
    }
}

void TextLineJustify::allocateMiddleLevelOffsets(SkScalar ideographicMaxLen, size_t prevClusterNotSpaceCount,
    ClusterLevelsIndices& clusterLevels, SkScalar& allocatedWidth)
{
    // Middle level allocation: WhitespaceBreak, between ideographic and non-ideographic characters
    if (allocatedWidth < 0 || nearlyZero(allocatedWidth)) {
        return;
    }
    constexpr size_t scaleFactor = 12;  // Defines the maximum width of 1 / 12 ideographs.
    size_t totalPartitions = prevClusterNotSpaceCount + clusterLevels.middleLevelIndices.size();

    SkScalar middleLevelMaxWidth = totalPartitions * ideographicMaxLen / scaleFactor;
    if (middleLevelMaxWidth > allocatedWidth && totalPartitions > 0) {
        clusterLevels.middleLevelOffset = allocatedWidth / totalPartitions;
        allocatedWidth = 0;
    } else {
        clusterLevels.middleLevelOffset = ideographicMaxLen / scaleFactor;
        allocatedWidth -= middleLevelMaxWidth;
    }
}

void TextLineJustify::allocateLowLevelOffsets(
    SkScalar ideographicMaxLen, ClusterLevelsIndices& clusterLevels, SkScalar& allocatedWidth)
{
    // Low level allocation: Between ideographic characters
    if (allocatedWidth < 0 || nearlyZero(allocatedWidth)) {
        return;
    }
    constexpr size_t scaleFactor = 6;  // Defines the maximum width of 1 / 6 ideographs.

    SkScalar lowLevelMaxWidth =
            clusterLevels.LowLevelIndices.size() * ideographicMaxLen / scaleFactor;
    if (lowLevelMaxWidth > allocatedWidth && clusterLevels.LowLevelIndices.size() > 0) {
        clusterLevels.lowLevelOffset = allocatedWidth / clusterLevels.LowLevelIndices.size();
        allocatedWidth = 0;
    } else {
        clusterLevels.lowLevelOffset = ideographicMaxLen / scaleFactor;
        allocatedWidth -= lowLevelMaxWidth;
    }
}

void TextLineJustify::allocateRemainingWidth(
    SkScalar allocatedWidth, size_t prevClusterNotSpaceCount, ClusterLevelsIndices& clusterLevels)
{
    // Bottom-up allocation: If the upper limit is reached, the remaining width is evenly allocated.
    if (allocatedWidth < 0 || nearlyZero(allocatedWidth)) {
        return;
    }
    const size_t totalPatches = clusterLevels.highLevelIndices.size() + clusterLevels.middleLevelIndices.size() +
                                clusterLevels.LowLevelIndices.size();
    const SkScalar remainingOffset = allocatedWidth / (totalPatches + prevClusterNotSpaceCount);
    std::for_each(clusterLevels.highLevelIndices.begin(), clusterLevels.highLevelIndices.end(),
        [remainingOffset](HighLevelInfo& val) { val.highLevelOffset += remainingOffset; });
    clusterLevels.middleLevelOffset += remainingOffset;
    clusterLevels.lowLevelOffset += remainingOffset;
}

TextLineJustify::ShiftLevel TextLineJustify::determineShiftLevelForIdeographic(
    const Cluster* prevCluster, MiddleLevelInfo& middleLevelInfo)
{
    if (prevCluster == nullptr) {
        return ShiftLevel::Undefined;
    }
    if (prevCluster->isIdeographic()) {
        return ShiftLevel::LowLevel;
    } else if (prevCluster->isPunctuation()) {
        return ShiftLevel::HighLevel;
    } else if (prevCluster->isWhitespaceBreak()) {
        return ShiftLevel::MiddleLevel;
    } else {
        middleLevelInfo.isPrevClusterSpace = false;
        return ShiftLevel::MiddleLevel;
    }
}

TextLineJustify::ShiftLevel TextLineJustify::determineShiftLevelForPunctuation(
    const Cluster* cluster, const Cluster* prevCluster, HighLevelInfo& highLevelInfo)
{
    if (cluster == nullptr || prevCluster == nullptr) {
        return ShiftLevel::Undefined;
    }
    // Prevents stretching between ellipsis unicode
    if (cluster->isEllipsis() && prevCluster->isEllipsis()) {
        return ShiftLevel::Undefined;
    }
    highLevelInfo.isClusterPunct = true;
    highLevelInfo.punctWidths = textLineRef.usingAutoSpaceWidth(cluster);
    return ShiftLevel::HighLevel;
}

TextLineJustify::ShiftLevel TextLineJustify::determineShiftLevelForWhitespaceBreak(const Cluster* prevCluster)
{
    if (prevCluster == nullptr) {
        return ShiftLevel::Undefined;
    }
    if (prevCluster->isPunctuation()) {
        return ShiftLevel::HighLevel;
    }
    return ShiftLevel::MiddleLevel;
}

TextLineJustify::ShiftLevel TextLineJustify::determineShiftLevelForOtherCases(
    const Cluster* prevCluster, MiddleLevelInfo& middleLevelInfo)
{
    if (prevCluster == nullptr) {
        return ShiftLevel::Undefined;
    }
    if (prevCluster->isIdeographic()) {
        middleLevelInfo.isPrevClusterSpace = false;
        return ShiftLevel::MiddleLevel;
    } else if (prevCluster->isWhitespaceBreak()) {
        return ShiftLevel::MiddleLevel;
    } else if (prevCluster->isPunctuation()) {
        return ShiftLevel::HighLevel;
    }
    return ShiftLevel::Undefined;
}

TextLineJustify::ShiftLevel TextLineJustify::determineShiftLevel(const Cluster* cluster, const Cluster* prevCluster,
    HighLevelInfo& highLevelInfo, MiddleLevelInfo& middleLevelInfo, SkScalar& ideographicMaxLen)
{
    if (cluster == nullptr || prevCluster == nullptr) {
        return ShiftLevel::Undefined;
    }
    ShiftLevel shiftLevel = ShiftLevel::Undefined;
    if (cluster->isIdeographic()) {
        ideographicMaxLen = std::max(ideographicMaxLen, cluster->width());
        shiftLevel = determineShiftLevelForIdeographic(prevCluster, middleLevelInfo);
    } else if (cluster->isPunctuation()) {
        shiftLevel = determineShiftLevelForPunctuation(cluster, prevCluster, highLevelInfo);
    } else if (cluster->isWhitespaceBreak()) {
        shiftLevel = determineShiftLevelForWhitespaceBreak(prevCluster);
    } else {
        shiftLevel = determineShiftLevelForOtherCases(prevCluster, middleLevelInfo);
    }
    return shiftLevel;
}

SkScalar TextLineJustify::calculateClusterShift(
    const Cluster* cluster, ClusterIndex index, const ClusterLevelsIndices& clusterLevels)
{
    if (cluster == nullptr) {
        return 0.0f;
    }
    SkScalar step = 0.0f;
    auto highLevelIterator =
        std::find_if(clusterLevels.highLevelIndices.begin(), clusterLevels.highLevelIndices.end(),
            [index](const HighLevelInfo& data) { return data.clusterIndex == index; });
    auto lowLevelIterator =
        std::find_if(clusterLevels.middleLevelIndices.begin(), clusterLevels.middleLevelIndices.end(),
            [index](const MiddleLevelInfo& data) { return data.clusterIndex == index; });
    if (highLevelIterator != clusterLevels.highLevelIndices.end()) {
        size_t idx = static_cast<size_t>(std::distance(clusterLevels.highLevelIndices.begin(), highLevelIterator));
        step = clusterLevels.highLevelIndices[idx].highLevelOffset;
    } else if (lowLevelIterator != clusterLevels.middleLevelIndices.end()) {
        // Because both sides of the WhitespaceBreak are equally widened, the
        // ideographic and non-ideographic characters are only widened once.
        // So the front is not WhitespaceBreak, and the count increases by 1.
        step = lowLevelIterator->isPrevClusterSpace ? clusterLevels.middleLevelOffset
                                                    : clusterLevels.middleLevelOffset * (1 + 1);
    } else if (std::find(clusterLevels.LowLevelIndices.begin(), clusterLevels.LowLevelIndices.end(), index) !=
            clusterLevels.LowLevelIndices.end()) {
        step = clusterLevels.lowLevelOffset;
    }
    return step;
}

void TextLineJustify::justifyShiftCluster(const SkScalar maxWidth, SkScalar textLen, SkScalar ideographicMaxLen,
    size_t prevClusterNotSpaceCount, ClusterLevelsIndices& clusterLevels)
{
    SkScalar allocatedWidth = maxWidth - textLen - (textLineRef.ellipsis() ? textLineRef.ellipsis()->fAdvanceX() : 0);
    const SkScalar verifyShift = allocatedWidth;
    // Allocate offsets for each level
    allocateHighLevelOffsets(ideographicMaxLen, clusterLevels, allocatedWidth);
    allocateMiddleLevelOffsets(ideographicMaxLen, prevClusterNotSpaceCount, clusterLevels, allocatedWidth);
    allocateLowLevelOffsets(ideographicMaxLen, clusterLevels, allocatedWidth);
    allocateRemainingWidth(allocatedWidth, prevClusterNotSpaceCount, clusterLevels);
    // Deal with the ghost spaces
    auto ghostShift = maxWidth - textLineRef.widthWithoutEllipsis();
    // Reallocate the width of each cluster: Clusters of different levels use different offsets.
    SkScalar shift = 0.0f;
    SkScalar prevShift = 0.0f;
    textLineRef.iterateThroughClustersInGlyphsOrder(false, true,
        [this, &shift, &prevShift, clusterLevels, ghostShift](
            const Cluster* cluster, ClusterIndex index, bool ghost) {
            if (cluster == nullptr) {
                return true;
            }
            if (ghost) {
                if (cluster->run().leftToRight()) {
                    textLineRef.updateClusterOffsets(cluster, ghostShift, ghostShift);
                }
                return true;
            }
            SkScalar step = calculateClusterShift(cluster, index, clusterLevels);
            shift += step;
            textLineRef.updateClusterOffsets(cluster, shift, prevShift);
            prevShift = shift;
            return true;
        });
    SkAssertResult(nearlyEqual(shift, verifyShift));
}

bool TextLineJustify::justify(SkScalar maxWidth)
{
    SkScalar textLen = 0.0f;
    SkScalar ideographicMaxLen = 0.0f;
    ClusterLevelsIndices clusterLevels;
    size_t prevClusterNotSpaceCount = 0;
    const Cluster* prevCluster = nullptr;
    bool isFirstCluster = true;
    // Calculate text length and define three types of labels to trace cluster stretch level.
    textLineRef.iterateThroughClustersInGlyphsOrder(
        false, false, [&](const Cluster* cluster, ClusterIndex index, bool ghost) {
            if (cluster != nullptr && isFirstCluster) {
                isFirstCluster = false;
                prevCluster = cluster;
                textLen += textLineRef.usingAutoSpaceWidth(cluster);
                ideographicMaxLen =
                    (cluster->isIdeographic()) ? std::max(ideographicMaxLen, cluster->width()) : ideographicMaxLen;
                return true;
            }
            HighLevelInfo highLevelInfo;
            MiddleLevelInfo middleLevelInfo;
            ShiftLevel shiftLevel =
                determineShiftLevel(cluster, prevCluster, highLevelInfo, middleLevelInfo, ideographicMaxLen);
            switch (shiftLevel) {
                case ShiftLevel::HighLevel:
                    highLevelInfo.clusterIndex = index;
                    clusterLevels.highLevelIndices.push_back(highLevelInfo);
                    break;
                case ShiftLevel::MiddleLevel:
                    // Because both sides of the WhitespaceBreak are equally widened, the
                    // ideographic and non-ideographic characters are only widened once.
                    // So the front is not WhitespaceBreak, and the count increases by 1.
                    prevClusterNotSpaceCount += middleLevelInfo.isPrevClusterSpace ? 0 : 1;
                    middleLevelInfo.clusterIndex = index;
                    clusterLevels.middleLevelIndices.push_back(middleLevelInfo);
                    break;
                case ShiftLevel::LowLevel:
                    clusterLevels.LowLevelIndices.push_back(index);
                    break;
                default:
                    break;
            }
            textLen += textLineRef.usingAutoSpaceWidth(cluster);
            prevCluster = cluster;
            return true;
        });
    if (clusterLevels.empty()) {
        textLineRef.justifyUpdateRtlWidth(maxWidth, textLen);
        return false;
    }
    justifyShiftCluster(maxWidth, textLen, ideographicMaxLen, prevClusterNotSpaceCount, clusterLevels);
    return true;
}

}  // namespace textlayout
}  // namespace skia
