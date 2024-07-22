// Copyright 2019 Google LLC.
#ifndef Paragraph_DEFINED
#define Paragraph_DEFINED

#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Metrics.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/include/TextLineBase.h"
#include <unordered_set>
#include "drawing.h"

class SkCanvas;

namespace skia {
namespace textlayout {

class ParagraphPainter;

class Paragraph {

public:
    Paragraph();

    Paragraph(ParagraphStyle style, sk_sp<FontCollection> fonts);

    virtual ~Paragraph() = default;

    SkScalar getMaxWidth() { return fWidth; }

    SkScalar getHeight() { return fHeight; }

    SkScalar getMinIntrinsicWidth() { return fMinIntrinsicWidth; }

    SkScalar getMaxIntrinsicWidth() { return fMaxIntrinsicWidth; }

    SkScalar getAlphabeticBaseline() { return fAlphabeticBaseline; }

    SkScalar getIdeographicBaseline() { return fIdeographicBaseline; }

    SkScalar getLongestLine() { return fLongestLine; }

    SkScalar getLongestLineWithIndent() { return fLongestLineWithIndent; }

    void setLongestLineWithIndent(SkScalar longestLineWithIndent) {
        fLongestLineWithIndent = longestLineWithIndent;
    }

    SkScalar getGlyphsBoundsTop() { return fGlyphsBoundsTop; }

    SkScalar getGlyphsBoundsBottom() { return fGlyphsBoundsBottom; }

    SkScalar getGlyphsBoundsLeft() { return fGlyphsBoundsLeft; }

    SkScalar getGlyphsBoundsRight() { return fGlyphsBoundsRight; }

    bool didExceedMaxLines() { return fExceededMaxLines; }

    virtual void layout(SkScalar width) = 0;

    virtual void paint(SkCanvas* canvas, SkScalar x, SkScalar y) = 0;

    virtual void paint(ParagraphPainter* painter, SkScalar x, SkScalar y) = 0;
#ifdef USE_SKIA_TXT
    virtual void paint(ParagraphPainter* painter, RSPath* path, SkScalar hOffset, SkScalar vOffset) = 0;
#endif
    // Returns a vector of bounding boxes that enclose all text between
    // start and end glyph indexes, including start and excluding end
    virtual std::vector<TextBox> getRectsForRange(unsigned start,
                                                  unsigned end,
                                                  RectHeightStyle rectHeightStyle,
                                                  RectWidthStyle rectWidthStyle) = 0;

    virtual std::vector<TextBox> getRectsForPlaceholders() = 0;

    // Returns the index of the glyph that corresponds to the provided coordinate,
    // with the top left corner as the origin, and +y direction as down
    virtual PositionWithAffinity getGlyphPositionAtCoordinate(SkScalar dx, SkScalar dy) = 0;

    // Finds the first and last glyphs that define a word containing
    // the glyph at index offset
    virtual SkRange<size_t> getWordBoundary(unsigned offset) = 0;

    virtual void getLineMetrics(std::vector<LineMetrics>&) = 0;

    virtual size_t lineNumber() = 0;

    virtual void markDirty() = 0;

    // This function will return the number of unresolved glyphs or
    // -1 if not applicable (has not been shaped yet - valid case)
    virtual int32_t unresolvedGlyphs() = 0;
    virtual std::unordered_set<SkUnichar> unresolvedCodepoints() = 0;

    // Experimental API that allows fast way to update some of "immutable" paragraph attributes
    // but not the text itself
    virtual void updateTextAlign(TextAlign textAlign) = 0;
    virtual void updateFontSize(size_t from, size_t to, SkScalar fontSize) = 0;
    virtual void updateForegroundPaint(size_t from, size_t to, SkPaint paint) = 0;
    virtual void updateBackgroundPaint(size_t from, size_t to, SkPaint paint) = 0;
    virtual std::vector<ParagraphPainter::PaintID> updateColor(size_t from, size_t to, SkColor color) = 0;

    enum VisitorFlags {
        kWhiteSpace_VisitorFlag = 1 << 0,
    };
    struct VisitorInfo {
        const SkFont&   font;
        SkPoint         origin;
        SkScalar        advanceX;
        int             count;
        const uint16_t* glyphs;     // count values
        const SkPoint*  positions;  // count values
        const uint32_t* utf8Starts; // count+1 values
        unsigned        flags;
    };

    // lineNumber begins at 0. If info is null, this signals the end of that line.
    using Visitor = std::function<void(int lineNumber, const VisitorInfo*)>;
    virtual void visit(const Visitor&) = 0;

    // Editing API
    virtual int getLineNumberAt(TextIndex codeUnitIndex) const = 0;

