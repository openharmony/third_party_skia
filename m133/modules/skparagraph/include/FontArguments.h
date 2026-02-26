// Copyright 2019 Google LLC.
#ifndef FontArguments_DEFINED
#define FontArguments_DEFINED

#include <functional>
#include <vector>
#include "include/core/SkFontArguments.h"
#include "include/core/SkTypeface.h"
#ifdef ENABLE_TEXT_ENHANCE
#include "drawing.h"
#endif

namespace skia {
namespace textlayout {

class FontArguments {
public:
    FontArguments(const SkFontArguments&);
    FontArguments(const FontArguments&) = default;
    FontArguments(FontArguments&&) = default;

    FontArguments& operator=(const FontArguments&) = default;
    FontArguments& operator=(FontArguments&&) = default;

#ifdef ENABLE_TEXT_ENHANCE
    std::shared_ptr<RSTypeface> CloneTypeface(std::shared_ptr<RSTypeface> typeface) const;
#else
    sk_sp<SkTypeface> CloneTypeface(const sk_sp<SkTypeface>& typeface) const;
#endif

    friend bool operator==(const FontArguments& a, const FontArguments& b);
    friend bool operator!=(const FontArguments& a, const FontArguments& b);
    friend struct std::hash<FontArguments>;

private:
    FontArguments() = delete;

    int fCollectionIndex;
    std::vector<SkFontArguments::VariationPosition::Coordinate> fCoordinates;
    int fPaletteIndex;
    std::vector<SkFontArguments::Palette::Override> fPaletteOverrides;
#ifdef ENABLE_TEXT_ENHANCE
    std::vector<uint32_t> fNormalizationListIndex;
#endif
};

}  // namespace textlayout
}  // namespace skia

namespace std {
    template<> struct hash<skia::textlayout::FontArguments> {
        size_t operator()(const skia::textlayout::FontArguments& args) const;
    };
}

#endif  // FontArguments_DEFINED
