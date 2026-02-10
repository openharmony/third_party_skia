// Copyright 2019 Google LLC.
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skparagraph/src/TextWrapper.h"

#ifdef ENABLE_TEXT_ENHANCE
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>

#include "include/DartTypes.h"
#include "log.h"
#include "modules/skparagraph/include/Hyphenator.h"
#include "modules/skparagraph/src/TextTabAlign.h"
#include "TextParameter.h"
#endif

namespace skia {
namespace textlayout {

namespace {
#ifdef ENABLE_TEXT_ENHANCE
const size_t BREAK_NUM_TWO = 2;
#endif
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

#ifdef ENABLE_TEXT_ENHANCE
void TextWrapper::matchHyphenResult(const std::vector<uint8_t>& result, ParagraphImpl* owner, size_t& pos,
                                    SkScalar maxWidth, SkScalar len)
{
    auto startPos = pos;
    size_t ix = 0;

    // Result array may have more code points than we have visible clusters
    int32_t prevIx = -1;
    // cumulatively iterate width vs breaks
    for (const auto& breakPos : result) {
        int32_t clusterIx = static_cast<int32_t>(owner->fClustersIndexFromCodeUnit[startPos + ix]);
        if (clusterIx == prevIx) {
            ++ix;
            continue;
        }
        prevIx = clusterIx;
        TEXT_LOGD("hyphen break width:%{public}f / %{public}f : %{public}f", len, maxWidth,
                  owner->cluster(clusterIx).width());
        len += owner->cluster(clusterIx).width();
        auto shouldBreak = (len > maxWidth);
        if (breakPos & 0x1) {
            // we need to break after previous char, but the result needs to be mapped
            pos = startPos + ix;
        }
        ++ix;
        if (shouldBreak) {
            break;
        }
    }
}

size_t TextWrapper::tryBreakWord(Cluster *startCluster, Cluster *endOfClusters,
                                 SkScalar widthBeforeCluster, SkScalar maxWidth)
{
    auto startPos = startCluster->textRange().start;
    auto endPos = startPos;
    auto owner = startCluster->getOwner();
    for (auto next = startCluster + 1; next != endOfClusters; next++) {
        // find the end boundary of current word (hard/soft/whitespace break)
        if (next->isWhitespaceBreak() || next->isHardBreak()) {
            break;
        } else {
            endPos = next->textRange().end;
        }
    }

    // Using end position cluster height as an estimate for reserved hyphen width
    // hyphen may not be shaped at this point. End pos may be non-drawable, so prefer
    // previous glyph before break
    auto mappedEnd = owner->fClustersIndexFromCodeUnit[endPos];
    auto len = widthBeforeCluster + owner->cluster(mappedEnd > 0 ? mappedEnd - 1 : 0).height();
    // ToDo: We can stop here based on hyhpenation frequency setting
    // but there is no need to proceed with hyphenation if we don't have space for even hyphen
    if (std::isnan(len) || len >= maxWidth) {
        return startCluster->textRange().start;
    }

    auto locale = owner->paragraphStyle().getTextStyle().getLocale();
    auto result = Hyphenator::getInstance().findBreakPositions(locale, owner->fText, startPos, endPos);
    endPos = startPos;
    matchHyphenResult(result, owner, endPos, maxWidth, len);

    return endPos;
}

bool TextWrapper::lookAheadByHyphen(Cluster* endOfClusters, SkScalar widthBeforeCluster, SkScalar maxWidth)
{
    auto startCluster = fClusters.startCluster();
    while (startCluster != endOfClusters && startCluster->isWhitespaceBreak()) {
        ++startCluster;
    }
    if (startCluster == endOfClusters) {
        return false;
    }
    auto endPos = tryBreakWord(startCluster, endOfClusters, widthBeforeCluster - fClusters.width(), maxWidth);
    // if break position found, set fClusters and fWords accrodingly and break
    if (endPos > startCluster->textRange().start) {
        // need to break before the mapped end cluster
        auto owner = startCluster->getOwner();
        fClusters = TextStretch(startCluster, &owner->cluster(owner->fClustersIndexFromCodeUnit[endPos]) - 1,
                                fClusters.metrics().getForceStrut());
        fWords.extend(fClusters);
        fBrokeLineWithHyphen = true;
        return false;
    }
    // else let the existing implementation do its best efforts
    return true;
}

// Since we allow cluster clipping when they don't fit
// we have to work with stretches - parts of clusters
void TextWrapper::lookAhead(SkScalar maxWidth, Cluster* endOfClusters, bool applyRoundingHack,
                            WordBreakType wordBreakType) {

    reset();
    fEndLine.metrics().clean();
    fWords.startFrom(fEndLine.startCluster(), fEndLine.startPos());
    fClusters.startFrom(fEndLine.startCluster(), fEndLine.startPos());
    fClip.startFrom(fEndLine.startCluster(), fEndLine.startPos());

    bool isFirstWord = true;
    TextTabAlign textTabAlign(endOfClusters->getOwner()->paragraphStyle().getTextTab());
    textTabAlign.init(maxWidth, endOfClusters);

    LineBreakerWithLittleRounding breaker(maxWidth, applyRoundingHack);
    Cluster* nextNonBreakingSpace = nullptr;
    SkScalar totalFakeSpacing = 0.0;
    bool attemptedHyphenate = false;

    for (auto cluster = fEndLine.endCluster(); cluster < endOfClusters; ++cluster) {
        if (cluster == fEndLine.endCluster()) {
            ClusterIndex headClusterIndex = cluster->getOwner()->clusterIndex(cluster->textRange().start);
            bool isProcessedHeadPunc = fParent->isShapedCompressHeadPunctuation(headClusterIndex);
            if (isProcessedHeadPunc) {
                fParent->setNeedUpdateRunCache(true);
            }
        }
        totalFakeSpacing += (cluster->needAutoSpacing() && cluster != fEndLine.endCluster()) ?
            (cluster - 1)->getFontSize() / AUTO_SPACING_WIDTH_RATIO : 0;
        SkScalar widthBeforeCluster = fWords.width() + fClusters.width() + totalFakeSpacing;
        if (cluster->isHardBreak()) {
            if (cluster != fEndLine.endCluster()) {
                isFirstWord = false;
            }
        } else if (
                // TODO: Trying to deal with flutter rounding problem. Must be removed...
                SkScalar width = cluster->width() + widthBeforeCluster;
                (!isFirstWord || wordBreakType != WordBreakType::NORMAL) &&
                breaker.breakLine(width)) {
            // if the hyphenator has already run as balancing algorithm, use the cluster information
            if (cluster->isHyphenBreak() && !fNeedEllipsis) {
                // we dont want to add the current cluster as the hyphenation algorithm marks breaks before a cluster
                // however, if we cannot fit anything to a line, we need to break out here
                if (fWords.empty() && fClusters.empty()) {
                    fClusters.extend(cluster);
                    fTooLongCluster = true;
                    break;
                }
                if (!fClusters.empty()) {
                    fWords.extend(fClusters);
                    fBrokeLineWithHyphen = true;
                    break;
                }
                // let hyphenator try before this if it is enabled
            } else if (cluster->isWhitespaceBreak() && ((wordBreakType != WordBreakType::BREAK_HYPHEN) ||
                       (wordBreakType == WordBreakType::BREAK_HYPHEN && attemptedHyphenate && !fNeedEllipsis))) {
                // It's the end of the word
                isFirstWord = false;
                fClusters.extend(cluster);

                bool tabAlignRet = false;
                if (cluster->isTabulation()) {
                    tabAlignRet = textTabAlign.processTab(fWords, fClusters, cluster, totalFakeSpacing);
                } else {
                    tabAlignRet = textTabAlign.processEndofWord(fWords, fClusters, cluster, totalFakeSpacing);
                }
                if (tabAlignRet) {
                    break;
                }
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

            // should do this only if hyphenation is enabled
            if (wordBreakType == WordBreakType::BREAK_HYPHEN && !attemptedHyphenate && !fClusters.empty() &&
                !fNeedEllipsis) {
                attemptedHyphenate = true;
                if (!lookAheadByHyphen(endOfClusters, widthBeforeCluster, breaker.fUpper)) {
                    break;
                }
            }

            textTabAlign.processEndofLine(fWords, fClusters, cluster, totalFakeSpacing);

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
            if (cluster->isTabulation()) {
                if (textTabAlign.processTab(fWords, fClusters, cluster, totalFakeSpacing)) {
                    break;
                }
                fClusters.extend(cluster);

                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
                fWords.extend(fClusters);
            } else {
                fClusters.extend(cluster);
                if (fClusters.endOfWord()) { // Keep adding clusters/words
                    if (textTabAlign.processEndofWord(fWords, fClusters, cluster, totalFakeSpacing)) {
                        if (wordBreakType == WordBreakType::BREAK_ALL) {
                            fClusters.trim(cluster);
                        }
                        break;
                    }

                    fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
                    fWords.extend(fClusters);
                } else {
                    if (textTabAlign.processCluster(fWords, fClusters, cluster, totalFakeSpacing)) {
                        fClusters.trim(cluster);
                        break;
                    }
                }
            }
        }

        if ((fHardLineBreak = cluster->isHardBreak())) {
            // Stop at the hard line break
            break;
        }
    }
}

void TextWrapper::moveForward(bool breakAll) {

    // We normally break lines by words.
    // The only way we may go to clusters is if the word is too long or
    // it's the first word and it has an ellipsis attached to it.
    // If nothing fits we show the clipping.
    fTooLongWord = breakAll;
    if (!fWords.empty()) {
        fEndLine.extend(fWords);
#ifdef SK_IGNORE_SKPARAGRAPH_ELLIPSIS_FIX
        if (!fTooLongWord || fNeedEllipsis) { // Ellipsis added to a word
#else
        if (!fTooLongWord && !fNeedEllipsis) { // Ellipsis added to a grapheme
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
    while (cluster < endOfClusters && cluster->isWhitespaceBreak() && !cluster->isTabulation()) {
        width += cluster->width();
        ++cluster;
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
        CalculateCumulativeLen(parent);

        if (parent_.getLineBreakStrategy() == LineBreakStrategy::BALANCED) {
            // calculate target width before breaks
            int64_t targetLines = 1 + cumulativeLen_ / maxWidth_;
            currentTarget_ = cumulativeLen_ / targetLines;
        }

        GenerateBreaks(parent);
    }

    void GenerateBreaks(ParagraphImpl& parent)
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

    void CalculateCumulativeLen(ParagraphImpl& parent)
    {
        auto startCluster = &parent.cluster(0);
        auto endCluster = &parent.cluster(0);
        auto locale = parent.paragraphStyle().getTextStyle().getLocale();
        for (size_t clusterIx = 0; clusterIx < parent.clusters().size(); clusterIx++) {
            if (parent.getLineBreakStrategy() == LineBreakStrategy::BALANCED) {
                auto& cluster = parent.cluster(clusterIx);
                auto len = cluster.width();
                cumulativeLen_ += len;
            }
            CalculateHyphenPos(clusterIx, startCluster, endCluster, parent, locale);
        }
    }

    void CalculateHyphenPos(size_t clusterIx, Cluster*& startCluster, Cluster*& endCluster, ParagraphImpl& parent,
                            const SkString& locale)
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

    void CheckHyphenBreak(std::vector<uint8_t> results, ParagraphImpl& parent, Cluster*& startCluster)
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

    std::vector<SkScalar>& GetResult()
    {
        return current_;
    }

    SkScalar calculateCurrentWidth(RecursiveParam& param, bool looped)
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
            return (lineNumber == other.lineNumber && fabs(begin - other.begin) < WIDTH_TOLERANCE &&
                fabs(width - other.width) < WIDTH_TOLERANCE);
        }
        bool operator<(const Index& other) const
        {
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
            BREAKTYPE_FORCED,
            BREAKTYPE_HYPHEN
        };
        Break(SkScalar w, BreakType t, bool ssws) : width(w), type(t), subsequentWhitespace(ssws) {}

        SkScalar width { 0.f };
        BreakType type { BreakType::BREAKTYPE_NONE };
        bool subsequentWhitespace { false };
        SkScalar reservedSpace { 0.f };
    };

    std::vector<Break> breaks_;
    size_t lastBreakPos_ { 0 };

    uint64_t cacheHits_ { 0 };
	bool fPrevWasWhitespace{false};
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
    initParent(parent);
    auto span = fParent->clusters();
    if (span.empty()) {
        return;
    }
    initializeFormattingState(maxWidth, span);

    if (fParent->getLineBreakStrategy() == LineBreakStrategy::BALANCED &&
        fParent->getWordBreakType() != WordBreakType::BREAK_ALL &&
        fParent->getWordBreakType() != WordBreakType::BREAK_HYPHEN &&
        (fParent->getParagraphStyle().getTextTab().location < 0)) {
        layoutLinesBalanced(maxWidth, addLine);
        return;
    }

    layoutLinesSimple(maxWidth, addLine);
    fParent->fParagraphStyle.setMaxLines(fParentOriMaxLines);
}

void TextWrapper::layoutLinesSimple(SkScalar maxWidth, const AddLineToParagraph& addLine) {
    auto span = fParent->clusters();
    // Resolve balanced line widths
    std::vector<SkScalar> balancedWidths = generateBalancedLayoutWidths();
    fEndLine = TextStretch(span.begin(), span.begin(), fParent->strutForceHeight() && fParent->strutEnabled());
    SkScalar newWidth = maxWidth;
    while (fEndLine.endCluster() != fEnd) {
        newWidth = calculateMaxLineLayoutWidth(balancedWidths, maxWidth);
        checkIsLastLine();
        checkNeedEllipsisByLastLine();

        this->lookAhead(newWidth, fEnd, fParent->getApplyRoundingHack(), fParent->getWordBreakType());

        this->moveForward(fParent->getWordBreakType() == WordBreakType::BREAK_ALL);
        if (fEndLine.endCluster() >= fEndLine.startCluster() || fFormattingContext.maxLines > 1) {
            if (!checkNeedEllipsisByMultiLineEllipsis()) {
                // Only if we have some text to ellipsize.
                fNeedEllipsis &= fEndLine.endCluster() < fEnd - 1;
            }
        }

        trimLineSpaces();
        handleSpecialCases();
        updateLineMetrics();
        checkHardLineBreakByEllipsis();

        TriggerFlag triggerFlag = triggerConstraintsLayout();
        if (triggerFlag == TriggerFlag::TRIGGERED) {
            continue;
        } else if (triggerFlag == TriggerFlag::SKIP) {
            break;
        }

        addFormattedLineToParagraph(addLine);
        advanceToNextLine();
        if (shouldBreakFormattingLoop()) {
            break;
        }
        ++fLineNumber;
    }
    finalizeTextLayout(addLine);
}

bool TextWrapper::isNewWidthToBeSetMax() {
    if ((fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::HEAD &&
        fParent->paragraphStyle().getMaxLines() == 1) || fParent->needCreateOneLineMiddleEllipsis()) {
            return true;
    } else if ((fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MULTILINE_HEAD ||
        fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MULTILINE_MIDDLE) &&
        fLineNumber >= fParent->paragraphStyle().getMaxLines() && fParent->paragraphStyle().ellipsized()) {
            return true;
    }

    return false;
}

TextWrapper::TriggerFlag TextWrapper::triggerConstraintsLayout() {
    if (!fParent->fUseLayoutConstraints) {
        return TriggerFlag::NOT_TIRGGERED;
    }

    SkScalar endLineHeight = fEndLine.metrics().height();
    if (fFormattingContext.needLineSpacing && !fFormattingContext.disableLastDescent) {
        endLineHeight += fFormattingContext.lineSpacing;
    }
    if (fFormattingContext.disableLastDescent) {
        endLineHeight -= fEndLine.metrics().descent() - fEndLine.metrics().rawDescent();
    }
    if (fHeight + endLineHeight < fParent->fConstraintsHeight) {
        return TriggerFlag::NOT_TIRGGERED;
    }

    auto& lines = fParent->exportTextLines();
    if (lines.empty()) {
        return TriggerFlag::SKIP;
    }
    TextLine& lastLine = lines.back();
    fHeight -= lastLine.height();

    // Update fEndLine and rooback pre line.
    fFormattingContext.maxLines = lines.size();
    fParent->getParagraphStyle().setMaxLines(fFormattingContext.maxLines);
    fEndLine.clean();
    Cluster* cluster = &fParent->cluster(lastLine.clusters().start);
    fEndLine.startFrom(cluster, cluster->startPos());
    lines.pop_back();
    --fLineNumber;

    return TriggerFlag::TRIGGERED;
}

SkScalar TextWrapper::calculateMaxLineLayoutWidth(const std::vector<SkScalar>& balancedWidths, SkScalar maxWidth) {
    fNoIndentWidth = maxWidth - fParent->detectIndents(fLineNumber - 1);
    if (isNewWidthToBeSetMax()) {
        return FLT_MAX;
    } else if (!balancedWidths.empty() && fLineNumber - 1 < balancedWidths.size()) {
        return balancedWidths[fLineNumber - 1];
    } else {
        return fNoIndentWidth;
    }
}

std::vector<SkScalar> TextWrapper::generateBalancedLayoutWidths() {
    std::vector<SkScalar> balancedWidths;

    // if word breaking strategy is nontrivial (balanced / optimal), AND word break mode is not BREAK_ALL
    if (fParent->getWordBreakType() != WordBreakType::BREAK_ALL &&
        fParent->getLineBreakStrategy() != LineBreakStrategy::GREEDY) {
        if (CalculateBestScore(balancedWidths, fNoIndentWidth, fParent, fFormattingContext.maxLines) < 0) {
            // if the line breaking strategy returns a negative score, the algorithm could not fit or break the text
            // fall back to default, greedy algorithm
            balancedWidths.clear();
        }
    }
    return balancedWidths;
}

std::vector<uint8_t> TextWrapper::findBreakPositions(Cluster* startCluster,
                                                     Cluster* endOfClusters,
                                                     SkScalar widthBeforeCluster,
                                                     SkScalar maxWidth) {
    auto startPos = startCluster->textRange().start;
    auto endPos = startPos;
    auto owner = startCluster->getOwner();
    for (auto next = startCluster + 1; next < endOfClusters; next++) {
        // find the end boundary of current word (hard/soft/whitespace break)
        if (next->isWhitespaceBreak() || next->isHardBreak()) {
            break;
        }
        endPos = next->textRange().end;
    }

    // Using end position cluster height as an estimate for reserved hyphen width
    // hyphen may not be shaped at this point. End pos may be non-drawable, so prefer
    // previous glyph before break
    auto mappedEnd = owner->fClustersIndexFromCodeUnit[endPos];
    auto len = widthBeforeCluster + owner->cluster(mappedEnd > 0 ? mappedEnd - 1 : 0).height();
    // ToDo: We can stop here based on hyhpenation frequency setting
    // but there is no need to proceed with hyphenation if we don't have space for even hyphen
    if (std::isnan(len) || len >= maxWidth) {
        return std::vector<uint8_t>{};
    }

    auto locale = owner->paragraphStyle().getTextStyle().getLocale();
    return Hyphenator::getInstance().findBreakPositions(locale, owner->fText, startPos, endPos);
}

void TextWrapper::pushToWordStretches() {
    fWordStretches.push_back(fClusters);
    fWordWidthGroups.push_back(fClusters.width());
    fClusters.clean();
}

void TextWrapper::pushToWordStretchesBatch() {
    fWordStretchesBatch.push_back(fWordStretches);
    fWordWidthGroupsBatch.push_back(fWordWidthGroups);
    fWordStretches.clear();
    fWordWidthGroups.clear();
}

void TextWrapper::checkConstraintsLayoutByTextStretch(TextStretch& lineStretch) {
    fTotalLineStretchesHeight += lineStretch.metrics().height();
    if (fFormattingContext.needLineSpacing) {
        fTotalLineStretchesHeight += fFormattingContext.lineSpacing;
    }

    size_t assumeLineCnt = fLineStretches.size();
    if (assumeLineCnt == 1 && fFormattingContext.disableFirstAscent) {
        fTotalLineStretchesHeight -= lineStretch.metrics().rawAscent() - lineStretch.metrics().ascent();
    }

    SkScalar lastDescent = 0.0;
    if (fFormattingContext.disableLastDescent) {
        lastDescent = lineStretch.metrics().descent() - lineStretch.metrics().rawDescent();
        if (fFormattingContext.needLineSpacing) {
            lastDescent += fFormattingContext.lineSpacing;
        }
    }

    if (fTotalLineStretchesHeight - lastDescent > fParent->fConstraintsHeight) {
        fFormattingContext.maxLines = assumeLineCnt - 1;
        fParent->fParagraphStyle.setMaxLines(fFormattingContext.maxLines);
        fCheckBalancedConstraintsLayout = false;
    }
}

void TextWrapper::generateLineStretches(const std::vector<std::pair<size_t, size_t>>& linesGroupInfo,
    std::vector<TextStretch>& wordStretches) {
    if (fParent->paragraphStyle().getIsEndAddParagraphSpacing() &&
        fParent->paragraphStyle().getParagraphSpacing() > 0) {
        fTotalLineStretchesHeight += fParent->paragraphStyle().getParagraphSpacing();
    }

    for (const std::pair<size_t, size_t>& pair : linesGroupInfo) {
        TextStretch endLine{};
        for (size_t i = pair.first; i <= pair.second; ++i) {
            if (i == pair.first) {
                endLine.setStartCluster(wordStretches[i].startCluster());
            }
            endLine.extend(wordStretches[i]);
        }
        fLineStretches.push_back(endLine);

        if (!fCheckBalancedConstraintsLayout) {
            continue;
        }
        checkConstraintsLayoutByTextStretch(endLine);
    }
}

void TextWrapper::extendCommonCluster(Cluster* cluster, TextTabAlign& textTabAlign,
    SkScalar& totalFakeSpacing, WordBreakType wordBreakType) {
    if (cluster->isTabulation()) {
        if (textTabAlign.processTab(fWords, fClusters, cluster, totalFakeSpacing)) {
            return;
        }
        fClusters.extend(cluster);

        fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
        pushToWordStretches();
    } else {
        fClusters.extend(cluster);
        if (fClusters.endOfWord()) { // Keep adding clusters/words
            textTabAlign.processEndofWord(fWords, fClusters, cluster, totalFakeSpacing);
            fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
            pushToWordStretches();
        } else {
            if (textTabAlign.processCluster(fWords, fClusters, cluster, totalFakeSpacing)) {
                fClusters.trim(cluster);
            }
        }
    }
}

void TextWrapper::generateWordStretches(WordBreakType wordBreakType) {
    fEndLine.metrics().clean();
    fClusters.startFrom(fStart, fStart->startPos());
    fClip.startFrom(fStart, fStart->startPos());

    TextTabAlign textTabAlign(fStart->getOwner()->paragraphStyle().getTextTab());
    textTabAlign.init(MAX_LINES_LIMIT, fStart);

    SkScalar totalFakeSpacing = 0.0;
    Cluster* start = fStart;

    for (Cluster* cluster = start; cluster < fEnd; ++cluster) {
        totalFakeSpacing += (cluster->needAutoSpacing() && cluster != start)
                                    ? (cluster - 1)->getFontSize() / AUTO_SPACING_WIDTH_RATIO
                                    : 0;
        if (cluster->run().isPlaceholder()) {
            if (!fClusters.empty()) {
                // Placeholder ends the previous word (placeholders are ignored in trimming)
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
                pushToWordStretches();
            }

            // Placeholder is separate word and its width now is counted in minIntrinsicWidth
            fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, cluster->width());
            fClusters.extend(cluster);
            pushToWordStretches();
        } else {
            extendCommonCluster(cluster, textTabAlign, totalFakeSpacing, wordBreakType);
        }

        if (cluster->isHardBreak()) {
            start = cluster;
            pushToWordStretchesBatch();
        }
    }

    if (!fEnd->isHardBreak()) {
        pushToWordStretchesBatch();
    }
}

SkScalar TextWrapper::getTextStretchTrimmedEndSpaceWidth(const TextStretch& stretch) {
    SkScalar width = stretch.width();
    for (Cluster* cluster = stretch.endCluster(); cluster >= stretch.startCluster(); --cluster) {
        if (nearlyEqual(cluster->width(), 0) && cluster->run().isPlaceholder()) {
            continue;
        }

        if (!cluster->isWhitespaceBreak()) {
            break;
        }
        width -= cluster->width();
    }
    return width;
}

void calculateCostTable(const std::vector<SkScalar>& clustersWidth,
                        SkScalar maxWidth,
                        std::vector<SkScalar>& costTable,
                        std::vector<std::pair<size_t, size_t>>& bestPick) {
    int clustersCnt = static_cast<int>(clustersWidth.size());
    for (int clustersIndex = clustersCnt - STRATEGY_START_POS; clustersIndex >= 0; --clustersIndex) {
        bestPick[clustersIndex].first = static_cast<size_t>(clustersIndex);
        SkScalar rowCurrentLen = 0;
        std::vector<SkScalar> costList;

        int maxWord = clustersIndex;
        for (int j = clustersIndex; j < clustersCnt; j++) {
            rowCurrentLen += clustersWidth[j];
            if (rowCurrentLen > maxWidth) {
                rowCurrentLen -= clustersWidth[j];
                maxWord = j - 1;
                break;
            }
            maxWord = j;
        }
        if (maxWord < clustersIndex) {
            maxWord = clustersIndex;
        }

        for (int j = clustersIndex; j <= maxWord; ++j) {
            SkScalar cost = std::pow(std::abs(std::accumulate(clustersWidth.begin() + clustersIndex,
                                                              clustersWidth.begin() + j + 1,
                                                              0) -
                                              maxWidth),
                                     STRATEGY_START_POS);
            if (j + 1 <= clustersCnt - 1) {
                cost += costTable[j + 1];
            }
            costList.push_back(cost);
        }

        SkScalar minCost = *std::min_element(costList.begin(), costList.end());
        std::vector<size_t> minCostIndices;
        for (size_t q = 0; q < costList.size(); ++q) {
            if (nearlyZero(costList[q], minCost)) {
                minCostIndices.push_back(q);
            }
        }
        size_t minCostIdx{costList.size() - 1};
        if (minCostIndices.size() > 0) {
            minCostIdx = minCostIndices[static_cast<int32_t>(minCostIndices.size() / MIN_COST_POS)];
        }
        costTable[clustersIndex] = minCost;
        bestPick[clustersIndex].second = static_cast<size_t>(clustersIndex + minCostIdx);
    }
}

std::vector<std::pair<size_t, size_t>> buildWordBalance(
        const std::vector<std::pair<size_t, size_t>>& bestPick, int clustersCnt) {
    int rowStart{0};
    std::vector<std::pair<size_t, size_t>> wordBalance;

    while (rowStart < clustersCnt) {
        int rowEnd = bestPick[rowStart].second;
        wordBalance.emplace_back(rowStart, rowEnd);
        rowStart = rowEnd + 1;
    }
    return wordBalance;
}

std::vector<std::pair<size_t, size_t>> TextWrapper::generateLinesGroupInfo(
        const std::vector<SkScalar>& clustersWidth, SkScalar maxWidth) {
    std::vector<std::pair<size_t, size_t>> wordBalance;
    if (clustersWidth.empty()) {
        return wordBalance;
    }
    int clustersCnt = static_cast<int>(clustersWidth.size());
    std::vector<SkScalar> costTable(clustersCnt, 0);
    std::vector<std::pair<size_t, size_t>> bestPick(clustersCnt, {0, 0});

    costTable[clustersCnt - 1] =
            std::pow(std::abs(clustersWidth[clustersCnt - 1] - maxWidth), STRATEGY_START_POS);
    bestPick[clustersCnt - 1] = {clustersCnt - 1, clustersCnt - 1};

    calculateCostTable(clustersWidth, maxWidth, costTable, bestPick);

    return buildWordBalance(bestPick, clustersCnt);
}

std::vector<SkScalar> TextWrapper::generateWordsWidthInfo(const std::vector<TextStretch>& wordStretches) {
    std::vector<SkScalar> result;
    result.reserve(wordStretches.size());
    std::transform(wordStretches.begin(),
                   wordStretches.end(),
                   std::back_inserter(result),
                   [](const TextStretch& word) { return word.width(); });
    return result;
}

void TextWrapper::formalizedClusters(std::vector<TextStretch>& clusters, SkScalar limitWidth) {
    for (auto it = clusters.begin(); it != clusters.end(); it++) {
        if (it->width() < limitWidth) {
            continue;
        }
        std::list<TextStretch> trimmedWordList{*it};

        for (auto trimmedItor = trimmedWordList.begin(); trimmedItor != trimmedWordList.end();) {
            if (trimmedItor->width() < limitWidth) {
                ++trimmedItor;
                continue;
            }
            auto result = trimmedItor->split();
            trimmedItor = trimmedWordList.erase(trimmedItor);
            trimmedItor = trimmedWordList.insert(trimmedItor, result.begin(), result.end());
            std::advance(trimmedItor, result.size());
        }

        if (trimmedWordList.size() == 0) {
            continue;
        }

        it = clusters.erase(it);
        it = clusters.insert(it, trimmedWordList.begin(), trimmedWordList.end());
        std::advance(it, trimmedWordList.size() - 1);
    }
}

void TextWrapper::generateTextLines(SkScalar maxWidth, const AddLineToParagraph& addLine) {
    processLineStretches(maxWidth, addLine);
    finalizeTextLayout(addLine);
}

void TextWrapper::initializeFormattingState(SkScalar maxWidth, const SkSpan<Cluster>& span) {
    const auto& style = fParent->paragraphStyle();
    fFormattingContext = {style.getMaxLines() == std::numeric_limits<size_t>::max(),
                          !SkIsFinite(maxWidth),
                          style.ellipsized(),
                          style.getTextHeightBehavior() & TextHeightBehavior::kDisableFirstAscent,
                          style.getTextHeightBehavior() & TextHeightBehavior::kDisableLastDescent,
                          style.getMaxLines(),
                          style.effective_align(),
                          style.getLineSpacing() > 0,
                          style.getLineSpacing(),
                          style.getStrutStyle().getLineBreakStrategy() == LineBreakStrategy::BALANCED ?
                              LayoutMode::OPTIMIZE : LayoutMode::SIMPLE};

    fFirstLine = true;
    fSoftLineMaxIntrinsicWidth = 0;
    fNoIndentWidth = maxWidth;
    fEnd = span.end() - 1;
    fStart = span.begin();
    fParentOriMaxLines = fFormattingContext.maxLines;
    fCheckBalancedConstraintsLayout = fParent->fUseLayoutConstraints;
}

void TextWrapper::processLineStretches(SkScalar maxWidth, const AddLineToParagraph& addLine) {
    for (TextStretch& line : fLineStretches) {
        prepareLineForFormatting(line);
        formatCurrentLine(addLine);

        advanceToNextLine();
        if (shouldBreakFormattingLoop()) {
            break;
        }
        ++fLineNumber;
    }
}

void TextWrapper::finalizeTextLayout(const AddLineToParagraph& addLine) {
    if (fParent->paragraphStyle().getIsEndAddParagraphSpacing() &&
        fParent->paragraphStyle().getParagraphSpacing() > 0) {
        fHeight += fParent->paragraphStyle().getParagraphSpacing();
    }
    processRemainingClusters();
    addFinalLineBreakIfNeeded(addLine);
    adjustFirstLastLineMetrics();
}

void TextWrapper::prepareLineForFormatting(TextStretch& line) {
    fEndLine = std::move(line);
    fHardLineBreak = fEndLine.endCluster()->isHardBreak();
}

void TextWrapper::formatCurrentLine(const AddLineToParagraph& addLine) {
    determineIfEllipsisNeeded();
    trimLineSpaces();
    handleSpecialCases();
    updateLineMetrics();
    addFormattedLineToParagraph(addLine);
}

void TextWrapper::checkHardLineBreakByEllipsis() {
    if (fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::HEAD && fFormattingContext.hasEllipsis) {
        fNeedEllipsis = fFormattingContext.maxLines <= 1;
        if (fNeedEllipsis) {
            fHardLineBreak = false;
        }
    }
    if (fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MIDDLE && fFormattingContext.hasEllipsis) {
        fNeedEllipsis = fFormattingContext.maxLines <= 1;
    }
}

bool TextWrapper::checkNeedEllipsisByMultiLineEllipsis() {
    if (fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MULTILINE_HEAD ||
        fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MULTILINE_MIDDLE) {
        if (fFormattingContext.layoutMode != LayoutMode::OPTIMIZE) {
            fNeedEllipsis &= fLineNumber >= fFormattingContext.maxLines;
            return true;
        }

        if (fFormattingContext.hasEllipsis) {
            fNeedEllipsis = fFormattingContext.hasEllipsis && fLineNumber >= fFormattingContext.maxLines;
            return true;
        }
    }
    return false;
}

void TextWrapper::checkNeedEllipsisByLastLine() {
    fNeedEllipsis = fFormattingContext.hasEllipsis && !fFormattingContext.endlessLine && fIsLastLine;
}

void TextWrapper::checkIsLastLine() {
    fIsLastLine = (fFormattingContext.hasEllipsis && fFormattingContext.unlimitedLines) ||
        fLineNumber >= fFormattingContext.maxLines;
}

void TextWrapper::determineIfEllipsisNeeded() {
    checkIsLastLine();
    checkNeedEllipsisByLastLine();
    checkNeedEllipsisByMultiLineEllipsis();
    checkHardLineBreakByEllipsis();
}

void TextWrapper::trimLineSpaces() {
    // Do not trim end spaces on the naturally last line of the left aligned text
    this->trimEndSpaces(fFormattingContext.align);
    // For soft line breaks add to the line all the spaces next to it
    std::tie(fCurrentStartLine, fCurrentStartPos, fCurrentLineWidthWithSpaces) =
            this->trimStartSpaces(fEnd);
}

void TextWrapper::handleSpecialCases() {
    // This is what we need to do to preserve a space before the ellipsis
    if (fNeedEllipsis && !fHardLineBreak &&
        !(fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MULTILINE_HEAD ||
        fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MULTILINE_MIDDLE)) {
        fEndLine.restoreBreak();
        fCurrentLineWidthWithSpaces = fEndLine.widthWithGhostSpaces();
    } else if (fBrokeLineWithHyphen) {
        fEndLine.shiftWidth(fEndLine.endCluster()->width());
    }
}

void TextWrapper::updateLineMetrics() {
    if (fEndLine.metrics().isClean()) {
        fEndLine.setMetrics(fParent->getEmptyMetrics());
    }
    updatePlaceholderMetrics();
    adjustLineMetricsForFirstLastLine();
    applyStrutMetrics();
}

void TextWrapper::updatePlaceholderMetrics() {
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
    fMaxRunMetrics = fEndLine.metrics();
    fMaxRunMetrics.fForceStrut = false;
}

void TextWrapper::adjustLineMetricsForFirstLastLine() {
    if (fFormattingContext.disableFirstAscent && fFirstLine) {
        fEndLine.metrics().fAscent = fEndLine.metrics().fRawAscent;
    }
    if (fFormattingContext.disableLastDescent && (fIsLastLine || (fCurrentStartLine == fEnd && !fHardLineBreak))) {
        fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
        fFormattingContext.needLineSpacing = false;
    }
}

void TextWrapper::applyStrutMetrics() {
    if (fParent->strutEnabled()) {
        fParent->strutMetrics().updateLineMetrics(fEndLine.metrics());
    }
}

TextWrapper::LineTextRanges TextWrapper::calculateLineTextRanges() {
    LineTextRanges ranges;

    ranges.textExcludingSpaces = TextRange(fEndLine.startCluster()->textRange().start,
                                           fEndLine.endCluster()->textRange().end);
    ranges.text = TextRange(fEndLine.startCluster()->textRange().start,
                            fEndLine.breakCluster()->textRange().start);
    ranges.textIncludingNewlines = TextRange(fEndLine.startCluster()->textRange().start,
                                             fCurrentStartLine->textRange().start);

    if (fCurrentStartLine == fEnd) {
        ranges.textIncludingNewlines.end = fParent->text().size();
        ranges.text.end = fParent->text().size();
    }

    ranges.clusters = ClusterRange(fEndLine.startCluster() - fStart, fEndLine.endCluster() - fStart + 1);
    ranges.clustersWithGhosts = ClusterRange(fEndLine.startCluster() - fStart, fCurrentStartLine - fStart);

    if (fEndLine.empty()) {
        ranges.textExcludingSpaces.end = ranges.textExcludingSpaces.start;
        ranges.clusters.end = ranges.clusters.start;
    }

    // In case of a force wrapping we don't have a break cluster and have to use the end cluster
    ranges.text.end = std::max(ranges.text.end, ranges.textExcludingSpaces.end);

    return ranges;
}

SkScalar TextWrapper::calculateLineHeight() {
    SkScalar height = fEndLine.metrics().height();
    if (fFormattingContext.needLineSpacing) {
        height += fFormattingContext.lineSpacing;
    }
    if (fHardLineBreak && !isLastLine() && fParent->paragraphStyle().getParagraphSpacing() > 0) {
        height += fParent->paragraphStyle().getParagraphSpacing();
    }
    return height;
}

void TextWrapper::addFormattedLineToParagraph(const AddLineToParagraph& addLine) {
    LineTextRanges ranges = calculateLineTextRanges();
    fParent->includeFontPadding(fFirstLine, fIsLastLine || (fCurrentStartLine == fEnd && !fHardLineBreak),
        fEndLine.metrics(), ranges.text);
    SkScalar lineHeight = calculateLineHeight();
    SkScalar offsetX = fParent->detectIndents(fLineNumber - 1);

    addLine(ranges.textExcludingSpaces,
            ranges.text,
            ranges.textIncludingNewlines,
            ranges.clusters,
            ranges.clustersWithGhosts,
            fCurrentLineWidthWithSpaces,
            fEndLine.startPos(),
            fEndLine.endPos(),
            SkVector::Make(offsetX, fHeight),
            SkVector::Make(fEndLine.width(), lineHeight),
            fEndLine.metrics(),
            fNeedEllipsis,
            offsetX,
            fNoIndentWidth);
    fHeight += lineHeight;
    fFirstLine = false;

    updateIntrinsicWidths();
}

void TextWrapper::updateIntrinsicWidths() {
    fSoftLineMaxIntrinsicWidth += fCurrentLineWidthWithSpaces;
    fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, fSoftLineMaxIntrinsicWidth);
    if (fHardLineBreak) {
        fSoftLineMaxIntrinsicWidth = 0;
    }
}

bool TextWrapper::shouldBreakFormattingLoop() {
    if (fFormattingContext.hasEllipsis && fFormattingContext.unlimitedLines) {
        // There is one case when we need an ellipsis on a separate line
        // after a line break when width is infinite
        if (!fHardLineBreak) {
            return true;
        }
    } else if (fIsLastLine) {
        fHardLineBreak = false;
        return true;
    }
    return false;
}

bool TextWrapper::isLastLine() const { return fLineNumber >= fFormattingContext.maxLines; }

void TextWrapper::advanceToNextLine() {
    prepareForNextLine();
}

void TextWrapper::prepareForNextLine() {
    if (!fHardLineBreak || fCurrentStartLine != fEnd) {
        fEndLine.clean();
    }
    fEndLine.startFrom(fCurrentStartLine, fCurrentStartPos);
    fParent->fMaxWidthWithTrailingSpaces =
            std::max(fParent->fMaxWidthWithTrailingSpaces, fCurrentLineWidthWithSpaces);
}

void TextWrapper::processRemainingClusters() {
    if (fEndLine.endCluster() == nullptr) {
        return;
    }

    float lastWordLength = 0.0f;
    auto cluster = fEndLine.endCluster();

    while (cluster != fEnd || cluster->endPos() < fEnd->endPos()) {
        fExceededMaxLines = true;

        if (cluster->isHardBreak()) {
            handleHardBreak(lastWordLength);
        } else if (cluster->isWhitespaceBreak()) {
            handleWhitespaceBreak(cluster, lastWordLength);
        } else if (cluster->run().isPlaceholder()) {
            handlePlaceholder(cluster, lastWordLength);
        } else {
            handleRegularCluster(cluster, lastWordLength);
        }

        ++cluster;
    }

    fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
    fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, fSoftLineMaxIntrinsicWidth);

