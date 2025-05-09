// Copyright 2019 Google LLC.
#ifndef LineBreaker_DEFINED
#define LineBreaker_DEFINED

#include <functional>  // std::function
#include <queue>
#include "include/core/SkSpan.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skparagraph/src/Run.h"

namespace skia {
namespace textlayout {

class ParagraphImpl;
class OneLineShaper : public SkShaper::RunHandler {
public:
    explicit OneLineShaper(ParagraphImpl* paragraph)
        : fParagraph(paragraph)
        , fHeight(0.0f)
        , fUseHalfLeading(false)
        , fBaselineShift(0.0f)
        , fAdvance(SkPoint::Make(0.0f, 0.0f))
        , fUnresolvedGlyphs(0)
        , fUniqueRunId(paragraph->fRuns.size()){ }

    bool shape();

    size_t unresolvedGlyphs() { return fUnresolvedGlyphs; }

private:

    struct RunBlock {
        RunBlock() : fRun(nullptr) { }

        // First unresolved block
        explicit RunBlock(TextRange text) : fRun(nullptr), fText(text) { }

        RunBlock(std::shared_ptr<Run> run, TextRange text, GlyphRange glyphs, size_t score)
            : fRun(std::move(run))
            , fText(text)
            , fGlyphs(glyphs) { }

        // Entire run comes as one block fully resolved
        explicit RunBlock(std::shared_ptr<Run> run)
            : fRun(std::move(run))
            , fText(fRun->fTextRange)
            , fGlyphs(GlyphRange(0, fRun->size())) { }

        std::shared_ptr<Run> fRun;
        TextRange fText;
        GlyphRange fGlyphs;
        bool isFullyResolved() { return fRun != nullptr && fGlyphs.width() == fRun->size(); }
    };

    using ShapeVisitor =
            std::function<SkScalar(TextRange textRange, SkSpan<Block>, SkScalar&, TextIndex, uint8_t)>;
    bool iterateThroughShapingRegions(const ShapeVisitor& shape);

    using ShapeSingleFontVisitor =
            std::function<void(Block, SkTArray<SkShaper::Feature>)>;
    void iterateThroughFontStyles(
            TextRange textRange, SkSpan<Block> styleSpan, const ShapeSingleFontVisitor& visitor);

    enum Resolved {
        Nothing,
        Something,
        Everything
    };

#ifndef USE_SKIA_TXT
    using TypefaceVisitor = std::function<Resolved(sk_sp<SkTypeface> typeface)>;
#else
    using TypefaceVisitor = std::function<Resolved(std::shared_ptr<RSTypeface> typeface)>;
#endif
    void matchResolvedFonts(const TextStyle& textStyle, const TypefaceVisitor& visitor);

#ifdef OHOS_SUPPORT
    bool isUnresolvedCombineGlyphRange(std::shared_ptr<Run> run, size_t glyphStart, size_t glyphEnd,
        size_t charStart) const;
    void splitUnresolvedBlockAndStageResolvedSubBlock(
        std::deque<RunBlock>& stagedUnresolvedBlocks, const RunBlock& unresolvedBlock);
    void shapeUnresolvedTextSeparatelyFromUnresolvedBlock(const TextStyle& textStyle, const TypefaceVisitor& visitor);
#endif

#ifdef SK_DEBUG
    void printState();
#endif
    void finish(const Block& block, SkScalar height, SkScalar& advanceX);

    void beginLine() override {}
    void runInfo(const RunInfo&) override {}
    void commitRunInfo() override {}
    void commitLine() override {}

    Buffer runBuffer(const RunInfo& info) override {
        fCurrentRun = std::make_shared<Run>(fParagraph,
                                           info,
                                           fCurrentText.start,
                                           fHeight,
                                           fUseHalfLeading,
                                           fBaselineShift,
                                           ++fUniqueRunId,
                                           fAdvance.fX);
        return fCurrentRun->newRunBuffer();
    }

    void commitRunBuffer(const RunInfo&) override;

#ifdef OHOS_SUPPORT
    void adjustRange(GlyphRange& glyphs, TextRange& textRange);
#endif
    TextRange clusteredText(GlyphRange& glyphs);
    ClusterIndex clusterIndex(GlyphIndex glyph) {
        return fCurrentText.start + fCurrentRun->fClusterIndexes[glyph];
    }
    void addFullyResolved();
    void addUnresolvedWithRun(GlyphRange glyphRange);
    void sortOutGlyphs(std::function<void(GlyphRange)>&& sortOutUnresolvedBLock);
    ClusterRange normalizeTextRange(GlyphRange glyphRange);
    void fillGaps(size_t);
    BlockRange generateBlockRange(const Block& block, const TextRange& textRange);

    ParagraphImpl* fParagraph;
    TextRange fCurrentText;
    SkScalar fHeight;
    bool fUseHalfLeading;
    SkScalar fBaselineShift;
    SkVector fAdvance;
    size_t fUnresolvedGlyphs;
    size_t fUniqueRunId;

    // TODO: Something that is not thead-safe since we don't need it
    std::shared_ptr<Run> fCurrentRun;
    std::deque<RunBlock> fUnresolvedBlocks;
    std::vector<RunBlock> fResolvedBlocks;

    // Keeping all resolved typefaces
    struct FontKey {

        FontKey() {}

#ifndef USE_SKIA_TXT
        FontKey(SkUnichar unicode, SkFontStyle fontStyle, SkString locale)
            : fUnicode(unicode), fFontStyle(fontStyle), fLocale(locale) { }
#else
        FontKey(SkUnichar unicode, RSFontStyle fontStyle, SkString locale)
            : fUnicode(unicode), fFontStyle(fontStyle), fLocale(locale) { }
#endif

        SkUnichar fUnicode;
#ifndef USE_SKIA_TXT
        SkFontStyle fFontStyle;
#else
        RSFontStyle fFontStyle;
#endif
        SkString fLocale;

        bool operator==(const FontKey& other) const;

        struct Hasher {
            uint32_t operator()(const FontKey& key) const;
        };
    };

#ifndef USE_SKIA_TXT
    SkTHashMap<FontKey, sk_sp<SkTypeface>, FontKey::Hasher> fFallbackFonts;
#else
    std::unordered_map<FontKey, std::shared_ptr<RSTypeface>, FontKey::Hasher> fFallbackFonts;
#endif
};

}  // namespace textlayout
}  // namespace skia
#endif
