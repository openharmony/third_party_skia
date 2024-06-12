// Copyright 2020 Google LLC.
#ifndef Decorations_DEFINED
#define Decorations_DEFINED

#include "include/core/SkPath.h"
#include "modules/skparagraph/include/ParagraphPainter.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/src/TextLine.h"

namespace skia {
namespace textlayout {

class Decorations {
    public:
    void paint(ParagraphPainter* painter, const TextStyle& textStyle, const TextLine::ClipContext& context, SkScalar baseline);
    SkScalar calculateThickness(const TextStyle& textStyle, const TextLine::ClipContext& context);
    void setThickness(SkScalar thickness)
    {
        fThickness = thickness;
    }
    void setUnderlinePosition(SkScalar thickness)
    {
        underlinePosition = thickness;
    }

    private:

#ifndef USE_SKIA_TXT
    void calculateThickness(TextStyle textStyle, sk_sp<SkTypeface> typeface);
#else
    void calculateThickness(TextStyle textStyle, std::shared_ptr<RSTypeface> typeface);
#endif
    void calculatePosition(TextDecoration decoration, SkScalar ascent, const TextDecorationStyle textDecorationStyle);
    void calculatePaint(const TextStyle& textStyle);
    void calculateWaves(const TextStyle& textStyle, SkRect clip);
    void calculateAvoidanceWaves(const TextStyle& textStyle, SkRect clip);
    void calculateGaps(const TextLine::ClipContext& context, const SkRect& rect, SkScalar baseline, SkScalar halo);

    SkScalar fThickness;
    SkScalar fPosition;
    SkScalar underlinePosition;

#ifndef USE_SKIA_TXT
    SkFontMetrics fFontMetrics;
    ParagraphPainter::DecorationStyle fDecorStyle;
    SkPath fPath;
#else
    RSFontMetrics fFontMetrics;
    ParagraphPainter::DecorationStyle fDecorStyle;
    RSPath fPath;
#endif
};
}  // namespace textlayout
}  // namespace skia
#endif
