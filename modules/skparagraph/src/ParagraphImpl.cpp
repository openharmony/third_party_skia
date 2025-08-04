// Copyright 2019 Google LLC.

#include "include/core/SkCanvas.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkSpan.h"
#include "include/core/SkTypeface.h"
#include "include/private/SkTFitsIn.h"
#include "include/private/SkTo.h"
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
#ifdef OHOS_SUPPORT
#include "trace.h"
#endif
#include "src/utils/SkUTF.h"
#include <math.h>
#include <algorithm>
#include <utility>

#ifdef OHOS_SUPPORT
#include "log.h"
#include "include/TextGlobalConfig.h"
#include "modules/skparagraph/src/TextLineBaseImpl.h"
#include "TextParameter.h"
#endif

namespace skia {
namespace textlayout {

namespace {
constexpr ParagraphPainter::PaintID INVALID_PAINT_ID = -1;

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

TextRange textRangeMergeBtoA(const TextRange& a, const TextRange& b) {
    if (a.width() <= 0 || b.width() <= 0 || a.end < b.start || a.start > b.end) {
        return a;
    }

    return TextRange(std::min(a.start, b.start), std::max(a.end, b.end));
}

#ifdef OHOS_SUPPORT
std::vector<SkUnichar> ParagraphImpl::convertUtf8ToUnicode(const SkString& utf8)
{
    fUnicodeIndexForUTF8Index.reset();
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
#ifdef OHOS_SUPPORT
            , fLongestLineWithIndent(0)
#endif
            , fExceededMaxLines(0)
{ }

ParagraphImpl::ParagraphImpl(const SkString& text,
                             ParagraphStyle style,
                             SkTArray<Block, true> blocks,
                             SkTArray<Placeholder, true> placeholders,
                             sk_sp<FontCollection> fonts,
                             std::shared_ptr<SkUnicode> unicode)
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
                             SkTArray<Block, true> blocks,
                             SkTArray<Placeholder, true> placeholders,
                             sk_sp<FontCollection> fonts,
                             std::shared_ptr<SkUnicode> unicode)
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

#ifndef USE_SKIA_TXT
bool ParagraphImpl::GetLineFontMetrics(const size_t lineNumber, size_t& charNumber,
    std::vector<SkFontMetrics>& fontMetrics) {
#else
bool ParagraphImpl::GetLineFontMetrics(const size_t lineNumber, size_t& charNumber,
    std::vector<RSFontMetrics>& fontMetrics) {
#endif
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
#ifndef USE_SKIA_TXT
            SkFontMetrics newFontMetrics;
            targetRun.fFont.getMetrics(&newFontMetrics);
#else
            RSFontMetrics newFontMetrics;
            targetRun.fFont.GetMetrics(&newFontMetrics);
#endif
#ifdef OHOS_SUPPORT
            auto decompressFont = targetRun.fFont;
            scaleFontWithCompressionConfig(decompressFont, ScaleOP::DECOMPRESS);
            metricsIncludeFontPadding(&newFontMetrics, decompressFont);
#endif
            fontMetrics.emplace_back(newFontMetrics);
        }
    }

