// Copyright 2019 Google LLC.
#ifndef Run_DEFINED
#define Run_DEFINED

#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSpan.h"
#include "include/core/SkTypes.h"
#include "include/private/SkTArray.h"
#include "modules/skparagraph/include/DartTypes.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skshaper/include/SkShaper.h"

#include <math.h>
#include <algorithm>
#include <functional>
#include <limits>
#include <tuple>

#ifdef OHOS_SUPPORT
#include "modules/skparagraph/include/ParagraphStyle.h"
#endif

class SkTextBlobBuilder;

namespace skia {
namespace textlayout {
constexpr int PARAM_64 = 64;

class Cluster;
class InternalLineMetrics;
class ParagraphImpl;
class TextLine;

typedef size_t RunIndex;
const size_t EMPTY_RUN = EMPTY_INDEX;

typedef size_t ClusterIndex;
typedef SkRange<size_t> ClusterRange;
const size_t EMPTY_CLUSTER = EMPTY_INDEX;
const SkRange<size_t> EMPTY_CLUSTERS = EMPTY_RANGE;

typedef size_t GraphemeIndex;
typedef SkRange<GraphemeIndex> GraphemeRange;

typedef size_t GlyphIndex;
typedef SkRange<GlyphIndex> GlyphRange;

// LTR: [start: end) where start <= end
// RTL: [end: start) where start >= end
class DirText {
    DirText(bool dir, size_t s, size_t e) : start(s), end(e) { }
    bool isLeftToRight() const { return start <= end; }
    size_t start;
    size_t end;
};

enum class RoundRectType {
    NONE,
    LEFT_ONLY,
    RIGHT_ONLY,
    ALL,
};

// first: words length, second: spacing width ratio
constexpr SkScalar AUTO_SPACING_WIDTH_RATIO = 8;

#ifdef OHOS_SUPPORT
enum class ScaleOP {
    COMPRESS,
    DECOMPRESS,
};

struct SplitPoint {
    size_t lineIndex;
    size_t runIndex;
    size_t headClusterIndex;
    size_t tailClusterIndex;
};
#ifdef USE_SKIA_TXT
void scaleFontWithCompressionConfig(RSFont& font, ScaleOP op);
void metricsIncludeFontPadding(RSFontMetrics* metrics, const RSFont& font);
#else
void scaleFontWithCompressionConfig(SkFont& font, ScaleOP op);
void metricsIncludeFontPadding(SkFontMetrics* metrics, const SkFont& font);
#endif
#endif

class Run {
public:
    Run(ParagraphImpl* owner,
        const SkShaper::RunHandler::RunInfo& info,
        size_t firstChar,
        SkScalar heightMultiplier,
        bool useHalfLeading,
        SkScalar baselineShift,
        size_t index,
        SkScalar shiftX);
    Run(const Run&) = default;
    Run& operator=(const Run&) = delete;
    Run(Run&&) = default;
    Run& operator=(Run&&) = delete;
    ~Run() = default;

    void setOwner(ParagraphImpl* owner) { fOwner = owner; }

    SkShaper::RunHandler::Buffer newRunBuffer();