    if (fParent->lines().empty()) {
        adjustMetricsForEmptyParagraph();
        fHeight = std::max(fHeight, fEndLine.metrics().height());
    }
}

void TextWrapper::handleHardBreak(float& lastWordLength) {
    fMaxIntrinsicWidth = std::max(fMaxIntrinsicWidth, fSoftLineMaxIntrinsicWidth);
    fSoftLineMaxIntrinsicWidth = 0;
    fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
    lastWordLength = 0;
}

void TextWrapper::handleWhitespaceBreak(Cluster* cluster, float& lastWordLength) {
    fSoftLineMaxIntrinsicWidth += cluster->width();
    fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
    lastWordLength = 0;
}

void TextWrapper::handlePlaceholder(Cluster* cluster, float& lastWordLength) {
    fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, lastWordLength);
    fSoftLineMaxIntrinsicWidth += cluster->width();
    fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, cluster->width());
    lastWordLength = 0;
}

void TextWrapper::handleRegularCluster(Cluster* cluster, float& lastWordLength) {
    fSoftLineMaxIntrinsicWidth += cluster->width();
    lastWordLength += cluster->width();
}

void TextWrapper::adjustMetricsForEmptyParagraph() {
    if (fFormattingContext.disableFirstAscent) {
        fEndLine.metrics().fAscent = fEndLine.metrics().fRawAscent;
    }
    if (fFormattingContext.disableLastDescent && (fIsLastLine || (fEndLine.endCluster() == fEnd && !fHardLineBreak))) {
        fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
    }
}

