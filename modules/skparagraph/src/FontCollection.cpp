// Copyright 2019 Google LLC.
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skshaper/include/SkShaper.h"

namespace skia {
namespace textlayout {

namespace {
#ifdef USE_ROSEN_DRAWING
std::shared_ptr<RSTypeface> RSLegacyMakeTypeface(std::shared_ptr<RSFontMgr> fontMgr, const char familyName[], RSFontStyle style)
{
    RSTypeface* typeface = fontMgr->MatchFamilyStyle(familyName, style);
    if (typeface == nullptr && familyName != nullptr) {
        typeface = fontMgr->MatchFamilyStyle(nullptr, style);
    }

    if (typeface) {
        return std::shared_ptr<RSTypeface>(typeface);
    }
    return nullptr;
}
#endif
}

bool FontCollection::FamilyKey::operator==(const FontCollection::FamilyKey& other) const {
    return fFamilyNames == other.fFamilyNames &&
           fFontStyle == other.fFontStyle &&
           fFontArguments == other.fFontArguments;
}

size_t FontCollection::FamilyKey::Hasher::operator()(const FontCollection::FamilyKey& key) const {
    size_t hash = 0;
    for (const SkString& family : key.fFamilyNames) {
        hash ^= std::hash<std::string>()(family.c_str());
    }
#ifndef USE_ROSEN_DRAWING
    return hash ^
           std::hash<uint32_t>()(key.fFontStyle.weight()) ^
           std::hash<uint32_t>()(key.fFontStyle.slant()) ^
           std::hash<std::optional<FontArguments>>()(key.fFontArguments);
#else
    return hash ^
           std::hash<uint32_t>()(key.fFontStyle.GetWeight()) ^
           std::hash<uint32_t>()(static_cast<uint32_t>(key.fFontStyle.GetSlant())) ^
           std::hash<std::optional<FontArguments>>()(key.fFontArguments);
#endif
}

FontCollection::FontCollection()
        : fEnableFontFallback(true)
        , fDefaultFamilyNames({SkString(DEFAULT_FONT_FAMILY)}) { }

size_t FontCollection::getFontManagersCount() const { return this->getFontManagerOrder().size(); }

#ifndef USE_ROSEN_DRAWING
void FontCollection::setAssetFontManager(sk_sp<SkFontMgr> font_manager) {
#else
void FontCollection::setAssetFontManager(std::shared_ptr<RSFontMgr> font_manager) {
#endif
    fAssetFontManager = font_manager;
}

#ifndef USE_ROSEN_DRAWING
void FontCollection::setDynamicFontManager(sk_sp<SkFontMgr> font_manager) {
#else
void FontCollection::setDynamicFontManager(std::shared_ptr<RSFontMgr> font_manager) {
#endif
    fDynamicFontManager = font_manager;
}

#ifndef USE_ROSEN_DRAWING
void FontCollection::setTestFontManager(sk_sp<SkFontMgr> font_manager) {
#else
void FontCollection::setTestFontManager(std::shared_ptr<RSFontMgr> font_manager) {
#endif
    fTestFontManager = font_manager;
}

#ifndef USE_ROSEN_DRAWING
void FontCollection::setDefaultFontManager(sk_sp<SkFontMgr> fontManager,
                                           const char defaultFamilyName[]) {
#else
void FontCollection::setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager,
                                           const char defaultFamilyName[]) {
#endif
    fDefaultFontManager = std::move(fontManager);
    fDefaultFamilyNames.emplace_back(defaultFamilyName);
}

#ifndef USE_ROSEN_DRAWING
void FontCollection::setDefaultFontManager(sk_sp<SkFontMgr> fontManager,
                                           const std::vector<SkString>& defaultFamilyNames) {
#else
void FontCollection::setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager,
                                           const std::vector<SkString>& defaultFamilyNames) {
#endif
    fDefaultFontManager = std::move(fontManager);
    fDefaultFamilyNames = defaultFamilyNames;
}

#ifndef USE_ROSEN_DRAWING
void FontCollection::setDefaultFontManager(sk_sp<SkFontMgr> fontManager) {
#else
void FontCollection::setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager) {
#endif
    fDefaultFontManager = fontManager;
}

// Return the available font managers in the order they should be queried.
#ifndef USE_ROSEN_DRAWING
std::vector<sk_sp<SkFontMgr>> FontCollection::getFontManagerOrder() const {
    std::vector<sk_sp<SkFontMgr>> order;
#else
std::vector<std::shared_ptr<RSFontMgr>> FontCollection::getFontManagerOrder() const {
    std::vector<std::shared_ptr<RSFontMgr>> order;
#endif
    if (fDynamicFontManager) {
        order.push_back(fDynamicFontManager);
    }
    if (fAssetFontManager) {
        order.push_back(fAssetFontManager);
    }
    if (fTestFontManager) {
        order.push_back(fTestFontManager);
    }
    if (fDefaultFontManager && fEnableFontFallback) {
        order.push_back(fDefaultFontManager);
    }
    return order;
}

#ifndef USE_ROSEN_DRAWING
std::vector<sk_sp<SkTypeface>> FontCollection::findTypefaces(const std::vector<SkString>& familyNames, SkFontStyle fontStyle) {
#else
std::vector<std::shared_ptr<RSTypeface>> FontCollection::findTypefaces(const std::vector<SkString>& familyNames, RSFontStyle fontStyle) {
#endif
    return findTypefaces(familyNames, fontStyle, std::nullopt);
}

#ifndef USE_ROSEN_DRAWING
std::vector<sk_sp<SkTypeface>> FontCollection::findTypefaces(const std::vector<SkString>& familyNames, SkFontStyle fontStyle, const std::optional<FontArguments>& fontArgs) {
    // Look inside the font collections cache first
    FamilyKey familyKey(familyNames, fontStyle, fontArgs);
    auto found = fTypefaces.find(familyKey);
    if (found) {
        return *found;
    }

    std::vector<sk_sp<SkTypeface>> typefaces;
    for (const SkString& familyName : familyNames) {
        sk_sp<SkTypeface> match = matchTypeface(familyName, fontStyle);
        if (match && fontArgs) {
            match = fontArgs->CloneTypeface(match);
        }
        if (match) {
            typefaces.emplace_back(std::move(match));
        }
    }

    if (typefaces.empty()) {
        sk_sp<SkTypeface> match;
        for (const SkString& familyName : fDefaultFamilyNames) {
            match = matchTypeface(familyName, fontStyle);
            if (match) {
                break;
            }
        }
        if (!match) {
            for (const auto& manager : this->getFontManagerOrder()) {
                match = manager->legacyMakeTypeface(nullptr, fontStyle);
                if (match) {
                    break;
                }
            }
        }
        if (match) {
            typefaces.emplace_back(std::move(match));
        }
    }

    fTypefaces.set(familyKey, typefaces);
    return typefaces;
}
#else
std::vector<std::shared_ptr<RSTypeface>> FontCollection::findTypefaces(const std::vector<SkString>& familyNames, RSFontStyle fontStyle, const std::optional<FontArguments>& fontArgs) {
    // Look inside the font collections cache first
    FamilyKey familyKey(familyNames, fontStyle, fontArgs);
    auto found = fTypefaces.find(familyKey);
    if (found != fTypefaces.end()) {
        return found->second;
    }

    std::vector<std::shared_ptr<RSTypeface>> typefaces;
    for (const auto& familyName : familyNames) {
        std::shared_ptr<RSTypeface> match = matchTypeface(familyName, fontStyle);
        if (match && fontArgs) {
            match = fontArgs->CloneTypeface(match);
        }
        if (match) {
            typefaces.emplace_back(std::move(match));
        }
    }

    if (typefaces.empty()) {
        std::shared_ptr<RSTypeface> match;
        for (const auto& familyName : fDefaultFamilyNames) {
            match = matchTypeface(familyName, fontStyle);
            if (match) {
                break;
            }
        }
        if (!match) {
            for (const auto& manager : this->getFontManagerOrder()) {
                match = RSLegacyMakeTypeface(manager, nullptr, fontStyle);
                if (match) {
                    break;
                }
            }
        }
        if (match) {
            typefaces.emplace_back(std::move(match));
        }
    }

    fTypefaces.emplace(familyKey, typefaces);
    return typefaces;
}
#endif

#ifndef USE_ROSEN_DRAWING
sk_sp<SkTypeface> FontCollection::matchTypeface(const SkString& familyName, SkFontStyle fontStyle) {
    for (const auto& manager : this->getFontManagerOrder()) {
        sk_sp<SkFontStyleSet> set(manager->matchFamily(familyName.c_str()));
        if (!set || set->count() == 0) {
            continue;
        }

        sk_sp<SkTypeface> match(set->matchStyle(fontStyle));
        if (match) {
            return match;
        }
    }

    return nullptr;
}
#else
std::shared_ptr<RSTypeface> FontCollection::matchTypeface(const SkString& familyName, RSFontStyle fontStyle) {
    for (const auto& manager : this->getFontManagerOrder()) {
        std::shared_ptr<RSFontStyleSet> set(manager->MatchFamily(familyName.c_str()));
        if (!set || set->Count() == 0) {
            continue;
        }

        std::shared_ptr<RSTypeface> match(set->MatchStyle(fontStyle));
        if (match) {
            return match;
        }
    }

    return nullptr;
}
#endif


// Find ANY font in available font managers that resolves the unicode codepoint
#ifndef USE_ROSEN_DRAWING
sk_sp<SkTypeface> FontCollection::defaultFallback(SkUnichar unicode, SkFontStyle fontStyle, const SkString& locale) {
#else
std::shared_ptr<RSTypeface> FontCollection::defaultFallback(SkUnichar unicode, RSFontStyle fontStyle, const SkString& locale) {
#endif

    for (const auto& manager : this->getFontManagerOrder()) {
        std::vector<const char*> bcp47;
        if (!locale.isEmpty()) {
            bcp47.push_back(locale.c_str());
        }
#ifndef USE_ROSEN_DRAWING
        sk_sp<SkTypeface> typeface(manager->matchFamilyStyleCharacter(
                nullptr, fontStyle, bcp47.data(), bcp47.size(), unicode));
#else
        std::shared_ptr<RSTypeface> typeface(manager->MatchFamilyStyleCharacter(
                nullptr, fontStyle, bcp47.data(), bcp47.size(), unicode));
#endif
        if (typeface != nullptr) {
            return typeface;
        }
    }
    return nullptr;
}

#ifndef USE_ROSEN_DRAWING
sk_sp<SkTypeface> FontCollection::defaultFallback() {
    if (fDefaultFontManager == nullptr) {
        return nullptr;
    }
    for (const SkString& familyName : fDefaultFamilyNames) {
        sk_sp<SkTypeface> match = sk_sp<SkTypeface>(fDefaultFontManager->matchFamilyStyle(familyName.c_str(),
                                                                        SkFontStyle()));
        if (match) {
            return match;
        }
    }
    return nullptr;
}
#else
std::shared_ptr<RSTypeface> FontCollection::defaultFallback() {
    if (fDefaultFontManager == nullptr) {
        return nullptr;
    }
    for (const auto& familyName : fDefaultFamilyNames) {
        std::shared_ptr<RSTypeface> match = std::shared_ptr<RSTypeface>(fDefaultFontManager->MatchFamilyStyle(familyName.c_str(),
                                                                        RSFontStyle()));
        if (match) {
            return match;
        }
    }
    return nullptr;
}
#endif

void FontCollection::disableFontFallback() { fEnableFontFallback = false; }
void FontCollection::enableFontFallback() { fEnableFontFallback = true; }

void FontCollection::clearCaches() {
    fParagraphCache.reset();
#ifndef USE_ROSEN_DRAWING
    fTypefaces.reset();
#else
    fTypefaces.clear();
#endif
    SkShaper::PurgeCaches();
}

}  // namespace textlayout
}  // namespace skia
