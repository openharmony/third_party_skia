// Copyright 2019 Google LLC.
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skparagraph/src/TextWrapper.h"

#ifdef OHOS_SUPPORT
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
} // namespace

#ifdef OHOS_SUPPORT
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
                            WordBreakType wordBreakType, bool needEllipsis) {

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
            if (cluster->isHyphenBreak() && !needEllipsis) {
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
                       (wordBreakType == WordBreakType::BREAK_HYPHEN && attemptedHyphenate && !needEllipsis))) {
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
                !needEllipsis) {
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
    while (cluster < endOfClusters && cluster->isWhitespaceBreak() && !cluster->isTabulation()) {
        width += cluster->width();
        ++cluster;
    }

    return std::make_tuple(cluster, 0, width);
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

    if (fParent->getLineBreakStrategy() == LineBreakStrategy::BALANCED &&
        fParent->getWordBreakType() != WordBreakType::BREAK_ALL &&
        (fParent->getParagraphStyle().getMaxLines() >= MAX_LINES_LIMIT ||
        fParent->getParagraphStyle().getEllipsisMod() == EllipsisModal::NONE)) {
        layoutLinesBalanced(parent, maxWidth, addLine);
        return;
    }

    layoutLinesSimple(parent, maxWidth, addLine);
}

void TextWrapper::layoutLinesSimple(ParagraphImpl* parent,
                                     SkScalar maxWidth,
                                     const AddLineToParagraph& addLine) {
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
    fEndLine = TextStretch(span.begin(), span.begin(), parent->strutForceHeight() && parent->strutEnabled());
    auto end = span.end() - 1;
    auto start = span.begin();
    InternalLineMetrics maxRunMetrics;
    bool needEllipsis = false;
    SkScalar newWidth = maxWidth;
    SkScalar noIndentWidth = maxWidth;
    while (fEndLine.endCluster() != end) {
        noIndentWidth = maxWidth - parent->detectIndents(fLineNumber - 1);
        if (maxLines == 1 &&
            (parent->paragraphStyle().getEllipsisMod() == EllipsisModal::HEAD ||
            parent->needCreateMiddleEllipsis())) {
            newWidth = FLT_MAX;
        } else {
            newWidth = maxWidth - parent->detectIndents(fLineNumber - 1);
        }
        auto lastLine = (hasEllipsis && unlimitedLines) || fLineNumber >= maxLines;
        needEllipsis = hasEllipsis && !endlessLine && lastLine;

        this->lookAhead(newWidth, end, parent->getApplyRoundingHack(), parent->getWordBreakType(), needEllipsis);

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
        } else if (fBrokeLineWithHyphen) {
            fEndLine.shiftWidth(fEndLine.endCluster()->width());
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
        if (disableLastDescent && (lastLine || (startLine == end && !fHardLineBreak))) {
            fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
        }

        if (parent->strutEnabled()) {
            // Make sure font metrics are not less than the strut
            parent->strutMetrics().updateLineMetrics(fEndLine.metrics());
        }

        SkScalar lineHeight = fEndLine.metrics().height();
        if (fHardLineBreak && !lastLine && parent->paragraphStyle().getParagraphSpacing() > 0) {
            lineHeight += parent->paragraphStyle().getParagraphSpacing();
        }
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
    if (parent->paragraphStyle().getIsEndAddParagraphSpacing() &&
        parent->paragraphStyle().getParagraphSpacing() > 0) {
        fHeight += parent->paragraphStyle().getParagraphSpacing();
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

std::vector<uint8_t> TextWrapper::findBreakPositions(Cluster* startCluster,
                                                     Cluster* endOfClusters,
                                                     SkScalar widthBeforeCluster,
                                                     SkScalar maxWidth) {
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
        return std::vector<uint8_t>{};
    }

    auto locale = owner->paragraphStyle().getTextStyle().getLocale();
    return Hyphenator::getInstance().findBreakPositions(locale, owner->fText, startPos, endPos);
}

void TextWrapper::pushToWordVector() {
    fWordStretches.push_back(fClusters);
    fClusters.clean();
}

void TextWrapper::generateLineStretches(const std::vector<std::pair<size_t, size_t>>& linesGroupInfo) {
    for (const std::pair<size_t, size_t>& pair : linesGroupInfo) {
        TextStretch endLine{};
        for (size_t i = pair.first; i <= pair.second; ++i) {
            if (i == pair.first) {
                endLine.setStartCluster(fWordStretches[i].startCluster());
            }
            endLine.extend(fWordStretches[i]);
        }
        fLineStretches.push_back(endLine);
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
        pushToWordVector();
    } else {
        fClusters.extend(cluster);
        if (fClusters.endOfWord()) { // Keep adding clusters/words
            if (textTabAlign.processEndofWord(fWords, fClusters, cluster, totalFakeSpacing)) {
                if (wordBreakType == WordBreakType::BREAK_ALL) {
                    fClusters.trim(cluster);
                }
                return;
            }

            fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
            pushToWordVector();
        } else {
            if (textTabAlign.processCluster(fWords, fClusters, cluster, totalFakeSpacing)) {
                fClusters.trim(cluster);
                return;
            }
        }
    }
}

void TextWrapper::generateWordStretches(const SkSpan<Cluster>& span, WordBreakType wordBreakType) {
    fEndLine.metrics().clean();

    Cluster* start = span.begin();
    Cluster* end = span.end() - 1;

    fClusters.startFrom(start, start->startPos());
    fClip.startFrom(start, start->startPos());

    bool isFirstWord = true;
    TextTabAlign textTabAlign(start->getOwner()->paragraphStyle().getTextTab());
    textTabAlign.init(MAX_LINES_LIMIT, start);

    SkScalar totalFakeSpacing = 0.0;

    for (Cluster* cluster = start; cluster < end; ++cluster) {
        totalFakeSpacing += (cluster->needAutoSpacing() && cluster != start)
                                    ? (cluster - 1)->getFontSize() / AUTO_SPACING_WIDTH_RATIO
                                    : 0;
        if (cluster->isHardBreak()) {
            if (cluster != start) {
                isFirstWord = false;
            }
        }

        if (cluster->isSoftBreak() || cluster->isWhitespaceBreak()) {
            isFirstWord = false;
        }

        if (cluster->run().isPlaceholder()) {
            if (!fClusters.empty()) {
                // Placeholder ends the previous word (placeholders are ignored in trimming)
                fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, getClustersTrimmedWidth());
                pushToWordVector();
            }

            // Placeholder is separate word and its width now is counted in minIntrinsicWidth
            fMinIntrinsicWidth = std::max(fMinIntrinsicWidth, cluster->width());
            fClusters.extend(cluster);
            pushToWordVector();
        } else {
            extendCommonCluster(cluster, textTabAlign, totalFakeSpacing, wordBreakType);
        }

        if ((fHardLineBreak = cluster->isHardBreak())) {
            fClusters.extend(cluster);
            pushToWordVector();
        }
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
    int clustersCnt = clustersWidth.size();
    for (int clustersIndex = clustersCnt - STRATEGY_START_POS; clustersIndex >= 0;
         clustersIndex--) {
        bestPick[clustersIndex].first = clustersIndex;
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
        size_t minCostIdx{0};
        minCostIdx = minCostIndices[static_cast<int32_t>(minCostIndices.size() / MIN_COST_POS)];
        costTable[clustersIndex] = minCost;
        bestPick[clustersIndex].second = clustersIndex + minCostIdx;
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
    int clustersCnt = clustersWidth.size();
    std::vector<SkScalar> costTable(clustersCnt, 0);
    std::vector<std::pair<size_t, size_t>> bestPick(clustersCnt, {0, 0});

    costTable[clustersCnt - 1] =
            std::pow(std::abs(clustersWidth[clustersCnt - 1] - maxWidth), STRATEGY_START_POS);
    bestPick[clustersCnt - 1] = {clustersCnt - 1, clustersCnt - 1};

    calculateCostTable(clustersWidth, maxWidth, costTable, bestPick);

    return buildWordBalance(bestPick, clustersCnt);
}

std::vector<SkScalar> TextWrapper::generateWordsWidthInfo() {
    std::vector<SkScalar> result;
    result.reserve(fWordStretches.size());
    std::transform(fWordStretches.begin(),
                   fWordStretches.end(),
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

void TextWrapper::generateTextLines(SkScalar maxWidth,
                                    const AddLineToParagraph& addLine,
                                    const SkSpan<Cluster>& span) {
    initializeFormattingState(maxWidth, span);
    processLineStretches(maxWidth, addLine);
    finalizeTextLayout(addLine);
}

void TextWrapper::initializeFormattingState(SkScalar maxWidth, const SkSpan<Cluster>& span) {
    const auto& style = fParent->paragraphStyle();
    fFormattingContext = {style.getMaxLines() == std::numeric_limits<size_t>::max(),
                          !SkScalarIsFinite(maxWidth),
                          style.ellipsized(),
                          style.getTextHeightBehavior() & TextHeightBehavior::kDisableFirstAscent,
                          style.getTextHeightBehavior() & TextHeightBehavior::kDisableLastDescent,
                          style.getMaxLines(),
                          style.effective_align()};

    fFirstLine = true;
    fSoftLineMaxIntrinsicWidth = 0;
    fNoIndentWidth = maxWidth;
    fEnd = span.end() - 1;
    fStart = span.begin();
}

void TextWrapper::processLineStretches(SkScalar maxWidth, const AddLineToParagraph& addLine) {
    for (TextStretch& line : fLineStretches) {
        prepareLineForFormatting(line, maxWidth);
        formatCurrentLine(addLine);

        if (shouldBreakFormattingLoop()) {
            break;
        }
        advanceToNextLine();
    }
}

void TextWrapper::finalizeTextLayout(const AddLineToParagraph& addLine) {
    processRemainingClusters();
    addFinalLineBreakIfNeeded(addLine);
    adjustFirstLastLineMetrics();

    if (fParent->paragraphStyle().getIsEndAddParagraphSpacing() &&
        fParent->paragraphStyle().getParagraphSpacing() > 0) {
        fHeight += fParent->paragraphStyle().getParagraphSpacing();
    }
}

void TextWrapper::prepareLineForFormatting(TextStretch& line, SkScalar maxWidth) {
    fEndLine = std::move(line);
    fNoIndentWidth = maxWidth - fParent->detectIndents(fLineNumber - 1);
}

void TextWrapper::formatCurrentLine(const AddLineToParagraph& addLine) {
    bool needEllipsis = determineIfEllipsisNeeded();
    trimLineSpaces();
    handleSpecialCases(needEllipsis);
    updateLineMetrics();
    addFormattedLineToParagraph(addLine, needEllipsis);
}

bool TextWrapper::determineIfEllipsisNeeded() {
    bool lastLine = (fFormattingContext.hasEllipsis && fFormattingContext.unlimitedLines) ||
                    fLineNumber >= fFormattingContext.maxLines;
    bool needEllipsis =
            fFormattingContext.hasEllipsis && !fFormattingContext.endlessLine && lastLine;

    if (fLineStretches.size() > fFormattingContext.maxLines) {
        needEllipsis = true;
    }

    if (fParent->paragraphStyle().getEllipsisMod() == EllipsisModal::HEAD &&
        fFormattingContext.hasEllipsis) {
        needEllipsis = fFormattingContext.maxLines <= 1;
        if (needEllipsis) {
            fHardLineBreak = false;
        }
    }

    return needEllipsis;
}

void TextWrapper::trimLineSpaces() {
    this->trimEndSpaces(fFormattingContext.align);
    std::tie(fCurrentStartLine, fCurrentStartPos, fCurrentLineWidthWithSpaces) =
            this->trimStartSpaces(fEnd);
}

void TextWrapper::handleSpecialCases(bool needEllipsis) {
    if (needEllipsis && !fHardLineBreak) {
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
    updateMetricsWithPlaceholder(runs, false);
    fMaxRunMetrics = fEndLine.metrics();
    fMaxRunMetrics.fForceStrut = false;
}

void TextWrapper::adjustLineMetricsForFirstLastLine() {
    if (fFormattingContext.disableFirstAscent && fFirstLine) {
        fEndLine.metrics().fAscent = fEndLine.metrics().fRawAscent;
    }
    if (fFormattingContext.disableLastDescent && isLastLine()) {
        fEndLine.metrics().fDescent = fEndLine.metrics().fRawDescent;
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

    ranges.clusters =
            ClusterRange(fEndLine.startCluster() - fStart, fEndLine.endCluster() - fStart + 1);

    ranges.clustersWithGhosts =
            ClusterRange(fEndLine.startCluster() - fStart, fCurrentStartLine - fStart);

    if (fEndLine.empty()) {
        ranges.textExcludingSpaces.end = ranges.textExcludingSpaces.start;
        ranges.clusters.end = ranges.clusters.start;
    }

    ranges.text.end = std::max(ranges.text.end, ranges.textExcludingSpaces.end);

    return ranges;
}

SkScalar TextWrapper::calculateLineHeight() {
    SkScalar height = fEndLine.metrics().height();
    if (fHardLineBreak && !isLastLine() && fParent->paragraphStyle().getParagraphSpacing() > 0) {
        height += fParent->paragraphStyle().getParagraphSpacing();
    }
    return height;
}

void TextWrapper::addFormattedLineToParagraph(const AddLineToParagraph& addLine,
                                              bool needEllipsis) {
    LineTextRanges ranges = calculateLineTextRanges();
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
            needEllipsis,
            offsetX,
            fNoIndentWidth);

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
        if (!fHardLineBreak) {
            return true;
        }
    } else if (isLastLine()) {
        fHardLineBreak = false;
        return true;
    }
    return false;
}

bool TextWrapper::isLastLine() const { return fLineNumber >= fFormattingContext.maxLines; }

void TextWrapper::advanceToNextLine() {
    fHeight += calculateLineHeight();
    prepareForNextLine();
    ++fLineNumber;
    fFirstLine = false;
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
    if (fFormattingContext.disableLastDescent && !fHardLineBreak) {
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

void TextWrapper::layoutLinesBalanced(ParagraphImpl* parent,
                                      SkScalar maxWidth,
                                      const AddLineToParagraph& addLine) {
    reset();
    SkSpan<Cluster> span = parent->clusters();
    if (span.empty()) {
        return;
    }

    // Generate word stretches
    generateWordStretches(span, parent->getWordBreakType());

    // Trimming a word that is longer than the line width
    formalizedClusters(fWordStretches, maxWidth);

    std::vector<SkScalar> clustersWidthVector = generateWordsWidthInfo();

    // Execute the balancing algorithm to generate line grouping information
    std::vector<std::pair<size_t, size_t>> linesGroupInfo =
            generateLinesGroupInfo(clustersWidthVector, maxWidth);

    generateLineStretches(linesGroupInfo);

    generateTextLines(maxWidth, addLine, span);
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
                        fClusters = TextStretch(fClusters.startCluster(), nextNonBreakingSpace,
                            fClusters.metrics().getForceStrut());
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
