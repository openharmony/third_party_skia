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
#ifndef TextWrapScorer_DEFINED
#define TextWrapScorer_DEFINED

#ifdef ENABLE_TEXT_ENHANCE

#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

#include "SkScalar.h"

class SkString;

namespace skia {
namespace textlayout {

class ParagraphImpl;
class Cluster;

const size_t BREAK_NUM_TWO = 2;

constexpr int64_t MINIMUM_FILL_RATIO = 75;
constexpr int64_t MINIMUM_FILL_RATIO_SQUARED = MINIMUM_FILL_RATIO * MINIMUM_FILL_RATIO;
constexpr int64_t GOOD_ENOUGH_LINE_SCORE = 95 * 95;
constexpr int64_t UNDERFLOW_SCORE = 100;
constexpr float BALANCED_LAST_LINE_MULTIPLIER = 1.4f;
constexpr int64_t BEST_LOCAL_SCORE = -1000000;
constexpr float  WIDTH_TOLERANCE = 5.f;
constexpr int64_t PARAM_2 = 2;
constexpr int64_t PARAM_10000 = 10000;

// mkay, this makes an assumption that we do the scoring runs in a single thread and holds the variables during
// recursion
struct TextWrapScorer {
    TextWrapScorer(SkScalar maxWidth, ParagraphImpl& parent, size_t maxLines);

    void GenerateBreaks(ParagraphImpl& parent);
    void CalculateCumulativeLen(ParagraphImpl& parent);
    void CalculateHyphenPos(size_t clusterIx, Cluster*& startCluster, Cluster*& endCluster,
                            ParagraphImpl& parent, const SkString& locale);
    void CheckHyphenBreak(std::vector<uint8_t> results, ParagraphImpl& parent, Cluster*& startCluster);

    struct RecursiveParam {
        int64_t targetLines;
        size_t maxLines;
        size_t lineNumber;
        SkScalar begin;
        SkScalar remainingTextWidth;
        SkScalar currentMax;
        size_t breakPos;
    };

    void Run();
    int64_t CalculateRecursive(RecursiveParam param);
    std::vector<SkScalar>& GetResult();
    SkScalar calculateCurrentWidth(RecursiveParam& param, bool looped);
    int64_t FindOptimalSolutionForCurrentLine(RecursiveParam& param);
    bool HandleLastLine(RecursiveParam& param, int64_t& overallScore, SkScalar& currentWidth, int64_t& score);
    void UpdateSolution(int64_t& bestLocalScore, const int64_t overallScore, std::vector<SkScalar>& currentBest);
    bool CanFitAnyCluster();

private:
    struct Index {
        size_t lineNumber { 0 };
        SkScalar begin { 0 };
        SkScalar width { 0 };
        bool operator==(const Index& other) const {
            return (lineNumber == other.lineNumber && fabs(begin - other.begin) < WIDTH_TOLERANCE &&
                fabs(width - other.width) < WIDTH_TOLERANCE);
        }
        bool operator<(const Index& other) const {
            return lineNumber < other.lineNumber ||
                (lineNumber == other.lineNumber && other.begin - begin > WIDTH_TOLERANCE) ||
                (lineNumber == other.lineNumber && fabs(begin - other.begin) < WIDTH_TOLERANCE &&
                other.width - width > WIDTH_TOLERANCE);
        }
    };

    struct Score {
        int64_t score { 0 };
        // in reversed order
        std::vector<SkScalar> widths;
    };

    struct Break {
        enum class BreakType {
            BREAKTYPE_NONE,
            BREAKTYPE_HARD,
            BREAKTYPE_WHITE_SPACE,
            BREAKTYPE_INTRA,
            BREAKTYPE_FORCED,
            BREAKTYPE_HYPHEN
        };
        Break(SkScalar w, BreakType t, bool ssws) : width(w), type(t), subsequentWhitespace(ssws) {}

        SkScalar width { 0.f };
        BreakType type { BreakType::BREAKTYPE_NONE };
        bool subsequentWhitespace { false };
        SkScalar reservedSpace { 0.f };
    };

    // to be seen if unordered map would be better fit
    std::map<Index, Score> cache_;

    SkScalar maxWidth_ { 0 };
    SkScalar currentTarget_ { 0 };
    SkScalar cumulativeLen_ { 0 };
    bool canFitAnyCluster_ { false };
    size_t maxLines_ { 0 };
    ParagraphImpl& parent_;
    std::vector<SkScalar> current_;

    std::vector<Break> breaks_;
    size_t lastBreakPos_ { 0 };

    uint64_t cacheHits_ { 0 };
    bool fPrevWasWhitespace{false};
};

}  // namespace textlayout
}  // namespace skia

#endif  // ENABLE_TEXT_ENHANCE

#endif  // TextWrapScorer_DEFINED
