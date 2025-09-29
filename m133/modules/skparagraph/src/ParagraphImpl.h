// Copyright 2019 Google LLC.
#ifndef ParagraphImpl_DEFINED
#define ParagraphImpl_DEFINED

#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPicture.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSpan.h"
#include "include/core/SkString.h"
#include "include/core/SkTypes.h"
#include "include/private/base/SkOnce.h"
#include "include/private/base/SkTArray.h"
#include "include/private/base/SkTemplates.h"
#include "modules/skparagraph/include/DartTypes.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphCache.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#ifdef ENABLE_TEXT_ENHANCE
#include "modules/skparagraph/include/TextLineBase.h"
#endif
#include "modules/skparagraph/include/TextShadow.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/src/Run.h"
#include "modules/skparagraph/src/TextLine.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "src/base/SkBitmaskEnum.h"
#include "src/core/SkTHash.h"

#include <memory>
#include <string>
#include <vector>

class SkCanvas;

namespace skia {
namespace textlayout {

class LineMetrics;
class TextLine;

template <typename T> bool operator==(const SkSpan<T>& a, const SkSpan<T>& b) {
    return a.size() == b.size() && a.begin() == b.begin();
}

template <typename T> bool operator<=(const SkSpan<T>& a, const SkSpan<T>& b) {
    return a.begin() >= b.begin() && a.end() <= b.end();
}

template <typename TStyle>
struct StyleBlock {
    StyleBlock() : fRange(EMPTY_RANGE), fStyle() { }
    StyleBlock(size_t start, size_t end, const TStyle& style) : fRange(start, end), fStyle(style) {}
    StyleBlock(TextRange textRange, const TStyle& style) : fRange(textRange), fStyle(style) {}
    void add(TextRange tail) {
        SkASSERT(fRange.end == tail.start);
        fRange = TextRange(fRange.start, fRange.start + fRange.width() + tail.width());
    }
    TextRange fRange;
    TStyle fStyle;
};

struct ResolvedFontDescriptor {
#ifdef ENABLE_TEXT_ENHANCE
    ResolvedFontDescriptor(TextIndex index, RSFont font)
        : fFont(font), fTextStart(index) { }
    RSFont fFont;
#else
    ResolvedFontDescriptor(TextIndex index, SkFont font)
            : fFont(std::move(font)), fTextStart(index) {}
    SkFont fFont;
#endif
    TextIndex fTextStart;
};

#ifndef ENABLE_TEXT_ENHANCE
enum InternalState {
  kUnknown = 0,
  kIndexed = 1,     // Text is indexed
  kShaped = 2,      // Text is shaped
  kLineBroken = 5,
  kFormatted = 6,
  kDrawn = 7
};
#endif

/*
struct BidiRegion {
    BidiRegion(size_t start, size_t end, uint8_t dir)
        : text(start, end), direction(dir) { }
    TextRange text;
    uint8_t direction;
};
*/
class ParagraphImpl final : public Paragraph {

public:
#ifdef ENABLE_TEXT_ENHANCE
    ParagraphImpl() = default;

#endif // ENABLE_TEXT_ENHANCE

    ParagraphImpl(const SkString& text,
                  ParagraphStyle style,
                  skia_private::TArray<Block, true> blocks,
                  skia_private::TArray<Placeholder, true> placeholders,
                  sk_sp<FontCollection> fonts,
                  sk_sp<SkUnicode> unicode);

    ParagraphImpl(const std::u16string& utf16text,
                  ParagraphStyle style,
                  skia_private::TArray<Block, true> blocks,
                  skia_private::TArray<Placeholder, true> placeholders,
                  sk_sp<FontCollection> fonts,
                  sk_sp<SkUnicode> unicode);

    ~ParagraphImpl() override;

    void layout(SkScalar width) override;
    void paint(SkCanvas* canvas, SkScalar x, SkScalar y) override;
    void paint(ParagraphPainter* canvas, SkScalar x, SkScalar y) override;
#ifdef ENABLE_TEXT_ENHANCE
    void paint(ParagraphPainter* canvas, RSPath* path, SkScalar hOffset, SkScalar vOffset) override;
    std::string GetDumpInfo() const override;
#endif // ENABLE_TEXT_ENHANCE
    std::vector<TextBox> getRectsForRange(unsigned start,
                                          unsigned end,
                                          RectHeightStyle rectHeightStyle,
                                          RectWidthStyle rectWidthStyle) override;
    std::vector<TextBox> getRectsForPlaceholders() override;
    void getLineMetrics(std::vector<LineMetrics>&) override;
    PositionWithAffinity getGlyphPositionAtCoordinate(SkScalar dx, SkScalar dy) override;
    SkRange<size_t> getWordBoundary(unsigned offset) override;
#ifdef ENABLE_TEXT_ENHANCE
    bool getApplyRoundingHack() const { return false; }
    size_t lineNumber() override { return fLineNumber; }
    TextRange getEllipsisTextRange() override;
    bool isRunCombinated() const override { return fRuns.size() < fTextStyles.size(); }
#else
    bool getApplyRoundingHack() const { return fParagraphStyle.getApplyRoundingHack(); }
    size_t lineNumber() override { return fLines.size(); }
#endif