    SkScalar posX(size_t index) const;
    void addX(size_t index, SkScalar shift) { fPositions[index].fX += shift; }
    SkScalar halfLetterspacing(size_t index) const { return fHalfLetterspacings[index]; }
    SkScalar posY(size_t index) const { return fPositions[index].fY; }
    size_t size() const { return fGlyphs.size(); }
    void setWidth(SkScalar width) { fAdvance.fX = width; }
    void setHeight(SkScalar height) { fAdvance.fY = height; }
    void shift(SkScalar shiftX, SkScalar shiftY) {
        fOffset.fX += shiftX;
        fOffset.fY += shiftY;
    }
    SkScalar fAdvanceX() const { return fAdvance.fX; }
    SkVector advance() const {
        return SkVector::Make(fAdvance.fX, fFontMetrics.fDescent - fFontMetrics.fAscent + fFontMetrics.fLeading);
    }
    SkVector offset() const { return fOffset; }
    SkScalar ascent() const { return fFontMetrics.fAscent + fBaselineShift; }
    SkScalar descent() const { return fFontMetrics.fDescent + fBaselineShift; }
    SkScalar leading() const { return fFontMetrics.fLeading; }
    SkScalar correctAscent() const { return fCorrectAscent + fBaselineShift; }
    SkScalar correctDescent() const { return fCorrectDescent + fBaselineShift; }
    SkScalar correctLeading() const { return fCorrectLeading; }
#ifndef USE_SKIA_TXT
    const SkFont& font() const { return fFont; }
#else
    const RSFont& font() const { return fFont; }
#endif
    bool leftToRight() const { return fBidiLevel % 2 == 0; }
    TextDirection getTextDirection() const { return leftToRight() ? TextDirection::kLtr : TextDirection::kRtl; }
    size_t index() const { return fIndex; }
    SkScalar heightMultiplier() const { return fHeightMultiplier; }
    bool useHalfLeading() const { return fUseHalfLeading; }
#ifdef OHOS_SUPPORT
    SkScalar getRunTotalShift() const { return fBaselineShift + getVerticalAlignShift(); }
#endif
    SkScalar baselineShift() const { return fBaselineShift; }
    PlaceholderStyle* placeholderStyle() const;
    bool isPlaceholder() const { return fPlaceholderIndex != std::numeric_limits<size_t>::max(); }
    size_t clusterIndex(size_t pos) const { return fClusterIndexes[pos]; }
#ifdef OHOS_SUPPORT
    size_t globalClusterIndex(size_t pos) const;
#else
    size_t globalClusterIndex(size_t pos) const { return fClusterStart + fClusterIndexes[pos]; }
#endif
    SkScalar positionX(size_t pos) const;
    SkScalar usingAutoSpaceWidth(const Cluster& cluster) const;
    TextRange textRange() const { return fTextRange; }
    ClusterRange clusterRange() const { return fClusterRange; }

    ParagraphImpl* owner() const { return fOwner; }

    bool isEllipsis() const { return fEllipsis; }

    void calculateMetrics();
    void updateMetrics(InternalLineMetrics* endlineMetrics);

    void setClusterRange(size_t from, size_t to) { fClusterRange = ClusterRange(from, to); }
    SkRect clip() const {
        return SkRect::MakeXYWH(fOffset.fX, fOffset.fY, fAdvance.fX, fAdvance.fY);
    }
    const SkSTArray<PARAM_64, SkPoint, true>& getAutoSpacings() const {
        return fAutoSpacings;
    }
    void addSpacesAtTheEnd(SkScalar space, Cluster* cluster);
    SkScalar addSpacesEvenly(SkScalar space, Cluster* cluster);
    SkScalar addSpacesEvenly(SkScalar space);
    void shift(const Cluster* cluster, SkScalar offset);

#ifdef OHOS_SUPPORT
    Run(const Run& run, size_t runIndex);
    size_t findSplitClusterPos(size_t target);
    void updateSplitRunRangeInfo(Run& splitRun, const TextLine& splitLine, size_t headIndex, size_t tailIndex);
    void updateSplitRunMesureInfo(Run& splitRun, size_t startClusterPos, size_t endClusterPos);
    void generateSplitRun(Run& splitRun, const SplitPoint& splitPoint);
    void extendClusterWidth(Cluster* cluster, SkScalar space);
    bool isTrailingSpaceIncluded(const ClusterRange& fTextLineClusterRange,
        const ClusterRange& fTextLineGhostClusterRange) const;
    void updatePlaceholderAlignmentIfNeeded(PlaceholderAlignment& alignment, TextVerticalAlign paragraphAlignment);
#endif
    SkScalar calculateHeight(LineMetricStyle ascentStyle, LineMetricStyle descentStyle) const {
        auto ascent = ascentStyle == LineMetricStyle::Typographic ? this->ascent()
                                    : this->correctAscent();
        auto descent = descentStyle == LineMetricStyle::Typographic ? this->descent()
                                      : this->correctDescent();
        return descent - ascent;
    }
    SkScalar calculateWidth(size_t start, size_t end, bool clip) const;

#ifndef USE_SKIA_TXT
    void copyTo(SkTextBlobBuilder& builder, size_t pos, size_t size) const;
#else
    void copyTo(RSTextBlobBuilder& builder, size_t pos, size_t size) const;
    void copyTo(RSTextBlobBuilder& builder,
                const RSPath* path,
                float hOffset,
                float vOffset,
                float fTextShift,
                size_t pos,
                size_t size) const;
#endif

