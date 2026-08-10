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
#ifdef ENABLE_TEXT_ENHANCE

#include "modules/skparagraph/src/TextWrapScorer.h"

#include "log.h"
#include "modules/skparagraph/include/Hyphenator.h"
#include "modules/skparagraph/src/ParagraphImpl.h"

namespace skia {
namespace textlayout {

TextWrapScorer::TextWrapScorer(SkScalar maxWidth, ParagraphImpl& parent, size_t maxLines)
    : maxWidth_(maxWidth), currentTarget_(maxWidth), maxLines_(maxLines), parent_(parent)
{
    minWidth_ = maxWidth_;
    CalculateCumulativeLen(parent);

    // If maxWidth cannot fit even a single cluster, skip balanced scoring
    if (!canFitAnyCluster_) {
        return;
    }

    if (parent_.getLineBreakStrategy() == LineBreakStrategy::BALANCED) {
        // calculate target width before breaks
        int64_t targetLines = 1 + cumulativeLen_ / maxWidth_;
        currentTarget_ = cumulativeLen_ / targetLines;
    }

    GenerateBreaks(parent);
}

void TextWrapScorer::GenerateBreaks(ParagraphImpl& parent)
{
    // we trust that clusters are sorted on parent
    bool prevWasWhitespace = false;
    SkScalar currentWidth = 0;
    size_t currentCount = 0; // in principle currentWidth != 0 should provide same result
    SkScalar cumulativeLen = 0;
    for (size_t ix = 0; ix < parent.clusters().size(); ix++) {
        auto& cluster = parent.clusters()[ix];
        auto len = cluster.width();
        cumulativeLen += len;
        currentWidth += len;
        currentCount++;
        if (cluster.isWhitespaceBreak()) {
            breaks_.emplace_back(cumulativeLen, Break::BreakType::BREAKTYPE_WHITE_SPACE, prevWasWhitespace);
            prevWasWhitespace = true;
            currentWidth = 0;
            currentCount = 0;
        } else if (cluster.isHardBreak()) {
            breaks_.emplace_back(cumulativeLen, Break::BreakType::BREAKTYPE_HARD, false);
            prevWasWhitespace = true;
            currentWidth = 0;
            currentCount = 0;
        } else if (cluster.isHyphenBreak()) {
            breaks_.emplace_back(cumulativeLen - cluster.width() + cluster.height(),
                                 Break::BreakType::BREAKTYPE_HYPHEN, false);
            breaks_.back().reservedSpace = cluster.height();
            prevWasWhitespace = true;
            currentWidth = 0;
            currentCount = 0;
        } else if (cluster.isIntraWordBreak()) {
            breaks_.emplace_back(cumulativeLen, Break::BreakType::BREAKTYPE_INTRA, false);
            prevWasWhitespace = true;
            currentWidth = 0;
            currentCount = 0;
        } else if (currentWidth > currentTarget_) {
            if (currentCount > 1) {
                cumulativeLen -= cluster.width();
                ix--;
            }
            breaks_.emplace_back(cumulativeLen, Break::BreakType::BREAKTYPE_FORCED, false);
            prevWasWhitespace = false;
            currentWidth = 0;
            currentCount = 0;
        } else {
            prevWasWhitespace = false;
        }
    }
}

void TextWrapScorer::CalculateCumulativeLen(ParagraphImpl& parent)
{
    auto startCluster = &parent.cluster(0);
    auto endCluster = &parent.cluster(0);
    auto locale = parent.paragraphStyle().getTextStyle().getLocale();
    for (size_t clusterIx = 0; clusterIx < parent.clusters().size(); clusterIx++) {
        const auto& cluster = parent.cluster(clusterIx);
        auto len = cluster.width();
        // A cluster is considered "fitable" only when it carries real, non-negligible
        // advance AND it is not a control character. Control chars (any width) and
        // zero-width format chars (e.g. ZWSP/ZWJ) cannot form meaningful lines, but
        // their tiny widths can fall into (0, maxWidth] after letterSpacing is no
        // longer applied to controls — that falsely enables the scorer and triggers
        // unbounded recursion. Both filters are needed: !nearlyZero does not catch
        // non-zero-width control chars, and !kControl does not catch Cf-class zero-widths.
        if (!nearlyZero(len) && len <= maxWidth_ &&
            !parent.codeUnitHasProperty(cluster.textRange().start, SkUnicode::CodeUnitFlags::kControl)) {
            minWidth_ = std::min(len, minWidth_);
            canFitAnyCluster_ = true;
        }
        if (parent.getLineBreakStrategy() == LineBreakStrategy::BALANCED) {
            cumulativeLen_ += len;
        }
        CalculateHyphenPos(clusterIx, startCluster, endCluster, parent, locale);
    }
}

void TextWrapScorer::CalculateHyphenPos(size_t clusterIx, Cluster*& startCluster, Cluster*& endCluster,
                                        ParagraphImpl& parent, const SkString& locale)
{
    auto& cluster = parent.cluster(clusterIx);
    const bool hyphenEnabled = parent.getWordBreakType() == WordBreakType::BREAK_HYPHEN;
    bool isWhitespace = (cluster.isHardBreak() || cluster.isWhitespaceBreak() || cluster.isTabulation());
    if (hyphenEnabled && !fPrevWasWhitespace && isWhitespace && endCluster > startCluster) {
        fPrevWasWhitespace = true;
        auto results = Hyphenator::getInstance().findBreakPositions(
            locale, parent.fText, startCluster->textRange().start, endCluster->textRange().end);
        CheckHyphenBreak(results, parent, startCluster);
        if (clusterIx + 1 < parent.clusters().size()) {
            startCluster = &cluster + 1;
        }
    } else if (!isWhitespace) {
        fPrevWasWhitespace = false;
        endCluster = &cluster;
    } else { //  fix "one character + space and then the actual target"
        uint32_t i = 1;
        while (clusterIx + i < parent.clusters().size()) {
            if (!parent.cluster(clusterIx + i).isWordBreak()) {
                startCluster = &cluster + i;
                break;
            } else {
                i++;
            }
        }
    }
}

void TextWrapScorer::CheckHyphenBreak(std::vector<uint8_t> results, ParagraphImpl& parent, Cluster*& startCluster)
{
    size_t prevClusterIx = 0;
    for (size_t resultIx = 0; resultIx < results.size(); resultIx++) {
        if (results[resultIx] & 0x1) {
            auto clusterPos = parent.clusterIndex(startCluster->textRange().start + resultIx);
            if (clusterPos != prevClusterIx) {
                parent.cluster(clusterPos).enableHyphenBreak();
                prevClusterIx = clusterPos;
            }
        }
    }
}

void TextWrapScorer::Run() {
    int64_t targetLines = 1 + cumulativeLen_ / maxWidth_;

    if (parent_.getLineBreakStrategy() == LineBreakStrategy::BALANCED) {
        currentTarget_ = cumulativeLen_ / targetLines;
    }

    if (targetLines < PARAM_2) {
        // need to have at least two lines for algo to do anything useful
        return;
    }

    // --- Iterative stack-based solver (replaces recursive CalculateRecursive) ---
    //
    // Each stack frame corresponds to one line being scored. The original algorithm
    // recursed via CalculateRecursive -> FindOptimalSolutionForCurrentLine ->
    // HandleLastLine -> CalculateRecursive, which could overflow the C++ call stack
    // when indent forced currentMax down to minWidth_ (each level consumed only a
    // few pixels of text, requiring thousands of levels).
    //
    // We now manage the call tree explicitly with a std::vector<Frame> stack.
    struct Frame {
        RecursiveParam param;
        size_t breakCursor = 0;       // per-frame break position (was global lastBreakPos_)

        // Best result from this line down through all children
        int64_t bestScore = BEST_LOCAL_SCORE;
        std::vector<SkScalar> bestWidths;

        // Current do-while iteration state
        bool looped = false;
        SkScalar iterWidth = 0;
        int64_t iterScore = 0;
        int64_t overallScore = 0;

        // Child communication: parent->childScore receives child's bestScore on pop
        int64_t childScore = BEST_LOCAL_SCORE;

        enum Phase {
            SETUP,           // compute currentMax, trim spaces, find breakPos
            NEXT_WIDTH,      // calculateCurrentWidth for this iteration
            CHECK_RECURSE,   // HandleLastLine: last line / need child / can't fit
            AFTER_CHILD,     // child returned — incorporate its result
            FINALIZE         // cache, update best, check loop condition
        } phase = SETUP;
    };

    std::vector<Frame> stack;
    stack.reserve(128);  // reasonable pre-allocation; actual depth bounded algorithmically
    stack.push_back({RecursiveParam{targetLines, maxLines_, 0, 0.f, cumulativeLen_}});

    // Helper: pop the top frame and communicate its result to the new top (parent).
    auto popFrame = [&]() {
        Frame& top = stack.back();
        current_ = std::move(top.bestWidths);
        int64_t score = top.bestScore;
        stack.pop_back();
        if (!stack.empty()) {
            stack.back().childScore = score;
        }
    };

    while (!stack.empty()) {
        Frame& f = stack.back();

        switch (f.phase) {
        // ---------------------------------------------------------------
        // SETUP — was the body of CalculateRecursive (before the call to
        //         FindOptimalSolutionForCurrentLine)
        // ---------------------------------------------------------------
        case Frame::SETUP: {
            // Terminal conditions (was: return BEST_LOCAL_SCORE)
            if (f.param.maxLines == 0 || f.param.remainingTextWidth <= 1.f) {
                popFrame();
                continue;
            }

            // Compute available width for this line, floored at minWidth_
            f.param.currentMax = maxWidth_ - parent_.detectIndents(f.param.lineNumber) -
                parent_.detectTailIndents(f.param.lineNumber);
            f.param.currentMax = std::max(minWidth_, std::min(f.param.currentMax, maxWidth_));
            if (f.param.currentMax <= SK_ScalarNearlyZero) {
                popFrame();
                continue;
            }

            // Trim whitespace at the beginning of a new line
            while ((f.param.lineNumber > 0) && (f.breakCursor + 1 < breaks_.size()) &&
                (breaks_[f.breakCursor + 1].subsequentWhitespace)) {
                f.param.remainingTextWidth +=
                    (f.param.begin - breaks_[++f.breakCursor].width);
                f.param.begin = breaks_[f.breakCursor].width;
            }

            // Advance past a forced break left by the previous line
            if (f.breakCursor < breaks_.size() &&
                breaks_[f.breakCursor].type == Break::BreakType::BREAKTYPE_FORCED) {
                f.breakCursor++;
            }
            f.param.breakPos = f.breakCursor;

            // Find the first break that exceeds the line width
            while (f.param.breakPos < breaks_.size() &&
                   breaks_[f.param.breakPos].width < (f.param.begin + f.param.currentMax)) {
                f.param.breakPos++;
            }

            // If no suitable break was found, insert a forced one
            if (f.param.breakPos == f.breakCursor &&
                f.param.remainingTextWidth > f.param.currentMax) {
                if (f.param.breakPos + 1 > breaks_.size()) {
                    breaks_.emplace_back(f.param.begin + f.param.currentMax,
                                        Break::BreakType::BREAKTYPE_FORCED, false);
                } else {
                    breaks_.insert(breaks_.cbegin() + f.param.breakPos + 1,
                                  Break(f.param.begin + f.param.currentMax,
                                        Break::BreakType::BREAKTYPE_FORCED, false));
                }
                f.param.breakPos += BREAK_NUM_TWO;
            }

            LOGD("Line %{public}lu about to loop %{public}f, %{public}lu, %{public}lu, max: %{public}f",
                 static_cast<unsigned long>(f.param.lineNumber), f.param.begin,
                 static_cast<unsigned long>(f.param.breakPos),
                 static_cast<unsigned long>(f.breakCursor), maxWidth_);

            f.phase = Frame::NEXT_WIDTH;
            break;
        }

        // ---------------------------------------------------------------
        // NEXT_WIDTH — was calculateCurrentWidth + cache lookup
        // ---------------------------------------------------------------
        case Frame::NEXT_WIDTH: {
            // ---- calculateCurrentWidth logic (inlined, per-frame breakCursor) ----
            SkScalar newWidth = f.param.currentMax;

            if (((f.param.breakPos > 0) && (f.param.breakPos - 1) < breaks_.size()) &&
                f.param.begin < breaks_[f.param.breakPos - 1].width) {
                newWidth = std::min(breaks_[--f.param.breakPos].width - f.param.begin,
                                    f.param.currentMax);
            }

            if (f.looped &&
                ((f.breakCursor == f.param.breakPos) ||
                 (newWidth / f.param.currentMax * UNDERFLOW_SCORE < MINIMUM_FILL_RATIO))) {
                LOGD("line %{public}lu breaking %{public}f, %{public}lu, %{public}f/%{public}f",
                     static_cast<unsigned long>(f.param.lineNumber), f.param.begin,
                     static_cast<unsigned long>(f.param.breakPos), newWidth, maxWidth_);
                popFrame();
                continue;
            }

            f.breakCursor = f.param.breakPos;
            f.iterWidth = std::min(newWidth, f.param.remainingTextWidth);

            // ---- cache lookup ----
            Index index{f.param.lineNumber, f.param.begin, f.iterWidth};
            const auto& ite = cache_.find(index);
            if (ite != cache_.cend()) {
                cacheHits_++;
                current_ = ite->second.widths;
                f.overallScore = ite->second.score;
                if (f.overallScore > f.bestScore) {
                    f.bestScore = f.overallScore;
                    f.bestWidths = current_;
                }
                f.looped = true;
                // Stay in NEXT_WIDTH for the next loop iteration
                continue;
            }

            // ---- compute line score ----
            SkScalar scoref = std::min(1.f, abs(currentTarget_ - f.iterWidth) / currentTarget_);
            f.iterScore = int64_t((1.f - scoref) * UNDERFLOW_SCORE);
            f.iterScore *= f.iterScore;

            current_.clear();
            f.overallScore = f.iterScore;

            f.phase = Frame::CHECK_RECURSE;
            break;
        }

        // ---------------------------------------------------------------
        // CHECK_RECURSE — was HandleLastLine
        // ---------------------------------------------------------------
        case Frame::CHECK_RECURSE: {
            // For hyphen breaks, account for the reserved hyphen width
            SkScalar adjustedWidth = f.iterWidth;
            if (f.param.breakPos < breaks_.size() &&
                breaks_[f.param.breakPos].type == Break::BreakType::BREAKTYPE_HYPHEN) {
                adjustedWidth = f.iterWidth - breaks_[f.param.breakPos].reservedSpace;
            }

            // Case 1: this is the last line (remaining text fits in current width)
            if (abs(adjustedWidth - f.param.remainingTextWidth) < 1.f) {
                if (parent_.getLineBreakStrategy() == LineBreakStrategy::HIGH_QUALITY) {
                    f.overallScore = std::max(MINIMUM_FILL_RATIO, f.overallScore);
                } else {
                    f.overallScore *= BALANCED_LAST_LINE_MULTIPLIER;
                }
                // Force the do-while loop to exit after this iteration
                f.iterWidth = f.param.currentMax;
                f.iterScore = MINIMUM_FILL_RATIO_SQUARED - 1;
                LOGD("last line %{public}lu reached",
                     static_cast<unsigned long>(f.param.lineNumber));
                f.phase = Frame::FINALIZE;
                continue;
            }

            // Case 2: need to recurse to children (remaining text can still fit)
            if (((f.param.remainingTextWidth - adjustedWidth) / maxWidth_) < f.param.maxLines) {
                // Push child frame — mark our state so we resume at AFTER_CHILD
                f.phase = Frame::AFTER_CHILD;
                Frame child;
                child.param.targetLines = f.param.targetLines - 1;
                child.param.maxLines =
                    f.param.maxLines > f.param.lineNumber
                        ? f.param.maxLines - f.param.lineNumber
                        : 0;
                child.param.lineNumber = f.param.lineNumber + 1;
                child.param.begin = f.param.begin + adjustedWidth;
                child.param.remainingTextWidth =
                    f.param.remainingTextWidth - adjustedWidth;
                child.breakCursor = f.param.breakPos;  // inherit parent's cursor
                stack.push_back(std::move(child));
                continue;
            }

            // Case 3: text won't fit — abandon this branch
            popFrame();
            continue;
        }

        // ---------------------------------------------------------------
        // AFTER_CHILD — child frame has been popped, its result is in
        //               f.childScore and current_
        // ---------------------------------------------------------------
        case Frame::AFTER_CHILD: {
            f.overallScore += f.childScore;
            // current_ was set by popFrame() to the child's bestWidths.
            // f.breakCursor is unchanged (child had its own copy).
            f.phase = Frame::FINALIZE;
            continue;
        }

        // ---------------------------------------------------------------
        // FINALIZE — cache the result, update best, check loop condition
        // ---------------------------------------------------------------
        case Frame::FINALIZE: {
            // Penalty for exceeding the target number of lines
            if (f.param.targetLines < 0) {
                f.overallScore += f.param.targetLines * PARAM_10000;
            }

            // Append this line's width to the widths accumulated by children
            current_.push_back(f.iterWidth);

            // Cache the composite result
            Index index{f.param.lineNumber, f.param.begin, f.iterWidth};
            cache_[index] = {f.overallScore, current_};

            // Update best-so-far for this frame
            if (f.overallScore > f.bestScore) {
                f.bestScore = f.overallScore;
                f.bestWidths = current_;
            }

            f.looped = true;

            // Check do-while loop condition
            if (f.iterScore > MINIMUM_FILL_RATIO_SQUARED &&
                !(f.param.lineNumber == 0 &&
                  f.bestScore > f.param.targetLines * GOOD_ENOUGH_LINE_SCORE)) {
                f.phase = Frame::NEXT_WIDTH;
            } else {
                popFrame();
            }
            break;
        }
        } // switch
    } // while

    // current_ was set by the last popFrame() (root frame) — GetResult() reads it.
    LOGD("cache_: %{public}zu", cache_.size());
}

int64_t TextWrapScorer::CalculateRecursive(RecursiveParam /*param*/)
{
    // DEPRECATED — kept for ABI compatibility. All work now happens in Run().
    return BEST_LOCAL_SCORE;
}

std::vector<SkScalar>& TextWrapScorer::GetResult()
{
    return current_;
}

SkScalar TextWrapScorer::calculateCurrentWidth(RecursiveParam& /*param*/, bool /*looped*/)
{
    // DEPRECATED — inlined into the iterative solver in Run().
    return 0;
}

int64_t TextWrapScorer::FindOptimalSolutionForCurrentLine(RecursiveParam& /*param*/)
{
    // DEPRECATED — inlined into the iterative solver in Run().
    return BEST_LOCAL_SCORE;
}

bool TextWrapScorer::HandleLastLine(RecursiveParam& /*param*/, int64_t& /*overallScore*/,
                                    SkScalar& /*currentWidth*/, int64_t& /*score*/)
{
    // DEPRECATED — inlined into the iterative solver in Run().
    return false;
}

void TextWrapScorer::UpdateSolution(
        int64_t& bestLocalScore, const int64_t overallScore, std::vector<SkScalar>& currentBest)
{
    if (overallScore > bestLocalScore) {
        bestLocalScore = overallScore;
        currentBest = current_;
    }
}

bool TextWrapScorer::CanFitAnyCluster()
{
    return canFitAnyCluster_;
}

}  // namespace textlayout
}  // namespace skia

#endif  // ENABLE_TEXT_ENHANCE
