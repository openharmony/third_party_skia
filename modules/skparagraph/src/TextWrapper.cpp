// Copyright 2019 Google LLC.
#include "include/ParagraphStyle.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skparagraph/src/TextWrapper.h"
#include <cfloat>
#include <cstring>
#include "Run.h"
#include "log.h"
#ifdef TXT_USE_PARAMETER
#include "parameter.h"
#endif

namespace skia {
namespace textlayout {

namespace {
struct LineBreakerWithLittleRounding {
    LineBreakerWithLittleRounding(SkScalar maxWidth, bool applyRoundingHack)
        : fLower(maxWidth - 0.25f)
        , fMaxWidth(maxWidth)
        , fUpper(maxWidth + 0.25f)
        , fApplyRoundingHack(applyRoundingHack) {}

    bool breakLine(SkScalar width) const {
        if (width < fLower) {
            return false;
        } else if (width > fUpper) {
            return true;
        }

        auto val = std::fabs(width);
        SkScalar roundedWidth;
        if (fApplyRoundingHack) {
            if (val < 10000) {
                roundedWidth = SkScalarRoundToScalar(width * 100) * (1.0f/100);
            } else if (val < 100000) {
                roundedWidth = SkScalarRoundToScalar(width *  10) * (1.0f/10);
            } else {
                roundedWidth = SkScalarFloorToScalar(width);
            }
        } else {
            if (val < 10000) {
                roundedWidth = SkScalarFloorToScalar(width * 100) * (1.0f/100);
            } else if (val < 100000) {
                roundedWidth = SkScalarFloorToScalar(width *  10) * (1.0f/10);
            } else {
                roundedWidth = SkScalarFloorToScalar(width);
            }
        }
        return roundedWidth > fMaxWidth;
    }

    const SkScalar fLower, fMaxWidth, fUpper;
    const bool fApplyRoundingHack;
};
}  // namespace

#ifdef OHOS_SUPPORT
SkScalar TextWrapper::calculateFakeSpacing(Cluster* cluster, bool autoSpacingEnable)
{
    if (!autoSpacingEnable || cluster == fEndLine.endCluster()) {
        return 0;
    }
    if ((cluster - 1)->isWhitespaceBreak() || cluster->isWhitespaceBreak()) {
        return 0;
    }
    if ((cluster - 1)->isHardBreak() || cluster->isHardBreak()) {
        return 0;
    }
    if ((cluster - 1)->isCopyright() || cluster->isCopyright()) {
        return (cluster - 1)->getFontSize() / AUTO_SPACING_WIDTH_RATIO;
    }
    if ((cluster->isCJK() && (cluster - 1)->isWestern()) || (cluster->isWestern() && (cluster - 1)->isCJK())) {
        return (cluster - 1)->getFontSize() / AUTO_SPACING_WIDTH_RATIO;
    }
    return 0;
}

// Since we allow cluster clipping when they don't fit
// we have to work with stretches - parts of clusters
void TextWrapper::lookAhead(SkScalar maxWidth, Cluster* endOfClusters, bool applyRoundingHack,
    WordBreakType wordBreakType, bool autoSpacingEnable) {

    reset();
    fEndLine.metrics().clean();
    fWords.startFrom(fEndLine.startCluster(), fEndLine.startPos());
    fClusters.startFrom(fEndLine.startCluster(), fEndLine.startPos());
    fClip.startFrom(fEndLine.startCluster(), fEndLine.startPos());

    bool isFirstWord = true;

    LineBreakerWithLittleRounding breaker(maxWidth, applyRoundingHack);
    Cluster* nextNonBreakingSpace = nullptr;
    SkScalar totalFakeSpacing = 0.0;
    for (auto cluster = fEndLine.endCluster(); cluster < endOfClusters; ++cluster) {
        auto fakeSpacing = calculateFakeSpacing(cluster, autoSpacingEnable);
        totalFakeSpacing += fakeSpacing;
        if (cluster->isHardBreak()) {
            if (cluster != fEndLine.endCluster()) {
                isFirstWord = false;
            }
        } else if (
                // TODO: Trying to deal with flutter rounding problem. Must be removed...
                SkScalar width = fWords.width() + fClusters.width() + cluster->width() + totalFakeSpacing;
                (!isFirstWord || wordBreakType != WordBreakType::NORMAL) &&
                breaker.breakLine(width)) {
            if (cluster->isWhitespaceBreak()) {
                // It's the end of the word
                isFirstWord = false;
                fClusters.extend(cluster);
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, this->getClustersTrimmedWidth());
                fWords.extend(fClusters);
                continue;
            } else if (cluster->run().isPlaceholder()) {
                isFirstWord = false;
                if (!fClusters.empty()) {
                    // Placeholder ends the previous word
                    fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, this->getClustersTrimmedWidth());
                    fWords.extend(fClusters);
                }

                if (cluster->width() > maxWidth && fWords.empty()) {
                    // Placeholder is the only text and it's longer than the line;
                    // it does not count in fMinIntrinsicWidth
                    fClusters.extend(cluster);
                    fTooLongCluster = true;
                    fTooLongWord = true;
                } else {
                    // Placeholder does not fit the line; it will be considered again on the next line
                }
                break;
            }

            // Walk further to see if there is a too long word, cluster or glyph
            SkScalar nextWordLength = fClusters.width();
            SkScalar nextShortWordLength = nextWordLength;
            for (auto further = cluster; further != endOfClusters; ++further) {
                if (further->isSoftBreak() || further->isHardBreak() || further->isWhitespaceBreak()) {
                    break;
                }
                if (further->run().isPlaceholder()) {
                  // Placeholder ends the word
                  break;
                }

                if (nextWordLength > 0 && nextWordLength <= maxWidth && further->isIntraWordBreak()) {
                    // The cluster is spaces but not the end of the word in a normal sense
                    nextNonBreakingSpace = further;
                    nextShortWordLength = nextWordLength;
                }

                if (maxWidth == 0) {
                    // This is a tricky flutter case: layout(width:0) places 1 cluster on each line
                    nextWordLength = std::max(nextWordLength, further->width());
                } else {
                    nextWordLength += further->width();
                }
            }
            if (nextWordLength > maxWidth) {
                if (nextNonBreakingSpace != nullptr) {
                    // We only get here if the non-breaking space improves our situation
                    // (allows us to break the text to fit the word)
                    if (SkScalar shortLength = fWords.width() + nextShortWordLength;
                        !breaker.breakLine(shortLength)) {
                        // We can add the short word to the existing line
                        fClusters = TextStretch(fClusters.startCluster(), nextNonBreakingSpace, fClusters.metrics().getForceStrut());
                        fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, nextShortWordLength);
                        fWords.extend(fClusters);
                    } else {
                        // We can place the short word on the next line
                        fClusters.clean();
                    }
                    // Either way we are not in "word is too long" situation anymore
                    break;
                }
                // If the word is too long we can break it right now and hope it's enough
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, nextWordLength);
                if (fClusters.endPos() - fClusters.startPos() > 1 ||
                    fWords.empty()) {
                    fTooLongWord = true;
                } else {
                    // Even if the word is too long there is a very little space on this line.
                    // let's deal with it on the next line.
                }
            }