    template<typename Visitor>
    void iterateThroughClustersInTextOrder(Visitor visitor);

#ifdef OHOS_SUPPORT
    template<typename Visitor>
    void iterateGlyphRangeInTextOrder(const GlyphRange& glyphRange, Visitor visitor);
    SkScalar getVerticalAlignShift() const { return fVerticalAlignShift; }
    void setVerticalAlignShift(SkScalar verticalAlignShift) { fVerticalAlignShift = verticalAlignShift; }
#endif

    using ClusterVisitor = std::function<void(Cluster* cluster)>;
    void iterateThroughClusters(const ClusterVisitor& visitor);

    std::tuple<bool, ClusterIndex, ClusterIndex> findLimitingClusters(TextRange text) const;
    std::tuple<bool, TextIndex, TextIndex> findLimitingGlyphClusters(TextRange text) const;
    std::tuple<bool, TextIndex, TextIndex> findLimitingGraphemes(TextRange text) const;
    SkSpan<const SkGlyphID> glyphs() const {
        return SkSpan<const SkGlyphID>(fGlyphs.begin(), fGlyphs.size());
    }
    SkSpan<const SkPoint> positions() const {
        return SkSpan<const SkPoint>(fPositions.begin(), fPositions.size());
    }
    SkSpan<const SkPoint> offsets() const {
        return SkSpan<const SkPoint>(fOffsets.begin(), fOffsets.size());
    }
    SkSpan<const uint32_t> clusterIndexes() const {
        return SkSpan<const uint32_t>(fClusterIndexes.begin(), fClusterIndexes.size());
    }
#ifdef OHOS_SUPPORT
    SkSpan<const SkPoint> advances() const {
        return SkSpan<const SkPoint>(fGlyphAdvances.begin(), fGlyphAdvances.size());
    }
#endif
    void commit() { }

    void resetJustificationShifts() {
        fJustificationShifts.reset();
    }

    void resetAutoSpacing() {
        fAutoSpacings.reset();
    }

    bool isResolved() const;

    SkScalar getTopInGroup() const { return fTopInGroup; }
    void setTopInGroup(SkScalar top) { fTopInGroup = top; }

    SkScalar getBottomInGroup() const { return fBottomInGroup; }
    void setBottomInGroup(SkScalar bottom) { fBottomInGroup = bottom; }

    SkScalar getMaxRoundRectRadius() const { return fMaxRoundRectRadius; }
    void setMaxRoundRectRadius(SkScalar radius) { fMaxRoundRectRadius = radius; }

    size_t getIndexInLine() const { return indexInLine; }
    void setIndexInLine(size_t index) { indexInLine = index; }

private:
    friend class ParagraphImpl;
    friend class TextLine;
    friend class InternalLineMetrics;
    friend class ParagraphCache;
    friend class OneLineShaper;

    ParagraphImpl* fOwner;
    TextRange fTextRange;
    ClusterRange fClusterRange;

#ifndef USE_SKIA_TXT
    SkFont fFont;
#else
    RSFont fFont;
#endif
    size_t fPlaceholderIndex;
    size_t fIndex;
    SkVector fAdvance;
    SkVector fOffset;
    TextIndex fClusterStart;
    SkShaper::RunHandler::Range fUtf8Range;

