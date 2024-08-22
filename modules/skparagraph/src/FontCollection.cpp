// Copyright 2019 Google LLC.
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skshaper/include/SkShaper.h"

namespace skia {
namespace textlayout {

namespace {
#ifdef USE_SKIA_TXT
std::shared_ptr<RSTypeface> RSLegacyMakeTypeface(
    std::shared_ptr<RSFontMgr> fontMgr, const char familyName[], RSFontStyle style)
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

constexpr int MAX_VARTYPEFACE_SIZE = 32;
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
#ifndef USE_SKIA_TXT
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
    : fEnableFontFallback(true),
    fDefaultFamilyNames({SkString(DEFAULT_FONT_FAMILY)}) {}

size_t FontCollection::getFontManagersCount() const {
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    return this->getFontManagerOrder().size();
}

#ifndef USE_SKIA_TXT
void FontCollection::setAssetFontManager(sk_sp<SkFontMgr> font_manager) {
#else
void FontCollection::setAssetFontManager(std::shared_ptr<RSFontMgr> font_manager) {
#endif
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fAssetFontManager = font_manager;
}

#ifndef USE_SKIA_TXT
void FontCollection::setDynamicFontManager(sk_sp<SkFontMgr> font_manager) {
#else
void FontCollection::setDynamicFontManager(std::shared_ptr<RSFontMgr> font_manager) {
#endif
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fDynamicFontManager = font_manager;
}

#ifndef USE_SKIA_TXT
void FontCollection::setTestFontManager(sk_sp<SkFontMgr> font_manager) {
#else
void FontCollection::setTestFontManager(std::shared_ptr<RSFontMgr> font_manager)
{
#endif
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fTestFontManager = font_manager;
}

#ifndef USE_SKIA_TXT
void FontCollection::setDefaultFontManager(sk_sp<SkFontMgr> fontManager,
                                           const char defaultFamilyName[]) {
#else
void FontCollection::setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager,
                                           const char defaultFamilyName[]) {
#endif
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fDefaultFontManager = std::move(fontManager);
    fDefaultFamilyNames.emplace_back(defaultFamilyName);
}

#ifndef USE_SKIA_TXT
void FontCollection::setDefaultFontManager(sk_sp<SkFontMgr> fontManager,
                                           const std::vector<SkString>& defaultFamilyNames) {
#else
void FontCollection::setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager,
                                           const std::vector<SkString>& defaultFamilyNames) {
#endif
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fDefaultFontManager = std::move(fontManager);
    fDefaultFamilyNames = defaultFamilyNames;
}

#ifndef USE_SKIA_TXT
void FontCollection::setDefaultFontManager(sk_sp<SkFontMgr> fontManager) {
#else
void FontCollection::setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager) {
#endif
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fDefaultFontManager = fontManager;
}

// Return the available font managers in the order they should be queried.
#ifndef USE_SKIA_TXT
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

#ifndef USE_SKIA_TXT
std::vector<sk_sp<SkTypeface>> FontCollection::findTypefaces(const std::vector<SkString>& familyNames, SkFontStyle fontStyle) {
#else
std::vector<std::shared_ptr<RSTypeface>> FontCollection::findTypefaces(
    const std::vector<SkString>& familyNames, RSFontStyle fontStyle)
{
#endif
    return findTypefaces(familyNames, fontStyle, std::nullopt);
}

#ifndef USE_SKIA_TXT
std::vector<sk_sp<SkTypeface>> FontCollection::findTypefaces(const std::vector<SkString>& familyNames,
    SkFontStyle fontStyle, const std::optional<FontArguments>& fontArgs) {
    // Look inside the font collections cache first
    FamilyKey familyKey(familyNames, fontStyle, fontArgs);
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        auto found = fTypefaces.find(familyKey);
        if (found) {
            return *found;
        }
    }

    std::vector<sk_sp<SkTypeface>> typefaces;
    for (const SkString& familyName : familyNames) {
        sk_sp<SkTypeface> match = matchTypeface(familyName, fontStyle);
        if (match && fontArgs) {
            match = CloneTypeface(match, fontArgs);
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
                match = CloneTypeface(match, fontArgs);
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

    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fTypefaces.set(familyKey, typefaces);
    return typefaces;
}
#else
std::vector<std::shared_ptr<RSTypeface>> FontCollection::findTypefaces(const std::vector<SkString>& familyNames,
    RSFontStyle fontStyle, const std::optional<FontArguments>& fontArgs)
{
    // Look inside the font collections cache first
    FamilyKey familyKey(familyNames, fontStyle, fontArgs);
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        auto found = fTypefaces.find(familyKey);
        if (found != fTypefaces.end()) {
            return found->second;
        }
    }

    std::vector<std::shared_ptr<RSTypeface>> typefaces;
    for (const auto& familyName : familyNames) {
        std::shared_ptr<RSTypeface> match = matchTypeface(familyName, fontStyle);
        if (match && fontArgs) {
            match = CloneTypeface(match, fontArgs);
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
                match = CloneTypeface(match, fontArgs);
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

    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fTypefaces.emplace(familyKey, typefaces);
    return typefaces;
}
#endif

#ifndef USE_SKIA_TXT
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
#ifndef USE_SKIA_TXT
sk_sp<SkTypeface> FontCollection::defaultFallback(SkUnichar unicode, SkFontStyle fontStyle, const SkString& locale) {
#else
std::shared_ptr<RSTypeface> FontCollection::defaultFallback(
    SkUnichar unicode, RSFontStyle fontStyle, const SkString& locale)
{
#endif
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    for (const auto& manager : this->getFontManagerOrder()) {
        std::vector<const char*> bcp47;
        if (!locale.isEmpty()) {
            bcp47.push_back(locale.c_str());
        }
#ifndef USE_SKIA_TXT
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

#ifndef USE_SKIA_TXT
sk_sp<SkTypeface> FontCollection::defaultFallback() {
    std::shared_lock<std::shared_mutex> readLock(mutex_);
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
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    if (fDefaultFontManager == nullptr) {
        return nullptr;
    }
    for (const auto& familyName : fDefaultFamilyNames) {
        std::shared_ptr<RSTypeface> match = std::shared_ptr<RSTypeface>(
            fDefaultFontManager->MatchFamilyStyle(familyName.c_str(), RSFontStyle()));
        if (match) {
            return match;
        }
    }
    return nullptr;
}
#endif

class SkLRUCacheMgr {
public:
    SkLRUCacheMgr(SkLRUCache<uint32_t, std::shared_ptr<RSTypeface>>& lruCache, SkMutex& mutex)
        :fLRUCache(lruCache), fMutex(mutex)
    {
        fMutex.acquire();
    }
    SkLRUCacheMgr(const SkLRUCacheMgr&) = delete;
    SkLRUCacheMgr(SkLRUCacheMgr&&) = delete;
    SkLRUCacheMgr& operator=(const SkLRUCacheMgr&) = delete;
    SkLRUCacheMgr& operator=(SkLRUCacheMgr&&) = delete;

    ~SkLRUCacheMgr() {
        fMutex.release();
    }

    std::shared_ptr<RSTypeface> find(uint32_t fontId) {
        auto face = fLRUCache.find(fontId);
        return face == nullptr ? nullptr : *face;
    }

    std::shared_ptr<RSTypeface> insert(uint32_t fontId, std::shared_ptr<RSTypeface> hbFont) {
        auto face = fLRUCache.insert(fontId, std::move(hbFont));
        return face == nullptr ? nullptr : *face;
    }

    void reset() {
        fLRUCache.reset();
    }

private:
    SkLRUCache<uint32_t, std::shared_ptr<RSTypeface>>& fLRUCache;
    SkMutex& fMutex;
};

static SkLRUCacheMgr GetLRUCacheInstance() {
    static SkMutex gFaceCacheMutex;
    static SkLRUCache<uint32_t, std::shared_ptr<RSTypeface>> gFaceCache(MAX_VARTYPEFACE_SIZE);
    return SkLRUCacheMgr(gFaceCache, gFaceCacheMutex);
}

#ifndef USE_SKIA_TXT
sk_sp<SkTypeface> FontCollection::CloneTypeface(sk_sp<SkTypeface> typeface,
    const std::optional<FontArguments>& fontArgs)
{
#else
std::shared_ptr<RSTypeface> FontCollection::CloneTypeface(std::shared_ptr<RSTypeface> typeface,
    const std::optional<FontArguments>& fontArgs)
{
    std::shared_lock<std::shared_mutex> readLock(mutex_);
#ifndef USE_SKIA_TXT
    if (!typeface || !fontArgs || typeface->isCustomTypeface()) {
#else
    if (!typeface || !fontArgs || typeface->IsCustomTypeface()) {
#endif
        return typeface;
    }

    size_t hash = 0;
    hash ^= std::hash<FontArguments>()(fontArgs.value());
#ifndef USE_SKIA_TXT
    hash ^= std::hash<uint32_t>()(typeface->uniqueID());
#else
    hash ^= std::hash<uint32_t>()(typeface->GetUniqueID());
#endif

//    std::unique_lock<std::mutex> lock(fMutex);
    auto cached = GetLRUCacheInstance().find(hash);
    if (cached) {
        return cached;
    } else {
        auto varTypeface = fontArgs->CloneTypeface(typeface);
        if (!varTypeface) {
            return typeface;
        }
        GetLRUCacheInstance().insert(hash, varTypeface);
        return varTypeface;
    }
}
#endif

void FontCollection::disableFontFallback() {
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fEnableFontFallback = false;
}
void FontCollection::enableFontFallback() {
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fEnableFontFallback = true;
}

void FontCollection::clearCaches() {
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    fParagraphCache.reset();
#ifndef USE_SKIA_TXT
    fTypefaces.reset();
#else
    fTypefaces.clear();
#endif
    SkShaper::PurgeCaches();
}

}  // namespace textlayout
}  // namespace skia
