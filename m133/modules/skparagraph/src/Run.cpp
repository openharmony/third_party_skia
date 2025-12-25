// Copyright 2019 Google LLC.
#include "include/core/SkFontMetrics.h"
#include "include/core/SkTextBlob.h"
#include "include/private/base/SkFloatingPoint.h"
#include "include/private/base/SkMalloc.h"
#include "include/private/base/SkTo.h"
#include "modules/skparagraph/include/DartTypes.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skparagraph/src/Run.h"
#include "modules/skshaper/include/SkShaper.h"
#include "src/base/SkUTF.h"

#ifdef ENABLE_TEXT_ENHANCE
#include "include/FontCollection.h"
#include "log.h"
#endif
namespace skia {
namespace textlayout {
#ifdef ENABLE_TEXT_ENHANCE
constexpr SkScalar PARAM_TWO = 2.0;
// 1px font size "HarmonyOS Sans" metrics
constexpr SkScalar DEFAULT_TOP = -1.056;
constexpr SkScalar DEFAULT_BOTTOM = 0.271;
constexpr SkScalar DEFAULT_ASCENT = -0.928;
constexpr SkScalar DEFAULT_DESCENT = 0.244;
constexpr SkScalar TIBETAN_FALLBACKLINESPCING_HEIGHT_SCALE = 0.8;
// unordered_map<familyName, ScaleParam>: compress <familyName> font height, shift font baseline.
// target font size = font size * ScaleParam.scale.
// target baseline = baseline - height * font size * ScaleParam.baselineShiftScale.
const std::unordered_map<std::string, ScaleParam> FONT_FAMILY_COMPRESSION_CONFIG = {
    {"Noto Serif Tibetan", ScaleParam{ .fontScale = 0.79, .baselineShiftScale = 0.1 }},
    {"Noto Sans Tibetan", ScaleParam{ .fontScale = 0.79, .baselineShiftScale = 0.1 }},
};
const std::unordered_map<std::string, ScaleParam> FONT_FAMILY_COMPRESSION_WITH_HEIGHT_ADAPTER_CONFIG = {
    {"Noto Serif Tibetan", ScaleParam{ .fontScale = 0.85, .baselineShiftScale = 0.11 }},
    {"Noto Sans Tibetan", ScaleParam{ .fontScale = 0.85, .baselineShiftScale = 0.11 }},
};
enum FontCompressionStatus {
    UNDEFINED,
    COMPRESSED,
    UNCOMPRESSED,
};
// the font padding does not take effect for these font families.
const std::unordered_set<std::string> FONT_PADDING_NOT_EFFECT_FAMILY = {
    "Harmony Clock_01",
    "Harmony Clock_02",
    "Harmony Clock_03",
    "Harmony Clock_04",
    "Harmony Clock_05",
    "Harmony Clock_06",
    "Harmony Clock_07",
    "Harmony Clock_08",
// symbol: need to ensure "the symbol height = the font size".
// so the height compression is not enabled for symbol.
    "HM Symbol",
};

FontCompressionStatus getFontCompressionStatus(const RSFont& font)
{
    auto typeface = font.GetTypeface();
    if (typeface == nullptr) {
        return FontCompressionStatus::UNDEFINED;
    }
    return (typeface->IsCustomTypeface() && !typeface->IsThemeTypeface())
                   ? FontCompressionStatus::UNCOMPRESSED
                   : FontCompressionStatus::COMPRESSED;
}
std::string getFamilyNameFromFont(const RSFont& font)
{
    auto typeface = font.GetTypeface();
    return typeface == nullptr ? "" : typeface->GetFamilyName();
}

const ScaleParam& findCompressionConfigWithFont(const RSFont& font)
{
    auto fontCompressionStatus = getFontCompressionStatus(font);
    if (fontCompressionStatus != FontCompressionStatus::COMPRESSED) {
        return DEFAULT_SCALE_PARAM;
    }

    const auto& config = FontCollection::IsAdapterTextHeightEnabled() ?
        FONT_FAMILY_COMPRESSION_WITH_HEIGHT_ADAPTER_CONFIG : FONT_FAMILY_COMPRESSION_CONFIG;
    std::string familyName = getFamilyNameFromFont(font);
    auto iter = config.find(familyName);
    if (iter == config.end()) {
        return DEFAULT_SCALE_PARAM;
    }
    return iter->second;
}

void metricsIncludeFontPadding(RSFontMetrics* metrics, const RSFont& font)
{
    if (metrics == nullptr) {
        return;
    }
    auto fontCompressionStatus = getFontCompressionStatus(font);
    auto typeface = font.GetTypeface();
    if (typeface == nullptr || fontCompressionStatus == FontCompressionStatus::UNDEFINED) {
        return;
    }
    SkScalar fontSize = font.GetSize();
    if (!FontCollection::IsAdapterTextHeightEnabled()) {
        if (fontCompressionStatus == FontCompressionStatus::COMPRESSED &&
            (!SkScalarNearlyZero(findCompressionConfigWithFont(font).fontScale) ||
            typeface->IsThemeTypeface())) {
            metrics->fAscent = DEFAULT_ASCENT * fontSize;
            metrics->fDescent = DEFAULT_DESCENT * fontSize;
        }
        return;
    }

    std::string curFamilyName = getFamilyNameFromFont(font);
    auto setIter = FONT_PADDING_NOT_EFFECT_FAMILY.find(curFamilyName);
    if (setIter == FONT_PADDING_NOT_EFFECT_FAMILY.end()) {
        if (fontCompressionStatus == FontCompressionStatus::COMPRESSED) {
            metrics->fAscent = DEFAULT_TOP * fontSize;
            metrics->fDescent = DEFAULT_BOTTOM * fontSize;
            return;
        }
        // use top and bottom as ascent and descent.
        // calculate height with top and bottom.(includeFontPadding)
        metrics->fAscent = metrics->fTop;
        metrics->fDescent = metrics->fBottom;
    }
}

void scaleFontWithCompressionConfig(RSFont& font, ScaleOP op)
{
    auto param = findCompressionConfigWithFont(font);
    scaleFontWithCompressionConfig(font, op, param);
}

void scaleFontWithCompressionConfig(RSFont& font, ScaleOP op, const ScaleParam& config)
{
    SkScalar fontSize = font.GetSize();
    if (SkScalarNearlyZero(config.fontScale)) {
        return;
    }
    switch (op) {
    case ScaleOP::COMPRESS:
        fontSize *= config.fontScale;
        break;
    case ScaleOP::DECOMPRESS:
        fontSize /= config.fontScale;
        break;
    default:
        return;
    }
    font.SetSize(fontSize);
}
#endif

Run::Run(ParagraphImpl* owner,
         const SkShaper::RunHandler::RunInfo& info,
         size_t firstChar,
         SkScalar heightMultiplier,
         bool useHalfLeading,
         SkScalar baselineShift,
         size_t index,
         SkScalar offsetX)
    : fOwner(owner)
    , fTextRange(firstChar + info.utf8Range.begin(), firstChar + info.utf8Range.end())
    , fClusterRange(EMPTY_CLUSTERS)
    , fFont(info.fFont)
    , fClusterStart(firstChar)
    , fGlyphData(std::make_shared<GlyphData>())
    , fGlyphs(fGlyphData->glyphs)
    , fPositions(fGlyphData->positions)
    , fOffsets(fGlyphData->offsets)
    , fClusterIndexes(fGlyphData->clusterIndexes)
#ifdef ENABLE_TEXT_ENHANCE
    , fGlyphAdvances(fGlyphData->advances)
#endif
    , fHeightMultiplier(heightMultiplier)
    , fUseHalfLeading(useHalfLeading)
    , fBaselineShift(baselineShift)
{
    fBidiLevel = info.fBidiLevel;
    fAdvance = info.fAdvance;
    fIndex = index;
    fUtf8Range = info.utf8Range;
    fOffset = SkVector::Make(offsetX, 0);

    fGlyphs.push_back_n(info.glyphCount);
    fPositions.push_back_n(info.glyphCount + 1);
    fOffsets.push_back_n(info.glyphCount + 1);
    fClusterIndexes.push_back_n(info.glyphCount + 1);
#ifdef ENABLE_TEXT_ENHANCE
    fGlyphAdvances.push_back_n(info.glyphCount + 1);
    fHalfLetterspacings.push_back_n(info.glyphCount + 1);
    std::fill(fHalfLetterspacings.begin(), fHalfLetterspacings.end(), 0.0);
    fScaleParam = findCompressionConfigWithFont(font());
#else
    info.fFont.getMetrics(&fFontMetrics);
#endif

#ifdef ENABLE_TEXT_ENHANCE
    handleAdapterHeight();
    initLimitHeightParam();
#endif

    this->calculateMetrics();

    // To make edge cases easier:
    fPositions[info.glyphCount] = fOffset + fAdvance;
    fOffsets[info.glyphCount] = {0, 0};
#ifdef ENABLE_TEXT_ENHANCE
    fGlyphAdvances[info.glyphCount] = {0, 0};
    if (leftToRight()) {
        fClusterIndexes[info.glyphCount] = info.utf8Range.end();
    } else {
        // First cluster index in rtl's language run is end of the utf8 range value
        fClusterIndexes[0] = info.utf8Range.end();
    }
    initRunHeightNominal();
#else
    fClusterIndexes[info.glyphCount] = this->leftToRight() ? info.utf8Range.end() : info.utf8Range.begin();
#endif
    fEllipsis = false;
    fPlaceholderIndex = std::numeric_limits<size_t>::max();
}

#ifdef ENABLE_TEXT_ENHANCE
void Run::handleAdapterHeight() {
    RSFont decompressFont = font();
    if (fOwner && fOwner->getParagraphStyle().getFallbackLineSpacing()) {
        if (!SkScalarNearlyZero(fScaleParam.fontScale)) {
            decompressFont.SetSize(font().GetSize() * TIBETAN_FALLBACKLINESPCING_HEIGHT_SCALE);
        }
        decompressFont.GetMetrics(&fFontMetrics);
        return;
    }

    fFont.GetMetrics(&fFontMetrics);
    scaleFontWithCompressionConfig(decompressFont, ScaleOP::DECOMPRESS, fScaleParam);
    metricsIncludeFontPadding(&fFontMetrics, decompressFont);
    fCompressionBaselineShift = (fFontMetrics.fDescent - fFontMetrics.fAscent) * fScaleParam.baselineShiftScale;
}

void Run::initRunHeightNominal() {
    fRunHeightNominal = nearlyEqual(calculateHeight(LineMetricStyle::Typographic, LineMetricStyle::Typographic),
        calculateHeight(LineMetricStyle::CSS, LineMetricStyle::CSS));
}

void Run::initLimitHeightParam() {
    if (fOwner == nullptr) {
        return;
    }

    BlockRange blockRange = fOwner->findAllBlocks(fTextRange);
    const SkSpan<Block>& blocks = fOwner->blocks(blockRange);
    // Preprocessing has been completed, and within the same run,
    // all textstyle's minLineHeight, maxLineHeight, and lineHeightStyle are equal.
    if (!blocks.empty()) {
        const TextStyle& style = blocks.front().fStyle;
        setLineHeightStyle(style.getLineHeightStyle());
        style.getMaxLineHeight() > 0 ?
            setMaxLineHeight(style.getMaxLineHeight()) : setMaxLineHeight(std::numeric_limits<float>::max());
        setMinLineHeight(style.getMinLineHeight());
    }
}

void Run::calculateMetricsWithoutHeightScale() {
    if (nearlyZero(fFont.GetSize())) {
        return;
    }
    SkScalar originRunHeight = fCorrectDescent - fCorrectAscent;
    SkScalar runHeight = std::min(std::max(originRunHeight, getMinLineHeight()), getMaxLineHeight());
    if (runHeight < originRunHeight && owner() && owner()->paragraphStyle().getFallbackLineSpacing()) {
        runHeight = originRunHeight;
    }
    const auto multiplier = nearlyZero(originRunHeight) ? 0 : runHeight / originRunHeight;
    fCorrectAscent *= multiplier;
    fCorrectDescent *= multiplier;
}
#endif

void Run::calculateMetrics() {
    fCorrectAscent = fFontMetrics.fAscent - fFontMetrics.fLeading * 0.5;
    fCorrectDescent = fFontMetrics.fDescent + fFontMetrics.fLeading * 0.5;
    fCorrectLeading = 0;
#ifdef ENABLE_TEXT_ENHANCE
    if (SkScalarNearlyZero(fHeightMultiplier)) {
        calculateMetricsWithoutHeightScale();
        return;
    }
    auto decompressFont = fFont;
    scaleFontWithCompressionConfig(decompressFont, ScaleOP::DECOMPRESS, fScaleParam);
    const auto fontIntrinsicHeight = fCorrectDescent - fCorrectAscent;
    if (nearlyZero(fontIntrinsicHeight)) {
        return;
    }
    SkScalar runHeight = fLineHeightStyle == LineHeightStyle::kFontSize ?
        fHeightMultiplier * decompressFont.GetSize() : fHeightMultiplier * fontIntrinsicHeight;
    // If the maxLineHeight is less than 0, it is meaningless and the it is set to max of type float
    if (runHeight >= 0) {
        runHeight = std::min(std::max(runHeight, getMinLineHeight()), getMaxLineHeight());
    }
    if (runHeight < fontIntrinsicHeight && owner() && owner()->paragraphStyle().getFallbackLineSpacing()) {
        runHeight = fontIntrinsicHeight;
    }
#else
    if (SkScalarNearlyZero(fHeightMultiplier)) {
        return;
    }
    const auto runHeight = fHeightMultiplier * fFont.getSize();
    const auto fontIntrinsicHeight = fCorrectDescent - fCorrectAscent;
#endif
    if (fUseHalfLeading) {
        const auto extraLeading = (runHeight - fontIntrinsicHeight) / 2;
        fCorrectAscent -= extraLeading;
        fCorrectDescent += extraLeading;
    } else {
        const auto multiplier = runHeight / fontIntrinsicHeight;
        fCorrectAscent *= multiplier;
        fCorrectDescent *= multiplier;
    }
    // If we shift the baseline we need to make sure the shifted text fits the line
    fCorrectAscent += fBaselineShift;
    fCorrectDescent += fBaselineShift;
}

#ifdef ENABLE_TEXT_ENHANCE
Run::Run(const Run& run, size_t runIndex)
    : fOwner(run.fOwner),
      fTextRange(run.textRange()),
      fClusterRange(run.clusterRange()),
      fFont(run.fFont),
      fPlaceholderIndex(run.fPlaceholderIndex),
      fIndex(runIndex),
      fAdvance(SkVector::Make(0, 0)),
      fOffset(SkVector::Make(0, 0)),
      fClusterStart(run.fClusterStart),
      fUtf8Range(run.fUtf8Range),
      fGlyphData(std::make_shared<GlyphData>()),
      fGlyphs(fGlyphData->glyphs),
      fPositions(fGlyphData->positions),
      fOffsets(fGlyphData->offsets),
      fClusterIndexes(fGlyphData->clusterIndexes),
      fGlyphAdvances(fGlyphData->advances),
      fJustificationShifts(),
      fAutoSpacings(),
      fHalfLetterspacings(),
      fFontMetrics(run.fFontMetrics),
      fHeightMultiplier(run.fHeightMultiplier),
      fUseHalfLeading(run.fUseHalfLeading),
      fBaselineShift(run.fBaselineShift),
      fCorrectAscent(run.fCorrectAscent),
      fCorrectDescent(run.fCorrectDescent),
      fCorrectLeading(run.fCorrectLeading),
      fEllipsis(run.fEllipsis),
      fBidiLevel(run.fBidiLevel),
      fTopInGroup(run.fTopInGroup),
      fBottomInGroup(run.fBottomInGroup),
      fMaxRoundRectRadius(run.fMaxRoundRectRadius),
      indexInLine(run.indexInLine),
      fCompressionBaselineShift(run.fCompressionBaselineShift),
      fVerticalAlignShift(run.fVerticalAlignShift),
      fMaxLineHeight(run.fMaxLineHeight),
      fMinLineHeight(run.fMinLineHeight),
      fLineHeightStyle(run.fLineHeightStyle),
      fRunHeightNominal(run.fRunHeightNominal) {}

size_t Run::findSplitClusterPos(size_t target) {
    int left = -1;
    int right = clusterIndexes().size();
    while (left + 1 < right) {
        int mid = left + (right - left) / 2;
        if (clusterIndexes()[mid] >= target) {
            if (leftToRight()) {
                right = mid;
                continue;
            }
            left = mid;
        } else {
            if (leftToRight()) {
                left = mid;
                continue;
            }
            right = mid;
        }
    }
    if (leftToRight()) {
        return static_cast<size_t>(right);
    }
    return static_cast<size_t>(left);
}

// Compatible with getCoordinate RTL scenario
size_t Run::globalClusterIndex(size_t pos) const {
    if (leftToRight() || pos == (size_t)fGlyphs.size()) {
        return fClusterStart + fClusterIndexes[pos];
    }
    return fClusterStart + fClusterIndexes[pos + 1];
}

void Run::updateSplitRunRangeInfo(Run& splitRun, size_t headIndex, size_t tailIndex) {
    splitRun.fTextRange.start = headIndex;
    splitRun.fClusterRange.start = fOwner->clusterIndex(headIndex);
    splitRun.fTextRange.end = tailIndex;
    splitRun.fUtf8Range = {splitRun.fTextRange.start, splitRun.fTextRange.width()};
    splitRun.fClusterRange.end = fOwner->clusterIndex(tailIndex);
}

void Run::updateSplitRunMesureInfo(Run& splitRun, size_t startClusterPos, size_t endClusterPos) {
    if (!leftToRight()) {
        std::swap(startClusterPos, endClusterPos);
    }
    SkScalar glyphPosVal = 0.0f;
    SkScalar posOffset = 0.0f;
    posOffset = fGlyphData->positions[startClusterPos].fX;
    for (; startClusterPos < endClusterPos; ++startClusterPos) {
        splitRun.fGlyphData->glyphs.push_back(fGlyphData->glyphs[startClusterPos]);
        glyphPosVal = fGlyphData->positions[startClusterPos].fX - posOffset;
        splitRun.fGlyphData->positions.push_back({glyphPosVal, fGlyphData->positions[startClusterPos].fY});
        splitRun.fGlyphData->offsets.push_back(fGlyphData->offsets[startClusterPos]);
        splitRun.fGlyphData->clusterIndexes.push_back(fGlyphData->clusterIndexes[startClusterPos]);
        splitRun.fGlyphData->advances.push_back(fGlyphData->advances[startClusterPos]);
        splitRun.fHalfLetterspacings.push_back(fHalfLetterspacings[startClusterPos]);
    }

    // Generate for ghost cluster
    glyphPosVal = fGlyphData->positions[startClusterPos].fX - posOffset;
    splitRun.fGlyphData->positions.push_back({glyphPosVal, fGlyphData->positions[startClusterPos].fY});
    splitRun.fGlyphData->offsets.push_back({0.0f, 0.0f});
    splitRun.fGlyphData->clusterIndexes.push_back(fGlyphData->clusterIndexes[startClusterPos]);
    splitRun.fGlyphData->advances.push_back({0.0f, 0.0f});
    splitRun.fPositions = splitRun.fGlyphData->positions;
    splitRun.fOffsets = splitRun.fGlyphData->offsets;
    splitRun.fClusterIndexes = splitRun.fGlyphData->clusterIndexes;
    splitRun.fGlyphAdvances = splitRun.fGlyphData->advances;
    splitRun.fGlyphs = splitRun.fGlyphData->glyphs;
    splitRun.fAdvance = {glyphPosVal, fAdvance.fY};
    splitRun.fHalfLetterspacings.push_back(fHalfLetterspacings[startClusterPos]);
}

void Run::generateSplitRun(Run& splitRun, const SplitPoint& splitPoint) {
    if (fGlyphData->positions.empty()) {
        return;
    }
    size_t tailIndex = splitPoint.tailClusterIndex;
    size_t headIndex = splitPoint.headClusterIndex;
    updateSplitRunRangeInfo(splitRun, headIndex, tailIndex);
    size_t startClusterPos = findSplitClusterPos(headIndex - fClusterStart);
    size_t endClusterPos = findSplitClusterPos(tailIndex - fClusterStart);
    if (endClusterPos >= clusterIndexes().size() || startClusterPos >= clusterIndexes().size()) {
        LOGE("Failed to find clusterPos by binary search algorithm");
        return;
    }
    updateSplitRunMesureInfo(splitRun, startClusterPos, endClusterPos);
}

void Run::updateCompressedRunMeasureInfo(Run& headCompressPuncRun) {
    fGlyphs[0] = headCompressPuncRun.glyphs()[0];
    fOffsets[0] = headCompressPuncRun.offsets()[0];
    fGlyphAdvances[0] = headCompressPuncRun.advances()[0];
    fPositions[1].fX = fPositions[0].fX + fGlyphAdvances[0].fX ;

    fGlyphData->glyphs[0] = fGlyphs[0];
    fGlyphData->offsets[0] = fOffsets[0];
    fGlyphData->advances[0] = fGlyphAdvances[0];
    fGlyphData->positions[1].set(fPositions[1].fX, fPositions[1].fY);
}

#endif

SkShaper::RunHandler::Buffer Run::newRunBuffer() {
#ifdef ENABLE_TEXT_ENHANCE
    return {fGlyphs.data(), fPositions.data(), fOffsets.data(), fClusterIndexes.data(), fOffset, fGlyphAdvances.data()};
#else
    return {fGlyphs.data(), fPositions.data(), fOffsets.data(), fClusterIndexes.data(), fOffset};
#endif
}

#ifdef ENABLE_TEXT_ENHANCE
SkScalar Run::usingAutoSpaceWidth(const Cluster& cluster) const
{
    return fOwner->clusterUsingAutoSpaceWidth(cluster);
}
void Run::copyTo(RSTextBlobBuilder& builder, size_t pos, size_t size) const {
    SkASSERT(pos + size <= this->size());
    const auto& blobBuffer = builder.AllocRunPos(fFont, SkToInt(size));
    if (!blobBuffer.glyphs || !fGlyphs.data() || pos + size > this->size()) {
        TEXT_LOGE("Failed to copy run, copy pos %{public}zu size %{public}zu", pos, size);
        return;
    }

    sk_careful_memcpy(blobBuffer.glyphs, fGlyphs.data() + pos, size * sizeof(SkGlyphID));
    auto points = reinterpret_cast<SkPoint*>(blobBuffer.pos);

    for (size_t i = 0; i < size; ++i) {
        auto point = fPositions[i + pos];
        if (!fJustificationShifts.empty()) {
            point.fX += fJustificationShifts[i + pos].fX;
        }
        if (!fAutoSpacings.empty()) {
            point.fX += fAutoSpacings[i + pos].fX;
        }
        point += fOffsets[i + pos];
        points[i] = point;
    }
}

void Run::copyTo(RSTextBlobBuilder& builder,
                 const RSPath* path,
                 float hOffset,
                 float vOffset,
                 float fTextShift,
                 size_t pos,
                 size_t size) const {
    SkASSERT(pos + size <= this->size());
    auto& blobBuffer = builder.AllocRunRSXform(fFont, SkToInt(size));
    if (!blobBuffer.glyphs || !fGlyphs.data() || (pos + size > this->size())) {
        TEXT_LOGE("Failed to copy run, copy pos %{public}zu size %{public}zu", pos, size);
        return;
    }

    sk_careful_memcpy(blobBuffer.glyphs, fGlyphs.data() + pos, size * sizeof(SkGlyphID));
    std::vector<float> widths(size);
    fFont.GetWidths(blobBuffer.glyphs, size, widths.data());
    RSXform* xform = reinterpret_cast<RSXform*>(blobBuffer.pos);
    for (size_t i = 0; i < size; ++i) {
        float halfWidth = widths[i + pos] * 0.5f;
        float x = hOffset + posX(i + pos) + halfWidth + fOffsets[i + pos].x() + fTextShift;
        if (!fJustificationShifts.empty()) {
            x += fJustificationShifts[i + pos].fX;
        }
        if (!fAutoSpacings.empty()) {
            x += fAutoSpacings[i + pos].fX;
        }
        RSPoint rsPos;
        RSPoint rsTan;
        if (!path->GetPositionAndTangent(x, rsPos, rsTan, false)) {
            rsPos.Set(x, vOffset);
            rsTan.Set(1, 0);
        }
        xform[i].cos_ = rsTan.GetX();
        xform[i].sin_ = rsTan.GetY();
        xform[i].tx_ = rsPos.GetX() - rsTan.GetY() * vOffset - halfWidth * rsTan.GetX();
        xform[i].ty_ = rsPos.GetY() + rsTan.GetX() * vOffset - halfWidth * rsTan.GetY();
    }
}
#else
void Run::copyTo(SkTextBlobBuilder& builder, size_t pos, size_t size) const {
    SkASSERT(pos + size <= this->size());
    const auto& blobBuffer = builder.allocRunPos(fFont, SkToInt(size));
    sk_careful_memcpy(blobBuffer.glyphs, fGlyphs.data() + pos, size * sizeof(SkGlyphID));

    for (size_t i = 0; i < size; ++i) {
        auto point = fPositions[i + pos];
        if (!fJustificationShifts.empty()) {
            point.fX += fJustificationShifts[i + pos].fX;
        }
        point += fOffsets[i + pos];
        blobBuffer.points()[i] = point;
    }
}
#endif

// Find a cluster range from text range (within one run)
// Cluster range is normalized ([start:end) start < end regardless of TextDirection
// Boolean value in triple indicates whether the cluster range was found or not
std::tuple<bool, ClusterIndex, ClusterIndex> Run::findLimitingClusters(TextRange text) const {
    if (text.width() == 0) {
        // Special Flutter case for "\n" and "...\n"
        if (text.end > this->fTextRange.start) {
            ClusterIndex index = fOwner->clusterIndex(text.end - 1);
            return std::make_tuple(true, index, index);
        } else {
            return std::make_tuple(false, 0, 0);
        }
    }

    ClusterRange clusterRange;
    bool found = true;
    // Deal with the case when either start or end are not align with glyph cluster edge
    // In such case we shift the text range to the right
    // (cutting from the left and adding to the right)
    if (leftToRight()) {
        // LTR: [start:end)
        found = clusterRange.start != fClusterRange.end;
        clusterRange.start = fOwner->clusterIndex(text.start);
        clusterRange.end = fOwner->clusterIndex(text.end - 1);
    } else {
        // RTL: (start:end]
#ifdef ENABLE_TEXT_ENHANCE
        clusterRange.start = fOwner->clusterIndex(text.end - 1);
        clusterRange.end = fOwner->clusterIndex(text.start);
#else
        clusterRange.start = fOwner->clusterIndex(text.end);
        clusterRange.end = fOwner->clusterIndex(text.start + 1);
#endif
        found = clusterRange.end != fClusterRange.start;
    }

    return std::make_tuple(
            found,
            clusterRange.start,
            clusterRange.end);
}

std::tuple<bool, TextIndex, TextIndex> Run::findLimitingGlyphClusters(TextRange text) const {
    TextIndex start = fOwner->findPreviousGlyphClusterBoundary(text.start);
    TextIndex end = fOwner->findNextGlyphClusterBoundary(text.end);
    return std::make_tuple(true, start, end);
}

// Adjust the text to grapheme edges so the first grapheme start is in the text and the last grapheme start is in the text
// It actually means that the first grapheme is entirely in the text and the last grapheme does not have to be
// 12345 234 2:2 -> 2,5 4:4
std::tuple<bool, TextIndex, TextIndex> Run::findLimitingGraphemes(TextRange text) const {
    TextIndex start = fOwner->findPreviousGraphemeBoundary(text.start);
    TextIndex end = fOwner->findNextGraphemeBoundary(text.end);
    return std::make_tuple(true, start, end);
}

void Run::iterateThroughClusters(const ClusterVisitor& visitor) {

    for (size_t index = 0; index < fClusterRange.width(); ++index) {
        auto correctIndex = leftToRight() ? fClusterRange.start + index : fClusterRange.end - index - 1;
        auto cluster = &fOwner->cluster(correctIndex);
        visitor(cluster);
    }
}

void Run::addSpacesAtTheEnd(SkScalar space, Cluster* cluster) {
    // Increment the run width
    fAdvance.fX += space;
    // Increment the cluster width
    cluster->space(space);
}

SkScalar Run::addSpacesEvenly(SkScalar space) {
    SkScalar shift = 0;
#ifdef ENABLE_TEXT_ENHANCE
    if (this->size()) {
        shift += space / PARAM_TWO;
    }
#endif
    for (size_t i = 0; i < this->size(); ++i) {
        fPositions[i].fX += shift;
#ifdef ENABLE_TEXT_ENHANCE
        fHalfLetterspacings[i] = space / PARAM_TWO;
#endif
        shift += space;
    }
#ifdef ENABLE_TEXT_ENHANCE
    if (this->size()) {
        shift -= space / PARAM_TWO;
    }
#endif
    fPositions[this->size()].fX += shift;
    fAdvance.fX += shift;
    return shift;
}

#ifdef ENABLE_TEXT_ENHANCE
SkScalar Run::addSpacesEvenly(SkScalar space, Cluster* cluster) {
    // Offset all the glyphs in the cluster
    SkScalar shift = 0;
    for (size_t i = cluster->startPos(); i < cluster->endPos(); ++i) {
        fPositions[i].fX += shift;
        fHalfLetterspacings[i] = space / PARAM_TWO;
        shift += space;
    }
    if (this->size() == cluster->endPos()) {
        // To make calculations easier
        fPositions[cluster->endPos()].fX += shift;
        fHalfLetterspacings[cluster->endPos()] = space / PARAM_TWO;
    }
    // Increment the run width
    fAdvance.fX += shift;
    // Increment the cluster width
    cluster->space(shift);
    cluster->setHalfLetterSpacing(space / PARAM_TWO);

    return shift;
}

#else
SkScalar Run::addSpacesEvenly(SkScalar space, Cluster* cluster) {
    // Offset all the glyphs in the cluster
    SkScalar shift = 0;
    for (size_t i = cluster->startPos(); i < cluster->endPos(); ++i) {
        fPositions[i].fX += shift;
        shift += space;
    }
    if (this->size() == cluster->endPos()) {
        // To make calculations easier
        fPositions[cluster->endPos()].fX += shift;
    }
    // Increment the run width
    fAdvance.fX += shift;
    // Increment the cluster width
    cluster->space(shift);
    cluster->setHalfLetterSpacing(space / 2);

    return shift;
}
#endif

void Run::shift(const Cluster* cluster, SkScalar offset) {
    if (offset == 0) {
        return;
    }
    for (size_t i = cluster->startPos(); i < cluster->endPos(); ++i) {
        fPositions[i].fX += offset;
    }
    if (this->size() == cluster->endPos()) {
        // To make calculations easier
        fPositions[cluster->endPos()].fX += offset;
    }
}

void Run::extend(const Cluster* cluster, SkScalar offset) {
    // Extend the cluster at the end
    fPositions[cluster->endPos()].fX += offset;
}

#ifdef ENABLE_TEXT_ENHANCE
void Run::extendClusterWidth(Cluster* cluster, SkScalar space) {
    addSpacesAtTheEnd(space, cluster);
    for (size_t pos = cluster->endPos(); pos < fPositions.size(); pos++) {
        fPositions[pos].fX += space;
    }
}

// Checks if the current line contains trailing spaces and current run is at the end of the line
bool Run::isTrailingSpaceIncluded(const ClusterRange& fTextLineClusterRange,
    const ClusterRange& fTextLineGhostClusterRange) const {
    return fTextLineGhostClusterRange.width() > 0 && this->clusterRange().width() > 0 &&
           fTextLineClusterRange.width() > 0 && fTextLineGhostClusterRange.end != fTextLineClusterRange.end &&
           fTextLineGhostClusterRange.end <= this->clusterRange().end &&
           fTextLineGhostClusterRange.end > this->clusterRange().start;
}

void Run::updatePlaceholderAlignmentIfNeeded(PlaceholderAlignment& alignment, TextVerticalAlign paragraphAlignment) {
    if (alignment != PlaceholderAlignment::kFollow) {
        return;
    }

    switch (paragraphAlignment) {
        case TextVerticalAlign::TOP:
            alignment = PlaceholderAlignment::kTop;
            break;
        case TextVerticalAlign::CENTER:
            alignment = PlaceholderAlignment::kMiddle;
            break;
        case TextVerticalAlign::BOTTOM:
            alignment = PlaceholderAlignment::kBottom;
            break;
        case TextVerticalAlign::BASELINE:
            alignment = PlaceholderAlignment::kAboveBaseline;
            break;
        default:
            break;
    }
}

void Run::updateMetrics(InternalLineMetrics* endlineMetrics) {

    SkASSERT(isPlaceholder());
    auto placeholderStyle = this->placeholderStyle();
    // Difference between the placeholder baseline and the line bottom
    SkScalar baselineAdjustment = 0;
    switch (placeholderStyle->fBaseline) {
        case TextBaseline::kAlphabetic:
            break;

        case TextBaseline::kIdeographic:
            baselineAdjustment = endlineMetrics->deltaBaselines() / 2;
            break;
    }

    auto height = placeholderStyle->fHeight;
    auto offset = placeholderStyle->fBaselineOffset;

    fFontMetrics.fLeading = 0;

    updatePlaceholderAlignmentIfNeeded(placeholderStyle->fAlignment,
        fOwner->getParagraphStyle().getVerticalAlignment());

    switch (placeholderStyle->fAlignment) {
        case PlaceholderAlignment::kBaseline:
            fFontMetrics.fAscent = baselineAdjustment - height - offset;
            fFontMetrics.fDescent = baselineAdjustment - offset;
            break;

        case PlaceholderAlignment::kFollow:
        case PlaceholderAlignment::kAboveBaseline:
            fFontMetrics.fAscent = baselineAdjustment - height;
            fFontMetrics.fDescent = baselineAdjustment;
            break;

        case PlaceholderAlignment::kBelowBaseline:
            fFontMetrics.fAscent = baselineAdjustment;
            fFontMetrics.fDescent = baselineAdjustment + height;
            break;

        case PlaceholderAlignment::kTop:
            fFontMetrics.fAscent = endlineMetrics->ascent();
            fFontMetrics.fDescent = height + fFontMetrics.fAscent;
            break;

        case PlaceholderAlignment::kBottom:
            fFontMetrics.fDescent = endlineMetrics->descent();
            fFontMetrics.fAscent = fFontMetrics.fDescent - height;
            break;

        case PlaceholderAlignment::kMiddle:
            auto mid = (endlineMetrics->ascent() + endlineMetrics->descent()) / PARAM_TWO;
            fFontMetrics.fDescent = mid + height / PARAM_TWO;
            fFontMetrics.fAscent = mid - height / PARAM_TWO;
            break;
    }

    this->calculateMetrics();

    // Make sure the placeholder can fit the line
    endlineMetrics->add(this);
}
#else
void Run::updateMetrics(InternalLineMetrics* endlineMetrics) {

    SkASSERT(isPlaceholder());
    auto placeholderStyle = this->placeholderStyle();
    // Difference between the placeholder baseline and the line bottom
    SkScalar baselineAdjustment = 0;
    switch (placeholderStyle->fBaseline) {
        case TextBaseline::kAlphabetic:
            break;

        case TextBaseline::kIdeographic:
            baselineAdjustment = endlineMetrics->deltaBaselines() / 2;
            break;
    }

    auto height = placeholderStyle->fHeight;
    auto offset = placeholderStyle->fBaselineOffset;

    fFontMetrics.fLeading = 0;
    switch (placeholderStyle->fAlignment) {
        case PlaceholderAlignment::kBaseline:
            fFontMetrics.fAscent = baselineAdjustment - offset;
            fFontMetrics.fDescent = baselineAdjustment + height - offset;
            break;

        case PlaceholderAlignment::kAboveBaseline:
            fFontMetrics.fAscent = baselineAdjustment - height;
            fFontMetrics.fDescent = baselineAdjustment;
            break;

        case PlaceholderAlignment::kBelowBaseline:
            fFontMetrics.fAscent = baselineAdjustment;
            fFontMetrics.fDescent = baselineAdjustment + height;
            break;

        case PlaceholderAlignment::kTop:
            fFontMetrics.fDescent = height + fFontMetrics.fAscent;
            break;

        case PlaceholderAlignment::kBottom:
            fFontMetrics.fAscent = fFontMetrics.fDescent - height;
            break;

        case PlaceholderAlignment::kMiddle:
            auto mid = (-fFontMetrics.fDescent - fFontMetrics.fAscent)/2.0;
            fFontMetrics.fDescent = height/2.0 - mid;
            fFontMetrics.fAscent =  - height/2.0 - mid;
            break;
    }

    this->calculateMetrics();

    // Make sure the placeholder can fit the line
    endlineMetrics->add(this);
}
#endif

SkScalar Cluster::sizeToChar(TextIndex ch) const {
    if (ch < fTextRange.start || ch >= fTextRange.end) {
        return 0;
    }
    auto shift = ch - fTextRange.start;
    auto ratio = shift * 1.0 / fTextRange.width();

    return SkDoubleToScalar(fWidth * ratio);
}

SkScalar Cluster::sizeFromChar(TextIndex ch) const {
    if (ch < fTextRange.start || ch >= fTextRange.end) {
        return 0;
    }
    auto shift = fTextRange.end - ch - 1;
    auto ratio = shift * 1.0 / fTextRange.width();

    return SkDoubleToScalar(fWidth * ratio);
}

size_t Cluster::roundPos(SkScalar s) const {
    auto ratio = (s * 1.0) / fWidth;
    return sk_double_floor2int(ratio * size());
}

SkScalar Cluster::trimmedWidth(size_t pos) const {
    // Find the width until the pos and return the min between trimmedWidth and the width(pos)
    // We don't have to take in account cluster shift since it's the same for 0 and for pos
    auto& run = fOwner->run(fRunIndex);
#ifdef ENABLE_TEXT_ENHANCE
    SkScalar delta = getHalfLetterSpacing() - run.halfLetterspacing(pos);
    return std::min(run.positionX(pos) - run.positionX(fStart) + delta, fWidth);
#else
    return std::min(run.positionX(pos) - run.positionX(fStart), fWidth);
#endif
}

SkScalar Run::positionX(size_t pos) const {
#ifdef ENABLE_TEXT_ENHANCE
    return posX(pos) + (fJustificationShifts.empty() ? 0 : fJustificationShifts[pos].fY) +
        (fAutoSpacings.empty() ? 0 : fAutoSpacings[pos].fY);
#else
    return posX(pos) + (fJustificationShifts.empty() ? 0 : fJustificationShifts[pos].fY);
#endif
}

#ifdef ENABLE_TEXT_ENHANCE
SkScalar Run::posX(size_t index) const {
    if (index < fPositions.size()) {
        return fPositions[index].fX;
    }
    LOGE("index:%{public}zu,size:%{public}d", index, fPositions.size());
    if (fPositions.empty()) {
        return 0.0f;
    }
    return fPositions[fPositions.size() - 1].fX;
}
#endif

PlaceholderStyle* Run::placeholderStyle() const {
    if (isPlaceholder()) {
        return &fOwner->placeholders()[fPlaceholderIndex].fStyle;
    } else {
        return nullptr;
    }
}

bool Run::isResolved() const {
    for (auto& glyph :fGlyphs) {
        if (glyph == 0) {
            return false;
        }
    }
    return true;
}

Run* Cluster::runOrNull() const {
    if (fRunIndex >= fOwner->runs().size()) {
        return nullptr;
    }
    return &fOwner->run(fRunIndex);
}

Run& Cluster::run() const {
    SkASSERT(fRunIndex < fOwner->runs().size());
    return fOwner->run(fRunIndex);
}

#ifdef ENABLE_TEXT_ENHANCE
RSFont Cluster::font() const {
#else
SkFont Cluster::font() const {
#endif
    SkASSERT(fRunIndex < fOwner->runs().size());
    return fOwner->run(fRunIndex).font();
}

bool Cluster::isSoftBreak() const {
    return fOwner->codeUnitHasProperty(fTextRange.end,
                                       SkUnicode::CodeUnitFlags::kSoftLineBreakBefore);
}

bool Cluster::isGraphemeBreak() const {
    return fOwner->codeUnitHasProperty(fTextRange.end, SkUnicode::CodeUnitFlags::kGraphemeStart);
}

#ifdef ENABLE_TEXT_ENHANCE
bool Cluster::isStartCombineBreak() const {
    return fOwner->codeUnitHasProperty(fTextRange.start, SkUnicode::CodeUnitFlags::kCombine);
}

bool Cluster::isEndCombineBreak() const {
    return fOwner->codeUnitHasProperty(fTextRange.end, SkUnicode::CodeUnitFlags::kCombine);
}
#endif
}  // namespace textlayout
}  // namespace skia