    // These fields are not modified after shaping completes and can safely be
    // shared among copies of the run that are held by different paragraphs.
    struct GlyphData {
        SkSTArray<64, SkGlyphID, true> glyphs;
        SkSTArray<64, SkPoint, true> positions;
        SkSTArray<64, SkPoint, true> offsets;
        SkSTArray<64, uint32_t, true> clusterIndexes;
#ifdef OHOS_SUPPORT
        SkSTArray<PARAM_64, SkPoint, true> advances;
#endif
    };
    std::shared_ptr<GlyphData> fGlyphData;
    SkSTArray<64, SkGlyphID, true>& fGlyphs;
    SkSTArray<64, SkPoint, true>& fPositions;
    SkSTArray<64, SkPoint, true>& fOffsets;
    SkSTArray<64, uint32_t, true>& fClusterIndexes;
#ifdef OHOS_SUPPORT
    SkSTArray<PARAM_64, SkPoint, true>& fGlyphAdvances;
#endif
    SkSTArray<64, SkPoint, true> fJustificationShifts; // For justification
                                                                   // (current and prev shifts)
    SkSTArray<PARAM_64, SkPoint, true> fAutoSpacings; // For auto spacing
                                                                   // (current and prev spacings)
    SkSTArray<PARAM_64, SkScalar, true> fHalfLetterspacings; // For letterspacing

#ifndef USE_SKIA_TXT
    SkFontMetrics fFontMetrics;
#else
    RSFontMetrics fFontMetrics;
#endif
    const SkScalar fHeightMultiplier;
    const bool fUseHalfLeading;
    const SkScalar fBaselineShift;
    SkScalar fCorrectAscent;
    SkScalar fCorrectDescent;
    SkScalar fCorrectLeading;

    bool fEllipsis;
    uint8_t fBidiLevel;

    SkScalar fTopInGroup = 0.0f;
    SkScalar fBottomInGroup = 0.0f;
    SkScalar fMaxRoundRectRadius = 0.0f;
    size_t indexInLine;
#ifdef OHOS_SUPPORT
    SkScalar fCompressionBaselineShift{0.0f};
    SkScalar fVerticalAlignShift{0.0f};
#endif
};

template<typename Visitor>
void Run::iterateThroughClustersInTextOrder(Visitor visitor) {
    // Can't figure out how to do it with one code for both cases without 100 ifs
    // Can't go through clusters because there are no cluster table yet
    if (leftToRight()) {
        size_t start = 0;
        size_t cluster = this->clusterIndex(start);
        for (size_t glyph = 1; glyph <= this->size(); ++glyph) {
            auto nextCluster = this->clusterIndex(glyph);
            if (nextCluster <= cluster) {
                continue;
            }

            visitor(start,
                    glyph,
                    fClusterStart + cluster,
                    fClusterStart + nextCluster,
                    this->calculateWidth(start, glyph, glyph == size()),
                    this->calculateHeight(LineMetricStyle::CSS, LineMetricStyle::CSS));

            start = glyph;
            cluster = nextCluster;
        }
    } else {
        size_t glyph = this->size();
        size_t cluster = this->fUtf8Range.begin();
        for (int32_t start = this->size() - 1; start >= 0; --start) {
#ifdef OHOS_SUPPORT
            size_t nextCluster =
                start == 0 ? this->fUtf8Range.end() : this->clusterIndex(start);
#else
            size_t nextCluster =
                    start == 0 ? this->fUtf8Range.end() : this->clusterIndex(start - 1);
#endif
            if (nextCluster <= cluster) {
                continue;
            }

            visitor(start,
                    glyph,
                    fClusterStart + cluster,
                    fClusterStart + nextCluster,
                    this->calculateWidth(start, glyph, glyph == 0),
                    this->calculateHeight(LineMetricStyle::CSS, LineMetricStyle::CSS));

            glyph = start;
            cluster = nextCluster;
        }
    }
}

#ifdef OHOS_SUPPORT
template<typename Visitor>
void Run::iterateGlyphRangeInTextOrder(const GlyphRange& glyphRange, Visitor visitor) {
    if (glyphRange.start >= glyphRange.end || glyphRange.end > size()) {
        return;
    }
    if (leftToRight()) {
        size_t start = glyphRange.start;
        size_t cluster = this->clusterIndex(start);
        for (size_t glyph = glyphRange.start + 1; glyph <= glyphRange.end; ++glyph) {
            auto nextCluster = this->clusterIndex(glyph);
            if (nextCluster <= cluster) {
                continue;
            }

            visitor(start, glyph, fClusterStart + cluster, fClusterStart + nextCluster);
            start = glyph;
            cluster = nextCluster;
        }
    } else {
        size_t glyph = glyphRange.end;
        size_t cluster = this->clusterIndex(glyphRange.end - 1);
        int32_t glyphStart = std::max((int32_t)glyphRange.start, 0);
        for (int32_t start = glyphRange.end - 1; start >= glyphStart; --start) {
            size_t nextCluster = start == 0 ? this->fUtf8Range.end() : this->clusterIndex(start - 1);
            if (nextCluster <= cluster) {
                continue;
            }

            visitor(start, glyph, fClusterStart + cluster, fClusterStart + nextCluster);
            glyph = start;
            cluster = nextCluster;
        }
    }
}
#endif

class Cluster {
public:

#ifdef OHOS_SUPPORT
    enum AutoSpacingFlag {
        NoFlag = 0,
        CJK,
        Western,
        Copyright
    };
#endif