void TextWrapper::addFinalLineBreakIfNeeded(const AddLineToParagraph& addLine) {
    if (!fHardLineBreak) {
        return;
    }

    if (fFormattingContext.disableLastDescent) {
        fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
    }

    if (fParent->strutEnabled()) {
        fParent->strutMetrics().updateLineMetrics(fEndLine.metrics());
    }

    ClusterRange clusters(fEndLine.breakCluster() - fStart, fEndLine.endCluster() - fStart);

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
            false,
            fParent->detectIndents(fLineNumber - 1),
            fNoIndentWidth);

    fHeight += fEndLine.metrics().height();
    fParent->lines().back().setMaxRunMetrics(fMaxRunMetrics);
}

void TextWrapper::adjustFirstLastLineMetrics() {
    if (fParent->lines().empty()) {
        return;
    }

    if (fFormattingContext.disableFirstAscent) {
        fParent->lines().front().setAscentStyle(LineMetricStyle::Typographic);
    }

    if (fFormattingContext.disableLastDescent) {
        fParent->lines().back().setDescentStyle(LineMetricStyle::Typographic);
    }
}

void TextWrapper::preProcessingForLineStretches() {
    if (fLineStretches.empty()) {
        return;
    }

    const ParagraphStyle& style = fParent->getParagraphStyle();
    EllipsisModal ellipsisMod = style.getEllipsisMod();
    if (style.getMaxLines() == 1 && fLineStretches.size() > 1 &&
        (ellipsisMod == EllipsisModal::HEAD || ellipsisMod == EllipsisModal::MIDDLE)) {
        TextStretch merged = fLineStretches.front();

        for (size_t i = 1; i < fLineStretches.size(); ++i) {
            merged.extend(fLineStretches[i]);
        }
        fLineStretches.clear();
        fLineStretches.push_back(merged);
    }

    if ((fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MULTILINE_HEAD ||
        fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::MULTILINE_MIDDLE) &&
        fParent->paragraphStyle().ellipsized() && fLineStretches.size() > style.getMaxLines()) {
        handleMultiLineEllipsis(style.getMaxLines());
    }
}

