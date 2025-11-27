// Copyright 2019 Google LLC.
#include "include/core/SkCanvas.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkSpan.h"
#include "include/core/SkTypeface.h"
#include "include/private/base/SkTFitsIn.h"
#include "include/private/base/SkTo.h"
#include "modules/skparagraph/include/Metrics.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphPainter.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/src/OneLineShaper.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skparagraph/src/ParagraphPainterImpl.h"
#include "modules/skparagraph/src/Run.h"
#include "modules/skparagraph/src/TextLine.h"
#include "modules/skparagraph/src/TextWrapper.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "src/base/SkUTF.h"
#include "src/core/SkTextBlobPriv.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <utility>

#ifdef ENABLE_TEXT_ENHANCE
#include <sstream>
#include "trace.h"
#include "log.h"
#include "include/TextGlobalConfig.h"
#include "modules/skparagraph/src/TextLineBaseImpl.h"
#include "TextParameter.h"
#endif

using namespace skia_private;

namespace skia {
namespace textlayout {

namespace {
#ifdef ENABLE_TEXT_ENHANCE
constexpr ParagraphPainter::PaintID INVALID_PAINT_ID = -1;
constexpr int FEATURE_NAME_INDEX_TWO = 2, FEATURE_NAME_INDEX_THREE = 3;
#endif

SkScalar littleRound(SkScalar a) {
    // This rounding is done to match Flutter tests. Must be removed..
    auto val = std::fabs(a);
    if (val < 10000) {
        return SkScalarRoundToScalar(a * 100.0)/100.0;
    } else if (val < 100000) {
        return SkScalarRoundToScalar(a * 10.0)/10.0;
    } else {
        return SkScalarFloorToScalar(a);
    }
}
}  // namespace

TextRange operator*(const TextRange& a, const TextRange& b) {
    if (a.start == b.start && a.end == b.end) return a;
    auto begin = std::max(a.start, b.start);
    auto end = std::min(a.end, b.end);
    return end > begin ? TextRange(begin, end) : EMPTY_TEXT;
}

#ifdef ENABLE_TEXT_ENHANCE
TextRange textRangeMergeBtoA(const TextRange& a, const TextRange& b) {
    if (a.width() <= 0 || b.width() <= 0 || a.end < b.start || a.start > b.end) {
        return a;
    }

    return TextRange(std::min(a.start, b.start), std::max(a.end, b.end));
}

std::vector<SkUnichar> ParagraphImpl::convertUtf8ToUnicode(const SkString& utf8)
{
    fUnicodeIndexForUTF8Index.clear();
    std::vector<SkUnichar> result;
    auto p = utf8.c_str();
    auto end = p + utf8.size();
    while (p < end) {
        auto tmp = p;
        auto unichar = SkUTF::NextUTF8(&p, end);
        for (auto i = 0; i < p - tmp; ++i) {
            fUnicodeIndexForUTF8Index.emplace_back(result.size());
        }
        result.emplace_back(unichar);
    }
    fUnicodeIndexForUTF8Index.emplace_back(result.size());
    return result;
}

bool ParagraphImpl::needCreateMiddleEllipsis()
{
    if (fParagraphStyle.getMaxLines() == 1 && fParagraphStyle.getEllipsisMod() == EllipsisModal::MIDDLE &&
        fParagraphStyle.ellipsized()) {
        return true;
    }
    return false;
}

Placeholder* ParagraphImpl::getPlaceholderByIndex(size_t placeholderIndex)
{
    if (fPlaceholders.size() <= placeholderIndex) {
        LOGE("Failed to get placeholder");
        return nullptr;
    }
    return &fPlaceholders[placeholderIndex];
}

bool ParagraphImpl::IsPlaceholderAlignedFollowParagraph(size_t placeholderIndex)
{
    Placeholder* placeholder = getPlaceholderByIndex(placeholderIndex);
    if (placeholder == nullptr) {
        return false;
    }
    return placeholder->fStyle.fAlignment == PlaceholderAlignment::kFollow;
}

bool ParagraphImpl::setPlaceholderAlignment(size_t placeholderIndex, PlaceholderAlignment alignment)
{
    Placeholder* placeholder = getPlaceholderByIndex(placeholderIndex);
    if (placeholder == nullptr) {
        return false;
    }
    placeholder->fStyle.fAlignment = alignment;
    return true;
}


Block& ParagraphImpl::getBlockByRun(const Run& run)
{
    TextRange textRange = run.textRange();
    BlockRange blocksRange = findAllBlocks(textRange);
    return block(blocksRange.start);
}
#endif

Paragraph::Paragraph(ParagraphStyle style, sk_sp<FontCollection> fonts)
            : fFontCollection(std::move(fonts))
            , fParagraphStyle(std::move(style))
            , fAlphabeticBaseline(0)
            , fIdeographicBaseline(0)
            , fHeight(0)
            , fWidth(0)
            , fMaxIntrinsicWidth(0)
            , fMinIntrinsicWidth(0)
            , fLongestLine(0)
#ifdef ENABLE_TEXT_ENHANCE
            , fLongestLineWithIndent(0)
#endif
            , fExceededMaxLines(0)
{
    SkASSERT(fFontCollection);
}

ParagraphImpl::ParagraphImpl(const SkString& text,
                             ParagraphStyle style,
                             TArray<Block, true> blocks,
                             TArray<Placeholder, true> placeholders,
                             sk_sp<FontCollection> fonts,
                             sk_sp<SkUnicode> unicode)
        : Paragraph(std::move(style), std::move(fonts))
        , fTextStyles(std::move(blocks))
        , fPlaceholders(std::move(placeholders))
        , fText(text)
        , fState(kUnknown)
        , fUnresolvedGlyphs(0)
        , fPicture(nullptr)
        , fStrutMetrics(false)
        , fOldWidth(0)
        , fOldHeight(0)
        , fUnicode(std::move(unicode))
        , fHasLineBreaks(false)
        , fHasWhitespacesInside(false)
        , fTrailingSpaces(0)
{
    SkASSERT(fUnicode);
}

ParagraphImpl::ParagraphImpl(const std::u16string& utf16text,
                             ParagraphStyle style,
                             TArray<Block, true> blocks,
                             TArray<Placeholder, true> placeholders,
                             sk_sp<FontCollection> fonts,
                             sk_sp<SkUnicode> unicode)
        : ParagraphImpl(SkString(),
                        std::move(style),
                        std::move(blocks),
                        std::move(placeholders),
                        std::move(fonts),
                        std::move(unicode))
{
    SkASSERT(fUnicode);
    fText =  SkUnicode::convertUtf16ToUtf8(utf16text);
}

ParagraphImpl::~ParagraphImpl() = default;

int32_t ParagraphImpl::unresolvedGlyphs() {
    if (fState < kShaped) {
        return -1;
    }

    return fUnresolvedGlyphs;
}

#ifdef ENABLE_TEXT_ENHANCE
bool ParagraphImpl::GetLineFontMetrics(const size_t lineNumber, size_t& charNumber,
    std::vector<RSFontMetrics>& fontMetrics) {
    if (lineNumber > fLines.size() || !lineNumber ||
        !fLines[lineNumber - 1].getLineAllRuns().size()) {
        return false;
    }

    size_t textRange = 0;
    size_t lineCharCount = fLines[lineNumber - 1].clusters().end -
        fLines[lineNumber - 1].clusters().start;

    for (auto& runIndex : fLines[lineNumber - 1].getLineAllRuns()) {
        Run& targetRun = this->run(runIndex);
        size_t runClock = 0;
        size_t currentRunCharNumber = targetRun.clusterRange().end -
            targetRun.clusterRange().start;
        for (;textRange < lineCharCount; textRange++) {
            if (++runClock > currentRunCharNumber) {
                break;
            }
            RSFontMetrics newFontMetrics;
            targetRun.fFont.GetMetrics(&newFontMetrics);
            auto decompressFont = targetRun.fFont;
            scaleFontWithCompressionConfig(decompressFont, ScaleOP::DECOMPRESS);
            metricsIncludeFontPadding(&newFontMetrics, decompressFont);
            fontMetrics.emplace_back(newFontMetrics);
        }
    }

    charNumber = lineCharCount;
    return true;
}
#endif

std::unordered_set<SkUnichar> ParagraphImpl::unresolvedCodepoints() {
    return fUnresolvedCodepoints;
}

void ParagraphImpl::addUnresolvedCodepoints(TextRange textRange) {
    fUnicode->forEachCodepoint(
        &fText[textRange.start], textRange.width(),
        [&](SkUnichar unichar, int32_t start, int32_t end, int32_t count) {
            fUnresolvedCodepoints.emplace(unichar);
        }
    );
}

#ifdef ENABLE_TEXT_ENHANCE
TextRange ParagraphImpl::resetRangeWithDeletedRange(const TextRange& sourceRange,
    const TextRange& deletedRange, const size_t& ellSize)
{
    if (sourceRange.end <= deletedRange.start) {
        return sourceRange;
    }
    auto changeSize = ellSize - deletedRange.width();

    if (sourceRange.start >= deletedRange.end) {
        return TextRange(sourceRange.start + changeSize, sourceRange.end + changeSize);
    }

    TextRange target;
    target.start = sourceRange.start <= deletedRange.start ? sourceRange.start : deletedRange.start + ellSize;
    target.end = sourceRange.end <= deletedRange.end ? deletedRange.start + ellSize : sourceRange.end + changeSize;
    return target.start <= target.end ? target : EMPTY_RANGE;
}

void ParagraphImpl::resetTextStyleRange(const TextRange& deletedRange)
{
    auto tmpTextStyle = fTextStyles;
    fTextStyles.clear();
    for (auto fs : tmpTextStyle) {
        auto newTextRange = resetRangeWithDeletedRange(fs.fRange, deletedRange, this->getEllipsis().size());
        LOGD("ParagraphImpl::resetTextStyleRange old = [%{public}lu,%{public}lu), new = [%{public}lu,%{public}lu)",
            static_cast<unsigned long>(fs.fRange.start), static_cast<unsigned long>(fs.fRange.end),
            static_cast<unsigned long>(newTextRange.start), static_cast<unsigned long>(newTextRange.end));
        if (newTextRange.width() == 0) {
            continue;
        }
        fs.fRange = newTextRange;
        fTextStyles.emplace_back(fs);
    }
}

void ParagraphImpl::resetPlaceholderRange(const TextRange& deletedRange)
{
    // reset fRange && fTextBefore && fBlockBefore
    auto ellSize = this->getEllipsis().size();
    auto tmpPlaceholders = fPlaceholders;
    fPlaceholders.clear();
    for (auto ph : tmpPlaceholders) {
        auto newTextRange = resetRangeWithDeletedRange(ph.fRange, deletedRange, ellSize);
        LOGD("ParagraphImpl::resetPlaceholderRange old = [%{public}lu,%{public}lu), new = [%{public}lu,%{public}lu)",
            static_cast<unsigned long>(ph.fRange.start), static_cast<unsigned long>(ph.fRange.end),
            static_cast<unsigned long>(newTextRange.start), static_cast<unsigned long>(newTextRange.end));
        if (newTextRange.empty()) {
            continue;
        }
        ph.fRange = newTextRange;
        newTextRange = ph.fTextBefore;
        newTextRange.start = fPlaceholders.empty() ? 0 : fPlaceholders.back().fRange.end;
        if (newTextRange.end > deletedRange.start) {
            newTextRange.end = newTextRange.end <= deletedRange.end ?
                deletedRange.start + ellSize : newTextRange.end + ellSize - deletedRange.width();
        }
        ph.fTextBefore = newTextRange;
        fPlaceholders.emplace_back(ph);
    }
}
#endif

void ParagraphImpl::layout(SkScalar rawWidth) {
#ifdef ENABLE_TEXT_ENHANCE
    TEXT_TRACE_FUNC();
    fLineNumber = 1;
    fLayoutRawWidth = rawWidth;
#endif
    // TODO: This rounding is done to match Flutter tests. Must be removed...
    auto floorWidth = rawWidth;

    if (getApplyRoundingHack()) {
        floorWidth = SkScalarFloorToScalar(floorWidth);
    }

#ifdef ENABLE_TEXT_ENHANCE
    fPaintRegion.reset();
    bool isMaxLinesZero = false;
    if (fParagraphStyle.getMaxLines() == 0) {
        if (fText.size() != 0) {
            isMaxLinesZero = true;
        }
    }
#endif

    if ((!SkIsFinite(rawWidth) || fLongestLine <= floorWidth) &&
        fState >= kLineBroken &&
         fLines.size() == 1 && fLines.front().ellipsis() == nullptr) {
        // Most common case: one line of text (and one line is never justified, so no cluster shifts)
        // We cannot mark it as kLineBroken because the new width can be bigger than the old width
        fWidth = floorWidth;
        fState = kShaped;
    } else if (fState >= kLineBroken && fOldWidth != floorWidth) {
        // We can use the results from SkShaper but have to do EVERYTHING ELSE again
        fState = kShaped;
    } else {
        // Nothing changed case: we can reuse the data from the last layout
    }

#ifdef ENABLE_TEXT_ENHANCE
    this->fUnicodeText = convertUtf8ToUnicode(fText);
    auto paragraphCache = fFontCollection->getParagraphCache();
#endif

    if (fState < kShaped) {
        // Check if we have the text in the cache and don't need to shape it again
#ifdef ENABLE_TEXT_ENHANCE
        if (!paragraphCache->findParagraph(this)) {
#else
        if (!fFontCollection->getParagraphCache()->findParagraph(this)) {
#endif
            if (fState < kIndexed) {
                // This only happens once at the first layout; the text is immutable
                // and there is no reason to repeat it
                if (this->computeCodeUnitProperties()) {
                    fState = kIndexed;
                }
            }
            this->fRuns.clear();
            this->fClusters.clear();
            this->fClustersIndexFromCodeUnit.clear();
            this->fClustersIndexFromCodeUnit.push_back_n(fText.size() + 1, EMPTY_INDEX);
            if (!this->shapeTextIntoEndlessLine()) {
                this->resetContext();

#ifdef ENABLE_TEXT_ENHANCE
                if (isMaxLinesZero) {
                    fExceededMaxLines  = true;
                }
#endif
                // TODO: merge the two next calls - they always come together
                this->resolveStrut();
                this->computeEmptyMetrics();
                this->fLines.clear();

                // Set the important values that are not zero
                fWidth = floorWidth;
                fHeight = fEmptyMetrics.height();
                if (fParagraphStyle.getStrutStyle().getStrutEnabled() &&
                    fParagraphStyle.getStrutStyle().getForceStrutHeight()) {
                    fHeight = fStrutMetrics.height();
                }
#ifdef ENABLE_TEXT_ENHANCE
                if (fParagraphStyle.getMaxLines() == 0) {
                    fHeight = 0;
                }
#endif
                fAlphabeticBaseline = fEmptyMetrics.alphabeticBaseline();
                fIdeographicBaseline = fEmptyMetrics.ideographicBaseline();
                fLongestLine = FLT_MIN - FLT_MAX;  // That is what flutter has
                fMinIntrinsicWidth = 0;
                fMaxIntrinsicWidth = 0;
                this->fOldWidth = floorWidth;
                this->fOldHeight = this->fHeight;

                return;
#ifdef ENABLE_TEXT_ENHANCE
            } else {
                // Add the paragraph to the cache
                paragraphCache->updateParagraph(this);
            }
#else
            } else {
                // Add the paragraph to the cache
                fFontCollection->getParagraphCache()->updateParagraph(this);
            }
#endif
        }
        fState = kShaped;
    }

    if (fState == kShaped) {
        this->resetContext();
        this->resolveStrut();
        this->computeEmptyMetrics();
        this->fLines.clear();
#ifdef ENABLE_TEXT_ENHANCE
        // fast path
        if (!fHasLineBreaks && !fHasWhitespacesInside && fPlaceholders.size() == 1 &&
            (fRuns.size() == 1 && preCalculateSingleRunAutoSpaceWidth(floorWidth)) &&
            !needBreakShapedTextIntoLines()) {
            positionShapedTextIntoLine(floorWidth);
        } else if (!paragraphCache->GetStoredLayout(*this)) {
            breakShapedTextIntoLines(floorWidth);
            // text breaking did not go to fast path and we did not have cached layout
            paragraphCache->SetStoredLayout(*this);
        }
#else
        this->breakShapedTextIntoLines(floorWidth);
#endif
        fState = kLineBroken;
    }

    if (fState == kLineBroken) {
#ifdef ENABLE_TEXT_ENHANCE
        if (paragraphStyle().getVerticalAlignment() != TextVerticalAlign::BASELINE &&
            paragraphStyle().getMaxLines() > 1) {
            // Collect split info for crossing line's run
            std::deque<SplitPoint> splitPoints;
            generateSplitPointsByLines(splitPoints);
            splitRuns(splitPoints);
        }
#endif

        // Build the picture lazily not until we actually have to paint (or never)
        this->resetShifts();
        this->formatLines(fWidth);
        fState = kFormatted;
    }

#ifdef ENABLE_TEXT_ENHANCE
    if (fParagraphStyle.getMaxLines() == 0) {
        fHeight = 0;
        fLines.clear();
    }
#endif

    this->fOldWidth = floorWidth;
    this->fOldHeight = this->fHeight;

    if (getApplyRoundingHack()) {
        // TODO: This rounding is done to match Flutter tests. Must be removed...
        fMinIntrinsicWidth = littleRound(fMinIntrinsicWidth);
        fMaxIntrinsicWidth = littleRound(fMaxIntrinsicWidth);
    }

    // TODO: This is strictly Flutter thing. Must be factored out into some flutter code
    if (fParagraphStyle.getMaxLines() == 1 ||
        (fParagraphStyle.unlimited_lines() && fParagraphStyle.ellipsized())) {
        fMinIntrinsicWidth = fMaxIntrinsicWidth;
    }

    // TODO: Since min and max are calculated differently it's possible to get a rounding error
    //  that would make min > max. Sort it out later, make it the same for now
    if (fMaxIntrinsicWidth < fMinIntrinsicWidth) {
        fMaxIntrinsicWidth = fMinIntrinsicWidth;
    }
#ifdef ENABLE_TEXT_ENHANCE
    if (fParagraphStyle.getMaxLines() == 0) {
        fLineNumber = 0;
    } else {
        fLineNumber = std::max(size_t(1), static_cast<size_t>(fLines.size()));
    }
#endif
    //SkDebugf("layout('%s', %f): %f %f\n", fText.c_str(), rawWidth, fMinIntrinsicWidth, fMaxIntrinsicWidth);
}

#ifdef ENABLE_TEXT_ENHANCE
void ParagraphImpl::updateSplitRunClusterInfo(const Run& run, bool isSplitRun) {
    size_t posOffset = run.leftToRight() ? cluster(run.clusterRange().start).startPos() :
        cluster(run.clusterRange().end - 1).startPos();
    for (size_t clusterIndex = run.clusterRange().start; clusterIndex < run.clusterRange().end; ++clusterIndex) {
        Cluster& updateCluster = this->cluster(clusterIndex);
        updateCluster.fRunIndex = run.index();
        // If the run has not been split, it only needs to update the run index
        if (!isSplitRun) {
            continue;
        }
        size_t width = updateCluster.size();
        updateCluster.fStart -= posOffset;
        updateCluster.fEnd = updateCluster.fStart + width;
    }
}

void ParagraphImpl::refreshLines() {
    for (TextLine& line : fLines) {
        line.refresh();
    }
}

bool ParagraphImpl::isTailOfLineNeedSplit(const Run& lineLastRun, size_t lineEnd, bool hasGenerated) {
    return !hasGenerated && ((lineLastRun.clusterRange().end != lineEnd) ||
        // Special case: the line of last run combines a hard break, such as "<\n"
        (cluster(lineEnd - 1).isHardBreak() &&
        !cluster(runByCluster(lineEnd - 1).clusterRange().start).isHardBreak()));
}

void ParagraphImpl::generateSplitPoint(
    std::deque<SplitPoint>& splitPoints, const Run& run, ClusterRange lineRange, size_t lineIndex) {
    size_t startIndex =
        std::max(cluster(lineRange.start).textRange().start, cluster(run.clusterRange().start).textRange().start);
    size_t endIndex =
        std::min(cluster(lineRange.end - 1).textRange().end, cluster(run.clusterRange().end- 1).textRange().end);
    // The run cross line
    splitPoints.push_back({lineIndex, run.index(), startIndex, endIndex});
}

void ParagraphImpl::generateSplitPointsByLines(std::deque<SplitPoint>& splitPoints) {
    for (size_t lineIndex = 0; lineIndex < (size_t)fLines.size(); ++lineIndex) {
        const TextLine& line = fLines[lineIndex];
        ClusterRange lineClusterRange = line.clustersWithSpaces();
        // Avoid abnormal split of the last line
        if (lineIndex == (size_t)fLines.size() - 1) {
            size_t lineEnd = line.clusters().end == 0 ? 0 : line.clusters().end - 1;
            lineClusterRange.end = cluster(lineEnd).run().clusterRange().end;
        }
        // Skip blank line
        if (lineClusterRange.empty()) {
            continue;
        }
        size_t lineStart = lineClusterRange.start;
        size_t lineEnd = lineClusterRange.end;
        // The next line's starting cluster index
        const Run& lineFirstRun = runByCluster(lineStart);
        const Run& lineLastRun = runByCluster(lineEnd - 1);
        // Each line may have 0, 1, or 2 Runs need to be split
        bool onlyGenerateOnce{false};
        if (lineFirstRun.clusterRange().start != lineStart) {
            generateSplitPoint(splitPoints, lineFirstRun, lineClusterRange, lineIndex);

            if (lineFirstRun.index() == lineLastRun.index()) {
                onlyGenerateOnce = true;
            }
        }

        if (isTailOfLineNeedSplit(lineLastRun, lineEnd, onlyGenerateOnce)) {
            generateSplitPoint(splitPoints, lineLastRun, lineClusterRange, lineIndex);
        }
    }
}

std::optional<SplitPoint> ParagraphImpl::generateSplitPoint(const ClusterRange& clusterRange) {
    if (clusterRange.empty()) {
        return std::nullopt;
    }

    const Cluster& startCluster = cluster(clusterRange.start);
    const Cluster& endCluster = cluster(clusterRange.end - 1);
    if (startCluster.runIndex() != endCluster.runIndex()) {
        return std::nullopt;
    }

    SplitPoint splitPoint;
    splitPoint.runIndex = startCluster.run().index();
    splitPoint.headClusterIndex = startCluster.textRange().start;
    splitPoint.tailClusterIndex = endCluster.textRange().end;
    auto it = std::find_if(fLines.begin(), fLines.end(), [&](const TextLine& line) {
        return line.clustersWithSpaces().contains(clusterRange);
    });
    splitPoint.lineIndex = (it != fLines.end()) ? std::distance(fLines.begin(), it) : -1;
    return splitPoint;
}

void ParagraphImpl::generateRunsBySplitPoints(std::deque<SplitPoint>& splitPoints,
    skia_private::TArray<Run, false>& runs) {
    size_t newRunGlobalIndex = 0;
    for (size_t runIndex = 0; runIndex < (size_t)fRuns.size(); ++runIndex) {
        Run& run = fRuns[runIndex];
        if (splitPoints.empty() || splitPoints.front().runIndex != run.fIndex) {
            // No need to split
            run.fIndex = newRunGlobalIndex++;
            updateSplitRunClusterInfo(run, false);
            runs.push_back(run);
            continue;
        }

        while (!splitPoints.empty()) {
            SplitPoint& splitPoint = splitPoints.front();
            size_t splitRunIndex = splitPoint.runIndex;
            if (splitRunIndex != runIndex) {
                break;
            }

            Run splitRun{run, newRunGlobalIndex++};
            run.generateSplitRun(splitRun, splitPoint);
            updateSplitRunClusterInfo(splitRun, true);
            runs.push_back(std::move(splitRun));
            splitPoints.pop_front();
        }
    }
}

void ParagraphImpl::splitRuns(std::deque<SplitPoint>& splitPoints) {
    if (splitPoints.empty()) {
        return;
    }
    skia_private::TArray<Run, false> newRuns;
    generateRunsBySplitPoints(splitPoints, newRuns);
    if (newRuns.empty()) {
        return;
    }
    fRuns = std::move(newRuns);
    refreshLines();
}

void ParagraphImpl::splitRunsWhenCompressPunction(ClusterIndex clusterIndex) {
    // Splits the head cluster of each line into a separate run.
    std::deque<SplitPoint> splitPoints;
    if (clusterIndex > 0) {
        ClusterRange lastClusterRunClusterRange = cluster(clusterIndex - 1).run().clusterRange();
        ClusterRange beforePuncSplitClusterRange = ClusterRange(lastClusterRunClusterRange.start, clusterIndex);
        std::optional<SplitPoint> beforePuncSplitPoint = generateSplitPoint(beforePuncSplitClusterRange);
        splitPoints.push_back(*beforePuncSplitPoint);
    }
    ClusterRange puncSplitClusterRange = ClusterRange(clusterIndex, clusterIndex + 1);
    std::optional<SplitPoint> puncSplitPoint = generateSplitPoint(puncSplitClusterRange);
    splitPoints.push_back(*puncSplitPoint);
    // The clusters size includes one extra element at the paragraph end.
    if (clusterIndex + 1 < clusters().size() - 1) {
        ClusterRange nextClusterRunClusterRange = cluster(clusterIndex + 1).run().clusterRange();
        ClusterRange afterPuncSplitClusterRange = ClusterRange(clusterIndex + 1, nextClusterRunClusterRange.end);
        std::optional<SplitPoint> afterPuncSplitPoint = generateSplitPoint(afterPuncSplitClusterRange);
        splitPoints.push_back(*afterPuncSplitPoint);
    }
    splitRuns(splitPoints);
}

bool ParagraphImpl::isShapedCompressHeadPunctuation(ClusterIndex clusterIndex)
{
    Cluster& originCluster = cluster(clusterIndex);
    if ((!paragraphStyle().getCompressHeadPunctuation()) || (!originCluster.isCompressPunctuation())) {
        return false;
    }
    // Shape a single cluster to get compressed glyph information.
    TextRange headPuncRange = originCluster.textRange();
    BlockRange headPuncBlockRange = findAllBlocks(headPuncRange);
    Block& compressBlock = block(headPuncBlockRange.start);
    TArray<SkShaper::Feature> adjustedFeatures = getAdjustedFontFeature(compressBlock, headPuncRange);
    Run& originRun = originCluster.run();
    std::vector<SkString> families = {SkString(originRun.fFont.GetTypeface()->GetFamilyName())};
    TextStyle updateTextStyle = compressBlock.fStyle;
    updateTextStyle.setFontFamilies(families);

    SkSpan<const char> headPuncSpan = text(headPuncRange);
    SkString headPuncStr = SkString(headPuncSpan.data(), headPuncSpan.size());
    std::unique_ptr<Run> headCompressPuncRun = shapeString(headPuncStr, updateTextStyle,
        adjustedFeatures.data(), adjustedFeatures.size());
    if (headCompressPuncRun == nullptr) {
        return false;
    }
    if (nearlyEqual(originCluster.width(), headCompressPuncRun->advances()[0].x())) {
        return false;
    }
    // Split runs and replace run information in punctuation split.
    splitRunsWhenCompressPunction(clusterIndex);
    Run& fixedRun = originCluster.run();
    TextRange splitedRange(fixedRun.clusterIndexes()[0] + fixedRun.fClusterStart,
        fixedRun.clusterIndexes()[1] + fixedRun.fClusterStart);
    if (splitedRange.start == originCluster.textRange().start && splitedRange.end == originCluster.textRange().end) {
        SkScalar spacing = headCompressPuncRun->advances()[0].x() - originCluster.width();
        originCluster.updateWidth(originCluster.width() + spacing);
        fixedRun.setWidth(fixedRun.fAdvanceX() + spacing);
        fixedRun.updateCompressedRunMeasureInfo(*headCompressPuncRun);
    }
    return true;
}

std::unique_ptr<Run> ParagraphImpl::shapeString(const SkString& str, const TextStyle& textStyle,
    const SkShaper::Feature* features, size_t featuresSize)
{
    auto shaped = [&](std::shared_ptr<RSTypeface> typeface, bool fallback) -> std::unique_ptr<Run> {
        ShapeHandler handler(textStyle.getHeight(), textStyle.getHalfLeading(), textStyle.getTotalVerticalShift(), str);
        RSFont font(typeface, textStyle.getCorrectFontSize(), 1, 0);
        font.SetEdging(RSDrawing::FontEdging::ANTI_ALIAS);
        font.SetHinting(RSDrawing::FontHinting::SLIGHT);
        font.SetSubpixel(true);
        std::unique_ptr<SkShaper> shaper = SkShapers::HB::ShapeDontWrapOrReorder(this->getUnicode(),
            fallback ? RSFontMgr::CreateDefaultFontMgr() : RSFontMgr::CreateDefaultFontMgr());
        const SkBidiIterator::Level defaultLevel = SkBidiIterator::kLTR;
        const char* utf8 = str.c_str();
        size_t utf8Bytes = str.size();

        std::unique_ptr<SkShaper::BiDiRunIterator> bidi = SkShapers::unicode::BidiRunIterator(this->getUnicode(), utf8,
            utf8Bytes, defaultLevel);
        SkASSERT(bidi);
        std::unique_ptr<SkShaper::LanguageRunIterator> language = SkShaper::MakeStdLanguageRunIterator(utf8, utf8Bytes);
        SkASSERT(language);
        std::unique_ptr<SkShaper::ScriptRunIterator> script = SkShapers::HB::ScriptRunIterator(utf8, utf8Bytes);
        SkASSERT(script);
        std::unique_ptr<SkShaper::FontRunIterator> fontRuns = SkShaper::MakeFontMgrRunIterator(utf8, utf8Bytes, font,
            RSFontMgr::CreateDefaultFontMgr());
        SkASSERT(fontRuns);

        shaper->shape(utf8, utf8Bytes, *fontRuns, *bidi, *script, *language, features, featuresSize,
                      std::numeric_limits<SkScalar>::max(), &handler);
        auto shapedRun = handler.run();

        shapedRun->fTextRange = TextRange(0, str.size());
        shapedRun->fOwner = this;
        return shapedRun;
    };
    // Check all allowed fonts.
    auto typefaces = this->fontCollection()->findTypefaces(textStyle.getFontFamilies(), textStyle.getFontStyle(),
        textStyle.getFontArguments());
    for (const auto& typeface : typefaces) {
        auto run = shaped(typeface, false);
        if (run->isResolved()) {
            return run;
        }
    }
    return nullptr;
}

TArray<SkShaper::Feature> ParagraphImpl::getAdjustedFontFeature(Block& compressBlock,
    TextRange headPunctuationRange)
{
    TArray<SkShaper::Feature> features;
    const TextStyle& updateTextStyle  = compressBlock.fStyle;

    for (auto& ff : updateTextStyle.getFontFeatures()) {
        //  Font Feature size always is 4.
        if (ff.fName.size() != 4) {
            TEXT_LOGW("Incorrect font feature: %{public}s = %{public}d", ff.fName.c_str(), ff.fValue);
            continue;
        }
        SkShaper::Feature feature = {
            SkSetFourByteTag(ff.fName[0], ff.fName[1], ff.fName[FEATURE_NAME_INDEX_TWO],
                ff.fName[FEATURE_NAME_INDEX_THREE]),
            SkToU32(ff.fValue),
            compressBlock.fRange.start,
            compressBlock.fRange.end
        };
        features.emplace_back(feature);
    }
    features.emplace_back(SkShaper::Feature{
        // Apply ss08 font feature to compress punctuation.
        SkSetFourByteTag('s', 's', '0', '8'), 1, compressBlock.fRange.start, compressBlock.fRange.end
    });
    // Map the block's features to subranges within the unresolved range.
    TArray<SkShaper::Feature> adjustedFeatures(features.size());
    for (const SkShaper::Feature& feature : features) {
        SkRange<size_t> featureRange(feature.start, feature.end);
        if (headPunctuationRange.intersects(featureRange)) {
            SkRange<size_t> adjustedRange = headPunctuationRange.intersection(featureRange);
            adjustedRange.Shift(-static_cast<std::make_signed_t<size_t>>(headPunctuationRange.start));
            adjustedFeatures.push_back({feature.tag, feature.value, adjustedRange.start, adjustedRange.end});
        }
    }
    return adjustedFeatures;
}

bool ParagraphImpl::needBreakShapedTextIntoLines()
{
    Cluster& headCluster = cluster(0);
    if (paragraphStyle().getCompressHeadPunctuation() && headCluster.isCompressPunctuation()) {
        return true;
    }
    return false;
}
#endif

void ParagraphImpl::paint(SkCanvas* canvas, SkScalar x, SkScalar y) {
#ifdef ENABLE_TEXT_ENHANCE
    TEXT_TRACE_FUNC();
    if (fState >= kFormatted) {
        fState = kDrawn;
    }
#endif
    CanvasParagraphPainter painter(canvas);
    paint(&painter, x, y);
}

void ParagraphImpl::paint(ParagraphPainter* painter, SkScalar x, SkScalar y) {
#ifdef ENABLE_TEXT_ENHANCE
    TEXT_TRACE_FUNC();
    if (fState >= kFormatted) {
        fState = kDrawn;
    }
    // Reset all text style vertical shift
    for (Block& block : fTextStyles) {
        block.fStyle.setVerticalAlignShift(0.0);
    }
#endif
    for (auto& line : fLines) {
#ifdef ENABLE_TEXT_ENHANCE
        line.updateTextLinePaintAttributes();
#endif
        line.paint(painter, x, y);
    }
}

#ifdef ENABLE_TEXT_ENHANCE
void ParagraphImpl::paint(ParagraphPainter* painter, RSPath* path, SkScalar hOffset, SkScalar vOffset) {
    TEXT_TRACE_FUNC();
    if (fState >= kFormatted) {
        fState = kDrawn;
    }
    auto& style = fTextStyles[0].fStyle;
    float align = 0.0f;
    switch (paragraphStyle().getTextAlign()) {
        case TextAlign::kCenter:
            align = -0.5f;
            break;
        case TextAlign::kRight:
            align = -1.0f;
            break;
        default:
            break;
    }
    hOffset += align * (fMaxIntrinsicWidth - style.getLetterSpacing() - path->GetLength(false));
    for (auto& line : fLines) {
        line.paint(painter, path, hOffset, vOffset);
    }
}

TextRange ParagraphImpl::getEllipsisTextRange() {
    if (fState < kLineBroken) {
        return EMPTY_RANGE;
    }
    if (!fEllipsisRange.empty()) {
        return fEllipsisRange;
    }
    this->ensureUTF16Mapping();
    for (const auto& line: fLines) {
        if (line.getTextRangeReplacedByEllipsis().empty()) {
            continue;
        }
        auto ellipsisClusterRange = line.getTextRangeReplacedByEllipsis();
        return TextRange(getUTF16Index(ellipsisClusterRange.start),
                                      getUTF16Index(ellipsisClusterRange.end));
    }
    return EMPTY_RANGE;
}
#endif

void ParagraphImpl::resetContext() {
    fAlphabeticBaseline = 0;
    fHeight = 0;
    fWidth = 0;
    fIdeographicBaseline = 0;
    fMaxIntrinsicWidth = 0;
    fMinIntrinsicWidth = 0;
    fLongestLine = 0;
#ifdef ENABLE_TEXT_ENHANCE
    fLongestLineWithIndent = 0;
#endif
    fMaxWidthWithTrailingSpaces = 0;
    fExceededMaxLines = false;
}

// shapeTextIntoEndlessLine is the thing that calls this method
bool ParagraphImpl::computeCodeUnitProperties() {
#ifdef ENABLE_TEXT_ENHANCE
    TEXT_TRACE_FUNC();
#endif
    if (nullptr == fUnicode) {
        return false;
    }

    // Get bidi regions
    auto textDirection = fParagraphStyle.getTextDirection() == TextDirection::kLtr
                              ? SkUnicode::TextDirection::kLTR
                              : SkUnicode::TextDirection::kRTL;
    if (!fUnicode->getBidiRegions(fText.c_str(), fText.size(), textDirection, &fBidiRegions)) {
        return false;
    }

    // Collect all spaces and some extra information
    // (and also substitute \t with a space while we are at it)
    if (!fUnicode->computeCodeUnitFlags(&fText[0],
                                        fText.size(),
#ifdef ENABLE_TEXT_ENHANCE
                                        this->paragraphStyle().getReplaceTabCharacters() ||
                                        (!(this->paragraphStyle().getTextTab().location < 1.0)),
#else
                                        this->paragraphStyle().getReplaceTabCharacters(),
#endif
                                        &fCodeUnitProperties)) {
        return false;
    }

    // Get some information about trailing spaces / hard line breaks
    fTrailingSpaces = fText.size();
    TextIndex firstWhitespace = EMPTY_INDEX;
    for (int i = 0; i < fCodeUnitProperties.size(); ++i) {
        auto flags = fCodeUnitProperties[i];
        if (SkUnicode::hasPartOfWhiteSpaceBreakFlag(flags)) {
            if (fTrailingSpaces  == fText.size()) {
                fTrailingSpaces = i;
            }
            if (firstWhitespace == EMPTY_INDEX) {
                firstWhitespace = i;
            }
        } else {
            fTrailingSpaces = fText.size();
        }
        if (SkUnicode::hasHardLineBreakFlag(flags)) {
            fHasLineBreaks = true;
        }
    }

    if (firstWhitespace < fTrailingSpaces) {
        fHasWhitespacesInside = true;
    }

    return true;
}

static bool is_ascii_7bit_space(int c) {
    SkASSERT(c >= 0 && c <= 127);

    // Extracted from https://en.wikipedia.org/wiki/Whitespace_character
    //
    enum WS {
        kHT    = 9,
        kLF    = 10,
        kVT    = 11,
        kFF    = 12,
        kCR    = 13,
        kSP    = 32,    // too big to use as shift
    };
#define M(shift)    (1 << (shift))
    constexpr uint32_t kSpaceMask = M(kHT) | M(kLF) | M(kVT) | M(kFF) | M(kCR);
    // we check for Space (32) explicitly, since it is too large to shift
    return (c == kSP) || (c <= 31 && (kSpaceMask & M(c)));
#undef M
}

#ifdef ENABLE_TEXT_ENHANCE
static const std::vector<SkRange<SkUnichar>> CJK_UNICODE_SET = {
    SkRange<SkUnichar>(0x1100, 0x11FF),
    SkRange<SkUnichar>(0x2E80, 0x2EFF),
    // [0x3040, 0x309F](Hiragana) + [0x30A0, 0x30FF](Katakana)
    SkRange<SkUnichar>(0x3040, 0x30FF),
    SkRange<SkUnichar>(0x3130, 0x318F),
    // [0x31C0, 0x31EF](CJK Strokes) + [0x31F0, 0x31FF](Katakana Phonetic Extensions)
    SkRange<SkUnichar>(0x31C0, 0x31FF),
    SkRange<SkUnichar>(0x3400, 0x4DBF),
    SkRange<SkUnichar>(0x4E00, 0x9FFF),
    SkRange<SkUnichar>(0xAC00, 0xD7AF),
    SkRange<SkUnichar>(0xF900, 0xFAFF),
    SkRange<SkUnichar>(0x20000, 0x2A6DF),
/*
    [0x2A700, 0x2B73F](CJK Unified Ideographs Extension C) +
    [0x2B740, 0x2B81F](CJK Unified Ideographs Extension D) +
    [0x2B820, 0x2CEAF](CJK Unified Ideographs Extension E) +
    [0x2CEB0, 0x2EBEF](CJK Unified Ideographs Extension F)
*/
    SkRange<SkUnichar>(0x2A700, 0x2EBEF),
    SkRange<SkUnichar>(0x2F800, 0x2FA1F),
    SkRange<SkUnichar>(0x30000, 0x3134F),
};

static const std::vector<SkRange<SkUnichar>> WESTERN_UNICODE_SET = {
    SkRange<SkUnichar>(0x0030, 0x0039), // Number
    SkRange<SkUnichar>(0x0041, 0x005A), // Base Latin
    SkRange<SkUnichar>(0x0061, 0x007A),
    SkRange<SkUnichar>(0x00C0, 0x00FF), // Latin Extended-1: À-ÿ
    SkRange<SkUnichar>(0x0100, 0x017F), // Latin Extended-A: Ā-ſ
    SkRange<SkUnichar>(0x018F, 0x0192), // Latin Extended-B (specific ranges)
    SkRange<SkUnichar>(0x01A0, 0x01A1),
    SkRange<SkUnichar>(0x01AF, 0x01B0),
    SkRange<SkUnichar>(0x01CD, 0x01DC),
    SkRange<SkUnichar>(0x01E5, 0x01E5),
    SkRange<SkUnichar>(0x01E7, 0x01E7),
    SkRange<SkUnichar>(0x01E9, 0x01E9),
    SkRange<SkUnichar>(0x01EF, 0x01F0),
    SkRange<SkUnichar>(0x01F9, 0x01FF),
    SkRange<SkUnichar>(0x0218, 0x0219),
    SkRange<SkUnichar>(0x021A, 0x021B),
    SkRange<SkUnichar>(0x021F, 0x021F),
    SkRange<SkUnichar>(0x0237, 0x0237),
    SkRange<SkUnichar>(0x0386, 0x0386), // Greek and Coptic
    SkRange<SkUnichar>(0x0388, 0x038A),
    SkRange<SkUnichar>(0x038C, 0x038C),
    SkRange<SkUnichar>(0x038E, 0x038F),
    SkRange<SkUnichar>(0x0390, 0x03A1),
    SkRange<SkUnichar>(0x03A3, 0x03CE),
    SkRange<SkUnichar>(0x03D1, 0x03D2),
    SkRange<SkUnichar>(0x03D6, 0x03D6),
    SkRange<SkUnichar>(0x0400, 0x045F), // Cyrillic
    SkRange<SkUnichar>(0x0462, 0x0463),
    SkRange<SkUnichar>(0x046B, 0x046B),
    SkRange<SkUnichar>(0x0472, 0x0475),
    SkRange<SkUnichar>(0x0490, 0x0493),
    SkRange<SkUnichar>(0x0497, 0x0497),
    SkRange<SkUnichar>(0x049A, 0x049D),
    SkRange<SkUnichar>(0x04A2, 0x04A3),
    SkRange<SkUnichar>(0x04AE, 0x04B3),
    SkRange<SkUnichar>(0x04B8, 0x04BB),
    SkRange<SkUnichar>(0x04CA, 0x04CA),
    SkRange<SkUnichar>(0x04D8, 0x04D9),
    SkRange<SkUnichar>(0x04E8, 0x04E9),
    SkRange<SkUnichar>(0x1E00, 0x1E01), // Latin Extended Additional
    SkRange<SkUnichar>(0x1E3E, 0x1E3F),
    SkRange<SkUnichar>(0x1E80, 0x1E85),
    SkRange<SkUnichar>(0x1EA0, 0x1EF9),
    SkRange<SkUnichar>(0x1F45, 0x1F45), // Greek Extended
    SkRange<SkUnichar>(0x1F4D, 0x1F4D)
};

constexpr SkUnichar COPYRIGHT_UNICODE = 0x00A9;

struct UnicodeIdentifier {
    static bool cmp(SkRange<SkUnichar> a, SkRange<SkUnichar> b) {
        return a.start < b.start;
    }
    const std::vector<SkRange<SkUnichar>>& fUnicodeSet;
    explicit UnicodeIdentifier(const std::vector<SkRange<SkUnichar>>& unicodeSet) : fUnicodeSet(unicodeSet) {}
    bool exist(SkUnichar c) const {
        auto pos = std::upper_bound(fUnicodeSet.begin(), fUnicodeSet.end(), SkRange<SkUnichar>(c, c), cmp);
        if (pos == fUnicodeSet.begin()) {
            return false;
        }
        --pos;
        return pos->end >= c;
    }
};

static const UnicodeIdentifier CJK_IDENTIFIER(CJK_UNICODE_SET);
static const UnicodeIdentifier WESTERN_IDENTIFIER(WESTERN_UNICODE_SET);

static Cluster::AutoSpacingFlag recognizeUnicodeAutoSpacingFlag(ParagraphImpl& paragraph, SkUnichar unicode)
{
    if (!paragraph.isAutoSpaceEnabled()) {
        return Cluster::AutoSpacingFlag::NoFlag;
    }
    if (WESTERN_IDENTIFIER.exist(unicode)) {
        return Cluster::AutoSpacingFlag::Western;
    }
    if (CJK_IDENTIFIER.exist(unicode)) {
        return Cluster::AutoSpacingFlag::CJK;
    }
    if (unicode == COPYRIGHT_UNICODE) {
        return Cluster::AutoSpacingFlag::Copyright;
    }
    return Cluster::AutoSpacingFlag::NoFlag;
}
#endif
Cluster::Cluster(ParagraphImpl* owner,
                 RunIndex runIndex,
                 size_t start,
                 size_t end,
                 SkSpan<const char> text,
                 SkScalar width,
                 SkScalar height)
        : fOwner(owner)
        , fRunIndex(runIndex)
        , fTextRange(text.begin() - fOwner->text().begin(), text.end() - fOwner->text().begin())
        , fGraphemeRange(EMPTY_RANGE)
        , fStart(start)
        , fEnd(end)
        , fWidth(width)
        , fHeight(height)
        , fHalfLetterSpacing(0.0)
        , fIsIdeographic(false) {
    size_t whiteSpacesBreakLen = 0;
    size_t intraWordBreakLen = 0;

    const char* ch = text.begin();
    if (text.end() - ch == 1 && *(const unsigned char*)ch <= 0x7F) {
        // I am not even sure it's worth it if we do not save a unicode call
        if (is_ascii_7bit_space(*ch)) {
            ++whiteSpacesBreakLen;
        }
#ifdef ENABLE_TEXT_ENHANCE
        fIsPunctuation = fOwner->codeUnitHasProperty(fTextRange.start, SkUnicode::CodeUnitFlags::kPunctuation);
        fIsEllipsis = fOwner->codeUnitHasProperty(fTextRange.start, SkUnicode::CodeUnitFlags::kEllipsis);
#endif
    } else {
        for (auto i = fTextRange.start; i < fTextRange.end; ++i) {
            if (fOwner->codeUnitHasProperty(i, SkUnicode::CodeUnitFlags::kPartOfWhiteSpaceBreak)) {
                ++whiteSpacesBreakLen;
            }
            if (fOwner->codeUnitHasProperty(i, SkUnicode::CodeUnitFlags::kPartOfIntraWordBreak)) {
                ++intraWordBreakLen;
            }
            if (fOwner->codeUnitHasProperty(i, SkUnicode::CodeUnitFlags::kIdeographic)) {
                fIsIdeographic = true;
            }
#ifdef ENABLE_TEXT_ENHANCE
            fIsPunctuation = fOwner->codeUnitHasProperty(i, SkUnicode::CodeUnitFlags::kPunctuation) | fIsPunctuation;
            fIsEllipsis = fOwner->codeUnitHasProperty(i, SkUnicode::CodeUnitFlags::kEllipsis) | fIsEllipsis;
            fNeedCompressPunctuation = fOwner->codeUnitHasProperty(i,
                SkUnicode::CodeUnitFlags::kNeedCompressHeadPunctuation);
#endif
        }
    }

    fIsWhiteSpaceBreak = whiteSpacesBreakLen == fTextRange.width();
    fIsIntraWordBreak = intraWordBreakLen == fTextRange.width();
    fIsHardBreak = fOwner->codeUnitHasProperty(fTextRange.end,
                                               SkUnicode::CodeUnitFlags::kHardLineBreakBefore);
#ifdef ENABLE_TEXT_ENHANCE
    fIsTabulation = fOwner->codeUnitHasProperty(fTextRange.start,
                                                SkUnicode::CodeUnitFlags::kTabulation);
    auto unicodeStart = fOwner->getUnicodeIndex(fTextRange.start);
    auto unicodeEnd = fOwner->getUnicodeIndex(fTextRange.end);
    SkUnichar unicode = 0;
    if (unicodeEnd - unicodeStart == 1 && unicodeStart < fOwner->unicodeText().size()) {
        unicode = fOwner->unicodeText()[unicodeStart];
    }

    auto curAutoSpacingFlag = recognizeUnicodeAutoSpacingFlag(*fOwner, unicode);
    auto lastAutoSpacingFlag = fOwner->getLastAutoSpacingFlag();
    fNeedAutoSpacing = curAutoSpacingFlag != Cluster::AutoSpacingFlag::NoFlag &&
        curAutoSpacingFlag != lastAutoSpacingFlag && lastAutoSpacingFlag != Cluster::AutoSpacingFlag::NoFlag;
    fOwner->setLastAutoSpacingFlag(curAutoSpacingFlag);
#endif
}

SkScalar Run::calculateWidth(size_t start, size_t end, bool clip) const {
    SkASSERT(start <= end);
    // clip |= end == size();  // Clip at the end of the run?
    auto correction = 0.0f;
    if (end > start && !fJustificationShifts.empty()) {
        // This is not a typo: we are using Point as a pair of SkScalars
        correction = fJustificationShifts[end - 1].fX -
                     fJustificationShifts[start].fY;
    }
#ifdef ENABLE_TEXT_ENHANCE
    if (end > start && !fAutoSpacings.empty()) {
        // This is not a tyopo: we are using Point as a pair of SkScalars
        correction += fAutoSpacings[end - 1].fX - fAutoSpacings[start].fY;
    }
#endif
    return posX(end) - posX(start) + correction;
}

// In some cases we apply spacing to glyphs first and then build the cluster table, in some we do
// the opposite - just to optimize the most common case.
void ParagraphImpl::applySpacingAndBuildClusterTable() {
#ifdef ENABLE_TEXT_ENHANCE
    TEXT_TRACE_FUNC();
#endif
    // Check all text styles to see what we have to do (if anything)
    size_t letterSpacingStyles = 0;
    bool hasWordSpacing = false;
    for (auto& block : fTextStyles) {
        if (block.fRange.width() > 0) {
            if (!SkScalarNearlyZero(block.fStyle.getLetterSpacing())) {
                ++letterSpacingStyles;
            }
            if (!SkScalarNearlyZero(block.fStyle.getWordSpacing())) {
                hasWordSpacing = true;
            }
        }
    }

    if (letterSpacingStyles == 0 && !hasWordSpacing) {
        // We don't have to do anything about spacing (most common case)
        this->buildClusterTable();
        return;
    }

    if (letterSpacingStyles == 1 && !hasWordSpacing && fTextStyles.size() == 1 &&
        fTextStyles[0].fRange.width() == fText.size() && fRuns.size() == 1) {
        // We have to letter space the entire paragraph (second most common case)
        auto& run = fRuns[0];
        auto& style = fTextStyles[0].fStyle;
#ifndef ENABLE_TEXT_ENHANCE
        run.addSpacesEvenly(style.getLetterSpacing());
#endif
        this->buildClusterTable();
#ifdef ENABLE_TEXT_ENHANCE
        SkScalar shift = 0;
        run.iterateThroughClusters([this, &run, &shift, &style](Cluster* cluster) {
            run.shift(cluster, shift);
            shift += run.addSpacesEvenly(style.getLetterSpacing(), cluster);
        });
#else
        // This is something Flutter requires
        for (auto& cluster : fClusters) {
            cluster.setHalfLetterSpacing(style.getLetterSpacing()/2);
        }
#endif
        return;
    }

    // The complex case: many text styles with spacing (possibly not adjusted to glyphs)
    this->buildClusterTable();

    // Walk through all the clusters in the direction of shaped text
    // (we have to walk through the styles in the same order, too)
    // Not breaking the iteration on every run!
    SkScalar shift = 0;
    bool soFarWhitespacesOnly = true;
    bool wordSpacingPending = false;
    Cluster* lastSpaceCluster = nullptr;
    for (auto& run : fRuns) {

        // Skip placeholder runs
        if (run.isPlaceholder()) {
            continue;
        }

        run.iterateThroughClusters([this, &run, &shift, &soFarWhitespacesOnly, &wordSpacingPending, &lastSpaceCluster](Cluster* cluster) {
            // Shift the cluster (shift collected from the previous clusters)
            run.shift(cluster, shift);

            // Synchronize styles (one cluster can be covered by few styles)
            Block* currentStyle = fTextStyles.begin();
            while (!cluster->startsIn(currentStyle->fRange)) {
                currentStyle++;
                SkASSERT(currentStyle != fTextStyles.end());
            }

            SkASSERT(!currentStyle->fStyle.isPlaceholder());

            // Process word spacing
            if (currentStyle->fStyle.getWordSpacing() != 0) {
                if (cluster->isWhitespaceBreak() && cluster->isSoftBreak()) {
                    if (!soFarWhitespacesOnly) {
                        lastSpaceCluster = cluster;
                        wordSpacingPending = true;
                    }
                } else if (wordSpacingPending) {
                    SkScalar spacing = currentStyle->fStyle.getWordSpacing();
                    if (cluster->fRunIndex != lastSpaceCluster->fRunIndex) {
                        // If the last space cluster belongs to the previous run
                        // we have to extend that cluster and that run
                        lastSpaceCluster->run().addSpacesAtTheEnd(spacing, lastSpaceCluster);
                        lastSpaceCluster->run().extend(lastSpaceCluster, spacing);
                    } else {
                        run.addSpacesAtTheEnd(spacing, lastSpaceCluster);
                    }
                    run.shift(cluster, spacing);
                    shift += spacing;
                    wordSpacingPending = false;
                }
            }
            // Process letter spacing
            if (currentStyle->fStyle.getLetterSpacing() != 0) {
                shift += run.addSpacesEvenly(currentStyle->fStyle.getLetterSpacing(), cluster);
            }

            if (soFarWhitespacesOnly && !cluster->isWhitespaceBreak()) {
                soFarWhitespacesOnly = false;
            }
        });
    }
}

#ifdef ENABLE_TEXT_ENHANCE
void ParagraphImpl::buildClusterPlaceholder(Run& run, size_t runIndex)
{
    if (run.isPlaceholder()) {
        // Add info to cluster indexes table (text -> cluster)
        for (auto i = run.textRange().start; i < run.textRange().end; ++i) {
          fClustersIndexFromCodeUnit[i] = fClusters.size();
        }
        // There are no glyphs but we want to have one cluster
        fClusters.emplace_back(this, runIndex, 0ul, 1ul, this->text(run.textRange()),
            run.advance().fX, run.advance().fY);
        fCodeUnitProperties[run.textRange().start] |= SkUnicode::CodeUnitFlags::kSoftLineBreakBefore;
        fCodeUnitProperties[run.textRange().end] |= SkUnicode::CodeUnitFlags::kSoftLineBreakBefore;
    } else {
        // Walk through the glyph in the direction of input text
        run.iterateThroughClustersInTextOrder([&run, runIndex, this](size_t glyphStart,
            size_t glyphEnd, size_t charStart, size_t charEnd, SkScalar width, SkScalar height) {
            SkASSERT(charEnd >= charStart);
            // Add info to cluster indexes table (text -> cluster)
            for (auto i = charStart; i < charEnd; ++i) {
              fClustersIndexFromCodeUnit[i] = fClusters.size();
            }
            SkSpan<const char> text(fText.c_str() + charStart, charEnd - charStart);
            fClusters.emplace_back(this, runIndex, glyphStart, glyphEnd, text, width, height);
            fCodeUnitProperties[charStart] |= SkUnicode::CodeUnitFlags::kGlyphClusterStart;
        });
    }
}
#endif

// Clusters in the order of the input text
void ParagraphImpl::buildClusterTable()
{
    // It's possible that one grapheme includes few runs; we cannot handle it
    // so we break graphemes by the runs instead
    // It's not the ideal solution and has to be revisited later
    int cluster_count = 1;
    for (auto& run : fRuns) {
        cluster_count += run.isPlaceholder() ? 1 : run.size();
        fCodeUnitProperties[run.fTextRange.start] |= SkUnicode::CodeUnitFlags::kGraphemeStart;
        fCodeUnitProperties[run.fTextRange.start] |= SkUnicode::CodeUnitFlags::kGlyphClusterStart;
    }
    if (!fRuns.empty()) {
        fCodeUnitProperties[fRuns.back().textRange().end] |= SkUnicode::CodeUnitFlags::kGraphemeStart;
        fCodeUnitProperties[fRuns.back().textRange().end] |= SkUnicode::CodeUnitFlags::kGlyphClusterStart;
    }
    fClusters.reserve_exact(fClusters.size() + cluster_count);

    // Walk through all the run in the direction of input text
    for (auto& run : fRuns) {
        auto runIndex = run.index();
        auto runStart = fClusters.size();
        if (run.isPlaceholder()) {
            // Add info to cluster indexes table (text -> cluster)
            for (auto i = run.textRange().start; i < run.textRange().end; ++i) {
              fClustersIndexFromCodeUnit[i] = fClusters.size();
            }
            // There are no glyphs but we want to have one cluster
            fClusters.emplace_back(this, runIndex, 0ul, 1ul, this->text(run.textRange()), run.advance().fX, run.advance().fY);
            fCodeUnitProperties[run.textRange().start] |= SkUnicode::CodeUnitFlags::kSoftLineBreakBefore;
            fCodeUnitProperties[run.textRange().end] |= SkUnicode::CodeUnitFlags::kSoftLineBreakBefore;
        } else {
            // Walk through the glyph in the direction of input text
#ifdef ENABLE_TEXT_ENHANCE
            run.iterateThroughClustersInTextOrder([&run, runIndex, this](size_t glyphStart,
                                                                   size_t glyphEnd,
                                                                   size_t charStart,
                                                                   size_t charEnd,
                                                                   SkScalar width,
                                                                   SkScalar height) {
#else
            run.iterateThroughClustersInTextOrder([runIndex, this](size_t glyphStart,
                                                                   size_t glyphEnd,
                                                                   size_t charStart,
                                                                   size_t charEnd,
                                                                   SkScalar width,
                                                                   SkScalar height) {
#endif
                SkASSERT(charEnd >= charStart);
                // Add info to cluster indexes table (text -> cluster)
                for (auto i = charStart; i < charEnd; ++i) {
                  fClustersIndexFromCodeUnit[i] = fClusters.size();
                }
                SkSpan<const char> text(fText.c_str() + charStart, charEnd - charStart);
                fClusters.emplace_back(this, runIndex, glyphStart, glyphEnd, text, width, height);
                fCodeUnitProperties[charStart] |= SkUnicode::CodeUnitFlags::kGlyphClusterStart;
            });
        }
        fCodeUnitProperties[run.textRange().start] |= SkUnicode::CodeUnitFlags::kGlyphClusterStart;

        run.setClusterRange(runStart, fClusters.size());
        fMaxIntrinsicWidth += run.advance().fX;
    }
    fClustersIndexFromCodeUnit[fText.size()] = fClusters.size();
    fClusters.emplace_back(this, EMPTY_RUN, 0, 0, this->text({fText.size(), fText.size()}), 0, 0);
}

bool ParagraphImpl::shapeTextIntoEndlessLine() {
#ifdef ENABLE_TEXT_ENHANCE
    TEXT_TRACE_FUNC();
#endif
    if (fText.size() == 0) {
        return false;
    }

    fUnresolvedCodepoints.clear();
    fFontSwitches.clear();

    OneLineShaper oneLineShaper(this);
    auto result = oneLineShaper.shape();
    fUnresolvedGlyphs = oneLineShaper.unresolvedGlyphs();

    this->applySpacingAndBuildClusterTable();

    return result;
}

#ifdef ENABLE_TEXT_ENHANCE
void ParagraphImpl::setIndents(const std::vector<SkScalar>& indents)
{
    fIndents = indents;
}

SkScalar ParagraphImpl::detectIndents(size_t index)
{
    SkScalar indent = 0.0;
    if (fIndents.size() > 0 && index < fIndents.size()) {
        indent = fIndents[index];
    } else {
        indent = fIndents.size() > 0 ? fIndents.back() : 0.0;
    }

    return indent;
}

void ParagraphImpl::positionShapedTextIntoLine(SkScalar maxWidth) {
    resetAutoSpacing();
    // This is a short version of a line breaking when we know that:
    // 1. We have only one line of text
    // 2. It's shaped into a single run
    // 3. There are no placeholders
    // 4. There are no linebreaks (which will format text into multiple lines)
    // 5. There are no whitespaces so the minIntrinsicWidth=maxIntrinsicWidth
    // (To think about that, the last condition is not quite right;
    // we should calculate minIntrinsicWidth by soft line breaks.
    // However, it's how it's done in Flutter now)
    auto& run = this->fRuns[0];
    auto advance = run.advance();
    auto textRange = TextRange(0, this->text().size());
    auto textExcludingSpaces = TextRange(0, fTrailingSpaces);
    InternalLineMetrics metrics(strutForceHeight() && strutEnabled());
    metrics.add(&run);
    auto disableFirstAscent = this->paragraphStyle().getTextHeightBehavior() & TextHeightBehavior::kDisableFirstAscent;
    auto disableLastDescent = this->paragraphStyle().getTextHeightBehavior() & TextHeightBehavior::kDisableLastDescent;
    if (disableFirstAscent) {
        metrics.fAscent = metrics.fRawAscent;
    }
    if (disableLastDescent) {
        metrics.fDescent = metrics.fRawDescent;
    }
    if (this->strutEnabled()) {
        this->strutMetrics().updateLineMetrics(metrics);
    }
    ClusterIndex trailingSpaces = fClusters.size();
    do {
        --trailingSpaces;
        auto& cluster = fClusters[trailingSpaces];
        if (!cluster.isWhitespaceBreak()) {
            ++trailingSpaces;
            break;
        }
        advance.fX -= cluster.width();
    } while (trailingSpaces != 0);

    advance.fY = metrics.height();
    if (this->paragraphStyle().getLineSpacing() > 0 && !disableLastDescent) {
        advance.fY += this->paragraphStyle().getLineSpacing();
    }
    SkScalar heightWithParagraphSpacing = advance.fY;
    if (this->paragraphStyle().getIsEndAddParagraphSpacing() &&
        this->paragraphStyle().getParagraphSpacing() > 0) {
        heightWithParagraphSpacing += this->paragraphStyle().getParagraphSpacing();
    }
    auto clusterRange = ClusterRange(0, trailingSpaces);
    auto clusterRangeWithGhosts = ClusterRange(0, this->clusters().size() - 1);
    SkScalar offsetX = this->detectIndents(0);
    auto& line = this->addLine(SkPoint::Make(offsetX, 0), advance, textExcludingSpaces, textRange, textRange,
        clusterRange, clusterRangeWithGhosts, run.advance().x(), metrics);
    auto spacing = line.autoSpacing();
    auto longestLine = std::max(run.advance().fX, advance.fX) + spacing;
    setSize(heightWithParagraphSpacing, maxWidth, longestLine);
    setLongestLineWithIndent(longestLine + offsetX);
    setIntrinsicSize(run.advance().fX, advance.fX,
        fLines.empty() ? fEmptyMetrics.alphabeticBaseline() : fLines.front().alphabeticBaseline(),
        fLines.empty() ? fEmptyMetrics.ideographicBaseline() : fLines.front().ideographicBaseline(),
        false);
}

void ParagraphImpl::breakShapedTextIntoLines(SkScalar maxWidth) {
    TEXT_TRACE_FUNC();
    resetAutoSpacing();
    resetIsNeedUpdateRunCache();
    TextWrapper textWrapper;
    textWrapper.breakTextIntoLines(
            this,
            maxWidth,
            [&](TextRange textExcludingSpaces,
                TextRange text,
                TextRange textWithNewlines,
                ClusterRange clusters,
                ClusterRange clustersWithGhosts,
                SkScalar widthWithSpaces,
                size_t startPos,
                size_t endPos,
                SkVector offset,
                SkVector advance,
                InternalLineMetrics metrics,
                bool addEllipsis,
                SkScalar indent,
                SkScalar noIndentWidth) {
                // TODO: Take in account clipped edges
                auto& line = this->addLine(offset, advance, textExcludingSpaces, text, textWithNewlines,
                    clusters, clustersWithGhosts, widthWithSpaces, metrics);
                line.autoSpacing();
                if (addEllipsis && this->paragraphStyle().getEllipsisMod() == EllipsisModal::TAIL) {
                    line.createTailEllipsis(noIndentWidth, this->getEllipsis(), true, this->getWordBreakType());
                } else if (addEllipsis && this->paragraphStyle().getEllipsisMod() == EllipsisModal::HEAD) {
                    line.createHeadEllipsis(noIndentWidth, this->getEllipsis(), true);
                }
                else if (needCreateMiddleEllipsis()) {
                    line.createMiddleEllipsis(noIndentWidth, this->getEllipsis());
                } else if (textWrapper.brokeLineWithHyphen()
                           || ((clusters.end == clustersWithGhosts.end) && (clusters.end >= 1)
                               && (clusters.end < this->fUnicodeText.size())
                               && (this->fUnicodeText[clusters.end - 1] == 0xad))) { // 0xad represents a soft hyphen
                    line.setBreakWithHyphen(true);
                }
                auto longestLine = std::max(line.width(), line.widthWithEllipsisSpaces());
                fLongestLine = std::max(fLongestLine, longestLine);
                fLongestLineWithIndent = std::max(fLongestLineWithIndent, longestLine + indent);
            });
    setSize(textWrapper.height(), maxWidth, fLongestLine);
    setIntrinsicSize(textWrapper.maxIntrinsicWidth(), textWrapper.minIntrinsicWidth(),
        fLines.empty() ? fEmptyMetrics.alphabeticBaseline() : fLines.front().alphabeticBaseline(),
        fLines.empty() ? fEmptyMetrics.ideographicBaseline() : fLines.front().ideographicBaseline(),
        textWrapper.exceededMaxLines());
}
#else
void ParagraphImpl::breakShapedTextIntoLines(SkScalar maxWidth) {

    if (!fHasLineBreaks &&
        !fHasWhitespacesInside &&
        fPlaceholders.size() == 1 &&
        fRuns.size() == 1 && fRuns[0].fAdvance.fX <= maxWidth) {
        // This is a short version of a line breaking when we know that:
        // 1. We have only one line of text
        // 2. It's shaped into a single run
        // 3. There are no placeholders
        // 4. There are no linebreaks (which will format text into multiple lines)
        // 5. There are no whitespaces so the minIntrinsicWidth=maxIntrinsicWidth
        // (To think about that, the last condition is not quite right;
        // we should calculate minIntrinsicWidth by soft line breaks.
        // However, it's how it's done in Flutter now)
        auto& run = this->fRuns[0];
        auto advance = run.advance();
        auto textRange = TextRange(0, this->text().size());
        auto textExcludingSpaces = TextRange(0, fTrailingSpaces);
        InternalLineMetrics metrics(this->strutForceHeight());
        metrics.add(&run);
        auto disableFirstAscent = this->paragraphStyle().getTextHeightBehavior() &
                                  TextHeightBehavior::kDisableFirstAscent;
        auto disableLastDescent = this->paragraphStyle().getTextHeightBehavior() &
                                  TextHeightBehavior::kDisableLastDescent;
        if (disableFirstAscent) {
            metrics.fAscent = metrics.fRawAscent;
        }
        if (disableLastDescent) {
            metrics.fDescent = metrics.fRawDescent;
        }
        if (this->strutEnabled()) {
            this->strutMetrics().updateLineMetrics(metrics);
        }
        ClusterIndex trailingSpaces = fClusters.size();
        do {
            --trailingSpaces;
            auto& cluster = fClusters[trailingSpaces];
            if (!cluster.isWhitespaceBreak()) {
                ++trailingSpaces;
                break;
            }
            advance.fX -= cluster.width();
        } while (trailingSpaces != 0);

        advance.fY = metrics.height();
        auto clusterRange = ClusterRange(0, trailingSpaces);
        auto clusterRangeWithGhosts = ClusterRange(0, this->clusters().size() - 1);
        this->addLine(SkPoint::Make(0, 0), advance,
                      textExcludingSpaces, textRange, textRange,
                      clusterRange, clusterRangeWithGhosts, run.advance().x(),
                      metrics);

        fLongestLine = nearlyZero(advance.fX) ? run.advance().fX : advance.fX;
        fHeight = advance.fY;
        fWidth = maxWidth;
        fMaxIntrinsicWidth = run.advance().fX;
        fMinIntrinsicWidth = advance.fX;
        fAlphabeticBaseline = fLines.empty() ? fEmptyMetrics.alphabeticBaseline() : fLines.front().alphabeticBaseline();
        fIdeographicBaseline = fLines.empty() ? fEmptyMetrics.ideographicBaseline() : fLines.front().ideographicBaseline();
        fExceededMaxLines = false;
        return;
    }

    TextWrapper textWrapper;
    textWrapper.breakTextIntoLines(
            this,
            maxWidth,
            [&](TextRange textExcludingSpaces,
                TextRange text,
                TextRange textWithNewlines,
                ClusterRange clusters,
                ClusterRange clustersWithGhosts,
                SkScalar widthWithSpaces,
                size_t startPos,
                size_t endPos,
                SkVector offset,
                SkVector advance,
                InternalLineMetrics metrics,
                bool addEllipsis) {
                // TODO: Take in account clipped edges
                auto& line = this->addLine(offset, advance, textExcludingSpaces, text, textWithNewlines, clusters, clustersWithGhosts, widthWithSpaces, metrics);
                if (addEllipsis) {
                    line.createEllipsis(maxWidth, this->getEllipsis(), true);
                }
                fLongestLine = std::max(fLongestLine, nearlyZero(line.width()) ? widthWithSpaces : line.width());
            });

    fHeight = textWrapper.height();
    fWidth = maxWidth;
    fMaxIntrinsicWidth = textWrapper.maxIntrinsicWidth();
    fMinIntrinsicWidth = textWrapper.minIntrinsicWidth();
    fAlphabeticBaseline = fLines.empty() ? fEmptyMetrics.alphabeticBaseline() : fLines.front().alphabeticBaseline();
    fIdeographicBaseline = fLines.empty() ? fEmptyMetrics.ideographicBaseline() : fLines.front().ideographicBaseline();
    fExceededMaxLines = textWrapper.exceededMaxLines();
}
#endif

void ParagraphImpl::formatLines(SkScalar maxWidth) {
#ifdef ENABLE_TEXT_ENHANCE
    TEXT_TRACE_FUNC();
#endif
    auto effectiveAlign = fParagraphStyle.effective_align();
    const bool isLeftAligned = effectiveAlign == TextAlign::kLeft
        || (effectiveAlign == TextAlign::kJustify && fParagraphStyle.getTextDirection() == TextDirection::kLtr);

    if (!SkIsFinite(maxWidth) && !isLeftAligned) {
        // Special case: clean all text in case of maxWidth == INF & align != left
        // We had to go through shaping though because we need all the measurement numbers
        fLines.clear();
        return;
    }

#ifdef ENABLE_TEXT_ENHANCE
    size_t iLineNumber = 0;
    for (auto& line : fLines) {
        SkScalar noIndentWidth = maxWidth - detectIndents(iLineNumber++);
        if (fParagraphStyle.getTextDirection() == TextDirection::kRtl) {
            line.setLineOffsetX(0);
        }
        line.format(effectiveAlign, noIndentWidth, this->paragraphStyle().getEllipsisMod());

        if (paragraphStyle().getVerticalAlignment() != TextVerticalAlign::BASELINE) {
            line.applyVerticalShift();
        }
    }
#else
    for (auto& line : fLines) {
        line.format(effectiveAlign, maxWidth);
    }
#endif
}

#ifdef ENABLE_TEXT_ENHANCE
void ParagraphImpl::resolveStrut() {
    auto strutStyle = this->paragraphStyle().getStrutStyle();
    if (!strutStyle.getStrutEnabled() || strutStyle.getFontSize() < 0) {
        return;
    }

    auto typefaces = fFontCollection->findTypefaces(strutStyle.getFontFamilies(),
        strutStyle.getFontStyle(), std::nullopt);
    if (typefaces.empty()) {
        SkDEBUGF("Could not resolve strut font\n");
        return;
    }

    RSFont font(typefaces.front(), strutStyle.getFontSize(), 1, 0);
    RSFontMetrics metrics;
    RSFont compressFont = font;
    scaleFontWithCompressionConfig(compressFont, ScaleOP::COMPRESS);
    compressFont.GetMetrics(&metrics);
    metricsIncludeFontPadding(&metrics, font);

    if (strutStyle.getHeightOverride()) {
        auto strutHeight = metrics.fDescent - metrics.fAscent;
        auto strutMultiplier = strutStyle.getHeight() * strutStyle.getFontSize();
        fStrutMetrics = InternalLineMetrics(
            (metrics.fAscent / strutHeight) * strutMultiplier,
            (metrics.fDescent / strutHeight) * strutMultiplier,
                strutStyle.getLeading() < 0 ? 0 : strutStyle.getLeading() * strutStyle.getFontSize(),
            metrics.fAscent, metrics.fDescent, metrics.fLeading);
    } else {
        fStrutMetrics = InternalLineMetrics(
                metrics.fAscent,
                metrics.fDescent,
                strutStyle.getLeading() < 0 ? 0 : strutStyle.getLeading() * strutStyle.getFontSize());
    }
    fStrutMetrics.setForceStrut(this->paragraphStyle().getStrutStyle().getForceStrutHeight());
}
#else
void ParagraphImpl::resolveStrut() {
    auto strutStyle = this->paragraphStyle().getStrutStyle();
    if (!strutStyle.getStrutEnabled() || strutStyle.getFontSize() < 0) {
        return;
    }

    std::vector<sk_sp<SkTypeface>> typefaces = fFontCollection->findTypefaces(strutStyle.getFontFamilies(),
        strutStyle.getFontStyle(), std::nullopt);
    if (typefaces.empty()) {
        SkDEBUGF("Could not resolve strut font\n");
        return;
    }

    SkFont font(typefaces.front(), strutStyle.getFontSize());
    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    const SkScalar strutLeading = strutStyle.getLeading() < 0 ? 0 : strutStyle.getLeading() * strutStyle.getFontSize();

    if (strutStyle.getHeightOverride()) {
        SkScalar strutAscent = 0.0f;
        SkScalar strutDescent = 0.0f;
        // The half leading flag doesn't take effect unless there's height override.
        if (strutStyle.getHalfLeading()) {
            const auto occupiedHeight = metrics.fDescent - metrics.fAscent;
            auto flexibleHeight = strutStyle.getHeight() * strutStyle.getFontSize() - occupiedHeight;
            // Distribute the flexible height evenly over and under.
            flexibleHeight /= 2;
            strutAscent = metrics.fAscent - flexibleHeight;
            strutDescent = metrics.fDescent + flexibleHeight;
        } else {
            const SkScalar strutMetricsHeight = metrics.fDescent - metrics.fAscent + metrics.fLeading;
            const auto strutHeightMultiplier = strutMetricsHeight == 0
              ? strutStyle.getHeight()
              : strutStyle.getHeight() * strutStyle.getFontSize() / strutMetricsHeight;
            strutAscent = metrics.fAscent * strutHeightMultiplier;
            strutDescent = metrics.fDescent * strutHeightMultiplier;
        }
        fStrutMetrics = InternalLineMetrics(
            strutAscent,
            strutDescent,
            strutLeading,
            metrics.fAscent, metrics.fDescent, metrics.fLeading);
    } else {
        fStrutMetrics = InternalLineMetrics(
                metrics.fAscent,
                metrics.fDescent,
                strutLeading);
    }
    fStrutMetrics.setForceStrut(this->paragraphStyle().getStrutStyle().getForceStrutHeight());
}
#endif

BlockRange ParagraphImpl::findAllBlocks(TextRange textRange) {
    BlockIndex begin = EMPTY_BLOCK;
    BlockIndex end = EMPTY_BLOCK;
    for (int index = 0; index < fTextStyles.size(); ++index) {
        auto& block = fTextStyles[index];
        if (block.fRange.end <= textRange.start) {
            continue;
        }
        if (block.fRange.start >= textRange.end) {
            break;
        }
        if (begin == EMPTY_BLOCK) {
            begin = index;
        }
        end = index;
    }

    if (begin == EMPTY_INDEX || end == EMPTY_INDEX) {
        // It's possible if some text is not covered with any text style
        // Not in Flutter but in direct use of SkParagraph
        return EMPTY_RANGE;
    }

    return { begin, end + 1 };
}

TextLine& ParagraphImpl::addLine(SkVector offset,
                                 SkVector advance,
                                 TextRange textExcludingSpaces,
                                 TextRange text,
                                 TextRange textIncludingNewLines,
                                 ClusterRange clusters,
                                 ClusterRange clustersWithGhosts,
                                 SkScalar widthWithSpaces,
                                 InternalLineMetrics sizes) {
    // Define a list of styles that covers the line
#ifdef ENABLE_TEXT_ENHANCE
    auto blocks = findAllBlocks(textIncludingNewLines);
#else
    auto blocks = findAllBlocks(textExcludingSpaces);
#endif
    return fLines.emplace_back(this, offset, advance, blocks,
                               textExcludingSpaces, text, textIncludingNewLines,
                               clusters, clustersWithGhosts, widthWithSpaces, sizes);
}

#ifdef ENABLE_TEXT_ENHANCE
TextBox ParagraphImpl::getEmptyTextRect(RectHeightStyle rectHeightStyle) const
{
    // get textStyle to calculate text box when text is empty(reference to ParagraphImpl::computeEmptyMetrics())
    bool emptyParagraph = fRuns.empty();
    TextStyle textStyle = paragraphStyle().getTextStyle();
    if (emptyParagraph && !fTextStyles.empty()) {
        textStyle = fTextStyles.back().fStyle;
    }

    // calculate text box(reference to TextLine::getRectsForRange(), switch(rectHeightStyle))
    TextBox box(SkRect::MakeXYWH(0, 0, 0, fHeight), fParagraphStyle.getTextDirection());
    auto verticalShift = fEmptyMetrics.rawAscent() - fEmptyMetrics.ascent();
    switch (rectHeightStyle) {
        case RectHeightStyle::kTight:
            if (textStyle.getHeightOverride() && textStyle.getHeight() > 0) {
                const auto effectiveBaseline = fEmptyMetrics.baseline() + fEmptyMetrics.delta();
                box.rect.fTop = effectiveBaseline + fEmptyMetrics.rawAscent();
                box.rect.fBottom = effectiveBaseline + fEmptyMetrics.rawDescent();
            }
            return box;
        case RectHeightStyle::kMax:
            box.rect.fBottom = fHeight;
            box.rect.fTop = fEmptyMetrics.delta();
            return box;
        case RectHeightStyle::kIncludeLineSpacingMiddle:
        case RectHeightStyle::kIncludeLineSpacingTop:
        case RectHeightStyle::kIncludeLineSpacingBottom:
            box.rect.fBottom = fHeight;
            box.rect.fTop = fEmptyMetrics.delta() + verticalShift;
            return box;
        case RectHeightStyle::kStrut:
            if (fParagraphStyle.getStrutStyle().getStrutEnabled() &&
                fParagraphStyle.getStrutStyle().getFontSize() > 0) {
                auto baseline = fEmptyMetrics.baseline();
                box.rect.fTop = baseline + fStrutMetrics.ascent();
                box.rect.fBottom = baseline + fStrutMetrics.descent();
            }
            return box;
        default:
            return box;
    }
}
#endif

// Returns a vector of bounding boxes that enclose all text between
// start and end glyph indexes, including start and excluding end
std::vector<TextBox> ParagraphImpl::getRectsForRange(unsigned start,
                                                     unsigned end,
                                                     RectHeightStyle rectHeightStyle,
                                                     RectWidthStyle rectWidthStyle) {
    std::vector<TextBox> results;
#ifdef ENABLE_TEXT_ENHANCE
    if (fText.isEmpty() || fState < kShaped) {
#else
    if (fText.isEmpty()) {
#endif
        if (start == 0 && end > 0) {
            // On account of implied "\n" that is always at the end of the text
            //SkDebugf("getRectsForRange(%d, %d): %f\n", start, end, fHeight);
#ifdef ENABLE_TEXT_ENHANCE
            results.emplace_back(getEmptyTextRect(rectHeightStyle));
#else
            results.emplace_back(SkRect::MakeXYWH(0, 0, 0, fHeight), fParagraphStyle.getTextDirection());
#endif
        }
        return results;
    }

    this->ensureUTF16Mapping();

    if (start >= end || start > SkToSizeT(fUTF8IndexForUTF16Index.size()) || end == 0) {
        return results;
    }

    // Adjust the text to grapheme edges
    // Apparently, text editor CAN move inside graphemes but CANNOT select a part of it.
    // I don't know why - the solution I have here returns an empty box for every query that
    // does not contain an end of a grapheme.
    // Once a cursor is inside a complex grapheme I can press backspace and cause trouble.
    // To avoid any problems, I will not allow any selection of a part of a grapheme.
    // One flutter test fails because of it but the editing experience is correct
    // (although you have to press the cursor many times before it moves to the next grapheme).
    TextRange text(fText.size(), fText.size());
    // TODO: This is probably a temp change that makes SkParagraph work as TxtLib
    //  (so we can compare the results). We now include in the selection box only the graphemes
    //  that belongs to the given [start:end) range entirely (not the ones that intersect with it)
    if (start < SkToSizeT(fUTF8IndexForUTF16Index.size())) {
        auto utf8 = fUTF8IndexForUTF16Index[start];
        // If start points to a trailing surrogate, skip it
        if (start > 0 && fUTF8IndexForUTF16Index[start - 1] == utf8) {
            utf8 = fUTF8IndexForUTF16Index[start + 1];
        }
        text.start = this->findNextGraphemeBoundary(utf8);
    }
    if (end < SkToSizeT(fUTF8IndexForUTF16Index.size())) {
        auto utf8 = this->findPreviousGraphemeBoundary(fUTF8IndexForUTF16Index[end]);
        text.end = utf8;
    }
    //SkDebugf("getRectsForRange(%d,%d) -> (%d:%d)\n", start, end, text.start, text.end);
    for (auto& line : fLines) {
        auto lineText = line.textWithNewlines();
#ifdef ENABLE_TEXT_ENHANCE
        lineText = textRangeMergeBtoA(lineText, line.getTextRangeReplacedByEllipsis());
#endif
        auto intersect = lineText * text;
        if (intersect.empty() && lineText.start != text.start) {
            continue;
        }

        line.getRectsForRange(intersect, rectHeightStyle, rectWidthStyle, results);
    }
/*
    SkDebugf("getRectsForRange(%d, %d)\n", start, end);
    for (auto& r : results) {
        r.rect.fLeft = littleRound(r.rect.fLeft);
        r.rect.fRight = littleRound(r.rect.fRight);
        r.rect.fTop = littleRound(r.rect.fTop);
        r.rect.fBottom = littleRound(r.rect.fBottom);
        SkDebugf("[%f:%f * %f:%f]\n", r.rect.fLeft, r.rect.fRight, r.rect.fTop, r.rect.fBottom);
    }
*/
    return results;
}

std::vector<TextBox> ParagraphImpl::getRectsForPlaceholders() {
  std::vector<TextBox> boxes;
#ifdef ENABLE_TEXT_ENHANCE
  // this method should not be called before kshaped
    if (fText.isEmpty() || fState < kShaped) {
#else
    if (fText.isEmpty()) {
#endif
       return boxes;
  }
  if (fPlaceholders.size() == 1) {
       // We always have one fake placeholder
       return boxes;
  }
  for (auto& line : fLines) {
      line.getRectsForPlaceholders(boxes);
  }
  /*
  SkDebugf("getRectsForPlaceholders('%s'): %d\n", fText.c_str(), boxes.size());
  for (auto& r : boxes) {
      r.rect.fLeft = littleRound(r.rect.fLeft);
      r.rect.fRight = littleRound(r.rect.fRight);
      r.rect.fTop = littleRound(r.rect.fTop);
      r.rect.fBottom = littleRound(r.rect.fBottom);
      SkDebugf("[%f:%f * %f:%f] %s\n", r.rect.fLeft, r.rect.fRight, r.rect.fTop, r.rect.fBottom,
               (r.direction == TextDirection::kLtr ? "left" : "right"));
  }
  */
  return boxes;
}

// TODO: Optimize (save cluster <-> codepoint connection)
PositionWithAffinity ParagraphImpl::getGlyphPositionAtCoordinate(SkScalar dx, SkScalar dy) {

    if (fText.isEmpty()) {
        return {0, Affinity::kDownstream};
    }

    this->ensureUTF16Mapping();

    for (auto& line : fLines) {
        // Let's figure out if we can stop looking
        auto offsetY = line.offset().fY;
        if (dy >= offsetY + line.height() && &line != &fLines.back()) {
            // This line is not good enough
            continue;
        }

        // This is so far the the line vertically closest to our coordinates
        // (or the first one, or the only one - all the same)

        auto result = line.getGlyphPositionAtCoordinate(dx);
        //SkDebugf("getGlyphPositionAtCoordinate(%f, %f): %d %s\n", dx, dy, result.position,
        //   result.affinity == Affinity::kUpstream ? "up" : "down");
        return result;
    }

    return {0, Affinity::kDownstream};
}

// Finds the first and last glyphs that define a word containing
// the glyph at index offset.
// By "glyph" they mean a character index - indicated by Minikin's code
SkRange<size_t> ParagraphImpl::getWordBoundary(unsigned offset) {

    if (fWords.empty()) {
        if (!fUnicode->getWords(fText.c_str(), fText.size(), nullptr, &fWords)) {
            return {0, 0 };
        }
    }

    int32_t start = 0;
    int32_t end = 0;
    for (size_t i = 0; i < fWords.size(); ++i) {
        auto word = fWords[i];
        if (word <= offset) {
            start = word;
            end = word;
        } else if (word > offset) {
            end = word;
            break;
        }
    }

    //SkDebugf("getWordBoundary(%d): %d - %d\n", offset, start, end);
    return { SkToU32(start), SkToU32(end) };
}

void ParagraphImpl::getLineMetrics(std::vector<LineMetrics>& metrics) {
    metrics.clear();
    for (auto& line : fLines) {
        metrics.emplace_back(line.getMetrics());
    }
}

SkSpan<const char> ParagraphImpl::text(TextRange textRange) {
    SkASSERT(textRange.start <= fText.size() && textRange.end <= fText.size());
    auto start = fText.c_str() + textRange.start;
    return SkSpan<const char>(start, textRange.width());
}

SkSpan<Cluster> ParagraphImpl::clusters(ClusterRange clusterRange) {
    SkASSERT(clusterRange.start < SkToSizeT(fClusters.size()) &&
             clusterRange.end <= SkToSizeT(fClusters.size()));
    return SkSpan<Cluster>(&fClusters[clusterRange.start], clusterRange.width());
}

Cluster& ParagraphImpl::cluster(ClusterIndex clusterIndex) {
    SkASSERT(clusterIndex < SkToSizeT(fClusters.size()));
    return fClusters[clusterIndex];
}

Run& ParagraphImpl::runByCluster(ClusterIndex clusterIndex) {
    auto start = cluster(clusterIndex);
    return this->run(start.fRunIndex);
}

SkSpan<Block> ParagraphImpl::blocks(BlockRange blockRange) {
    SkASSERT(blockRange.start < SkToSizeT(fTextStyles.size()) &&
             blockRange.end <= SkToSizeT(fTextStyles.size()));
    return SkSpan<Block>(&fTextStyles[blockRange.start], blockRange.width());
}

Block& ParagraphImpl::block(BlockIndex blockIndex) {
    SkASSERT(blockIndex < SkToSizeT(fTextStyles.size()));
    return fTextStyles[blockIndex];
}

void ParagraphImpl::setState(InternalState state) {
    if (fState <= state) {
        fState = state;
        return;
    }

    fState = state;
    switch (fState) {
        case kUnknown:
            SkASSERT(false);
            /*
            // The text is immutable and so are all the text indexing properties
            // taken from SkUnicode
            fCodeUnitProperties.reset();
            fWords.clear();
            fBidiRegions.clear();
            fUTF8IndexForUTF16Index.reset();
            fUTF16IndexForUTF8Index.reset();
            */
            [[fallthrough]];

        case kIndexed:
            fRuns.clear();
            fClusters.clear();
            [[fallthrough]];

        case kShaped:
            fLines.clear();
            [[fallthrough]];

        case kLineBroken:
            fPicture = nullptr;
            [[fallthrough]];

        default:
            break;
    }
}

void ParagraphImpl::computeEmptyMetrics() {

    // The empty metrics is used to define the height of the empty lines
    // Unfortunately, Flutter has 2 different cases for that:
    // 1. An empty line inside the text
    // 2. An empty paragraph
    // In the first case SkParagraph takes the metrics from the default paragraph style
    // In the second case it should take it from the current text style
    bool emptyParagraph = fRuns.empty();
    TextStyle textStyle = paragraphStyle().getTextStyle();
    if (emptyParagraph && !fTextStyles.empty()) {
        textStyle = fTextStyles.back().fStyle;
    }

    auto typefaces = fontCollection()->findTypefaces(
      textStyle.getFontFamilies(), textStyle.getFontStyle(), textStyle.getFontArguments());
    auto typeface = typefaces.empty() ? nullptr : typefaces.front();

#ifdef ENABLE_TEXT_ENHANCE
    RSFont font(typeface, textStyle.getFontSize(), 1, 0);
#else
    SkFont font(typeface, textStyle.getFontSize());
#endif
    fEmptyMetrics = InternalLineMetrics(font, paragraphStyle().getStrutStyle().getForceStrutHeight());

    if (!paragraphStyle().getStrutStyle().getForceStrutHeight() &&
        textStyle.getHeightOverride()) {
#ifdef ENABLE_TEXT_ENHANCE
        const auto intrinsicHeight = fEmptyMetrics.fDescent - fEmptyMetrics.fAscent + fEmptyMetrics.fLeading;
#else
        const auto intrinsicHeight = fEmptyMetrics.height();
#endif
        const auto strutHeight = textStyle.getHeight() * textStyle.getFontSize();
        if (paragraphStyle().getStrutStyle().getHalfLeading()) {
            fEmptyMetrics.update(
                fEmptyMetrics.ascent(),
                fEmptyMetrics.descent(),
                fEmptyMetrics.leading() + strutHeight - intrinsicHeight);
        } else {
            const auto multiplier = strutHeight / intrinsicHeight;
            fEmptyMetrics.update(
                fEmptyMetrics.ascent() * multiplier,
                fEmptyMetrics.descent() * multiplier,
                fEmptyMetrics.leading() * multiplier);
        }
    }

    if (emptyParagraph) {
        // For an empty text we apply both TextHeightBehaviour flags
        // In case of non-empty paragraph TextHeightBehaviour flags will be applied at the appropriate place
        // We have to do it here because we skip wrapping for an empty text
        auto disableFirstAscent = (paragraphStyle().getTextHeightBehavior() & TextHeightBehavior::kDisableFirstAscent) == TextHeightBehavior::kDisableFirstAscent;
        auto disableLastDescent = (paragraphStyle().getTextHeightBehavior() & TextHeightBehavior::kDisableLastDescent) == TextHeightBehavior::kDisableLastDescent;
        fEmptyMetrics.update(
            disableFirstAscent ? fEmptyMetrics.rawAscent() : fEmptyMetrics.ascent(),
            disableLastDescent ? fEmptyMetrics.rawDescent() : fEmptyMetrics.descent(),
            fEmptyMetrics.leading());
    }

    if (fParagraphStyle.getStrutStyle().getStrutEnabled()) {
        fStrutMetrics.updateLineMetrics(fEmptyMetrics);
    }
}

SkString ParagraphImpl::getEllipsis() const {

    auto ellipsis8 = fParagraphStyle.getEllipsis();
    auto ellipsis16 = fParagraphStyle.getEllipsisUtf16();
    if (!ellipsis8.isEmpty()) {
        return ellipsis8;
    } else {
        return SkUnicode::convertUtf16ToUtf8(fParagraphStyle.getEllipsisUtf16());
    }
}

#ifdef ENABLE_TEXT_ENHANCE
WordBreakType ParagraphImpl::getWordBreakType() const {
    return fParagraphStyle.getStrutStyle().getWordBreakType();
}

LineBreakStrategy ParagraphImpl::getLineBreakStrategy() const {
    return fParagraphStyle.getStrutStyle().getLineBreakStrategy();
}
#endif

void ParagraphImpl::updateFontSize(size_t from, size_t to, SkScalar fontSize) {

  SkASSERT(from == 0 && to == fText.size());
  auto defaultStyle = fParagraphStyle.getTextStyle();
  defaultStyle.setFontSize(fontSize);
  fParagraphStyle.setTextStyle(defaultStyle);

  for (auto& textStyle : fTextStyles) {
    textStyle.fStyle.setFontSize(fontSize);
  }

  fState = std::min(fState, kIndexed);
  fOldWidth = 0;
  fOldHeight = 0;
}

void ParagraphImpl::updateTextAlign(TextAlign textAlign) {
    fParagraphStyle.setTextAlign(textAlign);

    if (fState >= kLineBroken) {
        fState = kLineBroken;
    }
}

void ParagraphImpl::updateForegroundPaint(size_t from, size_t to, SkPaint paint) {
    SkASSERT(from == 0 && to == fText.size());
    auto defaultStyle = fParagraphStyle.getTextStyle();
    defaultStyle.setForegroundColor(paint);
    fParagraphStyle.setTextStyle(defaultStyle);

    for (auto& textStyle : fTextStyles) {
        textStyle.fStyle.setForegroundColor(paint);
    }
}

void ParagraphImpl::updateBackgroundPaint(size_t from, size_t to, SkPaint paint) {
    SkASSERT(from == 0 && to == fText.size());
    auto defaultStyle = fParagraphStyle.getTextStyle();
    defaultStyle.setBackgroundColor(paint);
    fParagraphStyle.setTextStyle(defaultStyle);

    for (auto& textStyle : fTextStyles) {
        textStyle.fStyle.setBackgroundColor(paint);
    }
}

#ifdef ENABLE_TEXT_ENHANCE
ParagraphPainter::PaintID ParagraphImpl::updateTextStyleColorAndForeground(TextStyle& textStyle, SkColor color)
{
    textStyle.setColor(color);
    if (textStyle.hasForeground()) {
        auto paintOrID = textStyle.getForegroundPaintOrID();
        SkPaint* paint = std::get_if<SkPaint>(&paintOrID);
        if (paint) {
            paint->setColor(color);
            textStyle.setForegroundPaint(*paint);
        } else {
            auto paintID = std::get_if<ParagraphPainter::PaintID>(&paintOrID);
            if (paintID) {
                return *paintID;
            }
        }
    }
    return INVALID_PAINT_ID;
}

std::vector<ParagraphPainter::PaintID> ParagraphImpl::updateColor(size_t from, size_t to, SkColor color,
    UtfEncodeType encodeType) {
    std::vector<ParagraphPainter::PaintID> unresolvedPaintID;
    if (from >= to) {
        return unresolvedPaintID;
    }
    this->ensureUTF16Mapping();
    if (encodeType == UtfEncodeType::kUtf8) {
        from = (from < SkToSizeT(fUTF8IndexForUTF16Index.size())) ? fUTF8IndexForUTF16Index[from] : fText.size();
        to = (to < SkToSizeT(fUTF8IndexForUTF16Index.size())) ? fUTF8IndexForUTF16Index[to] : fText.size();
    }
    if (from == 0 && to == fText.size()) {
        auto defaultStyle = fParagraphStyle.getTextStyle();
        auto paintID = updateTextStyleColorAndForeground(defaultStyle, color);
        if (paintID != INVALID_PAINT_ID) {
            unresolvedPaintID.emplace_back(paintID);
        }
        fParagraphStyle.setTextStyle(defaultStyle);
    }

    for (auto& textStyle : fTextStyles) {
        auto& fRange = textStyle.fRange;
        if (to < fRange.end) {
            break;
        }
        if (from > fRange.start) {
            continue;
        }
        auto paintID = updateTextStyleColorAndForeground(textStyle.fStyle, color);
        if (paintID != INVALID_PAINT_ID) {
            unresolvedPaintID.emplace_back(paintID);
        }
    }
    for (auto& line : fLines) {
        line.setTextBlobCachePopulated(false);
    }
    return unresolvedPaintID;
}

bool ParagraphImpl::isAutoSpaceEnabled() const
{
    return paragraphStyle().getEnableAutoSpace() || TextParameter::GetAutoSpacingEnable();
}

SkScalar ParagraphImpl::clusterUsingAutoSpaceWidth(const Cluster& cluster) const
{
    if(!isAutoSpaceEnabled()){
        return cluster.width();
    }
    Run& run = cluster.run();
    size_t start = cluster.startPos();
    size_t end = cluster.endPos();
    float correction = 0.0f;
    if (end > start && !run.getAutoSpacings().empty()) {
        correction = run.getAutoSpacings()[end - 1].fX - run.getAutoSpacings()[start].fY;
    }

    return cluster.width() + std::max(0.0f, correction);
}

bool ParagraphImpl::preCalculateSingleRunAutoSpaceWidth(SkScalar floorWidth)
{
    SkScalar singleRunWidth = fRuns[0].fAdvance.fX;
    if (!isAutoSpaceEnabled()) {
        return singleRunWidth <= floorWidth - this->detectIndents(0);
    }
    SkScalar totalFakeSpacing = 0.0f;
    ClusterIndex endOfClusters = fClusters.size();
    for (size_t cluster = 1; cluster < endOfClusters; ++cluster) {
        totalFakeSpacing += (fClusters[cluster].needAutoSpacing())
            ? fClusters[cluster - 1].getFontSize() / AUTO_SPACING_WIDTH_RATIO : 0;
    }
    singleRunWidth += totalFakeSpacing;
    return singleRunWidth <= floorWidth - this->detectIndents(0);
}

std::vector<TextBlobRecordInfo> ParagraphImpl::getTextBlobRecordInfo()
{
    std::vector<TextBlobRecordInfo> textBlobRecordInfos;
    for (auto& line : fLines) {
        for (auto& block : line.fTextBlobCache) {
            TextBlobRecordInfo recordInfo;
            recordInfo.fBlob = block.fBlob;
            recordInfo.fOffset = block.fOffset;
            recordInfo.fPaint = block.fPaint;
            textBlobRecordInfos.emplace_back(recordInfo);
        }
    }
    return textBlobRecordInfos;
}

bool ParagraphImpl::canPaintAllText() const
{
    for (auto& line : fLines) {
        if (line.ellipsis() != nullptr) {
            return false;
        }
    }
    return !fExceededMaxLines;
}
#endif
TArray<TextIndex> ParagraphImpl::countSurroundingGraphemes(TextRange textRange) const {
    textRange = textRange.intersection({0, fText.size()});
    TArray<TextIndex> graphemes;
    if ((fCodeUnitProperties[textRange.start] & SkUnicode::CodeUnitFlags::kGraphemeStart) == 0) {
        // Count the previous partial grapheme
        graphemes.emplace_back(textRange.start);
    }
    for (auto index = textRange.start; index < textRange.end; ++index) {
        if ((fCodeUnitProperties[index] & SkUnicode::CodeUnitFlags::kGraphemeStart) != 0) {
            graphemes.emplace_back(index);
        }
    }
    return graphemes;
}

TextIndex ParagraphImpl::findPreviousGraphemeBoundary(TextIndex utf8) const {
    while (utf8 > 0 &&
          (fCodeUnitProperties[utf8] & SkUnicode::CodeUnitFlags::kGraphemeStart) == 0) {
        --utf8;
    }
    return utf8;
}

TextIndex ParagraphImpl::findNextGraphemeBoundary(TextIndex utf8) const {
    while (utf8 < fText.size() &&
          (fCodeUnitProperties[utf8] & SkUnicode::CodeUnitFlags::kGraphemeStart) == 0) {
        ++utf8;
    }
    return utf8;
}

TextIndex ParagraphImpl::findNextGlyphClusterBoundary(TextIndex utf8) const {
    while (utf8 < fText.size() &&
          (fCodeUnitProperties[utf8] & SkUnicode::CodeUnitFlags::kGlyphClusterStart) == 0) {
        ++utf8;
    }
    return utf8;
}

TextIndex ParagraphImpl::findPreviousGlyphClusterBoundary(TextIndex utf8) const {
    while (utf8 > 0 &&
          (fCodeUnitProperties[utf8] & SkUnicode::CodeUnitFlags::kGlyphClusterStart) == 0) {
        --utf8;
    }
    return utf8;
}

void ParagraphImpl::ensureUTF16Mapping() {
    fillUTF16MappingOnce([&] {
        SkUnicode::extractUtfConversionMapping(
                this->text(),
                [&](size_t index) { fUTF8IndexForUTF16Index.emplace_back(index); },
                [&](size_t index) { fUTF16IndexForUTF8Index.emplace_back(index); });
    });
}

void ParagraphImpl::visit(const Visitor& visitor) {
#ifndef ENABLE_TEXT_ENHANCE
    int lineNumber = 0;
    for (auto& line : fLines) {
        line.ensureTextBlobCachePopulated();
        for (auto& rec : line.fTextBlobCache) {
            if (rec.fBlob == nullptr) {
                continue;
            }
            SkTextBlob::Iter iter(*rec.fBlob);
            SkTextBlob::Iter::ExperimentalRun run;

            STArray<128, uint32_t> clusterStorage;
            const Run* R = rec.fVisitor_Run;
            const uint32_t* clusterPtr = &R->fClusterIndexes[0];

            if (R->fClusterStart > 0) {
                int count = R->fClusterIndexes.size();
                clusterStorage.reset(count);
                for (int i = 0; i < count; ++i) {
                    clusterStorage[i] = R->fClusterStart + R->fClusterIndexes[i];
                }
                clusterPtr = &clusterStorage[0];
            }
            clusterPtr += rec.fVisitor_Pos;

            while (iter.experimentalNext(&run)) {
                const Paragraph::VisitorInfo info = {
                    run.font,
                    rec.fOffset,
                    rec.fClipRect.fRight,
                    run.count,
                    run.glyphs,
                    run.positions,
                    clusterPtr,
                    0,  // flags
                };
                visitor(lineNumber, &info);
                clusterPtr += run.count;
            }
        }
        visitor(lineNumber, nullptr);   // signal end of line
        lineNumber += 1;
    }
#endif
}

#ifdef ENABLE_TEXT_ENHANCE
int ParagraphImpl::getLineNumberAt(TextIndex codeUnitIndex) const {
    for (size_t i = 0; i < fLines.size(); ++i) {
        auto& line = fLines[i];
        if (line.text().contains({codeUnitIndex, codeUnitIndex + 1})) {
            return i;
        }
    }
    return -1;
}
#else
int ParagraphImpl::getLineNumberAt(TextIndex codeUnitIndex) const {
    if (codeUnitIndex >= fText.size()) {
        return -1;
    }
    size_t startLine = 0;
    size_t endLine = fLines.size() - 1;
    if (fLines.empty() || fLines[endLine].textWithNewlines().end <= codeUnitIndex) {
        return -1;
    }

    while (endLine > startLine) {
        // startLine + 1 <= endLine, so we have startLine <= midLine <= endLine - 1.
        const size_t midLine = (endLine + startLine) / 2;
        const TextRange midLineRange = fLines[midLine].textWithNewlines();
        if (codeUnitIndex < midLineRange.start) {
            endLine = midLine - 1;
        } else if (midLineRange.end <= codeUnitIndex) {
            startLine = midLine + 1;
        } else {
            return midLine;
        }
    }
    SkASSERT(startLine == endLine);
    return startLine;
}
#endif

int ParagraphImpl::getLineNumberAtUTF16Offset(size_t codeUnitIndex) {
    this->ensureUTF16Mapping();
    if (codeUnitIndex >= SkToSizeT(fUTF8IndexForUTF16Index.size())) {
        return -1;
    }
    const TextIndex utf8 = fUTF8IndexForUTF16Index[codeUnitIndex];
    return getLineNumberAt(utf8);
}

bool ParagraphImpl::getLineMetricsAt(int lineNumber, LineMetrics* lineMetrics) const {
    if (lineNumber < 0 || lineNumber >= fLines.size()) {
        return false;
    }
    auto& line = fLines[lineNumber];
    if (lineMetrics) {
        *lineMetrics = line.getMetrics();
    }
    return true;
}

TextRange ParagraphImpl::getActualTextRange(int lineNumber, bool includeSpaces) const {
    if (lineNumber < 0 || lineNumber >= fLines.size()) {
#ifdef ENABLE_TEXT_ENHANCE
        return {0, 0};
#else
        return EMPTY_TEXT;
#endif
    }
    auto& line = fLines[lineNumber];
    return includeSpaces ? line.text() : line.trimmedText();
}

#ifdef ENABLE_TEXT_ENHANCE
bool ParagraphImpl::getGlyphClusterAt(TextIndex codeUnitIndex, GlyphClusterInfo* glyphInfo) {
    for (size_t i = 0; i < fLines.size(); ++i) {
        auto& line = fLines[i];
        if (!line.text().contains({codeUnitIndex, codeUnitIndex})) {
            continue;
        }
        for (auto c = line.clustersWithSpaces().start; c < line.clustersWithSpaces().end; ++c) {
            auto& cluster = fClusters[c];
            if (cluster.contains(codeUnitIndex)) {
                std::vector<TextBox> boxes;
                line.getRectsForRange(cluster.textRange(),
                                      RectHeightStyle::kTight,
                                      RectWidthStyle::kTight,
                                      boxes);
                if (boxes.size() > 0) {
                    if (glyphInfo) {
                        *glyphInfo = {boxes[0].rect, cluster.textRange(), boxes[0].direction};
                    }
                    return true;
                }
            }
        }
        return false;
    }
    return false;
}
#else
bool ParagraphImpl::getGlyphClusterAt(TextIndex codeUnitIndex, GlyphClusterInfo* glyphInfo) {
    const int lineNumber = getLineNumberAt(codeUnitIndex);
    if (lineNumber == -1) {
        return false;
    }
    auto& line = fLines[lineNumber];
    for (auto c = line.clustersWithSpaces().start; c < line.clustersWithSpaces().end; ++c) {
        auto& cluster = fClusters[c];
        if (cluster.contains(codeUnitIndex)) {
            std::vector<TextBox> boxes;
            line.getRectsForRange(cluster.textRange(),
                                    RectHeightStyle::kTight,
                                    RectWidthStyle::kTight,
                                    boxes);
            if (!boxes.empty()) {
                if (glyphInfo) {
                    *glyphInfo = {boxes[0].rect, cluster.textRange(), boxes[0].direction};
                }
                return true;
            }
        }
    }
    return false;
}
#endif

#ifdef ENABLE_TEXT_ENHANCE
bool ParagraphImpl::getClosestGlyphClusterAt(SkScalar dx,
                                             SkScalar dy,
                                             GlyphClusterInfo* glyphInfo) {
    auto res = this->getGlyphPositionAtCoordinate(dx, dy);
    auto textIndex = res.position + (res.affinity == Affinity::kDownstream ? 0 : 1);
    GlyphClusterInfo gci;
    if (this->getGlyphClusterAt(textIndex, glyphInfo ? glyphInfo : &gci)) {
        return true;
    } else {
        return false;
    }
}
#else
bool ParagraphImpl::getClosestGlyphClusterAt(SkScalar dx,
                                             SkScalar dy,
                                             GlyphClusterInfo* glyphInfo) {
    const PositionWithAffinity res = this->getGlyphPositionAtCoordinate(dx, dy);
    SkASSERT(res.position != 0 || res.affinity != Affinity::kUpstream);
    const size_t utf16Offset = res.position + (res.affinity == Affinity::kDownstream ? 0 : -1);
    this->ensureUTF16Mapping();
    SkASSERT(utf16Offset < SkToSizeT(fUTF8IndexForUTF16Index.size()));
    return this->getGlyphClusterAt(fUTF8IndexForUTF16Index[utf16Offset], glyphInfo);
}
#endif

bool ParagraphImpl::getGlyphInfoAtUTF16Offset(size_t codeUnitIndex, GlyphInfo* glyphInfo) {
    this->ensureUTF16Mapping();
    if (codeUnitIndex >= SkToSizeT(fUTF8IndexForUTF16Index.size())) {
        return false;
    }
    const TextIndex utf8 = fUTF8IndexForUTF16Index[codeUnitIndex];
    const int lineNumber = getLineNumberAt(utf8);
    if (lineNumber == -1) {
        return false;
    }
    if (glyphInfo == nullptr) {
        return true;
    }
    const TextLine& line = fLines[lineNumber];
    const TextIndex startIndex = findPreviousGraphemeBoundary(utf8);
    const TextIndex endIndex = findNextGraphemeBoundary(utf8 + 1);
    const ClusterIndex glyphClusterIndex = clusterIndex(utf8);
    const Cluster& glyphCluster = cluster(glyphClusterIndex);

    // `startIndex` and `endIndex` must be on the same line.
    std::vector<TextBox> boxes;
    line.getRectsForRange({startIndex, endIndex}, RectHeightStyle::kTight, RectWidthStyle::kTight, boxes);
    // TODO: currently placeholders with height=0 and width=0 are ignored so boxes
    // can be empty. These placeholders should still be reported for their
    // offset information.
    if (glyphInfo && !boxes.empty()) {
        *glyphInfo = {
            boxes[0].rect,
            { fUTF16IndexForUTF8Index[startIndex], fUTF16IndexForUTF8Index[endIndex] },
            boxes[0].direction,
            glyphCluster.run().isEllipsis(),
        };
    }
    return true;
}

bool ParagraphImpl::getClosestUTF16GlyphInfoAt(SkScalar dx, SkScalar dy, GlyphInfo* glyphInfo) {
    const PositionWithAffinity res = this->getGlyphPositionAtCoordinate(dx, dy);
    SkASSERT(res.position != 0 || res.affinity != Affinity::kUpstream);
    const size_t utf16Offset = res.position + (res.affinity == Affinity::kDownstream ? 0 : -1);
    return getGlyphInfoAtUTF16Offset(utf16Offset, glyphInfo);
}

#ifdef ENABLE_TEXT_ENHANCE
RSFont ParagraphImpl::getFontAt(TextIndex codeUnitIndex) const
{
    for (auto& run : fRuns) {
        const auto textRange = run.textRange();
        if (textRange.start <= codeUnitIndex && codeUnitIndex < textRange.end) {
            return run.font();
        }
    }
    return RSFont();
}
#else
SkFont ParagraphImpl::getFontAt(TextIndex codeUnitIndex) const {
    for (auto& run : fRuns) {
        const auto textRange = run.textRange();
        if (textRange.start <= codeUnitIndex && codeUnitIndex < textRange.end) {
            return run.font();
        }
    }
    return SkFont();
}
#endif

#ifndef ENABLE_TEXT_ENHANCE
SkFont ParagraphImpl::getFontAtUTF16Offset(size_t codeUnitIndex) {
    ensureUTF16Mapping();
    if (codeUnitIndex >= SkToSizeT(fUTF8IndexForUTF16Index.size())) {
        return SkFont();
    }
    const TextIndex utf8 = fUTF8IndexForUTF16Index[codeUnitIndex];
    for (auto& run : fRuns) {
        const auto textRange = run.textRange();
        if (textRange.start <= utf8 && utf8 < textRange.end) {
            return run.font();
        }
    }
    return SkFont();
}
#endif

std::vector<Paragraph::FontInfo> ParagraphImpl::getFonts() const {
    std::vector<FontInfo> results;
    for (auto& run : fRuns) {
        results.emplace_back(run.font(), run.textRange());
    }
    return results;
}

#ifdef ENABLE_TEXT_ENHANCE
RSFontMetrics ParagraphImpl::measureText()
{
    RSFontMetrics metrics;
    if (fRuns.empty()) {
        return metrics;
    }

    auto& firstFont = const_cast<RSFont&>(fRuns.front().font());
    RSRect firstBounds;
    auto firstStr = text(fRuns.front().textRange());
    firstFont.GetMetrics(&metrics);
    auto decompressFont = firstFont;
    scaleFontWithCompressionConfig(decompressFont, ScaleOP::DECOMPRESS);
    metricsIncludeFontPadding(&metrics, decompressFont);
    firstFont.MeasureText(firstStr.data(), firstStr.size(), RSDrawing::TextEncoding::UTF8, &firstBounds);
    fGlyphsBoundsTop = firstBounds.GetTop();
    fGlyphsBoundsBottom = firstBounds.GetBottom();
    fGlyphsBoundsLeft = firstBounds.GetLeft();
    float realWidth = 0;
    for (size_t i = 0; i < fRuns.size(); ++i) {
        auto run = fRuns[i];
        auto& font = const_cast<RSFont&>(run.font());
        RSRect bounds;
        auto str = text(run.textRange());
        auto advance = font.MeasureText(str.data(), str.size(), RSDrawing::TextEncoding::UTF8, &bounds);
        realWidth += advance;
        if (i == 0) {
            realWidth -= ((advance - (bounds.GetRight() - bounds.GetLeft())) / 2);
        }
        if (i == (fRuns.size() - 1)) {
            realWidth -= ((advance - (bounds.GetRight() - bounds.GetLeft())) / 2);
        }
        fGlyphsBoundsTop = std::min(fGlyphsBoundsTop, bounds.GetTop());
        fGlyphsBoundsBottom = std::max(fGlyphsBoundsBottom, bounds.GetBottom());
    }
    fGlyphsBoundsRight = realWidth + fGlyphsBoundsLeft;
    return metrics;
}

std::vector<std::unique_ptr<TextLineBase>> ParagraphImpl::GetTextLines() {
    std::vector<std::unique_ptr<TextLineBase>> textLineBases;
    for (auto& line: fLines) {
        std::unique_ptr<TextLineBaseImpl> textLineBaseImplPtr =
            std::make_unique<TextLineBaseImpl>(std::make_unique<TextLine>(std::move(line)));
        textLineBases.emplace_back(std::move(textLineBaseImplPtr));
    }

    return textLineBases;
}

size_t ParagraphImpl::prefixByteCountUntilChar(size_t index) {
    convertUtf8ToUnicode(fText);
    if (fUnicodeIndexForUTF8Index.empty()) {
        return std::numeric_limits<size_t>::max();
    }
    auto it = std::lower_bound(fUnicodeIndexForUTF8Index.begin(), fUnicodeIndexForUTF8Index.end(), index);
    if (it != fUnicodeIndexForUTF8Index.end() && *it == index) {
        return std::distance(fUnicodeIndexForUTF8Index.begin(), it);
    } else {
        return std::numeric_limits<size_t>::max();
    }
}

void ParagraphImpl::copyProperties(const ParagraphImpl& source) {
    fText = source.fText;
    fTextStyles = source.fTextStyles;
    fPlaceholders = source.fPlaceholders;
    fParagraphStyle = source.fParagraphStyle;
    fFontCollection = source.fFontCollection;
    fUnicode = source.fUnicode;

    fState = kUnknown;
    fUnresolvedGlyphs = 0;
    fPicture = nullptr;
    fStrutMetrics = false;
    fOldWidth = 0;
    fOldHeight = 0;
    fHasLineBreaks = false;
    fHasWhitespacesInside = false;
    fTrailingSpaces = 0;
}

std::unique_ptr<Paragraph> ParagraphImpl::createCroppedCopy(size_t startIndex, size_t count) {
    std::unique_ptr<ParagraphImpl> paragraph = std::make_unique<ParagraphImpl>();
    paragraph->copyProperties(*this);

    // change range
    auto validStart = prefixByteCountUntilChar(startIndex);
    if (validStart == std::numeric_limits<size_t>::max()) {
        return nullptr;
    }
    // For example, when the clipped string str1 is "123456789"
    // startIndex=2, count=std:: numeric_imits<size_t>:: max(), the resulting string str2 is "3456789".
    // When startIndex=3 and count=3, crop the generated string str3 to "456"
    TextRange firstDeleteRange(0, validStart);
    paragraph->fText.remove(0, validStart);
    paragraph->resetTextStyleRange(firstDeleteRange);
    paragraph->resetPlaceholderRange(firstDeleteRange);

    if (count != std::numeric_limits<size_t>::max()) {
        auto invalidStart = paragraph->prefixByteCountUntilChar(count);
        if (invalidStart == std::numeric_limits<size_t>::max()) {
            return nullptr;
        }
        auto invalidEnd = paragraph->fText.size();
        TextRange secodeDeleteRange(invalidStart, invalidEnd);
        paragraph->fText.remove(invalidStart, invalidEnd - invalidStart);
        paragraph->resetTextStyleRange(secodeDeleteRange);
        paragraph->resetPlaceholderRange(secodeDeleteRange);
    }
    return paragraph;
}

void ParagraphImpl::initUnicodeText() {
    this->fUnicodeText = convertUtf8ToUnicode(fText);
}

// Currently, only support to generate text and text shadow paint regions.
// Can't accurately calculate the paint region of italic fonts(including fake italic).
SkIRect ParagraphImpl::generatePaintRegion(SkScalar x, SkScalar y) {
    if (fState < kFormatted) {
        TEXT_LOGW("Call generatePaintRegion when paragraph is not formatted");
        return SkIRect::MakeXYWH(x, y, 0, 0);
    }

    if (fPaintRegion.has_value()) {
        return fPaintRegion.value().makeOffset(x, y).roundOut();
    }

    fPaintRegion = SkRect::MakeEmpty();
    for (auto& line : fLines) {
        SkRect linePaintRegion = line.generatePaintRegion(0, 0);
        fPaintRegion->join(linePaintRegion);
    }
    return fPaintRegion.value().makeOffset(x, y).roundOut();
}

std::unique_ptr<Paragraph> ParagraphImpl::CloneSelf()
{
    std::unique_ptr<ParagraphImpl> paragraph = std::make_unique<ParagraphImpl>();

    paragraph->fFontCollection = this->fFontCollection;
    paragraph->fParagraphStyle = this->fParagraphStyle;
    paragraph->fAlphabeticBaseline = this->fAlphabeticBaseline;
    paragraph->fIdeographicBaseline = this->fIdeographicBaseline;
    paragraph->fGlyphsBoundsTop = this->fGlyphsBoundsTop;
    paragraph->fGlyphsBoundsBottom = this->fGlyphsBoundsBottom;
    paragraph->fGlyphsBoundsLeft = this->fGlyphsBoundsLeft;
    paragraph->fGlyphsBoundsRight = this->fGlyphsBoundsRight;
    paragraph->fHeight = this->fHeight;
    paragraph->fWidth = this->fWidth;
    paragraph->fMaxIntrinsicWidth = this->fMaxIntrinsicWidth;
    paragraph->fMinIntrinsicWidth = this->fMinIntrinsicWidth;
    paragraph->fLongestLine = this->fLongestLine;
    paragraph->fLongestLineWithIndent = this->fLongestLineWithIndent;
    paragraph->fExceededMaxLines = this->fExceededMaxLines;

    paragraph->fLetterSpaceStyles = this->fLetterSpaceStyles;
    paragraph->fWordSpaceStyles = this->fWordSpaceStyles;
    paragraph->fBackgroundStyles = this->fBackgroundStyles;
    paragraph->fForegroundStyles = this->fForegroundStyles;
    paragraph->fShadowStyles = this->fShadowStyles;
    paragraph->fDecorationStyles = this->fDecorationStyles;
    paragraph->fTextStyles = this->fTextStyles;
    paragraph->fPlaceholders = this->fPlaceholders;
    paragraph->fText = this->fText;

    paragraph->fState = this->fState;
    paragraph->fRuns = this->fRuns;
    paragraph->fClusters = this->fClusters;
    paragraph->fCodeUnitProperties = this->fCodeUnitProperties;
    paragraph->fClustersIndexFromCodeUnit = this->fClustersIndexFromCodeUnit;

    paragraph->fWords = this->fWords;
    paragraph->fIndents = this->fIndents;
    paragraph->fBidiRegions = this->fBidiRegions;

    paragraph->fUTF8IndexForUTF16Index = this->fUTF8IndexForUTF16Index;
    paragraph->fUTF16IndexForUTF8Index = this->fUTF16IndexForUTF8Index;
    paragraph->fUnresolvedGlyphs = this->fUnresolvedGlyphs;
    paragraph->fUnresolvedCodepoints = this->fUnresolvedCodepoints;

    for (auto& line : this->fLines) {
        paragraph->fLines.emplace_back(line.CloneSelf());
    }

    paragraph->fPicture = this->fPicture;
    paragraph->fFontSwitches = this->fFontSwitches;
    paragraph->fEmptyMetrics = this->fEmptyMetrics;
    paragraph->fStrutMetrics = this->fStrutMetrics;

    paragraph->fOldWidth = this->fOldWidth;
    paragraph->fOldHeight = this->fOldHeight;
    paragraph->fMaxWidthWithTrailingSpaces = this->fMaxWidthWithTrailingSpaces;

    paragraph->fUnicode = this->fUnicode;
    paragraph->fHasLineBreaks = this->fHasLineBreaks;
    paragraph->fHasWhitespacesInside = this->fHasWhitespacesInside;
    paragraph->fTrailingSpaces = this->fTrailingSpaces;
    paragraph->fLineNumber = this->fLineNumber;
    paragraph->fEllipsisRange = this->fEllipsisRange;

    for (auto& run : paragraph->fRuns) {
        run.setOwner(paragraph.get());
    }
    for (auto& cluster : paragraph->fClusters) {
        cluster.setOwner(paragraph.get());
    }
    for (auto& line : paragraph->fLines) {
        line.setParagraphImpl(paragraph.get());
    }
    return paragraph;
}

#endif

#ifndef ENABLE_TEXT_ENHANCE
void ParagraphImpl::extendedVisit(const ExtendedVisitor& visitor) {
    int lineNumber = 0;
    for (auto& line : fLines) {
        line.iterateThroughVisualRuns(
            false,
            [&](const Run* run,
                SkScalar runOffsetInLine,
                TextRange textRange,
                SkScalar* runWidthInLine) {
                *runWidthInLine = line.iterateThroughSingleRunByStyles(
                TextLine::TextAdjustment::GlyphCluster,
                run,
                runOffsetInLine,
                textRange,
                StyleType::kNone,
                [&](TextRange textRange,
                    const TextStyle& style,
                    const TextLine::ClipContext& context) {
                    SkScalar correctedBaseline = SkScalarFloorToScalar(
                        line.baseline() + style.getBaselineShift() + 0.5);
                    SkPoint offset =
                        SkPoint::Make(line.offset().fX + context.fTextShift,
                                      line.offset().fY + correctedBaseline);
                    SkRect rect = context.clip.makeOffset(line.offset());
                    AutoSTArray<16, SkRect> glyphBounds;
                    glyphBounds.reset(SkToInt(run->size()));
                    run->font().getBounds(run->glyphs().data(),
                                          SkToInt(run->size()),
                                          glyphBounds.data(),
                                          nullptr);
                    STArray<128, uint32_t> clusterStorage;
                    const uint32_t* clusterPtr = run->clusterIndexes().data();
                    if (run->fClusterStart > 0) {
                        clusterStorage.reset(context.size);
                        for (size_t i = 0; i < context.size; ++i) {
                          clusterStorage[i] =
                              run->fClusterStart + run->fClusterIndexes[i];
                        }
                        clusterPtr = &clusterStorage[0];
                    }
                    const Paragraph::ExtendedVisitorInfo info = {
                        run->font(),
                        offset,
                        SkSize::Make(rect.width(), rect.height()),
                        SkToS16(context.size),
                        &run->glyphs()[context.pos],
                        &run->fPositions[context.pos],
                        &glyphBounds[context.pos],
                        clusterPtr,
                        0,  // flags
                    };
                    visitor(lineNumber, &info);
                });
            return true;
            });
        visitor(lineNumber, nullptr);   // signal end of line
        lineNumber += 1;
    }
}

int ParagraphImpl::getPath(int lineNumber, SkPath* dest) {
    int notConverted = 0;
    auto& line = fLines[lineNumber];
    line.iterateThroughVisualRuns(
              false,
              [&](const Run* run,
                  SkScalar runOffsetInLine,
                  TextRange textRange,
                  SkScalar* runWidthInLine) {
          *runWidthInLine = line.iterateThroughSingleRunByStyles(
          TextLine::TextAdjustment::GlyphCluster,
          run,
          runOffsetInLine,
          textRange,
          StyleType::kNone,
          [&](TextRange textRange,
              const TextStyle& style,
              const TextLine::ClipContext& context) {
              const SkFont& font = run->font();
              SkScalar correctedBaseline = SkScalarFloorToScalar(
                line.baseline() + style.getBaselineShift() + 0.5);
              SkPoint offset =
                  SkPoint::Make(line.offset().fX + context.fTextShift,
                                line.offset().fY + correctedBaseline);
              SkRect rect = context.clip.makeOffset(offset);
              struct Rec {
                  SkPath* fPath;
                  SkPoint fOffset;
                  const SkPoint* fPos;
                  int fNotConverted;
              } rec =
                  {dest, SkPoint::Make(rect.left(), rect.top()),
                   &run->positions()[context.pos], 0};
              font.getPaths(&run->glyphs()[context.pos], context.size,
                    [](const SkPath* path, const SkMatrix& mx, void* ctx) {
                        Rec* rec = reinterpret_cast<Rec*>(ctx);
                        if (path) {
                            SkMatrix total = mx;
                            total.postTranslate(rec->fPos->fX + rec->fOffset.fX,
                                                rec->fPos->fY + rec->fOffset.fY);
                            rec->fPath->addPath(*path, total);
                        } else {
                            rec->fNotConverted++;
                        }
                        rec->fPos += 1; // move to the next glyph's position
                    }, &rec);
              notConverted += rec.fNotConverted;
          });
        return true;
    });

    return notConverted;
}
#endif

SkPath Paragraph::GetPath(SkTextBlob* textBlob) {
    SkPath path;
    SkTextBlobRunIterator iter(textBlob);
    while (!iter.done()) {
        SkFont font = iter.font();
        struct Rec { SkPath* fDst; SkPoint fOffset; const SkPoint* fPos; } rec =
            {&path, {textBlob->bounds().left(), textBlob->bounds().top()},
             iter.points()};
        font.getPaths(iter.glyphs(), iter.glyphCount(),
            [](const SkPath* src, const SkMatrix& mx, void* ctx) {
                Rec* rec = (Rec*)ctx;
                if (src) {
                    SkMatrix tmp(mx);
                    tmp.postTranslate(rec->fPos->fX - rec->fOffset.fX,
                                      rec->fPos->fY - rec->fOffset.fY);
                    rec->fDst->addPath(*src, tmp);
                }
                rec->fPos += 1;
            },
            &rec);
        iter.next();
    }
    return path;
}

bool ParagraphImpl::containsEmoji(SkTextBlob* textBlob) {
    bool result = false;
    SkTextBlobRunIterator iter(textBlob);
    while (!iter.done() && !result) {
        // Walk through all the text by codepoints
        this->getUnicode()->forEachCodepoint(iter.text(), iter.textSize(),
           [&](SkUnichar unichar, int32_t start, int32_t end, int32_t count) {
                if (this->getUnicode()->isEmoji(unichar)) {
                    result = true;
                }
            });
        iter.next();
    }
    return result;
}

bool ParagraphImpl::containsColorFontOrBitmap(SkTextBlob* textBlob) {
    SkTextBlobRunIterator iter(textBlob);
    bool flag = false;
    while (!iter.done() && !flag) {
        iter.font().getPaths(
            (const SkGlyphID*) iter.glyphs(),
            iter.glyphCount(),
            [](const SkPath* path, const SkMatrix& mx, void* ctx) {
                if (path == nullptr) {
                    bool* flag1 = (bool*)ctx;
                    *flag1 = true;
                }
            }, &flag);
        iter.next();
    }
    return flag;
}

#ifdef ENABLE_TEXT_ENHANCE
std::string_view ParagraphImpl::GetState() const
{
    static std::unordered_map<InternalState, std::string_view> state = {
        {kUnknown, "Unknow"},
        {kIndexed, "Indexed"},
        {kShaped, "Shaped"},
        {kLineBroken, "LineBroken"},
        {kFormatted, "Formatted"},
        {kDrawn, "Drawn"},
    };
    return state[fState];
}

std::string ParagraphImpl::GetDumpInfo() const
{
    std::ostringstream paragraphInfo;
    paragraphInfo << "Paragraph dump:";
    paragraphInfo << "Text sz:" << fText.size() << ",State:" << GetState()
                  << ",TextDraw:" << (fSkipTextBlobDrawing ? "T" : "F") << ",";
    uint32_t glyphSize = 0;
    uint32_t runIndex = 0;
    for (auto& run : fRuns) {
        paragraphInfo << "Run" << runIndex << " glyph sz:" << run.size()
                      << ",rng[" << run.textRange().start << "-" << run.textRange().end << "),";
        runIndex++;
        glyphSize += run.size();
    }
    uint32_t blockIndex = 0;
    for (auto& block : fTextStyles) {
        paragraphInfo << "Blk" << blockIndex
                      << " rng[" << block.fRange.start << "-"<< block.fRange.end << ")"
                      << ",sz:" << block.fStyle.getFontSize()
                      << ",clr:" << std::hex << block.fStyle.getColor() << std::dec
                      << ",ht:" << block.fStyle.getHeight()
                      << ",wt:" << block.fStyle.getFontStyle().GetWeight()
                      << ",wd:" << block.fStyle.getFontStyle().GetWidth()
                      << ",slt:" << block.fStyle.getFontStyle().GetSlant() << ",";
        blockIndex++;
    }
    paragraphInfo << "Paragraph glyph sz:" << glyphSize << ",";
    uint32_t lineIndex = 0;
    for (auto& line : fLines) {
        if (lineIndex > 0) {
            paragraphInfo << ",";
        }
        auto runs = line.getLineAllRuns();
        auto runSize = runs.size();
        if (runSize !=0 ) {
            paragraphInfo << "L" << lineIndex << " run rng:" << runs[0] << "-" << runs[runSize - 1];
        }
        lineIndex++;
    }
    return paragraphInfo.str();
}
#endif

}  // namespace textlayout
}  // namespace skia
