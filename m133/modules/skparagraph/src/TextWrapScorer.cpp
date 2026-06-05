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
        auto len = parent.cluster(clusterIx).width();
        if (!canFitAnyCluster_ && len > 0 && len <= maxWidth_) {
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
    CalculateRecursive(RecursiveParam{
        targetLines, maxLines_, 0, 0.f, cumulativeLen_,
    });
    LOGD("cache_: %{public}zu", cache_.size());
}

int64_t TextWrapScorer::CalculateRecursive(RecursiveParam param)
{
    if (param.maxLines == 0 || param.remainingTextWidth <= 1.f) {
        return BEST_LOCAL_SCORE;
    }

    // This should come precalculated
    param.currentMax = maxWidth_ - parent_.detectIndents(param.lineNumber) -
        parent_.detectTailIndents(param.lineNumber);
    if (param.currentMax <= SK_ScalarNearlyZero) {
        return BEST_LOCAL_SCORE;
    }

    // trim possible spaces at the beginning of line
    while ((param.lineNumber > 0) && (lastBreakPos_ + 1 < breaks_.size()) &&
        (breaks_[lastBreakPos_ + 1].subsequentWhitespace)) {
        param.remainingTextWidth += (param.begin - breaks_[++lastBreakPos_].width);
        param.begin = breaks_[lastBreakPos_].width;
    }

    if (lastBreakPos_ < breaks_.size() && breaks_[lastBreakPos_].type == Break::BreakType::BREAKTYPE_FORCED) {
        lastBreakPos_++;
    }
    param.breakPos = lastBreakPos_;

    while (param.breakPos < breaks_.size() && breaks_[param.breakPos].width < (param.begin + param.currentMax)) {
        param.breakPos++;
    }

    if (param.breakPos == lastBreakPos_ && param.remainingTextWidth > param.currentMax) {
        // If we were unable to find a break that matches the criteria, insert new one
        // This may happen if there is a long word and per line indent for this particular line
        if (param.breakPos + 1 > breaks_.size()) {
            breaks_.emplace_back(param.begin + param.currentMax, Break::BreakType::BREAKTYPE_FORCED, false);
        } else {
            breaks_.insert(breaks_.cbegin() + param.breakPos + 1, Break(param.begin + param.currentMax,
                Break::BreakType::BREAKTYPE_FORCED, false));
        }
        param.breakPos += BREAK_NUM_TWO;
    }

    LOGD("Line %{public}lu about to loop %{public}f, %{public}lu, %{public}lu, max: %{public}f",
        static_cast<unsigned long>(param.lineNumber), param.begin, static_cast<unsigned long>(param.breakPos),
        static_cast<unsigned long>(lastBreakPos_), maxWidth_);

    return FindOptimalSolutionForCurrentLine(param);
}

std::vector<SkScalar>& TextWrapScorer::GetResult()
{
    return current_;
}

SkScalar TextWrapScorer::calculateCurrentWidth(RecursiveParam& param, bool looped)
{
    SkScalar newWidth = param.currentMax;

    if (((param.breakPos > 0) && (param.breakPos - 1) < breaks_.size()) &&
        param.begin < breaks_[param.breakPos - 1].width) {
        newWidth = std::min(breaks_[--param.breakPos].width - param.begin, param.currentMax);
    }

    if (looped
        && ((lastBreakPos_ == param.breakPos)
            || (newWidth / param.currentMax * UNDERFLOW_SCORE < MINIMUM_FILL_RATIO))) {
        LOGD("line %{public}lu breaking %{public}f, %{public}lu, %{public}f/%{public}f",
             static_cast<unsigned long>(param.lineNumber), param.begin, static_cast<unsigned long>(param.breakPos),
             newWidth, maxWidth_);
        return 0;
    }

    lastBreakPos_ = param.breakPos;

    return std::min(newWidth, param.remainingTextWidth);
}