    TextLine& addLine(SkVector offset, SkVector advance,
                      TextRange textExcludingSpaces, TextRange text, TextRange textIncludingNewlines,
                      ClusterRange clusters, ClusterRange clustersWithGhosts, SkScalar widthWithSpaces,
                      InternalLineMetrics sizes);

    SkSpan<const char> text() const { return SkSpan<const char>(fText.c_str(), fText.size()); }

#ifdef ENABLE_TEXT_ENHANCE
    std::vector<SkUnichar> convertUtf8ToUnicode(const SkString& utf8);
    std::unique_ptr<Paragraph> createCroppedCopy(
            size_t startIndex, size_t count = std::numeric_limits<size_t>::max()) override;
    void initUnicodeText() override;
    const std::vector<SkUnichar>& unicodeText() const override { return fUnicodeText; }
    size_t getUnicodeIndex(TextIndex index) const override {
        if (index >= fUnicodeIndexForUTF8Index.size()) {
            return fUnicodeIndexForUTF8Index.empty() ? 0 : fUnicodeIndexForUTF8Index.back() + 1;
        }
        return fUnicodeIndexForUTF8Index[index];
    }
#endif
    InternalState state() const { return fState; }
    SkSpan<Run> runs() { return SkSpan<Run>(fRuns.data(), fRuns.size()); }
    SkSpan<Block> styles() {
        return SkSpan<Block>(fTextStyles.data(), fTextStyles.size());
    }
    SkSpan<Placeholder> placeholders() {
        return SkSpan<Placeholder>(fPlaceholders.data(), fPlaceholders.size());
    }
    SkSpan<TextLine> lines() { return SkSpan<TextLine>(fLines.data(), fLines.size()); }
    const ParagraphStyle& paragraphStyle() const { return fParagraphStyle; }
    SkSpan<Cluster> clusters() { return SkSpan<Cluster>(fClusters.begin(), fClusters.size()); }
    sk_sp<FontCollection> fontCollection() const { return fFontCollection; }
    void formatLines(SkScalar maxWidth);
    void ensureUTF16Mapping();
    skia_private::TArray<TextIndex> countSurroundingGraphemes(TextRange textRange) const;
    TextIndex findNextGraphemeBoundary(TextIndex utf8) const;
    TextIndex findPreviousGraphemeBoundary(TextIndex utf8) const;
    TextIndex findNextGlyphClusterBoundary(TextIndex utf8) const;
    TextIndex findPreviousGlyphClusterBoundary(TextIndex utf8) const;
    size_t getUTF16Index(TextIndex index) const {
        return fUTF16IndexForUTF8Index[index];
    }

#ifdef ENABLE_TEXT_ENHANCE
    size_t getUTF16IndexWithOverflowCheck(TextIndex index) const {
        if (index >= fUTF16IndexForUTF8Index.size()) {
            // This branch is entered only if the index of the ellipsis exceeds the size of the fUTF16IndexForUTF8Index
            return fUTF16IndexForUTF8Index.back();
        }
        return fUTF16IndexForUTF8Index[index];
    }
    WordBreakType getWordBreakType() const;
    LineBreakStrategy getLineBreakStrategy() const;
    void positionShapedTextIntoLine(SkScalar maxWidth);
    void buildClusterPlaceholder(Run& run, size_t runIndex);
    std::vector<ParagraphPainter::PaintID> updateColor(size_t from, size_t to, SkColor color,
        UtfEncodeType encodeType) override;
    SkIRect generatePaintRegion(SkScalar x, SkScalar y) override;
    skia_private::TArray<Block, true>& exportTextStyles() override { return fTextStyles; }
    bool preCalculateSingleRunAutoSpaceWidth(SkScalar floorWidth);
    void setIndents(const std::vector<SkScalar>& indents) override;
    SkScalar detectIndents(size_t index) override;
    SkScalar getTextSplitRatio() const override { return fParagraphStyle.getTextSplitRatio(); }
    std::vector<std::unique_ptr<TextLineBase>> GetTextLines() override;
    std::unique_ptr<Paragraph> CloneSelf() override;
    uint32_t& hash() {
        return hash_;
    }
    RSFontMetrics measureText() override;
    bool GetLineFontMetrics(const size_t lineNumber, size_t& charNumber,
        std::vector<RSFontMetrics>& fontMetrics) override;
    size_t GetMaxLines() const override { return fParagraphStyle.getMaxLines(); }
    void setLastAutoSpacingFlag(Cluster::AutoSpacingFlag flag) { fLastAutoSpacingFlag = flag; }
    const Cluster::AutoSpacingFlag& getLastAutoSpacingFlag() const { return fLastAutoSpacingFlag; }
    void resetAutoSpacing() {
        for (auto& run : fRuns) {
            run.resetAutoSpacing();
        }
    }
    bool isAutoSpaceEnabled() const;
    SkScalar clusterUsingAutoSpaceWidth(const Cluster& cluster) const;
    const SkString& getText() const { return fText; }
    void updateSplitRunClusterInfo(const Run& run, bool isSplitRun);
    void refreshLines();
    bool isTailOfLineNeedSplit(const Run& lineLastRun, size_t lineEnd, bool hasGenerated);
    void generateSplitPoint(
        std::vector<SplitPoint>& splitPoints, const Run& run, ClusterRange lineRange, size_t lineIndex);
    void generateSplitPoints(std::vector<SplitPoint>& splitPoints);
    void generateRunsBySplitPoints(std::vector<SplitPoint>& splitPoints, skia_private::TArray<Run, false>& runs);
    void splitRuns();
#endif