    enum BreakType {
        None,
        GraphemeBreak,  // calculated for all clusters (UBRK_CHARACTER)
        SoftLineBreak,  // calculated for all clusters (UBRK_LINE & UBRK_CHARACTER)
        HardLineBreak,  // calculated for all clusters (UBRK_LINE)
    };

    Cluster()
            : fOwner(nullptr)
            , fRunIndex(EMPTY_RUN)
            , fTextRange(EMPTY_TEXT)
            , fGraphemeRange(EMPTY_RANGE)
            , fStart(0)
            , fEnd()
            , fWidth()
            , fHeight()
            , fHalfLetterSpacing(0.0) {}

    Cluster(ParagraphImpl* owner,
            RunIndex runIndex,
            size_t start,
            size_t end,
            SkSpan<const char> text,
            SkScalar width,
            SkScalar height);

    Cluster(TextRange textRange) : fTextRange(textRange), fGraphemeRange(EMPTY_RANGE) { }

    Cluster(const Cluster&) = default;
    ~Cluster() = default;

    SkScalar sizeToChar(TextIndex ch) const;
    SkScalar sizeFromChar(TextIndex ch) const;

    size_t roundPos(SkScalar s) const;

    void space(SkScalar shift) {
        fWidth += shift;
    }

    ParagraphImpl* getOwner() const { return fOwner; }
    void setOwner(ParagraphImpl* owner) { fOwner = owner; }

    bool isWhitespaceBreak() const { return fIsWhiteSpaceBreak; }
    bool isIntraWordBreak() const { return fIsIntraWordBreak; }
    bool isHardBreak() const { return fIsHardBreak; }
    bool isIdeographic() const { return fIsIdeographic; }
    bool isWordBreak() const { return isWhitespaceBreak() || isHardBreak() || isSoftBreak() || run().isPlaceholder(); }
#ifdef OHOS_SUPPORT
    bool isTabulation() const { return fIsTabulation; }
    bool isPunctuation() const { return fIsPunctuation; }
    bool isEllipsis() const { return fIsEllipsis; }
    bool needAutoSpacing() const { return fNeedAutoSpacing; }
    void enableHyphenBreak() { fHyphenBreak = true; }
    bool isHyphenBreak() const { return fHyphenBreak; }
    bool isStartCombineBreak() const;
    bool isEndCombineBreak() const;
#endif

    bool isSoftBreak() const;
    bool isGraphemeBreak() const;
    bool canBreakLineAfter() const { return isHardBreak() || isSoftBreak(); }
    size_t startPos() const { return fStart; }
    size_t endPos() const { return fEnd; }
    SkScalar width() const { return fWidth; }
    SkScalar height() const { return fHeight; }
    size_t size() const { return fEnd - fStart; }

    void setHalfLetterSpacing(SkScalar halfLetterSpacing) { fHalfLetterSpacing = halfLetterSpacing; }
    SkScalar getHalfLetterSpacing() const { return fHalfLetterSpacing; }

    TextRange textRange() const { return fTextRange; }

    RunIndex runIndex() const { return fRunIndex; }
    ParagraphImpl* owner() const { return fOwner; }