void TextWrapper::handleMultiLineEllipsis(size_t maxLines) {
    std::vector<TextStretch> multiEllipsisLineStretches;

    // Put content exceeding maxline in a single line for subsequent ellipsis generation in one line.
    for (size_t i = 0; i < fLineStretches.size(); ++i) {
        if (i < maxLines - 1) {
            multiEllipsisLineStretches.push_back(fLineStretches[i]);
            continue;
        } else if (i == maxLines - 1) {
            TextStretch merged = fLineStretches[i];
            mergeStretchesUntilHardBreak(merged, i);
            multiEllipsisLineStretches.push_back(merged);
            break;
        }
    }
    fLineStretches = multiEllipsisLineStretches;
}

void TextWrapper::mergeStretchesUntilHardBreak(TextStretch& merged, size_t startIndex) {
    bool lastLineStretchesBreak = fLineStretches[startIndex].endCluster() == nullptr ?
        false : fLineStretches[startIndex].endCluster()->isHardBreak();

    for (size_t j = startIndex + 1; j < fLineStretches.size(); ++j) {
        if (lastLineStretchesBreak) {
            break;
        }
        // Update previous line's hard break flag.
        lastLineStretchesBreak = fLineStretches[j].endCluster() == nullptr ?
            false : fLineStretches[j].endCluster()->isHardBreak();
        // Merge subsequent line stretches into the current line.
        merged.extend(fLineStretches[j]);
    }
}

void TextWrapper::layoutLinesBalanced(SkScalar maxWidth, const AddLineToParagraph& addLine) {
    reset();

    // Generate word stretches
    generateWordStretches(fParent->getWordBreakType());

    for (std::vector<TextStretch>& wordStretches : fWordStretchesBatch) {
        // Trimming a word that is longer than the line width
        formalizedClusters(wordStretches, maxWidth);

        std::vector<SkScalar> clustersWidthVector = generateWordsWidthInfo(wordStretches);

        // Execute the balancing algorithm to generate line grouping information
        std::vector<std::pair<size_t, size_t>> linesGroupInfo =
                generateLinesGroupInfo(clustersWidthVector, maxWidth);

        generateLineStretches(linesGroupInfo, wordStretches);
    }

    preProcessingForLineStretches();

    generateTextLines(maxWidth, addLine);
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
    auto endlessLine = !SkIsFinite(maxWidth);
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