    bool strutEnabled() const { return paragraphStyle().getStrutStyle().getStrutEnabled(); }
    bool strutForceHeight() const {
        return paragraphStyle().getStrutStyle().getForceStrutHeight();
    }
    bool strutHeightOverride() const {
        return paragraphStyle().getStrutStyle().getHeightOverride();
    }
    InternalLineMetrics strutMetrics() const { return fStrutMetrics; }

    SkString getEllipsis() const;

    SkSpan<const char> text(TextRange textRange);
    SkSpan<Cluster> clusters(ClusterRange clusterRange);
    Cluster& cluster(ClusterIndex clusterIndex);
    ClusterIndex clusterIndex(TextIndex textIndex) {
        auto clusterIndex = this->fClustersIndexFromCodeUnit[textIndex];
        SkASSERT(clusterIndex != EMPTY_INDEX);
        return clusterIndex;
    }
    Run& run(RunIndex runIndex) {
        SkASSERT(runIndex < SkToSizeT(fRuns.size()));
        return fRuns[runIndex];
    }

    Run& runByCluster(ClusterIndex clusterIndex);
    SkSpan<Block> blocks(BlockRange blockRange);
    Block& block(BlockIndex blockIndex);
    skia_private::TArray<ResolvedFontDescriptor> resolvedFonts() const { return fFontSwitches; }

    void markDirty() override {
        if (fState > kIndexed) {
            fState = kIndexed;
        }
    }

    int32_t unresolvedGlyphs() override;
    std::unordered_set<SkUnichar> unresolvedCodepoints() override;
    void addUnresolvedCodepoints(TextRange textRange);

#ifdef ENABLE_TEXT_ENHANCE
    void setState(InternalState state) override;
    InternalState getState() const override { return state(); }
    std::vector<TextBlobRecordInfo> getTextBlobRecordInfo() override;
    bool hasSkipTextBlobDrawing() const override { return fSkipTextBlobDrawing; }
    void setSkipTextBlobDrawing(bool state) override { fSkipTextBlobDrawing = state; }
    bool canPaintAllText() const override;
#else
    void setState(InternalState state);
#endif

    sk_sp<SkPicture> getPicture() { return fPicture; }

    SkScalar widthWithTrailingSpaces() { return fMaxWidthWithTrailingSpaces; }

    void resetContext();
    void resolveStrut();

    bool computeCodeUnitProperties();
    void applySpacingAndBuildClusterTable();
    void buildClusterTable();
    bool shapeTextIntoEndlessLine();
    void breakShapedTextIntoLines(SkScalar maxWidth);

    void updateTextAlign(TextAlign textAlign) override;
    void updateFontSize(size_t from, size_t to, SkScalar fontSize) override;
    void updateForegroundPaint(size_t from, size_t to, SkPaint paint) override;
    void updateBackgroundPaint(size_t from, size_t to, SkPaint paint) override;