    /* Returns line metrics info for the line
     *
     * @param lineNumber    a line number
     * @param lineMetrics   an address to return the info (in case of null just skipped)
     * @return              true if the line is found; false if not
     */
    virtual bool getLineMetricsAt(int lineNumber, LineMetrics* lineMetrics) const = 0;

    /* Returns the visible text on the line (excluding a possible ellipsis)
     *
     * @param lineNumber    a line number
     * @param includeSpaces indicates if the whitespaces should be included
     * @return              the range of the text that is shown in the line
     */
    virtual TextRange getActualTextRange(int lineNumber, bool includeSpaces) const = 0;

    struct GlyphClusterInfo {
        SkRect fBounds;
        TextRange fClusterTextRange;
        TextDirection fGlyphClusterPosition;
    };

    /** Finds a glyph cluster for text index
     *
     * @param codeUnitIndex   a text index
     * @param glyphInfo       a glyph cluster info filled if not null
     * @return                true if glyph cluster was found; false if not
     */
    virtual bool getGlyphClusterAt(TextIndex codeUnitIndex, GlyphClusterInfo* glyphInfo) = 0;

    /** Finds the closest glyph cluster for a visual text position
     *
     * @param dx              x coordinate
     * @param dy              y coordinate
     * @param glyphInfo       a glyph cluster info filled if not null
     * @return
     */
    virtual bool getClosestGlyphClusterAt(SkScalar dx,
                                          SkScalar dy,
                                          GlyphClusterInfo* glyphInfo) = 0;

#ifndef USE_SKIA_TXT
    struct FontInfo {
        FontInfo(const SkFont font, const TextRange textRange)
            : fFont(font), fTextRange(textRange) { }
        virtual ~FontInfo() = default;
        FontInfo(const FontInfo& ) = default;
        SkFont fFont;
        TextRange fTextRange;
    };
#else
    struct FontInfo {
        FontInfo(const RSFont font, const TextRange textRange)
            : fFont(font), fTextRange(textRange) { }
        virtual ~FontInfo() = default;
        FontInfo(const FontInfo& ) = default;
        RSFont fFont;
        TextRange fTextRange;
    };
#endif

    struct TextCutRecord {
        size_t charbegin;
        size_t charOver;
        SkScalar phraseWidth;
    };

    /** Returns the font that is used to shape the text at the position
     *
     * @param codeUnitIndex   text index
     * @return                font info or an empty font info if the text is not found
     */
#ifndef USE_SKIA_TXT
    virtual SkFont getFontAt(TextIndex codeUnitIndex) const = 0;
#else
    virtual RSFont getFontAt(TextIndex codeUnitIndex) const = 0;
#endif

    /** Returns the information about all the fonts used to shape the paragraph text
     *
     * @return                a list of fonts and text ranges
     */
    virtual std::vector<FontInfo> getFonts() const = 0;

    virtual void setIndents(const std::vector<SkScalar>& indents) = 0;

    virtual SkScalar detectIndents(size_t index) = 0;

    virtual SkScalar getTextSplitRatio() const = 0;

#ifndef USE_SKIA_TXT
    virtual SkFontMetrics measureText() = 0;
#else
    virtual RSFontMetrics measureText() = 0;
#endif

#ifndef USE_SKIA_TXT
    virtual bool GetLineFontMetrics(const size_t lineNumber, size_t& charNumber,
        std::vector<SkFontMetrics>& fontMetrics) = 0;
#else
    virtual bool GetLineFontMetrics(const size_t lineNumber, size_t& charNumber,
        std::vector<RSFontMetrics>& fontMetrics) = 0;
#endif
    virtual std::vector<std::unique_ptr<TextLineBase>> GetTextLines() = 0;
    virtual std::unique_ptr<Paragraph> CloneSelf() = 0;

protected:
    sk_sp<FontCollection> fFontCollection;
    ParagraphStyle fParagraphStyle;

    // Things for Flutter
    SkScalar fAlphabeticBaseline;
    SkScalar fIdeographicBaseline;
    SkScalar fGlyphsBoundsTop;
    SkScalar fGlyphsBoundsBottom;
    SkScalar fGlyphsBoundsLeft;
    SkScalar fGlyphsBoundsRight;
    SkScalar fHeight;
    SkScalar fWidth;
    SkScalar fMaxIntrinsicWidth;
    SkScalar fMinIntrinsicWidth;
    SkScalar fLongestLine;
    SkScalar fLongestLineWithIndent;
    bool fExceededMaxLines;
};
}  // namespace textlayout
}  // namespace skia

#endif  // Paragraph_DEFINED