    Run* runOrNull() const;
    Run& run() const;
#ifndef USE_SKIA_TXT
    SkFont font() const;
#else
    RSFont font() const;
#endif

    SkScalar trimmedWidth(size_t pos) const;

    bool contains(TextIndex ch) const { return ch >= fTextRange.start && ch < fTextRange.end; }

    bool belongs(TextRange text) const {
        return fTextRange.start >= text.start && fTextRange.end <= text.end;
    }

    bool startsIn(TextRange text) const {
        return fTextRange.start >= text.start && fTextRange.start < text.end;
    }

#ifdef OHOS_SUPPORT
    SkScalar getFontSize() const {
        return font().GetSize();
    }

    TextBadgeType getBadgeType() const { return fBadgeType; }

    void setBadgeType(TextBadgeType badgeType) {
        fBadgeType = badgeType;
    }
#endif

private:

    friend ParagraphImpl;

    ParagraphImpl* fOwner;
    RunIndex fRunIndex;
    TextRange fTextRange;
    GraphemeRange fGraphemeRange;

    size_t fStart;
    size_t fEnd;
    SkScalar fWidth;
    SkScalar fHeight;
    SkScalar fHalfLetterSpacing;

    bool fIsWhiteSpaceBreak;
    bool fIsIntraWordBreak;
    bool fIsHardBreak;
    bool fIsIdeographic;
#ifdef OHOS_SUPPORT
    bool fIsTabulation;
    bool fIsPunctuation{false};
    bool fIsEllipsis{false};
    bool fNeedAutoSpacing{false}; // depend on last cluster flag
    bool fHyphenBreak{false};
    TextBadgeType fBadgeType{TextBadgeType::BADGE_NONE};
#endif
};

class InternalLineMetrics {
public:

    InternalLineMetrics() {
        clean();
        fForceStrut = false;
    }

    InternalLineMetrics(bool forceStrut) {
        clean();
        fForceStrut = forceStrut;
    }

    InternalLineMetrics(SkScalar a, SkScalar d, SkScalar l) {
        fAscent = a;
        fDescent = d;
        fLeading = l;
        fRawAscent = a;
        fRawDescent = d;
        fRawLeading = l;
        fForceStrut = false;
    }