    void visit(const Visitor&) override;
#ifndef ENABLE_TEXT_ENHANCE
    void extendedVisit(const ExtendedVisitor&) override;
    int getPath(int lineNumber, SkPath* dest) override;
#endif
    bool containsColorFontOrBitmap(SkTextBlob* textBlob) override;
    bool containsEmoji(SkTextBlob* textBlob) override;

    int getLineNumberAt(TextIndex codeUnitIndex) const override;
    int getLineNumberAtUTF16Offset(size_t codeUnitIndex) override;
    bool getLineMetricsAt(int lineNumber, LineMetrics* lineMetrics) const override;
    TextRange getActualTextRange(int lineNumber, bool includeSpaces) const override;
    bool getGlyphClusterAt(TextIndex codeUnitIndex, GlyphClusterInfo* glyphInfo) override;
    bool getClosestGlyphClusterAt(SkScalar dx,
                                  SkScalar dy,
                                  GlyphClusterInfo* glyphInfo) override;

    bool getGlyphInfoAtUTF16Offset(size_t codeUnitIndex, GlyphInfo* graphemeInfo) override;
    bool getClosestUTF16GlyphInfoAt(SkScalar dx, SkScalar dy, GlyphInfo* graphemeInfo) override;

#ifdef ENABLE_TEXT_ENHANCE
    RSFont getFontAt(TextIndex codeUnitIndex) const override;
#else
    SkFont getFontAt(TextIndex codeUnitIndex) const override;
#endif
#ifndef ENABLE_TEXT_ENHANCE
    SkFont getFontAtUTF16Offset(size_t codeUnitIndex) override;
#endif
    std::vector<FontInfo> getFonts() const override;

    InternalLineMetrics getEmptyMetrics() const { return fEmptyMetrics; }
    InternalLineMetrics getStrutMetrics() const { return fStrutMetrics; }

    BlockRange findAllBlocks(TextRange textRange);

    void resetShifts() {
        for (auto& run : fRuns) {
            run.resetJustificationShifts();
        }
    }

    bool codeUnitHasProperty(size_t index, SkUnicode::CodeUnitFlags property) const {
        return (fCodeUnitProperties[index] & property) == property;
    }
    sk_sp<SkUnicode> getUnicode() { return fUnicode; }

#ifdef ENABLE_TEXT_ENHANCE
    bool needCreateMiddleEllipsis();
    Placeholder* getPlaceholderByIndex(size_t placeholderIndex);
    bool IsPlaceholderAlignedFollowParagraph(size_t placeholderIndex);
    bool setPlaceholderAlignment(size_t placeholderIndex, PlaceholderAlignment alignment);
    Block& getBlockByRun(const Run& run);
    int getEllipsisRunIndexOffset() const { return fEllipsisRunIndexOffset; }
    void setEllipsisRunIndexOffset(int offset) { fEllipsisRunIndexOffset = offset; }
    bool IsEllipsisReplaceFitCluster() const { return fIsEllipsisReplaceFitCluster; }
    void setIsEllipsisReplaceFitCluster(bool state) { fIsEllipsisReplaceFitCluster = state; }
#endif
private:
    friend class ParagraphBuilder;
    friend class ParagraphCacheKey;
    friend class ParagraphCacheValue;
    friend class ParagraphCache;

    friend class TextWrapper;
    friend class OneLineShaper;
    void computeEmptyMetrics();
#ifdef ENABLE_TEXT_ENHANCE
    friend struct TextWrapScorer;
    TextRange resetRangeWithDeletedRange(const TextRange& sourceRange,
        const TextRange& deletedRange, const size_t& ellSize);
    void resetTextStyleRange(const TextRange& deletedRange);
    void resetPlaceholderRange(const TextRange& deletedRange);
    void setSize(SkScalar height, SkScalar width, SkScalar longestLine) {
        fHeight = height;
        fWidth = width;
        fLongestLine = longestLine;
    }
    void getSize(SkScalar& height, SkScalar& width, SkScalar& longestLine) {
        height = fHeight;
        width = fWidth;
        longestLine = fLongestLine;
    }
    void setIntrinsicSize(SkScalar maxIntrinsicWidth, SkScalar minIntrinsicWidth, SkScalar alphabeticBaseline,
                          SkScalar ideographicBaseline, bool exceededMaxLines) {
        fMaxIntrinsicWidth = maxIntrinsicWidth;
        fMinIntrinsicWidth = minIntrinsicWidth;
        fAlphabeticBaseline = alphabeticBaseline;
        fIdeographicBaseline = ideographicBaseline;
        fExceededMaxLines = exceededMaxLines;
    }
    void getIntrinsicSize(SkScalar& maxIntrinsicWidth, SkScalar& minIntrinsicWidth, SkScalar& alphabeticBaseline,
                          SkScalar& ideographicBaseline, bool& exceededMaxLines) {
        maxIntrinsicWidth = fMaxIntrinsicWidth;
        minIntrinsicWidth = fMinIntrinsicWidth;
        alphabeticBaseline = fAlphabeticBaseline ;
        ideographicBaseline = fIdeographicBaseline;
        exceededMaxLines = fExceededMaxLines;
    }

