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
constexpr float WIDTH_TOLERANCE = 5.f;
constexpr int64_t PARAM_2 = 2;
constexpr int64_t PARAM_10000 = 10000;

// Single-shot scorer: one Run() call evaluates all candidate line-break combinations
// via an iterative stack machine. Member fields hold per-pass state and are not
// guarded for concurrent use.
struct TextWrapScorer {
    TextWrapScorer(SkScalar maxWidth, ParagraphImpl& parent, size_t maxLines);

    void GenerateBreaks(ParagraphImpl& parent);
    void CalculateCumulativeLen(ParagraphImpl& parent);
    void CalculateHyphenPos(size_t clusterIx, Cluster*& startCluster, Cluster*& endCluster, ParagraphImpl& parent,
        const SkString& locale);
    void CheckHyphenBreak(std::vector<uint8_t> results, ParagraphImpl& parent, Cluster*& startCluster);

    void Run();
    std::vector<SkScalar>& GetResult();
    void UpdateSolution(int64_t& bestLocalScore, const int64_t overallScore, std::vector<SkScalar>& currentBest);
    bool CanFitAnyCluster();

private:
    struct Index {
        size_t lineNumber{0};
        SkScalar begin{0};
        SkScalar width{0};
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
        int64_t score{0};
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

        SkScalar width{0.f};
        BreakType type{BreakType::BREAKTYPE_NONE};
        bool subsequentWhitespace{false};
        SkScalar reservedSpace{0.f};
    };

    struct LineParam {
        int64_t targetLines{0};
        size_t maxLines{0};
        size_t lineNumber{0};
        SkScalar begin{0};
        SkScalar remainingTextWidth{0};
        SkScalar currentMax{0};
        size_t breakPos{0};
    };

    struct Frame {
        LineParam param;
        size_t breakCursor{0};  // break position inherited by the current frame

        // Best result from this line down through all children
        int64_t bestScore = BEST_LOCAL_SCORE;
        std::vector<SkScalar> bestWidths;

        // Current do-while iteration state
        bool looped{false};
        SkScalar iterWidth{0};
        SkScalar cacheKeyWidth{0};  // original iterWidth, preserved for the cache key
                                     // (iterWidth itself may be overwritten on the
                                     //  last-line path so the two can diverge)
        int64_t iterScore{0};
        int64_t overallScore{0};

        // Child communication: parent->childScore receives child's bestScore on pop
        int64_t childScore = BEST_LOCAL_SCORE;

        enum Phase {
            SETUP,          // compute currentMax, trim leading whitespace, locate breakPos
            NEXT_WIDTH,     // try the next candidate width for this line (one do-while step)
            CHECK_RECURSE,  // last-line detection / push child frame / abandon branch
            AFTER_CHILD,    // child frame finished — fold its score into this frame
            FINALIZE        // cache result, update best-so-far, evaluate loop condition
        } phase = SETUP;
    };

    // memoisation table: (lineNumber, begin, width) → (score, accumulated widths)
    std::map<Index, Score> cache_;

    SkScalar minWidth_{0};
    SkScalar maxWidth_{0};
    SkScalar currentTarget_{0};
    SkScalar cumulativeLen_{0};
    bool canFitAnyCluster_{false};
    size_t maxLines_{0};
    ParagraphImpl& parent_;
    std::vector<SkScalar> current_;

    std::vector<Break> breaks_;

    uint64_t cacheHits_{0};
    bool fPrevWasWhitespace{false};

    void PopFrame(std::vector<Frame>& stack);
    void SetupFrame(Frame& f, std::vector<Frame>& stack);
    void NextWidthFrame(Frame& f, std::vector<Frame>& stack);
    void CheckRecurseFrame(Frame& f, std::vector<Frame>& stack);
    void AfterChildFrame(Frame& f);
    void FinalizeFrame(Frame& f, std::vector<Frame>& stack);
};

}  // namespace textlayout
}  // namespace skia

#endif  // ENABLE_TEXT_ENHANCE

#endif  // TextWrapScorer_DEFINED