    InternalLineMetrics(SkScalar a, SkScalar d, SkScalar l, SkScalar ra, SkScalar rd, SkScalar rl) {
        fAscent = a;
        fDescent = d;
        fLeading = l;
        fRawAscent = ra;
        fRawDescent = rd;
        fRawLeading = rl;
        fForceStrut = false;
    }

#ifdef OHOS_SUPPORT
#ifdef USE_SKIA_TXT
    InternalLineMetrics(const RSFont& font, bool forceStrut) {
        RSFontMetrics metrics;
        auto compressFont = font;
        scaleFontWithCompressionConfig(compressFont, ScaleOP::COMPRESS);
        compressFont.GetMetrics(&metrics);
#else
    InternalLineMetrics(const SkFont& font, bool forceStrut) {
        SkFontMetrics metrics;
        auto compressFont = font;
        scaleFontWithCompressionConfig(compressFont, ScaleOP::COMPRESS);
        compressFont.getMetrics(&metrics);
#endif
        metricsIncludeFontPadding(&metrics, font);
#else
    InternalLineMetrics(const SkFont& font, bool forceStrut) {
        SkFontMetrics metrics;
        font.getMetrics(&metrics);
#endif
        fAscent = metrics.fAscent;
        fDescent = metrics.fDescent;
        fLeading = metrics.fLeading;
        fRawAscent = metrics.fAscent;
        fRawDescent = metrics.fDescent;
        fRawLeading = metrics.fLeading;
        fForceStrut = forceStrut;
    }

    void add(Run* run) {
        if (fForceStrut) {
            return;
        }
        fAscent = std::min(fAscent, run->correctAscent());
        fDescent = std::max(fDescent, run->correctDescent());
        fLeading = std::max(fLeading, run->correctLeading());

        fRawAscent = std::min(fRawAscent, run->ascent());
        fRawDescent = std::max(fRawDescent, run->descent());
        fRawLeading = std::max(fRawLeading, run->leading());
    }

    void add(InternalLineMetrics other) {
        fAscent = std::min(fAscent, other.fAscent);
        fDescent = std::max(fDescent, other.fDescent);
        fLeading = std::max(fLeading, other.fLeading);
        fRawAscent = std::min(fRawAscent, other.fRawAscent);
        fRawDescent = std::max(fRawDescent, other.fRawDescent);
        fRawLeading = std::max(fRawLeading, other.fRawLeading);
    }

    void clean() {
        fAscent = SK_ScalarMax;
        fDescent = SK_ScalarMin;
        fLeading = 0;
        fRawAscent = SK_ScalarMax;
        fRawDescent = SK_ScalarMin;
        fRawLeading = 0;
    }

    bool isClean() {
        return (fAscent == SK_ScalarMax &&
                fDescent == SK_ScalarMin &&
                fLeading == 0 &&
                fRawAscent == SK_ScalarMax &&
                fRawDescent == SK_ScalarMin &&
                fRawLeading == 0);
    }

    SkScalar delta() const { return height() - ideographicBaseline(); }

    void updateLineMetrics(InternalLineMetrics& metrics) {
        if (metrics.fForceStrut) {
            metrics.fAscent = fAscent;
            metrics.fDescent = fDescent;
            metrics.fLeading = fLeading;
            metrics.fRawAscent = fRawAscent;
            metrics.fRawDescent = fRawDescent;
            metrics.fRawLeading = fRawLeading;
        } else {
            // This is another of those flutter changes. To be removed...
            metrics.fAscent = std::min(metrics.fAscent, fAscent - fLeading / 2.0f);
            metrics.fDescent = std::max(metrics.fDescent, fDescent + fLeading / 2.0f);
            metrics.fRawAscent = std::min(metrics.fRawAscent, fRawAscent - fRawLeading / 2.0f);
            metrics.fRawDescent = std::max(metrics.fRawDescent, fRawDescent + fRawLeading / 2.0f);
        }
    }

#ifdef OHOS_SUPPORT
    SkScalar runTop(const Run* run, LineMetricStyle ascentStyle) const {
        return fLeading / 2 - fAscent + (ascentStyle == LineMetricStyle::Typographic ?
            run->ascent() : run->correctAscent() + run->getVerticalAlignShift()) + delta();
    }
#else
    SkScalar runTop(const Run* run, LineMetricStyle ascentStyle) const {
        return fLeading / 2 - fAscent +
          (ascentStyle == LineMetricStyle::Typographic ? run->ascent() : run->correctAscent()) + delta();
    }
#endif

    SkScalar height() const {
        return ::round((double)fDescent - fAscent + fLeading);
    }

    void update(SkScalar a, SkScalar d, SkScalar l) {
        fAscent = a;
        fDescent = d;
        fLeading = l;
    }

    void updateRawData(SkScalar ra, SkScalar rd) {
        fRawAscent = ra;
        fRawDescent = rd;
    }

    SkScalar alphabeticBaseline() const { return fLeading / 2 - fAscent; }
    SkScalar ideographicBaseline() const { return fDescent - fAscent + fLeading; }
    SkScalar deltaBaselines() const { return fLeading / 2 + fDescent; }
    SkScalar baseline() const { return fLeading / 2 - fAscent; }
    SkScalar ascent() const { return fAscent; }
    SkScalar descent() const { return fDescent; }
    SkScalar leading() const { return fLeading; }
    SkScalar rawAscent() const { return fRawAscent; }
    SkScalar rawDescent() const { return fRawDescent; }
    void setForceStrut(bool value) { fForceStrut = value; }
    bool getForceStrut() const { return fForceStrut; }

private:

    friend class ParagraphImpl;
    friend class TextWrapper;
    friend class TextLine;

    SkScalar fAscent;
    SkScalar fDescent;
    SkScalar fLeading;

    SkScalar fRawAscent;
    SkScalar fRawDescent;
    SkScalar fRawLeading;

    bool fForceStrut;
};
}  // namespace textlayout
}  // namespace skia

#endif  // Run_DEFINED