    ParagraphPainter::PaintID updateTextStyleColorAndForeground(TextStyle& TextStyle, SkColor color);
    TextBox getEmptyTextRect(RectHeightStyle rectHeightStyle) const;
    size_t prefixByteCountUntilChar(size_t index);
    void copyProperties(const ParagraphImpl& source);
    std::string_view GetState() const;
#endif

    // Input
    skia_private::TArray<StyleBlock<SkScalar>> fLetterSpaceStyles;
    skia_private::TArray<StyleBlock<SkScalar>> fWordSpaceStyles;
    skia_private::TArray<StyleBlock<SkPaint>> fBackgroundStyles;
    skia_private::TArray<StyleBlock<SkPaint>> fForegroundStyles;
    skia_private::TArray<StyleBlock<std::vector<TextShadow>>> fShadowStyles;
    skia_private::TArray<StyleBlock<Decoration>> fDecorationStyles;
    skia_private::TArray<Block, true> fTextStyles; // TODO: take out only the font stuff
    skia_private::TArray<Placeholder, true> fPlaceholders;
    SkString fText;

    // Internal structures
    InternalState fState;
    skia_private::TArray<Run, false> fRuns;         // kShaped
    skia_private::TArray<Cluster, true> fClusters;  // kClusterized (cached: text, word spacing, letter spacing, resolved fonts)
    skia_private::TArray<SkUnicode::CodeUnitFlags, true> fCodeUnitProperties;
    skia_private::TArray<size_t, true> fClustersIndexFromCodeUnit;
    std::vector<size_t> fWords;
    std::vector<SkUnicode::BidiRegion> fBidiRegions;
    // These two arrays are used in measuring methods (getRectsForRange, getGlyphPositionAtCoordinate)
    // They are filled lazily whenever they need and cached
    skia_private::TArray<TextIndex, true> fUTF8IndexForUTF16Index;
    skia_private::TArray<size_t, true> fUTF16IndexForUTF8Index;
    SkOnce fillUTF16MappingOnce;
    size_t fUnresolvedGlyphs;
    std::unordered_set<SkUnichar> fUnresolvedCodepoints;

    skia_private::TArray<TextLine, false> fLines;   // kFormatted   (cached: width, max lines, ellipsis, text align)
    sk_sp<SkPicture> fPicture;          // kRecorded    (cached: text styles)

    skia_private::TArray<ResolvedFontDescriptor> fFontSwitches;

    InternalLineMetrics fEmptyMetrics;
    InternalLineMetrics fStrutMetrics;

    SkScalar fOldWidth;
    SkScalar fOldHeight;
    SkScalar fMaxWidthWithTrailingSpaces;

    sk_sp<SkUnicode> fUnicode;
    bool fHasLineBreaks;
    bool fHasWhitespacesInside;
    TextIndex fTrailingSpaces;

#ifdef ENABLE_TEXT_ENHANCE
    std::vector<SkScalar> fIndents;
    std::vector<SkUnichar> fUnicodeText;
    skia_private::TArray<size_t, true> fUnicodeIndexForUTF8Index;
    SkScalar fLayoutRawWidth{0};

    size_t fLineNumber{0};
    uint32_t hash_{0u};

    TextRange fEllipsisRange{EMPTY_RANGE};
    std::optional<SkRect> fPaintRegion;
    // just for building cluster table, record the last built unicode autospacing flag;
    Cluster::AutoSpacingFlag fLastAutoSpacingFlag;
    bool fSkipTextBlobDrawing{false};
    int fEllipsisRunIndexOffset{0};
    bool fIsEllipsisReplaceFitCluster{false};
#endif
};
}  // namespace textlayout
}  // namespace skia


#endif  // ParagraphImpl_DEFINED