    charNumber = lineCharCount;
    return true;
}

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
    fTextStyles.reset();
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
    fPlaceholders.reset();
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

void ParagraphImpl::layout(SkScalar rawWidth) {
#ifdef OHOS_SUPPORT
    TEXT_TRACE_FUNC();
#endif
    fLineNumber = 1;
    fLayoutRawWidth = rawWidth;
    // TODO: This rounding is done to match Flutter tests. Must be removed...
    auto floorWidth = rawWidth;

    if (getApplyRoundingHack()) {
        floorWidth = SkScalarFloorToScalar(floorWidth);
    }

#ifdef OHOS_SUPPORT
    fPaintRegion.reset();
    bool isMaxLinesZero = false;
    if (fParagraphStyle.getMaxLines() == 0) {
        if (fText.size() != 0) {
            isMaxLinesZero = true;
        }
    }
#endif

    if ((!SkScalarIsFinite(rawWidth) || fLongestLine <= floorWidth) &&
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
#ifdef OHOS_SUPPORT
    this->fUnicodeText = convertUtf8ToUnicode(fText);
#endif
    auto paragraphCache = fFontCollection->getParagraphCache();

    if (fState < kShaped) {
        // Check if we have the text in the cache and don't need to shape it again
        if (!paragraphCache->findParagraph(this)) {
            if (fState < kIndexed) {
                // This only happens once at the first layout; the text is immutable
                // and there is no reason to repeat it
                if (this->computeCodeUnitProperties()) {
                    fState = kIndexed;
                }
            }
            this->fRuns.reset();
            this->fClusters.reset();
            this->fClustersIndexFromCodeUnit.reset();
            this->fClustersIndexFromCodeUnit.push_back_n(fText.size() + 1, EMPTY_INDEX);
            if (!this->shapeTextIntoEndlessLine()) {
                this->resetContext();

#ifdef OHOS_SUPPORT
                if (isMaxLinesZero) {
                    fExceededMaxLines  = true;
                }
#endif
                // TODO: merge the two next calls - they always come together
                this->resolveStrut();
                this->computeEmptyMetrics();
                this->fLines.reset();

                // Set the important values that are not zero
                fWidth = floorWidth;
                fHeight = fEmptyMetrics.height();
                if (fParagraphStyle.getStrutStyle().getStrutEnabled() &&
                    fParagraphStyle.getStrutStyle().getForceStrutHeight()) {
                    fHeight = fStrutMetrics.height();
                }
                if (fParagraphStyle.getMaxLines() == 0) {
                    fHeight = 0;
                }
                fAlphabeticBaseline = fEmptyMetrics.alphabeticBaseline();
                fIdeographicBaseline = fEmptyMetrics.ideographicBaseline();
                fLongestLine = FLT_MIN - FLT_MAX;  // That is what flutter has
                fMinIntrinsicWidth = 0;
                fMaxIntrinsicWidth = 0;
                this->fOldWidth = floorWidth;
                this->fOldHeight = this->fHeight;

                return;
            } else {
                // Add the paragraph to the cache
                paragraphCache->updateParagraph(this);
            }
        }
        fState = kShaped;
    }

    if (fState == kShaped) {
        this->resetContext();
        this->resolveStrut();
        this->computeEmptyMetrics();
        this->fLines.reset();
#ifdef OHOS_SUPPORT
        // fast path
        if (!fHasLineBreaks &&
            !fHasWhitespacesInside &&
            fPlaceholders.size() == 1 &&
            (fRuns.size() == 1 && preCalculateSingleRunAutoSpaceWidth(floorWidth))) {
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
#ifdef OHOS_SUPPORT
        if (paragraphStyle().getVerticalAlignment() != TextVerticalAlign::BASELINE &&
            paragraphStyle().getMaxLines() > 1) {
            splitRuns();
        }
#endif
        // Build the picture lazily not until we actually have to paint (or never)
        this->resetShifts();
        this->formatLines(fWidth);
        fState = kFormatted;
    }

    if (fParagraphStyle.getMaxLines() == 0) {
        fHeight = 0;
#ifdef OHOS_SUPPORT
        fLines.reset();
#endif
    }

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
    if (fParagraphStyle.getMaxLines() == 0) {
        fLineNumber = 0;
    } else {
        fLineNumber = std::max(size_t(1), fLines.size());
    }
    //SkDebugf("layout('%s', %f): %f %f\n", fText.c_str(), rawWidth, fMinIntrinsicWidth, fMaxIntrinsicWidth);
}

#ifdef OHOS_SUPPORT
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
    std::vector<SplitPoint>& splitPoints, const Run& run, ClusterRange lineRange, size_t lineIndex) {
    size_t startIndex =
        std::max(cluster(lineRange.start).textRange().start, cluster(run.clusterRange().start).textRange().start);
    size_t endIndex =
        std::min(cluster(lineRange.end - 1).textRange().end, cluster(run.clusterRange().end- 1).textRange().end);
    // The run cross line
    splitPoints.push_back({lineIndex, run.index(), startIndex, endIndex});
}

void ParagraphImpl::generateSplitPoints(std::vector<SplitPoint>& splitPoints) {
    for (size_t lineIndex = 0; lineIndex < fLines.size(); ++lineIndex) {
        const TextLine& line = fLines[lineIndex];
        ClusterRange lineClusterRange = line.clustersWithSpaces();
        // Avoid abnormal split of the last line
        if (lineIndex == fLines.size() - 1) {
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

void ParagraphImpl::generateRunsBySplitPoints(std::vector<SplitPoint>& splitPoints, SkTArray<Run, false>& runs) {
    std::reverse(splitPoints.begin(), splitPoints.end());
    size_t newRunGlobalIndex = 0;
    for (size_t runIndex = 0; runIndex < fRuns.size(); ++runIndex) {
        Run& run = fRuns[runIndex];
        if (splitPoints.empty() || splitPoints.back().runIndex != run.fIndex) {
            // No need to split
            run.fIndex = newRunGlobalIndex++;
            updateSplitRunClusterInfo(run, false);
            runs.push_back(run);
            continue;
        }

        while (!splitPoints.empty()) {
            SplitPoint& splitPoint = splitPoints.back();
            size_t splitRunIndex = splitPoint.runIndex;
            if (splitRunIndex != runIndex) {
                break;
            }

            Run splitRun{run, newRunGlobalIndex++};
            run.generateSplitRun(splitRun, splitPoint);
            updateSplitRunClusterInfo(splitRun, true);
            runs.push_back(std::move(splitRun));
            splitPoints.pop_back();
        }
    }
}

void ParagraphImpl::splitRuns() {
    // Collect split info for crossing line's run
    std::vector<SplitPoint> splitPoints{};
    generateSplitPoints(splitPoints);
    if (splitPoints.empty()) {
        return;
    }
    SkTArray<Run, false> newRuns;
    generateRunsBySplitPoints(splitPoints, newRuns);
    if (newRuns.empty()) {
        return;
    }
    fRuns = std::move(newRuns);
    refreshLines();
}
#endif

void ParagraphImpl::paint(SkCanvas* canvas, SkScalar x, SkScalar y) {
#ifdef OHOS_SUPPORT
    TEXT_TRACE_FUNC();
#endif
    CanvasParagraphPainter painter(canvas);
    paint(&painter, x, y);
}

void ParagraphImpl::paint(ParagraphPainter* painter, SkScalar x, SkScalar y) {
#ifdef OHOS_SUPPORT
    TEXT_TRACE_FUNC();
    // Reset all text style vertical shift
    for (Block& block : fTextStyles) {
        block.fStyle.setVerticalAlignShift(0.0);
    }
#endif
    for (auto& line : fLines) {
#ifdef OHOS_SUPPORT
        line.updateTextLinePaintAttributes();
#endif
        line.paint(painter, x, y);
    }
}

void ParagraphImpl::paint(ParagraphPainter* painter, RSPath* path, SkScalar hOffset, SkScalar vOffset) {
#ifdef OHOS_SUPPORT
    TEXT_TRACE_FUNC();
#endif
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

#ifdef OHOS_SUPPORT
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
#ifdef OHOS_SUPPORT
    fLongestLineWithIndent = 0;
#endif
    fMaxWidthWithTrailingSpaces = 0;
    fExceededMaxLines = false;
}

// shapeTextIntoEndlessLine is the thing that calls this method
bool ParagraphImpl::computeCodeUnitProperties() {
#ifdef OHOS_SUPPORT
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

    const char *locale = fParagraphStyle.getTextStyle().getLocale().c_str();
    // Collect all spaces and some extra information
    // (and also substitute \t with a space while we are at it)
    if (!fUnicode->computeCodeUnitFlags(&fText[0],
                                        fText.size(),
#ifdef OHOS_SUPPORT
                                        this->paragraphStyle().getReplaceTabCharacters() ||
                                        (!(this->paragraphStyle().getTextTab().location < 1.0)),
                                        locale,
#else
                                        this->paragraphStyle().getReplaceTabCharacters(),
#endif
                                        &fCodeUnitProperties)) {
        return false;
    }

    // Get some information about trailing spaces / hard line breaks
    fTrailingSpaces = fText.size();
    TextIndex firstWhitespace = EMPTY_INDEX;
    for (size_t i = 0; i < fCodeUnitProperties.size(); ++i) {
        auto flags = fCodeUnitProperties[i];
        if (SkUnicode::isPartOfWhiteSpaceBreak(flags)) {
            if (fTrailingSpaces  == fText.size()) {
                fTrailingSpaces = i;
            }
            if (firstWhitespace == EMPTY_INDEX) {
                firstWhitespace = i;
            }
        } else {
            fTrailingSpaces = fText.size();
        }
        if (SkUnicode::isHardLineBreak(flags)) {
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

#ifdef OHOS_SUPPORT
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
    if (text.end() - ch == 1 && *(unsigned char*)ch <= 0x7F) {
        // I am not even sure it's worth it if we do not save a unicode call
        if (is_ascii_7bit_space(*ch)) {
            ++whiteSpacesBreakLen;
        }
#ifdef OHOS_SUPPORT
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
#ifdef OHOS_SUPPORT
            fIsPunctuation = fOwner->codeUnitHasProperty(i, SkUnicode::CodeUnitFlags::kPunctuation) | fIsPunctuation;
            fIsEllipsis = fOwner->codeUnitHasProperty(i, SkUnicode::CodeUnitFlags::kEllipsis) | fIsEllipsis;
#endif
        }
    }

    fIsWhiteSpaceBreak = whiteSpacesBreakLen == fTextRange.width();
    fIsIntraWordBreak = intraWordBreakLen == fTextRange.width();
    fIsHardBreak = fOwner->codeUnitHasProperty(fTextRange.end,
                                               SkUnicode::CodeUnitFlags::kHardLineBreakBefore);
#ifdef OHOS_SUPPORT
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
    if (end > start && !fAutoSpacings.empty()) {
        // This is not a tyopo: we are using Point as a pair of SkScalars
        correction += fAutoSpacings[end - 1].fX - fAutoSpacings[start].fY;
    }
    return posX(end) - posX(start) + correction;
}

// In some cases we apply spacing to glyphs first and then build the cluster table, in some we do
// the opposite - just to optimize the most common case.
void ParagraphImpl::applySpacingAndBuildClusterTable() {
#ifdef OHOS_SUPPORT
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
        this->buildClusterTable();
        SkScalar shift = 0;
        run.iterateThroughClusters([this, &run, &shift, &style](Cluster* cluster) {
            run.shift(cluster, shift);
            shift += run.addSpacesEvenly(style.getLetterSpacing(), cluster);
        });
        return;
    }

    // The complex case: many text styles with spacing (possibly not adjusted to glyphs)
    this->buildClusterTable();

    // Walk through all the clusters in the direction of shaped text
    // (we have to walk through the styles in the same order, too)
    SkScalar shift = 0;
    for (auto& run : fRuns) {

        // Skip placeholder runs
        if (run.isPlaceholder()) {
            continue;
        }
        bool soFarWhitespacesOnly = true;
        bool wordSpacingPending = false;
        Cluster* lastSpaceCluster = nullptr;
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
                    run.addSpacesAtTheEnd(spacing, lastSpaceCluster);
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

// Clusters in the order of the input text
void ParagraphImpl::buildClusterTable() {
    // It's possible that one grapheme includes few runs; we cannot handle it
    // so we break graphemes by the runs instead
    // It's not the ideal solution and has to be revisited later
    size_t cluster_count = 1;
    for (auto& run : fRuns) {
        cluster_count += run.isPlaceholder() ? 1 : run.size();
        fCodeUnitProperties[run.fTextRange.start] |= SkUnicode::CodeUnitFlags::kGraphemeStart;
        fCodeUnitProperties[run.fTextRange.start] |= SkUnicode::CodeUnitFlags::kGlyphClusterStart;
    }
    if (!fRuns.empty()) {
        fCodeUnitProperties[fRuns.back().textRange().end] |= SkUnicode::CodeUnitFlags::kGraphemeStart;
        fCodeUnitProperties[fRuns.back().textRange().end] |= SkUnicode::CodeUnitFlags::kGlyphClusterStart;
    }
    fClusters.reserve_back(fClusters.size() + cluster_count);

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
            run.iterateThroughClustersInTextOrder([&run, runIndex, this](size_t glyphStart,
                                                                   size_t glyphEnd,
                                                                   size_t charStart,
                                                                   size_t charEnd,
                                                                   SkScalar width,
                                                                   SkScalar height) {
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
#ifdef OHOS_SUPPORT
    TEXT_TRACE_FUNC();
#endif
    if (fText.size() == 0) {
        return false;
    }

    fUnresolvedCodepoints.clear();
    fFontSwitches.reset();

    OneLineShaper oneLineShaper(this);
    auto result = oneLineShaper.shape();
    fUnresolvedGlyphs = oneLineShaper.unresolvedGlyphs();

    this->applySpacingAndBuildClusterTable();

    return result;
}

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

#ifdef OHOS_SUPPORT
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
                fLongestLine = std::max(fLongestLine, nearlyZero(advance.fX) ? widthWithSpaces : advance.fX);
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
#ifdef OHOS_SUPPORT
    TEXT_TRACE_FUNC();
#endif
    auto effectiveAlign = fParagraphStyle.effective_align();
    const bool isLeftAligned = effectiveAlign == TextAlign::kLeft
        || (effectiveAlign == TextAlign::kJustify && fParagraphStyle.getTextDirection() == TextDirection::kLtr);

    if (!SkScalarIsFinite(maxWidth) && !isLeftAligned) {
        // Special case: clean all text in case of maxWidth == INF & align != left
        // We had to go through shaping though because we need all the measurement numbers
        fLines.reset();
        return;
    }

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
}

void ParagraphImpl::resolveStrut() {
    auto strutStyle = this->paragraphStyle().getStrutStyle();
    if (!strutStyle.getStrutEnabled() || strutStyle.getFontSize() < 0) {
        return;
    }

    auto typefaces = fFontCollection->findTypefaces(strutStyle.getFontFamilies(), strutStyle.getFontStyle(), std::nullopt);
    if (typefaces.empty()) {
        SkDEBUGF("Could not resolve strut font\n");
        return;
    }

#ifndef USE_SKIA_TXT
    SkFont font(typefaces.front(), strutStyle.getFontSize());
    SkFontMetrics metrics;
#ifdef OHOS_SUPPORT
    SkFont compressFont = font;
    scaleFontWithCompressionConfig(compressFont, ScaleOP::COMPRESS);
    compressFont.getMetrics(&metrics);
    metricsIncludeFontPadding(&metrics, font);
#else
    font.getMetrics(&metrics);
#endif
#else
    RSFont font(typefaces.front(), strutStyle.getFontSize(), 1, 0);
    RSFontMetrics metrics;
#ifdef OHOS_SUPPORT
    RSFont compressFont = font;
    scaleFontWithCompressionConfig(compressFont, ScaleOP::COMPRESS);
    compressFont.GetMetrics(&metrics);
    metricsIncludeFontPadding(&metrics, font);
#else
    font.GetMetrics(&metrics);
#endif
#endif

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

BlockRange ParagraphImpl::findAllBlocks(TextRange textRange) {
    BlockIndex begin = EMPTY_BLOCK;
    BlockIndex end = EMPTY_BLOCK;
    for (BlockIndex index = 0; index < fTextStyles.size(); ++index) {
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
    auto blocks = findAllBlocks(textIncludingNewLines);
    return fLines.emplace_back(this, offset, advance, blocks,
                               textExcludingSpaces, text, textIncludingNewLines,
                               clusters, clustersWithGhosts, widthWithSpaces, sizes);
}

#ifdef OHOS_SUPPORT
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
    // this method should not be called before kshaped
    if (fText.isEmpty() || fState < kShaped) {
        if (start == 0 && end > 0) {
            // On account of implied "\n" that is always at the end of the text
            //SkDebugf("getRectsForRange(%d, %d): %f\n", start, end, fHeight);
#ifdef OHOS_SUPPORT
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
        lineText = textRangeMergeBtoA(lineText, line.getTextRangeReplacedByEllipsis());
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
  // this method should not be called before kshaped
  if (fText.isEmpty() || fState < kShaped) {
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
            fRuns.reset();
            fClusters.reset();
            [[fallthrough]];

        case kShaped:
            fLines.reset();
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

#ifndef USE_SKIA_TXT
    SkFont font(typeface, textStyle.getFontSize());
#else
    RSFont font(typeface, textStyle.getFontSize(), 1, 0);
#endif
    fEmptyMetrics = InternalLineMetrics(font, paragraphStyle().getStrutStyle().getForceStrutHeight());

    if (!paragraphStyle().getStrutStyle().getForceStrutHeight() &&
        textStyle.getHeightOverride()) {
#ifdef OHOS_SUPPORT
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

WordBreakType ParagraphImpl::getWordBreakType() const {
    return fParagraphStyle.getStrutStyle().getWordBreakType();
}

LineBreakStrategy ParagraphImpl::getLineBreakStrategy() const {
    return fParagraphStyle.getStrutStyle().getLineBreakStrategy();
}

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

#ifdef OHOS_SUPPORT
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

SkTArray<TextIndex> ParagraphImpl::countSurroundingGraphemes(TextRange textRange) const {
    textRange = textRange.intersection({0, fText.size()});
    SkTArray<TextIndex> graphemes;
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
        fUnicode->extractUtfConversionMapping(
                this->text(),
                [&](size_t index) { fUTF8IndexForUTF16Index.emplace_back(index); },
                [&](size_t index) { fUTF16IndexForUTF8Index.emplace_back(index); });
    });
}

void ParagraphImpl::visit(const Visitor& visitor) {
#ifndef USE_SKIA_TXT
    int lineNumber = 0;
    for (auto& line : fLines) {
        line.ensureTextBlobCachePopulated();
        for (auto& rec : line.fTextBlobCache) {
            SkTextBlob::Iter iter(*rec.fBlob);
            SkTextBlob::Iter::ExperimentalRun run;

            SkSTArray<128, uint32_t> clusterStorage;
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

int ParagraphImpl::getLineNumberAt(TextIndex codeUnitIndex) const {
    for (size_t i = 0; i < fLines.size(); ++i) {
        auto& line = fLines[i];
        if (line.text().contains({codeUnitIndex, codeUnitIndex + 1})) {
            return i;
        }
    }
    return -1;
}

bool ParagraphImpl::getLineMetricsAt(int lineNumber, LineMetrics* lineMetrics) const {
    if (lineNumber < 0 || static_cast<size_t>(lineNumber) >= fLines.size()) {
        return false;
    }
    auto& line = fLines[lineNumber];
    if (lineMetrics) {
        *lineMetrics = line.getMetrics();
    }
    return true;
}

TextRange ParagraphImpl::getActualTextRange(int lineNumber, bool includeSpaces) const {
    if (lineNumber < 0 || static_cast<size_t>(lineNumber) >= fLines.size()) {
        return EMPTY_TEXT;
    }
    auto& line = fLines[lineNumber];
    return includeSpaces ? line.text() : line.trimmedText();
}

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

#ifndef USE_SKIA_TXT
SkFont ParagraphImpl::getFontAt(TextIndex codeUnitIndex) const {
    for (auto& run : fRuns) {
        if (run.textRange().contains({codeUnitIndex, codeUnitIndex})) {
            return run.font();
        }
    }
    return SkFont();
}
#else
RSFont ParagraphImpl::getFontAt(TextIndex codeUnitIndex) const
{
    for (auto& run : fRuns) {
        if (run.textRange().contains({codeUnitIndex, codeUnitIndex})) {
            return run.font();
        }
    }
    return RSFont();
}
#endif

std::vector<Paragraph::FontInfo> ParagraphImpl::getFonts() const {
    std::vector<FontInfo> results;
    for (auto& run : fRuns) {
        results.emplace_back(run.font(), run.textRange());
    }
    return results;
}

#ifndef USE_SKIA_TXT
SkFontMetrics ParagraphImpl::measureText() {
    SkFontMetrics metrics;
    if (fRuns.empty()) {
        return metrics;
    }

    const auto& firstFont = fRuns.front().font();
    SkRect firstBounds;
    auto firstStr = text(fRuns.front().textRange());
    firstFont.getMetrics(&metrics);
#ifdef OHOS_SUPPORT
    auto decompressFont = firstFont;
    scaleFontWithCompressionConfig(decompressFont, ScaleOP::DECOMPRESS);
    metricsIncludeFontPadding(&metrics, decompressFont);
#endif
    firstFont.measureText(firstStr.data(), firstStr.size(), SkTextEncoding::kUTF8, &firstBounds, nullptr);
    fGlyphsBoundsTop = firstBounds.top();
    fGlyphsBoundsBottom = firstBounds.bottom();
    fGlyphsBoundsLeft = firstBounds.left();
    SkScalar realWidth = 0;
    for (size_t i = 0; i < fRuns.size(); ++i) {
        auto run = fRuns[i];
        const auto& font = run.font();
        SkRect bounds;
        auto str = text(run.textRange());
        auto advance = font.measureText(str.data(), str.size(), SkTextEncoding::kUTF8, &bounds, nullptr);
        realWidth += advance;
        if (i == 0) {
            realWidth -= ((advance - (bounds.right() - bounds.left())) / 2);
        }
        if (i == (fRuns.size() - 1)) {
            realWidth -= ((advance - (bounds.right() - bounds.left())) / 2);
        }
        fGlyphsBoundsTop = std::min(fGlyphsBoundsTop, bounds.top());
        fGlyphsBoundsBottom = std::max(fGlyphsBoundsBottom, bounds.bottom());
    }
    fGlyphsBoundsRight = realWidth + fGlyphsBoundsLeft;
    return metrics;
}
#else
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
#ifdef OHOS_SUPPORT
    auto decompressFont = firstFont;
    scaleFontWithCompressionConfig(decompressFont, ScaleOP::DECOMPRESS);
    metricsIncludeFontPadding(&metrics, decompressFont);
#endif
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
#endif

std::vector<std::unique_ptr<TextLineBase>> ParagraphImpl::GetTextLines() {
    std::vector<std::unique_ptr<TextLineBase>> textLineBases;
    for (auto& line: fLines) {
#ifdef OHOS_SUPPORT
        std::unique_ptr<TextLineBaseImpl> textLineBaseImplPtr =
            std::make_unique<TextLineBaseImpl>(std::make_unique<TextLine>(std::move(line)));
#else
        std::unique_ptr<TextLineBaseImpl> textLineBaseImplPtr = std::make_unique<TextLineBaseImpl>(&line);
#endif
        textLineBases.emplace_back(std::move(textLineBaseImplPtr));
    }

    return textLineBases;
}

#ifdef OHOS_SUPPORT
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
#endif

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
#ifdef OHOS_SUPPORT
    paragraph->fLongestLineWithIndent = this->fLongestLineWithIndent;
#endif
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

}  // namespace textlayout
}  // namespace skia