            if (fWords.empty() && breaker.breakLine(cluster->width())) {
                fClusters.extend(cluster);
                fTooLongCluster = true;
                fTooLongWord = true;
            }
            break;
        }

        if (cluster->isSoftBreak() || cluster->isWhitespaceBreak()) {
            isFirstWord = false;
        }

        if (cluster->run().isPlaceholder()) {
            if (!fClusters.empty()) {
                // Placeholder ends the previous word (placeholders are ignored in trimming)
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
                fWords.extend(fClusters);
            }

            // Placeholder is separate word and its width now is counted in minIntrinsicWidth
            fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, cluster->width());
            fWords.extend(cluster);
        } else {
            fClusters.extend(cluster);

            // Keep adding clusters/words
            if (fClusters.endOfWord()) {
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
                fWords.extend(fClusters);
            }
        }

        if ((fHardLineBreak = cluster->isHardBreak())) {
            // Stop at the hard line break
            break;
        }
    }
}

void TextWrapper::moveForward(bool hasEllipsis, bool breakAll) {

    // We normally break lines by words.
    // The only way we may go to clusters is if the word is too long or
    // it's the first word and it has an ellipsis attached to it.
    // If nothing fits we show the clipping.
    fTooLongWord = breakAll;
    if (!fWords.empty()) {
        fEndLine.extend(fWords);
#ifdef SK_IGNORE_SKPARAGRAPH_ELLIPSIS_FIX
        if (!fTooLongWord || hasEllipsis) { // Ellipsis added to a word
#else
        if (!fTooLongWord && !hasEllipsis) { // Ellipsis added to a grapheme
#endif
            return;
        }
    }
    if (!fClusters.empty()) {
        fEndLine.extend(fClusters);
        if (!fTooLongCluster) {
            return;
        }
    }

    if (!fClip.empty()) {
        // Flutter: forget the clipped cluster but keep the metrics
        fEndLine.metrics().add(fClip.metrics());
    }
}

// Special case for start/end cluster since they can be clipped
void TextWrapper::trimEndSpaces(TextAlign align) {
    // Remember the breaking position
    fEndLine.saveBreak();
    // Skip all space cluster at the end
    for (auto cluster = fEndLine.endCluster();
         cluster >= fEndLine.startCluster() && cluster->isWhitespaceBreak();
         --cluster) {
        fEndLine.trim(cluster);
    }
    fEndLine.trim();
}

SkScalar TextWrapper::getClustersTrimmedWidth() {
    // Move the end of the line to the left
    SkScalar width = 0;
    bool trailingSpaces = true;
    for (auto cluster = fClusters.endCluster(); cluster >= fClusters.startCluster(); --cluster) {
        if (cluster->run().isPlaceholder()) {
            continue;
        }
        if (trailingSpaces) {
            if (!cluster->isWhitespaceBreak()) {
                width += cluster->trimmedWidth(cluster->endPos());
                trailingSpaces = false;
            }
            continue;
        }
        width += cluster->width();
    }
    return width;
}

// Trim the beginning spaces in case of soft line break
std::tuple<Cluster*, size_t, SkScalar> TextWrapper::trimStartSpaces(Cluster* endOfClusters) {

    if (fHardLineBreak) {
        // End of line is always end of cluster, but need to skip \n
        auto width = fEndLine.width();
        auto cluster = fEndLine.endCluster() + 1;
        while (cluster < fEndLine.breakCluster() && cluster->isWhitespaceBreak())  {
            width += cluster->width();
            ++cluster;
        }
        return std::make_tuple(fEndLine.breakCluster() + 1, 0, width);
    }

    // breakCluster points to the end of the line;
    // It's a soft line break so we need to move lineStart forward skipping all the spaces
    auto width = fEndLine.widthWithGhostSpaces();
    auto cluster = fEndLine.breakCluster() + 1;
    while (cluster < endOfClusters && cluster->isWhitespaceBreak()) {
        width += cluster->width();
        ++cluster;
    }

    if (fEndLine.breakCluster()->isWhitespaceBreak() && fEndLine.breakCluster() < endOfClusters) {
        // In case of a soft line break by the whitespace
        // fBreak should point to the beginning of the next line
        // (it only matters when there are trailing spaces)
        fEndLine.shiftBreak();
    }

    return std::make_tuple(cluster, 0, width);
}

// calculate heuristics for different variants and select the least bad

// calculate the total space required
// define the goal for line numbers (max vs space required).
// If text could fit, it has substantially larger score compared to nicer wrap with overflow

// iterate: select nontrivial candidates with some maximum offset and set the penalty / benefit of variants
// goals: 0) fit maximum amount of text
//        1) fill lines
//        2) make line lengths even
//        2.5) define a cost for hyphenation - not done
//        3) try to make it fast

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
    TextWrapScorer(SkScalar maxWidth, ParagraphImpl& parent, size_t maxLines)
        : maxWidth_(maxWidth), currentTarget_(maxWidth), maxLines_(maxLines), parent_(parent)
    {
        if (parent_.getLineBreakStrategy() == LineBreakStrategy::BALANCED) {
            // calculate cumulative length  & target width before breakes
            for (auto& cluster : parent.clusters()) {
                auto len = cluster.width();
                cumulativeLen_ += len;
            }
            int64_t targetLines = 1 + cumulativeLen_ / maxWidth_;
            currentTarget_ = cumulativeLen_ / targetLines;
        }

        // we trust that clusters are sorted on parent
        bool prevWasWhitespace = false;
        SkScalar currentWidth = 0;
        SkScalar cumulativeLen_ = 0;
        for (size_t ix = 0; ix < parent.clusters().size(); ix++) {
            auto& cluster = parent.clusters()[ix];
            auto len = cluster.width();
            cumulativeLen_ += len;
            currentWidth += len;

            if (cluster.isWhitespaceBreak()) {
                breaks_.emplace_back(cumulativeLen_, Break::BreakType::BREAKTYPE_WHITE_SPACE, prevWasWhitespace);
                prevWasWhitespace = true;
                currentWidth = 0;
            } else if (cluster.isHardBreak()) {
                breaks_.emplace_back(cumulativeLen_, Break::BreakType::BREAKTYPE_HARD, false);
                prevWasWhitespace = true;
                currentWidth = 0;
            } else if (cluster.isIntraWordBreak()) {
                breaks_.emplace_back(cumulativeLen_, Break::BreakType::BREAKTYPE_INTRA, false);
                prevWasWhitespace = true;
                currentWidth = 0;
            } else if (currentWidth > currentTarget_) {
                cumulativeLen_ -= cluster.width();
                ix--;
                breaks_.emplace_back(cumulativeLen_, Break::BreakType::BREAKTYPE_FORCED, false);
                prevWasWhitespace = false;
                currentWidth = 0;
            } else {
                prevWasWhitespace = false;
            }
        }
    }

    struct RecursiveParam {
        int64_t targetLines;
        size_t maxLines;
        size_t lineNumber;
        SkScalar begin;
        SkScalar remainingTextWidth;
        SkScalar currentMax;
        size_t breakPos;
    };

    void Run() {
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

    int64_t CalculateRecursive(RecursiveParam param)
    {
        if (param.maxLines == 0 || param.remainingTextWidth <= 1.f) {
            return BEST_LOCAL_SCORE;
        }

        // This should come precalculated
        param.currentMax = maxWidth_ - parent_.detectIndents(param.lineNumber);
        if (nearlyZero(param.currentMax)) {
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
            breaks_.insert(breaks_.cbegin() + param.breakPos + 1, Break(param.begin + param.currentMax, Break::BreakType::BREAKTYPE_FORCED, false));
            param.breakPos += 2;
        }

        LOGD("Line %{public}lu about to loop %{public}f, %{public}lu, %{public}lu, max: %{public}f",
            static_cast<unsigned long>(param.lineNumber), param.begin, static_cast<unsigned long>(param.breakPos),
            static_cast<unsigned long>(lastBreakPos_), maxWidth_);

        return FindOptimalSolutionForCurrentLine(param);
    }

    std::vector<SkScalar>& GetResult()
    {
        return current_;
    }

    int64_t FindOptimalSolutionForCurrentLine(RecursiveParam& param)
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
            SkScalar newWidth = param.currentMax;

            if (param.breakPos > 0 && param.begin < breaks_[param.breakPos - 1].width) {
                newWidth = std::min(breaks_[--param.breakPos].width - param.begin, param.currentMax);
            }

            if (looped && ((lastBreakPos_ == param.breakPos) || (newWidth/param.currentMax*UNDERFLOW_SCORE < MINIMUM_FILL_RATIO))) {
                LOGD("line %{public}lu breaking %{public}f, %{public}lu, %{public}f/%{public}f",
                    static_cast<unsigned long>(param.lineNumber), param.begin, static_cast<unsigned long>(param.breakPos), newWidth, maxWidth_);
                break;
            }

            lastBreakPos_ = param.breakPos;

            SkScalar currentWidth = std::min(newWidth, param.remainingTextWidth);
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
            if (!HandleLastLine(param, overallScore, currentWidth, score)) {
                break;
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

    bool HandleLastLine(RecursiveParam& param, int64_t& overallScore, SkScalar& currentWidth, int64_t&score)
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
                param.maxLines - param.lineNumber,
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

    void UpdateSolution(int64_t& bestLocalScore, const int64_t overallScore, std::vector<SkScalar>& currentBest)
    {
        if (overallScore > bestLocalScore) {
            bestLocalScore = overallScore;
            currentBest = current_;
        }
    }

private:
    struct Index {
        size_t lineNumber { 0 };
        SkScalar begin { 0 };
        SkScalar width { 0 };
        bool operator==(const Index& other) const
        {
            return (
                lineNumber == other.lineNumber && fabs(begin - other.begin) < WIDTH_TOLERANCE && fabs(width - other.width) < WIDTH_TOLERANCE);
        }
        bool operator<(const Index& other) const
        {
            return lineNumber < other.lineNumber || (lineNumber == other.lineNumber && other.begin - begin > WIDTH_TOLERANCE) ||
                (lineNumber == other.lineNumber && fabs(begin - other.begin) < WIDTH_TOLERANCE && other.width - width > WIDTH_TOLERANCE);
        }
    };

    struct Score {
        int64_t score { 0 };
        // in reversed order
        std::vector<SkScalar> widths;
    };

    // to be seen if unordered map would be better fit
    std::map<Index, Score> cache_;

    SkScalar maxWidth_ { 0 };
    SkScalar currentTarget_ { 0 };
    SkScalar cumulativeLen_ { 0 };
    size_t maxLines_ { 0 };
    ParagraphImpl& parent_;
    std::vector<SkScalar> current_;

    struct Break {
        enum class BreakType {
            BREAKTYPE_NONE,
            BREAKTYPE_HARD,
            BREAKTYPE_WHITE_SPACE,
            BREAKTYPE_INTRA,
            BREAKTYPE_FORCED
        };
        Break(SkScalar w, BreakType t, bool ssws) : width(w), type(t), subsequentWhitespace(ssws) {}

        SkScalar width { 0.f };
        BreakType type { BreakType::BREAKTYPE_NONE };
        bool subsequentWhitespace { false };
    };

    std::vector<Break> breaks_;
    size_t lastBreakPos_ { 0 };

    uint64_t cacheHits_ { 0 };
};

uint64_t TextWrapper::CalculateBestScore(std::vector<SkScalar>& widthOut, SkScalar maxWidth,
    ParagraphImpl* parent, size_t maxLines) {
    if (maxLines == 0 || !parent || nearlyZero(maxWidth)) {
        return -1;
    }

    TextWrapScorer* scorer = new TextWrapScorer(maxWidth, *parent, maxLines);
    scorer->Run();
    while (scorer && scorer->GetResult().size()) {
        auto width = scorer->GetResult().back();
        widthOut.push_back(width);
        LOGD("width %{public}f", width);
        scorer->GetResult().pop_back();
    }

    delete scorer;
    return 0;
}

void TextWrapper::updateMetricsWithPlaceholder(std::vector<Run*>& runs, bool iterateByCluster) {
    if (!iterateByCluster) {
        Run* lastRun = nullptr;
        for (auto& run : runs) {
            if (run == lastRun) {
                continue;
            }
            lastRun = run;
            if (lastRun != nullptr && lastRun->placeholderStyle() != nullptr) {
                SkASSERT(lastRun->size() == 1);
                // Update the placeholder metrics so we can get the placeholder positions later
                // and the line metrics (to make sure the placeholder fits)
                lastRun->updateMetrics(&fEndLine.metrics());
            }
        }
        return;
    }
    runs.clear();
    // Deal with placeholder clusters == runs[@size==1]
    Run* lastRun = nullptr;
    for (auto cluster = fEndLine.startCluster(); cluster <= fEndLine.endCluster(); ++cluster) {
        auto r = cluster->runOrNull();
        if (r == lastRun) {
            continue;
        }
        lastRun = r;
        if (lastRun != nullptr && lastRun->placeholderStyle() != nullptr) {
            SkASSERT(lastRun->size() == 1);
            // Update the placeholder metrics so we can get the placeholder positions later
            // and the line metrics (to make sure the placeholder fits)
            lastRun->updateMetrics(&fEndLine.metrics());
            runs.emplace_back(lastRun);
        }
    }
}

// TODO: refactor the code for line ending (with/without ellipsis)
void TextWrapper::breakTextIntoLines(ParagraphImpl* parent,
                                     SkScalar maxWidth,
                                     const AddLineToParagraph& addLine) {
    fHeight = 0;
    fMinIntrinsicWidth = std::numeric_limits<SkScalar>::min();
    fMaxIntrinsicWidth = std::numeric_limits<SkScalar>::min();

    auto span = parent->clusters();
    if (span.empty()) {
        return;
    }
    auto maxLines = parent->paragraphStyle().getMaxLines();
    auto align = parent->paragraphStyle().effective_align();
    auto unlimitedLines = maxLines == std::numeric_limits<size_t>::max();
    auto endlessLine = !SkScalarIsFinite(maxWidth);
    auto hasEllipsis = parent->paragraphStyle().ellipsized();

    auto disableFirstAscent = parent->paragraphStyle().getTextHeightBehavior() & TextHeightBehavior::kDisableFirstAscent;
    auto disableLastDescent = parent->paragraphStyle().getTextHeightBehavior() & TextHeightBehavior::kDisableLastDescent;
    bool firstLine = true; // We only interested in fist line if we have to disable the first ascent

#ifdef OHOS_SUPPORT
    bool autoSpacingEnableFlag = false;
#ifdef TXT_USE_PARAMETER
    static constexpr int AUTO_SPACING_ENABLE_LENGTH = 10;
    char autoSpacingEnable[AUTO_SPACING_ENABLE_LENGTH] = {0};
    GetParameter("persist.sys.text.autospacing.enable", "0", autoSpacingEnable, AUTO_SPACING_ENABLE_LENGTH);
    autoSpacingEnableFlag = std::strcmp(autoSpacingEnable, "0") != 0;
#endif
    // Resolve balanced line widths
    std::vector<SkScalar> balancedWidths;

    // if word breaking strategy is nontrivial (balanced / optimal), AND word break mode is not BREAK_ALL
    if (parent->getWordBreakType() != WordBreakType::BREAK_ALL && parent->getLineBreakStrategy() != LineBreakStrategy::GREEDY) {
        if (CalculateBestScore(balancedWidths, maxWidth, parent, maxLines) < 0) {
            // if the line breaking strategy returns a negative score, the algorithm could not fit or break the text
            // fall back to default, greedy algorithm
            balancedWidths.clear();
        }
    }
    LOGD("Got %{public}lu", static_cast<unsigned long>(balancedWidths.size()));
#endif

    SkScalar softLineMaxIntrinsicWidth = 0;
    fEndLine = TextStretch(span.begin(), span.begin(), parent->strutForceHeight());
    auto end = span.end() - 1;
    auto start = span.begin();
    InternalLineMetrics maxRunMetrics;
    bool needEllipsis = false;
    SkScalar newWidth = maxWidth;
    SkScalar noIndentWidth = maxWidth;
    while (fEndLine.endCluster() != end) {
        noIndentWidth = maxWidth - parent->detectIndents(fLineNumber - 1);
        if (maxLines == 1 && parent->paragraphStyle().getEllipsisMod() == EllipsisModal::HEAD) {
            newWidth = FLT_MAX;
        } else if (!balancedWidths.empty() && fLineNumber - 1 < balancedWidths.size()) {
            newWidth = balancedWidths[fLineNumber - 1];
        } else {
            newWidth = maxWidth - parent->detectIndents(fLineNumber - 1);
        }
        this->lookAhead(newWidth, end, parent->getApplyRoundingHack(), parent->getWordBreakType(),
            autoSpacingEnableFlag);

        auto lastLine = (hasEllipsis && unlimitedLines) || fLineNumber >= maxLines;
        needEllipsis = hasEllipsis && !endlessLine && lastLine;

        this->moveForward(needEllipsis, parent->getWordBreakType() == WordBreakType::BREAK_ALL);
        if (fEndLine.endCluster() >= fEndLine.startCluster() || maxLines > 1) {
            needEllipsis &= fEndLine.endCluster() < end - 1; // Only if we have some text to ellipsize
        }

        // Do not trim end spaces on the naturally last line of the left aligned text
        this->trimEndSpaces(align);

        // For soft line breaks add to the line all the spaces next to it
        Cluster* startLine;
        size_t pos;
        SkScalar widthWithSpaces;
        std::tie(startLine, pos, widthWithSpaces) = this->trimStartSpaces(end);

        if (needEllipsis && !fHardLineBreak) {
            // This is what we need to do to preserve a space before the ellipsis
            fEndLine.restoreBreak();
            widthWithSpaces = fEndLine.widthWithGhostSpaces();
        }

        // If the line is empty with the hard line break, let's take the paragraph font (flutter???)
        if (fEndLine.metrics().isClean()) {
            fEndLine.setMetrics(parent->getEmptyMetrics());
        }

        std::vector<Run*> runs;
        updateMetricsWithPlaceholder(runs, true);
        // update again for some case
        // such as :
        //      placeholderA(width: 100, height: 100, align: bottom) placeholderB(width: 200, height: 200, align: top)
        // without second update: the placeholderA bottom will be set 0, and placeholderB bottom will be set 100
        // so the fEndline bottom will be set 100, is not equal placeholderA bottom
        updateMetricsWithPlaceholder(runs, false);
        // Before we update the line metrics with struts,
        // let's save it for GetRectsForRange(RectHeightStyle::kMax)
        maxRunMetrics = fEndLine.metrics();
        maxRunMetrics.fForceStrut = false;

        // TODO: keep start/end/break info for text and runs but in a better way that below
        TextRange textExcludingSpaces(fEndLine.startCluster()->textRange().start, fEndLine.endCluster()->textRange().end);
        TextRange text(fEndLine.startCluster()->textRange().start, fEndLine.breakCluster()->textRange().start);
        TextRange textIncludingNewlines(fEndLine.startCluster()->textRange().start, startLine->textRange().start);
        if (startLine == end) {
            textIncludingNewlines.end = parent->text().size();
            text.end = parent->text().size();
        }
        ClusterRange clusters(fEndLine.startCluster() - start, fEndLine.endCluster() - start + 1);
        ClusterRange clustersWithGhosts(fEndLine.startCluster() - start, startLine - start);

        if (disableFirstAscent && firstLine) {
            fEndLine.metrics().fAscent = fEndLine.metrics().fRawAscent;
        }
        if (disableLastDescent && (lastLine || (startLine == end && !fHardLineBreak ))) {
            fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
        }

        if (parent->strutEnabled()) {
            // Make sure font metrics are not less than the strut
            parent->strutMetrics().updateLineMetrics(fEndLine.metrics());
        }

        SkScalar lineHeight = fEndLine.metrics().height();
        firstLine = false;

        if (fEndLine.empty()) {
            // Correct text and clusters (make it empty for an empty line)
            textExcludingSpaces.end = textExcludingSpaces.start;
            clusters.end = clusters.start;
        }

        // In case of a force wrapping we don't have a break cluster and have to use the end cluster
        text.end = std::max(text.end, textExcludingSpaces.end);

        if (parent->paragraphStyle().getEllipsisMod() == EllipsisModal::HEAD && hasEllipsis) {
            needEllipsis = maxLines <= 1;
            if (needEllipsis) {
                fHardLineBreak = false;
            }
        }

        SkScalar offsetX = parent->detectIndents(fLineNumber - 1);
        addLine(textExcludingSpaces,
                text,
                textIncludingNewlines, clusters, clustersWithGhosts, widthWithSpaces,
                fEndLine.startPos(),
                fEndLine.endPos(),
                SkVector::Make(offsetX, fHeight),
                SkVector::Make(fEndLine.width(), lineHeight),
                fEndLine.metrics(),
                needEllipsis,
                offsetX,
                noIndentWidth);

        softLineMaxIntrinsicWidth += widthWithSpaces;

        fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, softLineMaxIntrinsicWidth);
        if (fHardLineBreak) {
            softLineMaxIntrinsicWidth = 0;
        }
        // Start a new line
        fHeight += lineHeight;
        if (!fHardLineBreak || startLine != end) {
            fEndLine.clean();
        }
        fEndLine.startFrom(startLine, pos);
        parent->fMaxWidthWithTrailingSpaces = std::max(parent->fMaxWidthWithTrailingSpaces, widthWithSpaces);

        if (hasEllipsis && unlimitedLines) {
            // There is one case when we need an ellipsis on a separate line
            // after a line break when width is infinite
            if (!fHardLineBreak) {
                break;
            }
        } else if (lastLine) {
            // There is nothing more to draw
            fHardLineBreak = false;
            break;
        }

        ++fLineNumber;
    }

    // We finished formatting the text but we need to scan the rest for some numbers
    // TODO: make it a case of a normal flow
    if (fEndLine.endCluster() != nullptr) {
        auto lastWordLength = 0.0f;
        auto cluster = fEndLine.endCluster();
        while (cluster != end || cluster->endPos() < end->endPos()) {
            fExceededMaxLines = true;
            if (cluster->isHardBreak()) {
                // Hard line break ends the word and the line
                fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, softLineMaxIntrinsicWidth);
                softLineMaxIntrinsicWidth = 0;
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
                lastWordLength = 0;
            } else if (cluster->isWhitespaceBreak()) {
                // Whitespaces end the word
                softLineMaxIntrinsicWidth += cluster->width();
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
                lastWordLength = 0;
            } else if (cluster->run().isPlaceholder()) {
                // Placeholder ends the previous word and creates a separate one
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
                // Placeholder width now counts in fMinIntrinsicWidth
                softLineMaxIntrinsicWidth += cluster->width();
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, cluster->width());
                lastWordLength = 0;
            } else {
                // Nothing out of ordinary - just add this cluster to the word and to the line
                softLineMaxIntrinsicWidth += cluster->width();
                lastWordLength += cluster->width();
            }
            ++cluster;
        }
        fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
        fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, softLineMaxIntrinsicWidth);

        if (parent->lines().empty()) {
            // In case we could not place even a single cluster on the line
            if (disableFirstAscent) {
                fEndLine.metrics().fAscent = fEndLine.metrics().fRawAscent;
            }
            if (disableLastDescent && !fHardLineBreak) {
                fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
            }
            fHeight = std::max(fHeight, fEndLine.metrics().height());
        }
    }

    if (fHardLineBreak) {
        if (disableLastDescent) {
            fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
        }

        // Last character is a line break
        if (parent->strutEnabled()) {
            // Make sure font metrics are not less than the strut
            parent->strutMetrics().updateLineMetrics(fEndLine.metrics());
        }

        ClusterRange clusters(fEndLine.breakCluster() - start, fEndLine.endCluster() - start);
        addLine(fEndLine.breakCluster()->textRange(),
                fEndLine.breakCluster()->textRange(),
                fEndLine.endCluster()->textRange(),
                clusters,
                clusters,
                0,
                0,
                0,
                SkVector::Make(0, fHeight),
                SkVector::Make(0, fEndLine.metrics().height()),
                fEndLine.metrics(),
                needEllipsis,
                parent->detectIndents(fLineNumber - 1),
                noIndentWidth);
        fHeight += fEndLine.metrics().height();
        parent->lines().back().setMaxRunMetrics(maxRunMetrics);
    }

    if (parent->lines().empty()) {
        return;
    }
    // Correct line metric styles for the first and for the last lines if needed
    if (disableFirstAscent) {
        parent->lines().front().setAscentStyle(LineMetricStyle::Typographic);
    }
    if (disableLastDescent) {
        parent->lines().back().setDescentStyle(LineMetricStyle::Typographic);
    }
}
#else
// Since we allow cluster clipping when they don't fit
// we have to work with stretches - parts of clusters
void TextWrapper::lookAhead(SkScalar maxWidth, Cluster* endOfClusters, bool applyRoundingHack) {
    reset();
    fEndLine.metrics().clean();
    fWords.startFrom(fEndLine.startCluster(), fEndLine.startPos());
    fClusters.startFrom(fEndLine.startCluster(), fEndLine.startPos());
    fClip.startFrom(fEndLine.startCluster(), fEndLine.startPos());
    LineBreakerWithLittleRounding breaker(maxWidth, applyRoundingHack);
    Cluster* nextNonBreakingSpace = nullptr;
    for (auto cluster = fEndLine.endCluster(); cluster < endOfClusters; ++cluster) {
        if (cluster->isHardBreak()) {
        } else if (
                // TODO: Trying to deal with flutter rounding problem. Must be removed...
                SkScalar width = fWords.width() + fClusters.width() + cluster->width();
                breaker.breakLine(width)) {
            if (cluster->isWhitespaceBreak()) {
                // It's the end of the word
                fClusters.extend(cluster);
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, this->getClustersTrimmedWidth());
                fWords.extend(fClusters);
                continue;
            } else if (cluster->run().isPlaceholder()) {
                if (!fClusters.empty()) {
                    // Placeholder ends the previous word
                    fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, this->getClustersTrimmedWidth());
                    fWords.extend(fClusters);
                }
                if (cluster->width() > maxWidth && fWords.empty()) {
                    // Placeholder is the only text and it's longer than the line;
                    // it does not count in fMinIntrinsicWidth
                    fClusters.extend(cluster);
                    fTooLongCluster = true;
                    fTooLongWord = true;
                } else {
                    // Placeholder does not fit the line; it will be considered again on the next line
                }
                break;
            }
            // Walk further to see if there is a too long word, cluster or glyph
            SkScalar nextWordLength = fClusters.width();
            SkScalar nextShortWordLength = nextWordLength;
            for (auto further = cluster; further != endOfClusters; ++further) {
                if (further->isSoftBreak() || further->isHardBreak() || further->isWhitespaceBreak()) {
                    break;
                }
                if (further->run().isPlaceholder()) {
                  // Placeholder ends the word
                  break;
                }
                if (nextWordLength > 0 && nextWordLength <= maxWidth && further->isIntraWordBreak()) {
                    // The cluster is spaces but not the end of the word in a normal sense
                    nextNonBreakingSpace = further;
                    nextShortWordLength = nextWordLength;
                }
                if (maxWidth == 0) {
                    // This is a tricky flutter case: layout(width:0) places 1 cluster on each line
                    nextWordLength = std::max(nextWordLength, further->width());
                } else {
                    nextWordLength += further->width();
                }
            }
            if (nextWordLength > maxWidth) {
                if (nextNonBreakingSpace != nullptr) {
                    // We only get here if the non-breaking space improves our situation
                    // (allows us to break the text to fit the word)
                    if (SkScalar shortLength = fWords.width() + nextShortWordLength;
                        !breaker.breakLine(shortLength)) {
                        // We can add the short word to the existing line
                        fClusters = TextStretch(fClusters.startCluster(), nextNonBreakingSpace, fClusters.metrics().getForceStrut());
                        fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, nextShortWordLength);
                        fWords.extend(fClusters);
                    } else {
                        // We can place the short word on the next line
                        fClusters.clean();
                    }
                    // Either way we are not in "word is too long" situation anymore
                    break;
                }
                // If the word is too long we can break it right now and hope it's enough
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, nextWordLength);
                if (fClusters.endPos() - fClusters.startPos() > 1 ||
                    fWords.empty()) {
                    fTooLongWord = true;
                } else {
                    // Even if the word is too long there is a very little space on this line.
                    // let's deal with it on the next line.
                }
            }
            if (cluster->width() > maxWidth) {
                fClusters.extend(cluster);
                fTooLongCluster = true;
                fTooLongWord = true;
            }
            break;
        }
        if (cluster->run().isPlaceholder()) {
            if (!fClusters.empty()) {
                // Placeholder ends the previous word (placeholders are ignored in trimming)
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
                fWords.extend(fClusters);
            }
            // Placeholder is separate word and its width now is counted in minIntrinsicWidth
            fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, cluster->width());
            fWords.extend(cluster);
        } else {
            fClusters.extend(cluster);
            // Keep adding clusters/words
            if (fClusters.endOfWord()) {
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
                fWords.extend(fClusters);
            }
        }
        if ((fHardLineBreak = cluster->isHardBreak())) {
            // Stop at the hard line break
            break;
        }
    }
}
void TextWrapper::moveForward(bool hasEllipsis) {
    // We normally break lines by words.
    // The only way we may go to clusters is if the word is too long or
    // it's the first word and it has an ellipsis attached to it.
    // If nothing fits we show the clipping.
    if (!fWords.empty()) {
        fEndLine.extend(fWords);
#ifdef SK_IGNORE_SKPARAGRAPH_ELLIPSIS_FIX
        if (!fTooLongWord || hasEllipsis) { // Ellipsis added to a word
#else
        if (!fTooLongWord && !hasEllipsis) { // Ellipsis added to a grapheme
#endif
            return;
        }
    }
    if (!fClusters.empty()) {
        fEndLine.extend(fClusters);
        if (!fTooLongCluster) {
            return;
        }
    }
    if (!fClip.empty()) {
        // Flutter: forget the clipped cluster but keep the metrics
        fEndLine.metrics().add(fClip.metrics());
    }
}
// Special case for start/end cluster since they can be clipped
void TextWrapper::trimEndSpaces(TextAlign align) {
    // Remember the breaking position
    fEndLine.saveBreak();
    // Skip all space cluster at the end
    for (auto cluster = fEndLine.endCluster();
         cluster >= fEndLine.startCluster() && cluster->isWhitespaceBreak();
         --cluster) {
        fEndLine.trim(cluster);
    }
    fEndLine.trim();
}
SkScalar TextWrapper::getClustersTrimmedWidth() {
    // Move the end of the line to the left
    SkScalar width = 0;
    bool trailingSpaces = true;
    for (auto cluster = fClusters.endCluster(); cluster >= fClusters.startCluster(); --cluster) {
        if (cluster->run().isPlaceholder()) {
            continue;
        }
        if (trailingSpaces) {
            if (!cluster->isWhitespaceBreak()) {
                width += cluster->trimmedWidth(cluster->endPos());
                trailingSpaces = false;
            }
            continue;
        }
        width += cluster->width();
    }
    return width;
}
// Trim the beginning spaces in case of soft line break
std::tuple<Cluster*, size_t, SkScalar> TextWrapper::trimStartSpaces(Cluster* endOfClusters) {
    if (fHardLineBreak) {
        // End of line is always end of cluster, but need to skip \n
        auto width = fEndLine.width();
        auto cluster = fEndLine.endCluster() + 1;
        while (cluster < fEndLine.breakCluster() && cluster->isWhitespaceBreak())  {
            width += cluster->width();
            ++cluster;
        }
        return std::make_tuple(fEndLine.breakCluster() + 1, 0, width);
    }
    // breakCluster points to the end of the line;
    // It's a soft line break so we need to move lineStart forward skipping all the spaces
    auto width = fEndLine.widthWithGhostSpaces();
    auto cluster = fEndLine.breakCluster() + 1;
    while (cluster < endOfClusters && cluster->isWhitespaceBreak()) {
        width += cluster->width();
        ++cluster;
    }
    if (fEndLine.breakCluster()->isWhitespaceBreak() && fEndLine.breakCluster() < endOfClusters) {
        // In case of a soft line break by the whitespace
        // fBreak should point to the beginning of the next line
        // (it only matters when there are trailing spaces)
        fEndLine.shiftBreak();
    }
    return std::make_tuple(cluster, 0, width);
}
// TODO: refactor the code for line ending (with/without ellipsis)
void TextWrapper::breakTextIntoLines(ParagraphImpl* parent,
                                     SkScalar maxWidth,
                                     const AddLineToParagraph& addLine) {
    fHeight = 0;
    fMinIntrinsicWidth = std::numeric_limits<SkScalar>::min();
    fMaxIntrinsicWidth = std::numeric_limits<SkScalar>::min();
    auto span = parent->clusters();
    if (span.empty()) {
        return;
    }
    auto maxLines = parent->paragraphStyle().getMaxLines();
    auto align = parent->paragraphStyle().effective_align();
    auto unlimitedLines = maxLines == std::numeric_limits<size_t>::max();
    auto endlessLine = !SkScalarIsFinite(maxWidth);
    auto hasEllipsis = parent->paragraphStyle().ellipsized();
    auto disableFirstAscent = parent->paragraphStyle().getTextHeightBehavior() & TextHeightBehavior::kDisableFirstAscent;
    auto disableLastDescent = parent->paragraphStyle().getTextHeightBehavior() & TextHeightBehavior::kDisableLastDescent;
    bool firstLine = true; // We only interested in fist line if we have to disable the first ascent
    SkScalar softLineMaxIntrinsicWidth = 0;
    fEndLine = TextStretch(span.begin(), span.begin(), parent->strutForceHeight());
    auto end = span.end() - 1;
    auto start = span.begin();
    InternalLineMetrics maxRunMetrics;
    bool needEllipsis = false;
    while (fEndLine.endCluster() != end) {
        this->lookAhead(maxWidth, end, parent->getApplyRoundingHack());
        auto lastLine = (hasEllipsis && unlimitedLines) || fLineNumber >= maxLines;
        needEllipsis = hasEllipsis && !endlessLine && lastLine;
        this->moveForward(needEllipsis);
        needEllipsis &= fEndLine.endCluster() < end - 1; // Only if we have some text to ellipsize
        // Do not trim end spaces on the naturally last line of the left aligned text
        this->trimEndSpaces(align);
        // For soft line breaks add to the line all the spaces next to it
        Cluster* startLine;
        size_t pos;
        SkScalar widthWithSpaces;
        std::tie(startLine, pos, widthWithSpaces) = this->trimStartSpaces(end);
        if (needEllipsis && !fHardLineBreak) {
            // This is what we need to do to preserve a space before the ellipsis
            fEndLine.restoreBreak();
            widthWithSpaces = fEndLine.widthWithGhostSpaces();
        }
        // If the line is empty with the hard line break, let's take the paragraph font (flutter???)
        if (fEndLine.metrics().isClean()) {
            fEndLine.setMetrics(parent->getEmptyMetrics());
        }
        // Deal with placeholder clusters == runs[@size==1]
        Run* lastRun = nullptr;
        for (auto cluster = fEndLine.startCluster(); cluster <= fEndLine.endCluster(); ++cluster) {
            auto r = cluster->runOrNull();
            if (r == lastRun) {
                continue;
            }
            lastRun = r;
            if (lastRun->placeholderStyle() != nullptr) {
                SkASSERT(lastRun->size() == 1);
                // Update the placeholder metrics so we can get the placeholder positions later
                // and the line metrics (to make sure the placeholder fits)
                lastRun->updateMetrics(&fEndLine.metrics());
            }
        }
        // Before we update the line metrics with struts,
        // let's save it for GetRectsForRange(RectHeightStyle::kMax)
        maxRunMetrics = fEndLine.metrics();
        maxRunMetrics.fForceStrut = false;
        // TODO: keep start/end/break info for text and runs but in a better way that below
        TextRange textExcludingSpaces(fEndLine.startCluster()->textRange().start, fEndLine.endCluster()->textRange().end);
        TextRange text(fEndLine.startCluster()->textRange().start, fEndLine.breakCluster()->textRange().start);
        TextRange textIncludingNewlines(fEndLine.startCluster()->textRange().start, startLine->textRange().start);
        if (startLine == end) {
            textIncludingNewlines.end = parent->text().size();
            text.end = parent->text().size();
        }
        ClusterRange clusters(fEndLine.startCluster() - start, fEndLine.endCluster() - start + 1);
        ClusterRange clustersWithGhosts(fEndLine.startCluster() - start, startLine - start);
        if (disableFirstAscent && firstLine) {
            fEndLine.metrics().fAscent = fEndLine.metrics().fRawAscent;
        }
        if (disableLastDescent && (lastLine || (startLine == end && !fHardLineBreak ))) {
            fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
        }
        if (parent->strutEnabled()) {
            // Make sure font metrics are not less than the strut
            parent->strutMetrics().updateLineMetrics(fEndLine.metrics());
        }
        SkScalar lineHeight = fEndLine.metrics().height();
        firstLine = false;
        if (fEndLine.empty()) {
            // Correct text and clusters (make it empty for an empty line)
            textExcludingSpaces.end = textExcludingSpaces.start;
            clusters.end = clusters.start;
        }
        // In case of a force wrapping we don't have a break cluster and have to use the end cluster
        text.end = std::max(text.end, textExcludingSpaces.end);
        addLine(textExcludingSpaces,
                text,
                textIncludingNewlines, clusters, clustersWithGhosts, widthWithSpaces,
                fEndLine.startPos(),
                fEndLine.endPos(),
                SkVector::Make(0, fHeight),
                SkVector::Make(fEndLine.width(), lineHeight),
                fEndLine.metrics(),
                needEllipsis && !fHardLineBreak);
        softLineMaxIntrinsicWidth += widthWithSpaces;
        fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, softLineMaxIntrinsicWidth);
        if (fHardLineBreak) {
            softLineMaxIntrinsicWidth = 0;
        }
        // Start a new line
        fHeight += lineHeight;
        if (!fHardLineBreak || startLine != end) {
            fEndLine.clean();
        }
        fEndLine.startFrom(startLine, pos);
        parent->fMaxWidthWithTrailingSpaces = std::max(parent->fMaxWidthWithTrailingSpaces, widthWithSpaces);
        if (hasEllipsis && unlimitedLines) {
            // There is one case when we need an ellipsis on a separate line
            // after a line break when width is infinite
            if (!fHardLineBreak) {
                break;
            }
        } else if (lastLine) {
            // There is nothing more to draw
            fHardLineBreak = false;
            break;
        }
        ++fLineNumber;
    }
    // We finished formatting the text but we need to scan the rest for some numbers
    // TODO: make it a case of a normal flow
    if (fEndLine.endCluster() != nullptr) {
        auto lastWordLength = 0.0f;
        auto cluster = fEndLine.endCluster();
        while (cluster != end || cluster->endPos() < end->endPos()) {
            fExceededMaxLines = true;
            if (cluster->isHardBreak()) {
                // Hard line break ends the word and the line
                fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, softLineMaxIntrinsicWidth);
                softLineMaxIntrinsicWidth = 0;
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
                lastWordLength = 0;
            } else if (cluster->isWhitespaceBreak()) {
                // Whitespaces end the word
                softLineMaxIntrinsicWidth += cluster->width();
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
                lastWordLength = 0;
            } else if (cluster->run().isPlaceholder()) {
                // Placeholder ends the previous word and creates a separate one
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
                // Placeholder width now counts in fMinIntrinsicWidth
                softLineMaxIntrinsicWidth += cluster->width();
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, cluster->width());
                lastWordLength = 0;
            } else {
                // Nothing out of ordinary - just add this cluster to the word and to the line
                softLineMaxIntrinsicWidth += cluster->width();
                lastWordLength += cluster->width();
            }
            ++cluster;
        }
        fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
        fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, softLineMaxIntrinsicWidth);
        if (parent->lines().empty()) {
            // In case we could not place even a single cluster on the line
            if (disableFirstAscent) {
                fEndLine.metrics().fAscent = fEndLine.metrics().fRawAscent;
            }
            if (disableLastDescent && !fHardLineBreak) {
                fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
            }
            fHeight = std::max(fHeight, fEndLine.metrics().height());
        }
    }
    if (fHardLineBreak) {
        if (disableLastDescent) {
            fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
        }
        // Last character is a line break
        if (parent->strutEnabled()) {
            // Make sure font metrics are not less than the strut
            parent->strutMetrics().updateLineMetrics(fEndLine.metrics());
        }
        ClusterRange clusters(fEndLine.breakCluster() - start, fEndLine.endCluster() - start);
        addLine(fEndLine.breakCluster()->textRange(),
                fEndLine.breakCluster()->textRange(),
                fEndLine.endCluster()->textRange(),
                clusters,
                clusters,
                0,
                0,
                0,
                SkVector::Make(0, fHeight),
                SkVector::Make(0, fEndLine.metrics().height()),
                fEndLine.metrics(),
                needEllipsis);
        fHeight += fEndLine.metrics().height();
        parent->lines().back().setMaxRunMetrics(maxRunMetrics);
    }
    if (parent->lines().empty()) {
        return;
    }
    // Correct line metric styles for the first and for the last lines if needed
    if (disableFirstAscent) {
        parent->lines().front().setAscentStyle(LineMetricStyle::Typographic);
    }
    if (disableLastDescent) {
        parent->lines().back().setDescentStyle(LineMetricStyle::Typographic);
    }
}
#endif
}  // namespace textlayout
}  // namespace skia
