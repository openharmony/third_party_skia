// Copyright 2019 Google LLC.

#include "include/core/SkCanvas.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSpan.h"
#include "include/core/SkString.h"
#include "include/core/SkTypes.h"
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
#include "modules/skparagraph/src/TextLineBaseImpl.h"
#include "modules/skparagraph/src/Run.h"
#include "modules/skparagraph/src/TextLine.h"
#include "modules/skparagraph/src/TextWrapper.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "src/utils/SkUTF.h"
#include <math.h>
#include <algorithm>
#include <string>
#include <utility>
#include "log.h"
#ifdef TXT_AUTO_SPACING
#include "parameter.h"
#endif

namespace skia {
namespace textlayout {

namespace {
constexpr int PARAM_DOUBLE = 2;
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

std::vector<SkUnichar> ParagraphImpl::convertUtf8ToUnicode(const SkString& utf8)
{
    fUnicodeIndexForUTF8Index.reset();
    std::vector<SkUnichar> result;
    auto p = utf8.c_str();
    auto end = p + utf8.size();
    while (p < end) {
        auto tmp = p;
        auto unichar = SkUTF::NextUTF8(&p, end);
        for(auto i = 0; i < p - tmp; ++i) {
            fUnicodeIndexForUTF8Index.emplace_back(result.size());
        }
        result.emplace_back(unichar);
    }
    fUnicodeIndexForUTF8Index.emplace_back(result.size());
    return result;
}

Paragraph::Paragraph()
{ }

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

ParagraphImpl::ParagraphImpl()
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
            if(++runClock > currentRunCharNumber) {
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
            metricsIncludeFontPadding(&newFontMetrics);
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

bool ParagraphImpl::middleEllipsisDeal()
{
    if (fRuns.empty()) {
        return false;
    }
    isMiddleEllipsis = false;
    const SkString& ell = this->getEllipsis();
    const char *ellStr = ell.c_str();
    size_t start = 0;
    size_t end = 0;
    size_t charbegin = 0;
    size_t charend = 0;
    if (fRuns.begin()->leftToRight()) {
        if (ltrTextSize[0].phraseWidth >= fOldMaxWidth) {
            fText.reset();
            fText.set(ellStr);
            end = 1;
            charend = ell.size();
        } else {
            scanTextCutPoint(ltrTextSize, start, end);
            if (end) {
                charbegin = ltrTextSize[start].charbegin;
                charend = ltrTextSize[end].charOver;
                fText.remove(ltrTextSize[start].charbegin, ltrTextSize[end].charOver - ltrTextSize[start].charbegin);
                fText.insert(ltrTextSize[start].charbegin, ellStr);
            }
        }
        ltrTextSize.clear();
    } else {
        scanTextCutPoint(rtlTextSize, start, end);
        if (start < 1 || end + PARAM_DOUBLE >= rtlTextSize.size()) {
            start = 0;
            end = 0;
        }
        if (end) {
            charbegin = rtlTextSize[start - 1].charbegin;
            charend = rtlTextSize[end + PARAM_DOUBLE].charbegin;
            fText.remove(rtlTextSize[start - 1].charbegin,
                rtlTextSize[end + PARAM_DOUBLE].charbegin - rtlTextSize[start - 1].charbegin);
            fText.insert(rtlTextSize[start - 1].charbegin, ellStr);
        }
        rtlTextSize.clear();
    }

    if (end != 0) {
        TextRange deletedRange(charbegin, charend);
        resetTextStyleRange(deletedRange);
        resetPlaceholderRange(deletedRange);
    }

    // end = 0 means the text does not exceed the width limit
    return end != 0;
}

SkScalar ParagraphImpl::resetEllipsisWidth(SkScalar ellipsisWidth, size_t& lastRunIndex, const size_t textIndex)
{
    auto targetCluster = cluster(clusterIndex(textIndex));
    if (lastRunIndex != targetCluster.runIndex()) {
        TextLine textLine;
        textLine.setParagraphImpl(this);
        auto blockRange = findAllBlocks(TextRange(textIndex, textIndex + 1));
        textLine.setBlockRange(blockRange);
        const SkString& ellipsis = this->getEllipsis();
        std::unique_ptr<Run> ellipsisRun;
        ellipsisRun = textLine.shapeEllipsis(ellipsis, &targetCluster);
        lastRunIndex = targetCluster.runIndex();
        ellipsisWidth = ellipsisRun->fAdvanceX();
        ellipsisRun.reset();
    }
    return ellipsisWidth;
}

void ParagraphImpl::scanRTLTextCutPoint(const std::vector<TextCutRecord>& rawTextSize, size_t& start, size_t& end)
{
    size_t lastRunIndex = EMPTY_RUN;
    auto runTimeEllipsisWidth = resetEllipsisWidth(0, lastRunIndex, 0);
    float measureWidth = runTimeEllipsisWidth;
    size_t left = 0;
    size_t right = rawTextSize.size() - 1;
    while (left < rawTextSize.size() && measureWidth < fOldMaxWidth && left <= right) {
        measureWidth += rawTextSize[left++].phraseWidth;
        if (right > left && measureWidth < fOldMaxWidth) {
            measureWidth += rawTextSize[right--].phraseWidth;
        }
        measureWidth -= runTimeEllipsisWidth;
        runTimeEllipsisWidth = resetEllipsisWidth(runTimeEllipsisWidth, lastRunIndex, left);
        measureWidth += runTimeEllipsisWidth;
    }

    if (right < left) {
        right = left;
    }

    if (measureWidth >= fOldMaxWidth || fParagraphStyle.getTextOverflower()) {
        start = left;
        end = right;
    } else {
        start = 0;
        end = 0;
    }
}

void ParagraphImpl::scanLTRTextCutPoint(const std::vector<TextCutRecord>& rawTextSize, size_t& start, size_t& end)
{
    size_t lastRunIndex = EMPTY_RUN;
    auto runTimeEllipsisWidth = resetEllipsisWidth(0, lastRunIndex, 0);
    float measureWidth = runTimeEllipsisWidth;
    size_t begin = 0;
    size_t last = rawTextSize.size() - 1;
    bool rightExit = false;
    while (begin < last && !rightExit && measureWidth < fOldMaxWidth) {
        measureWidth += rawTextSize[begin++].phraseWidth;
        if (measureWidth > fOldMaxWidth) {
            --begin;
            break;
        }
        if (last > begin && measureWidth < fOldMaxWidth) {
            measureWidth += rawTextSize[last--].phraseWidth;
            if (measureWidth > fOldMaxWidth) {
                rightExit = true;
                ++last;
            }
        }
        measureWidth -= runTimeEllipsisWidth;
        runTimeEllipsisWidth = resetEllipsisWidth(runTimeEllipsisWidth, lastRunIndex, begin);
        measureWidth += runTimeEllipsisWidth;
    }

    if (measureWidth >= fOldMaxWidth || fParagraphStyle.getTextOverflower()) {
        start = begin;
        end = last;
    } else {
        start = 0;
        end = 0;
    }
}

void ParagraphImpl::scanTextCutPoint(const std::vector<TextCutRecord>& rawTextSize, size_t& start, size_t& end)
{
    if (allTextWidth <= fOldMaxWidth || !rawTextSize.size()) {
        allTextWidth = 0;
        return;
    }

    if (fRuns.begin()->leftToRight()) {
        scanLTRTextCutPoint(rawTextSize, start, end);
    } else {
        scanRTLTextCutPoint(rawTextSize, start, end);
    }
}

bool ParagraphImpl::shapeForMiddleEllipsis(SkScalar rawWidth)
{
    if (fParagraphStyle.getMaxLines() != 1 || fParagraphStyle.getEllipsisMod() != EllipsisModal::MIDDLE ||
        !fParagraphStyle.ellipsized()) {
        return true;
    }
    fOldMaxWidth = rawWidth;
    isMiddleEllipsis = true;
    allTextWidth = 0;
    this->computeCodeUnitProperties();
    this->fRuns.reset();
    this->fClusters.reset();
    this->fClustersIndexFromCodeUnit.reset();
    this->fClustersIndexFromCodeUnit.push_back_n(fText.size() + 1, EMPTY_INDEX);
    if (!this->shapeTextIntoEndlessLine()) {
        return false;
    }
    return middleEllipsisDeal();
}

void ParagraphImpl::prepareForMiddleEllipsis(SkScalar rawWidth)
{
    if (fParagraphStyle.getMaxLines() != 1 || fParagraphStyle.getEllipsisMod() != EllipsisModal::MIDDLE ||
        !fParagraphStyle.ellipsized()) {
        return;
    }
    std::shared_ptr<ParagraphImpl> tmpParagraph = std::make_shared<ParagraphImpl>(fText, fParagraphStyle, fTextStyles,
        fPlaceholders, fFontCollection, fUnicode);
    if (tmpParagraph->shapeForMiddleEllipsis(rawWidth)) {
        fText = tmpParagraph->fText;
        fTextStyles = tmpParagraph->fTextStyles;
        fPlaceholders = tmpParagraph->fPlaceholders;
    }
}

void ParagraphImpl::layout(SkScalar rawWidth) {
    fLineNumber = 1;
    allTextWidth = 0;
    fLayoutRawWidth = rawWidth;
    // TODO: This rounding is done to match Flutter tests. Must be removed...
    auto floorWidth = rawWidth;

    if (fParagraphStyle.getMaxLines() == 1 &&
        fParagraphStyle.getEllipsisMod() == EllipsisModal::MIDDLE) {
        fOldMaxWidth = rawWidth;
        isMiddleEllipsis = true;
    }
    if (getApplyRoundingHack()) {
        floorWidth = SkScalarFloorToScalar(floorWidth);
    }

    if (fParagraphStyle.getMaxLines() == 0) {
        fText.reset();
    }

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

    this->prepareForMiddleEllipsis(rawWidth);
    this->fUnicodeText = convertUtf8ToUnicode(fText);
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

        // fast path
        if (!fHasLineBreaks &&
            !fHasWhitespacesInside &&
            fPlaceholders.size() == 1 &&
            (fRuns.size() == 1 && fRuns[0].fAdvance.fX <= floorWidth - this->detectIndents(0))) {
            positionShapedTextIntoLine(floorWidth);
        } else {
            breakShapedTextIntoLines(floorWidth);
        }
        fState = kLineBroken;
    }

    if (fState == kLineBroken) {
        // Build the picture lazily not until we actually have to paint (or never)
        this->resetShifts();
        this->formatLines(fWidth);
        fState = kFormatted;
    }

    if (fParagraphStyle.getMaxLines() == 0) {
        fHeight = 0;
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

void ParagraphImpl::paint(SkCanvas* canvas, SkScalar x, SkScalar y) {
    CanvasParagraphPainter painter(canvas);
    paint(&painter, x, y);
}

void ParagraphImpl::paint(ParagraphPainter* painter, SkScalar x, SkScalar y) {
    for (auto& line : fLines) {
        line.paint(painter, x, y);
    }
}

void ParagraphImpl::paint(ParagraphPainter* painter, RSPath* path, SkScalar hOffset, SkScalar vOffset) {
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
                                        this->paragraphStyle().getReplaceTabCharacters(),
                                        &fCodeUnitProperties)) {
        return false;
    }

    // Get some information about trailing spaces / hard line breaks
    fTrailingSpaces = fText.size();
    TextIndex firstWhitespace = EMPTY_INDEX;
    for (int i = 0; i < fCodeUnitProperties.size(); ++i) {
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

static std::vector<SkRange<SkUnichar>> CJK_UNICODE_SET = {
    SkRange<SkUnichar>(0x4E00, 0x9FFF),
    SkRange<SkUnichar>(0x3400, 0x4DBF),
    SkRange<SkUnichar>(0x20000, 0x2A6DF),
    SkRange<SkUnichar>(0x2A700, 0x2B73F),
    SkRange<SkUnichar>(0x2B740, 0x2B81F),
    SkRange<SkUnichar>(0x2B820, 0x2CEAF),
    SkRange<SkUnichar>(0x2CEB0, 0x2EBEF),
    SkRange<SkUnichar>(0x30000, 0x3134F),
    SkRange<SkUnichar>(0xF900, 0xFAFF),
    SkRange<SkUnichar>(0x3040, 0x309F),
    SkRange<SkUnichar>(0x30A0, 0x30FF),
    SkRange<SkUnichar>(0x31F0, 0x31FF),
    SkRange<SkUnichar>(0x1100, 0x11FF),
    SkRange<SkUnichar>(0x3130, 0x318F),
    SkRange<SkUnichar>(0xAC00, 0xD7AF),
    SkRange<SkUnichar>(0x31C0, 0x31EF),
    SkRange<SkUnichar>(0x2E80, 0x2EFF),
    SkRange<SkUnichar>(0x2F800, 0x2FA1F),
};

static std::vector<SkRange<SkUnichar>> WESTERN_UNICODE_SET = {
    SkRange<SkUnichar>(0x0041, 0x005A),
    SkRange<SkUnichar>(0x0061, 0x007A),
    SkRange<SkUnichar>(0x0030, 0x0039),
};

constexpr SkUnichar COPYRIGHT_UNICODE = 0x00A9;

struct UnicodeSet {
    std::unordered_set<SkUnichar> set_;
    explicit UnicodeSet(const std::vector<SkRange<SkUnichar>>& unicodeSet) {
#ifdef TXT_AUTO_SPACING
        static constexpr int AUTO_SPACING_ENABLE_LENGTH = 10;
        char autoSpacingEnable[AUTO_SPACING_ENABLE_LENGTH] = {0};
        GetParameter("persist.sys.text.autospacing.enable", "0", autoSpacingEnable, AUTO_SPACING_ENABLE_LENGTH);
        if (!std::strcmp(autoSpacingEnable, "0")) {
            return;
        }
#else
        return;
#endif
        for (auto unicodeSetRange : unicodeSet) {
            for (auto i = unicodeSetRange.start; i <= unicodeSetRange.end; ++i) {
                set_.insert(i);
            }
        }
    }
    bool exist(SkUnichar c) const {
        return set_.find(c) != set_.end();
    }
};

static const UnicodeSet CJK_SET(CJK_UNICODE_SET);
static const UnicodeSet WESTERN_SET(WESTERN_UNICODE_SET);

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
        }
    }

