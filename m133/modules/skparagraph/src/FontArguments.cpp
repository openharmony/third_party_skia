// Copyright 2019 Google LLC.

#include "modules/skparagraph/include/FontArguments.h"
#ifdef ENABLE_TEXT_ENHANCE
#include "text/font_variation_info.h"
#endif
static bool operator==(const SkFontArguments::VariationPosition::Coordinate& a,
                       const SkFontArguments::VariationPosition::Coordinate& b) {
   return a.axis == b.axis && a.value == b.value;
}

static bool operator==(const SkFontArguments::Palette::Override& a,
                       const SkFontArguments::Palette::Override& b) {
   return a.index == b.index && a.color == b.color;
}

namespace std {

size_t hash<skia::textlayout::FontArguments>::operator()(const skia::textlayout::FontArguments& args) const {
    size_t hash = 0;
    hash ^= std::hash<int>()(args.fCollectionIndex);
    for (const auto& coord : args.fCoordinates) {
        hash ^= std::hash<SkFourByteTag>()(coord.axis);
        hash ^= std::hash<float>()(coord.value);
    }
    hash ^= std::hash<int>()(args.fPaletteIndex);
    for (const auto& override : args.fPaletteOverrides) {
        hash ^= std::hash<int>()(override.index);
        hash ^= std::hash<SkColor>()(override.color);
    }
    return hash;
}

}  // namespace std

namespace skia {
namespace textlayout {

FontArguments::FontArguments(const SkFontArguments& args)
        : fCollectionIndex(args.getCollectionIndex()),
          fCoordinates(args.getVariationDesignPosition().coordinates,
                       args.getVariationDesignPosition().coordinates +
                       args.getVariationDesignPosition().coordinateCount),
          fPaletteIndex(args.getPalette().index),
          fPaletteOverrides(args.getPalette().overrides,
                            args.getPalette().overrides +
                            args.getPalette().overrideCount) {
#ifdef ENABLE_TEXT_ENHANCE
    for (auto tag : args.getNormalizationList()) {
        for (uint32_t i = 0; i < fCoordinates.size(); i++) {
            if (fCoordinates[i].axis == tag) {
                fNormalizationListIndex.push_back(tag);
                break;
            }
        }
    }
#endif
}

bool operator==(const FontArguments& a, const FontArguments& b) {
    return a.fCollectionIndex == b.fCollectionIndex &&
           a.fCoordinates == b.fCoordinates &&
           a.fPaletteIndex == b.fPaletteIndex &&
           a.fPaletteOverrides == b.fPaletteOverrides;
}

bool operator!=(const skia::textlayout::FontArguments& a, const skia::textlayout::FontArguments& b) {
    return !(a == b);
}

#ifdef ENABLE_TEXT_ENHANCE
void MappingAxisValue(SkFontArguments::VariationPosition::Coordinate& coord,
    const std::vector<OHOS::Rosen::Drawing::FontAxisInfo>& axisInfoList)
{
    constexpr size_t axisLen = 4;
    for (auto& info : axisInfoList) {
        if (info.axisTag.length() != axisLen) {
            continue;
        }
        SkFourByteTag axisTag = SkSetFourByteTag(info.axisTag[0], info.axisTag[1], info.axisTag[2], info.axisTag[3]);
        if (coord.axis != axisTag) {
            continue;
        }
        if (coord.value > 1 || coord.value < -1) {
            coord.value = info.defaultValue;
            return;
        }
        if (coord.value >= 0) {
            coord.value = info.defaultValue + coord.value * (info.maxValue - info.defaultValue);
        } else {
            coord.value = info.defaultValue + coord.value * (info.defaultValue - info.minValue);
        }
        return;
    }
}

std::shared_ptr<RSTypeface> FontArguments::CloneTypeface(std::shared_ptr<RSTypeface> typeface) const
{
    auto fontAxisInfo = OHOS::Rosen::Drawing::FontVariationInfo::GenerateFontVariationAxisInfo(typeface, {});
    std::vector<SkFontArguments::VariationPosition::Coordinate> coordinates{fCoordinates};
    for (auto index : fNormalizationListIndex) {
        MappingAxisValue(coordinates[index], fontAxisInfo);
    }

    RSFontArguments::VariationPosition position{
        (RSFontArguments::VariationPosition::Coordinate*)coordinates.data(),
        static_cast<int>(coordinates.size())
    };

    RSFontArguments::Palette palette{
        fPaletteIndex,
        (RSFontArguments::Palette::Override*)fPaletteOverrides.data(),
        static_cast<int>(fPaletteOverrides.size())
    };

    RSFontArguments args;
    args.SetCollectionIndex(fCollectionIndex);
    args.SetVariationDesignPosition(position);
    args.SetPalette(palette);

    return typeface->MakeClone(args);
}
#else
sk_sp<SkTypeface> FontArguments::CloneTypeface(const sk_sp<SkTypeface>& typeface) const {
    SkFontArguments::VariationPosition position{
        fCoordinates.data(),
        static_cast<int>(fCoordinates.size())
    };

    SkFontArguments::Palette palette{
        fPaletteIndex,
        fPaletteOverrides.data(),
        static_cast<int>(fPaletteOverrides.size())
    };

    SkFontArguments args;
    args.setCollectionIndex(fCollectionIndex);
    args.setVariationDesignPosition(position);
    args.setPalette(palette);

    return typeface->makeClone(args);
}
#endif

}  // namespace textlayout
}  // namespace skia