int64_t TextWrapScorer::FindOptimalSolutionForCurrentLine(RecursiveParam& param)
{
    // have this in reversed order to avoid extra insertions
    std::vector<SkScalar> currentBest;
    bool looped = false;
    int64_t score = 0;
    int64_t overallScore = score;
    int64_t bestLocalScore = BEST_LOCAL_SCORE;
    do {
        // until the given threshold is crossed (minimum line fill rate)
        // re-break this line, if a result is different, calculate score
        SkScalar currentWidth = calculateCurrentWidth(param, looped);
        if (currentWidth == 0) {
            break;
        }
        Index index { param.lineNumber, param.begin, currentWidth };

        // check cache
        const auto& ite = cache_.find(index);
        if (ite != cache_.cend()) {
            cacheHits_++;
            current_ = ite->second.widths;
            overallScore = ite->second.score;
            UpdateSolution(bestLocalScore, overallScore, currentBest);
            looped = true;
            continue;
        }
        SkScalar scoref = std::min(1.f, abs(currentTarget_ - currentWidth) / currentTarget_);
        score = int64_t((1.f - scoref) * UNDERFLOW_SCORE);
        score *= score;

        current_.clear();
        overallScore = score;

        // Handle last line
        if (param.breakPos < breaks_.size() && breaks_[param.breakPos].type == Break::BreakType::BREAKTYPE_HYPHEN) {
            auto copy = currentWidth - breaks_[param.breakPos].reservedSpace;
            // name is bit confusing as the method enters also recursion
            // with hyphen break this never is the last line
            if (!HandleLastLine(param, overallScore, copy, score)) {
                break;
            }
        } else { // real last line may update currentWidth
            if (!HandleLastLine(param, overallScore, currentWidth, score)) {
                break;
            }
        }
        // we have exceeded target number of lines, add some penalty
        if (param.targetLines < 0) {
            overallScore += param.targetLines * PARAM_10000; // MINIMUM_FILL_RATIO;
        }

        // We always hold the best possible score of children at this point
        current_.push_back(currentWidth);
        cache_[index] = { overallScore, current_ };

        UpdateSolution(bestLocalScore, overallScore, currentBest);
        looped = true;
    } while (score > MINIMUM_FILL_RATIO_SQUARED &&
        !(param.lineNumber == 0 && bestLocalScore > param.targetLines * GOOD_ENOUGH_LINE_SCORE));
    current_ = currentBest;
    return bestLocalScore;
}

bool TextWrapScorer::HandleLastLine(RecursiveParam& param, int64_t& overallScore, SkScalar& currentWidth, int64_t&score)
{
    // Handle last line
    if (abs(currentWidth - param.remainingTextWidth) < 1.f) {
        // this is last line, with high-quality wrapping, relax the score a bit
        if (parent_.getLineBreakStrategy() == LineBreakStrategy::HIGH_QUALITY) {
            overallScore = std::max(MINIMUM_FILL_RATIO, overallScore);
        } else {
            overallScore *= BALANCED_LAST_LINE_MULTIPLIER;
        }

        // let's break the loop, under no same condition / fill-rate added rows can result to a better
        // score.
        currentWidth = param.currentMax;
        score = MINIMUM_FILL_RATIO_SQUARED - 1;
        LOGD("last line %{public}lu reached", static_cast<unsigned long>(param.lineNumber));
        return true;
    }
    if (((param.remainingTextWidth - currentWidth) / maxWidth_) < param.maxLines) {
        // recursively calculate best score for children
        overallScore += CalculateRecursive(RecursiveParam{
            param.targetLines - 1,
            param.maxLines > param.lineNumber ? param.maxLines - param.lineNumber : 0,
            param.lineNumber + 1,
            param.begin + currentWidth,
            param.remainingTextWidth - currentWidth
        });
        lastBreakPos_ = param.breakPos; // restore our ix
        return true;
    }

    // the text is not going to fit anyway (anymore), no need to push it
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