    fIsWhiteSpaceBreak = whiteSpacesBreakLen == fTextRange.width();
    fIsIntraWordBreak = intraWordBreakLen == fTextRange.width();
    fIsHardBreak = fOwner->codeUnitHasProperty(fTextRange.end,
                                               SkUnicode::CodeUnitFlags::kHardLineBreakBefore);
    auto unicodeStart = fOwner->getUnicodeIndex(fTextRange.start);
    auto unicodeEnd = fOwner->getUnicodeIndex(fTextRange.end);
    SkUnichar unicode = 0;
    if (unicodeEnd - unicodeStart == 1 && unicodeStart < fOwner->unicodeText().size()) {
        unicode = fOwner->unicodeText()[unicodeStart];
    }
    fIsCopyright = unicode == COPYRIGHT_UNICODE;
    fIsCJK = CJK_SET.exist(unicode);
    fIsWestern = WESTERN_SET.exist(unicode);
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

void ParagraphImpl::middleEllipsisAddText(size_t charStart,
                                          size_t charEnd,
                                          SkScalar& allTextWidth,
                                          SkScalar width,
                                          bool isLeftToRight) {
    if (isMiddleEllipsis) {
        TextCutRecord textCount;
        textCount.charbegin = charStart;
        textCount.charOver = charEnd;
        textCount.phraseWidth = width;
        allTextWidth += width;
        if (isLeftToRight) {
            this->ltrTextSize.emplace_back(textCount);
        } else {
            this->rtlTextSize.emplace_back(textCount);
        }
    }
}

// Clusters in the order of the input text
void ParagraphImpl::buildClusterTable() {
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

                middleEllipsisAddText(charStart, charEnd, allTextWidth, width, run.leftToRight());
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

    SkScalar offsetX = this->detectIndents(0);
    this->addLine(SkPoint::Make(offsetX, 0), advance,
                  textExcludingSpaces, textRange, textRange,
                  clusterRange, clusterRangeWithGhosts, run.advance().x(),
                  metrics);
    auto longestLine = std::max(run.advance().fX, advance.fX);
    setSize(advance.fY, maxWidth, longestLine);
    setLongestLineWithIndent(std::min(longestLine + offsetX, maxWidth));
    setIntrinsicSize(run.advance().fX, advance.fX,
            fLines.empty() ? fEmptyMetrics.alphabeticBaseline() : fLines.front().alphabeticBaseline(),
            fLines.empty() ? fEmptyMetrics.ideographicBaseline() : fLines.front().ideographicBaseline(),
            false);
}

void ParagraphImpl::breakShapedTextIntoLines(SkScalar maxWidth) {
    resetAutoSpacing();
    for (auto& run : fRuns) {
        run.resetAutoSpacing();
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
                bool addEllipsis,
                SkScalar indent,
                SkScalar noIndentWidth) {
                // TODO: Take in account clipped edges
                auto& line = this->addLine(offset, advance, textExcludingSpaces, text, textWithNewlines, clusters, clustersWithGhosts, widthWithSpaces, metrics);
                if (addEllipsis && this->paragraphStyle().getEllipsisMod() == EllipsisModal::TAIL) {
                    line.createTailEllipsis(noIndentWidth, this->getEllipsis(), true, this->getWordBreakType());
                } else if (addEllipsis && this->paragraphStyle().getEllipsisMod() == EllipsisModal::HEAD) {
                    line.createHeadEllipsis(noIndentWidth, this->getEllipsis(), true);
                }
                auto spacing = line.autoSpacing();
                auto longestLine = std::max(line.width(), widthWithSpaces) + spacing;
                fLongestLine = std::max(fLongestLine, longestLine);
                fLongestLineWithIndent =
                        std::min(std::max(fLongestLineWithIndent, longestLine + indent), maxWidth);
            });
    setSize(textWrapper.height(), maxWidth, textWrapper.maxIntrinsicWidth());
    setIntrinsicSize(textWrapper.maxIntrinsicWidth(), textWrapper.minIntrinsicWidth(),
        fLines.empty() ? fEmptyMetrics.alphabeticBaseline() : fLines.front().alphabeticBaseline(),
        fLines.empty() ? fEmptyMetrics.ideographicBaseline() : fLines.front().ideographicBaseline(),
        textWrapper.exceededMaxLines());
}

void ParagraphImpl::formatLines(SkScalar maxWidth) {
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
    font.getMetrics(&metrics);
#else
    RSFont font(typefaces.front(), strutStyle.getFontSize(), 1, 0);
    RSFontMetrics metrics;
    font.GetMetrics(&metrics);
#endif
#ifdef OHOS_SUPPORT
    metricsIncludeFontPadding(&metrics);
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
    auto blocks = findAllBlocks(textExcludingSpaces);
    return fLines.emplace_back(this, offset, advance, blocks,
                               textExcludingSpaces, text, textIncludingNewLines,
                               clusters, clustersWithGhosts, widthWithSpaces, sizes);
}

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
            results.emplace_back(SkRect::MakeXYWH(0, 0, 0, fHeight), fParagraphStyle.getTextDirection());
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

std::vector<ParagraphPainter::PaintID> ParagraphImpl::updateColor(size_t from, size_t to, SkColor color) {
    std::vector<ParagraphPainter::PaintID> unresolvedPaintID;
    if (from >= to) {
        return unresolvedPaintID;
    }
    this->ensureUTF16Mapping();
    from = (from < SkToSizeT(fUTF8IndexForUTF16Index.size())) ? fUTF8IndexForUTF16Index[from] : fText.size();
    to = (to < SkToSizeT(fUTF8IndexForUTF16Index.size())) ? fUTF8IndexForUTF16Index[to] : fText.size();
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
    for (auto i = 0; i < fLines.size(); ++i) {
        auto& line = fLines[i];
        if (line.text().contains({codeUnitIndex, codeUnitIndex + 1})) {
            return i;
        }
    }
    return -1;
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
        return EMPTY_TEXT;
    }
    auto& line = fLines[lineNumber];
    return includeSpaces ? line.text() : line.trimmedText();
}

bool ParagraphImpl::getGlyphClusterAt(TextIndex codeUnitIndex, GlyphClusterInfo* glyphInfo) {
    for (auto i = 0; i < fLines.size(); ++i) {
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
    metricsIncludeFontPadding(&metrics);
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
    metricsIncludeFontPadding(&metrics);
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
        std::unique_ptr<TextLineBaseImpl> textLineBaseImplPtr = std::make_unique<TextLineBaseImpl>(&line);
        textLineBases.emplace_back(std::move(textLineBaseImplPtr));
    }

    return textLineBases;
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
    paragraph->rtlTextSize = this->rtlTextSize;
    paragraph->ltrTextSize = this->ltrTextSize;
    paragraph->fBidiRegions = this->fBidiRegions;

    paragraph->fUTF8IndexForUTF16Index = this->fUTF8IndexForUTF16Index;
    paragraph->fUTF16IndexForUTF8Index = this->fUTF16IndexForUTF8Index;
    paragraph->fUnresolvedGlyphs = this->fUnresolvedGlyphs;
    paragraph->isMiddleEllipsis = this->isMiddleEllipsis;
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
    paragraph->fOldMaxWidth = this->fOldMaxWidth;
    paragraph->allTextWidth = this->allTextWidth;

    paragraph->fUnicode = this->fUnicode;
    paragraph->fHasLineBreaks = this->fHasLineBreaks;
    paragraph->fHasWhitespacesInside = this->fHasWhitespacesInside;
    paragraph->fTrailingSpaces = this->fTrailingSpaces;
    paragraph->fLineNumber = this->fLineNumber;

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
