// Copyright 2019 Google LLC.
#ifndef FontCollection_DEFINED
#define FontCollection_DEFINED

#include <memory>
#include <optional>
#include <set>
#ifdef ENABLE_TEXT_ENHANCE
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include "drawing.h"
#endif
#include "include/core/SkFontMgr.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSpan.h"
#include "modules/skparagraph/include/FontArguments.h"
#include "modules/skparagraph/include/ParagraphCache.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "src/core/SkTHash.h"

namespace skia {
namespace textlayout {

class TextStyle;
class Paragraph;
class FontCollection : public SkRefCnt {
public:
    FontCollection();

    size_t getFontManagersCount() const;

#ifdef ENABLE_TEXT_ENHANCE
    void setAssetFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setDynamicFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setTestFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setGlobalFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager, const char defaultFamilyName[]);
    void setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager, const std::vector<SkString>& defaultFamilyNames);

    std::shared_ptr<RSFontMgr> getFallbackManager() const
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        return fDefaultFontManager;
    }

    std::vector<std::shared_ptr<RSTypeface>> findTypefaces(
        const std::vector<SkString>& familyNames, RSFontStyle fontStyle);
    std::vector<std::shared_ptr<RSTypeface>> findTypefaces(
        const std::vector<SkString>& familyNames, RSFontStyle fontStyle, const std::optional<FontArguments>& fontArgs);

    std::shared_ptr<RSTypeface> defaultFallback(SkUnichar unicode, RSFontStyle fontStyle, const SkString& locale);
    std::shared_ptr<RSTypeface> defaultFallback();
    std::shared_ptr<RSTypeface> CloneTypeface(std::shared_ptr<RSTypeface> typeface,
        const std::optional<FontArguments>& fontArgs);
    void removeCacheByUniqueId(uint32_t uniqueId);
#else
    void setAssetFontManager(sk_sp<SkFontMgr> fontManager);
    void setDynamicFontManager(sk_sp<SkFontMgr> fontManager);
    void setTestFontManager(sk_sp<SkFontMgr> fontManager);
    void setDefaultFontManager(sk_sp<SkFontMgr> fontManager);
    void setDefaultFontManager(sk_sp<SkFontMgr> fontManager, const char defaultFamilyName[]);
    void setDefaultFontManager(sk_sp<SkFontMgr> fontManager, const std::vector<SkString>& defaultFamilyNames);

    sk_sp<SkFontMgr> getFallbackManager() const { return fDefaultFontManager; }

    std::vector<sk_sp<SkTypeface>> findTypefaces(const std::vector<SkString>& familyNames, SkFontStyle fontStyle);
    std::vector<sk_sp<SkTypeface>> findTypefaces(const std::vector<SkString>& familyNames, SkFontStyle fontStyle, const std::optional<FontArguments>& fontArgs);

    sk_sp<SkTypeface> defaultFallback(SkUnichar unicode, SkFontStyle fontStyle, const SkString& locale);
    sk_sp<SkTypeface> defaultEmojiFallback(SkUnichar emojiStart, SkFontStyle fontStyle, const SkString& locale);
    sk_sp<SkTypeface> defaultFallback();
#endif

    void disableFontFallback();
    void enableFontFallback();
    bool fontFallbackEnabled() { return fEnableFontFallback; }

    ParagraphCache* getParagraphCache() { return &fParagraphCache; }

    void clearCaches();

#ifdef ENABLE_TEXT_ENHANCE
    // set fIsAdpaterTextHeightEnabled with once_flag.
    static void SetAdapterTextHeightEnabled(bool adapterTextHeightEnabled)
    {
        static std::once_flag flag;
        std::call_once(flag, [adapterTextHeightEnabled]() {
            fIsAdpaterTextHeightEnabled = adapterTextHeightEnabled;
        });
    }

    static bool IsAdapterTextHeightEnabled()
    {
        return fIsAdpaterTextHeightEnabled;
    }
#endif
private:
#ifdef ENABLE_TEXT_ENHANCE
    std::vector<std::shared_ptr<RSFontMgr>> getFontManagerOrder() const;

    std::shared_ptr<RSTypeface> matchTypeface(const SkString& familyName, RSFontStyle fontStyle);

    void updateTypefacesMatch(std::vector<std::shared_ptr<RSTypeface>>& typefaces,
        RSFontStyle fontStyle, const std::optional<FontArguments>& fontArgs);
#else
    std::vector<sk_sp<SkFontMgr>> getFontManagerOrder() const;

    sk_sp<SkTypeface> matchTypeface(const SkString& familyName, SkFontStyle fontStyle);
#endif

    struct FamilyKey {
#ifdef ENABLE_TEXT_ENHANCE
        FamilyKey(
            const std::vector<SkString>& familyNames, RSFontStyle style, const std::optional<FontArguments>& args)
                : fFamilyNames(familyNames), fFontStyle(style), fFontArguments(args) {}
#else
        FamilyKey(const std::vector<SkString>& familyNames, SkFontStyle style, const std::optional<FontArguments>& args)
                : fFamilyNames(familyNames), fFontStyle(style), fFontArguments(args) {}
#endif

        FamilyKey() {}

        std::vector<SkString> fFamilyNames;
#ifdef ENABLE_TEXT_ENHANCE
        RSFontStyle fFontStyle;
#else
        SkFontStyle fFontStyle;
#endif
        std::optional<FontArguments> fFontArguments;

        bool operator==(const FamilyKey& other) const;

        struct Hasher {
            size_t operator()(const FamilyKey& key) const;
        };
    };
#ifdef ENABLE_TEXT_ENHANCE
    static bool fIsAdpaterTextHeightEnabled;
#endif

    bool fEnableFontFallback;
#ifdef ENABLE_TEXT_ENHANCE
    std::unordered_map<FamilyKey, std::vector<std::shared_ptr<RSTypeface>>, FamilyKey::Hasher> fTypefaces;
    std::shared_ptr<RSFontMgr> fDefaultFontManager{nullptr};
    std::shared_ptr<RSFontMgr> fGlobalFontManager{nullptr};
    std::shared_ptr<RSFontMgr> fAssetFontManager{nullptr};
    std::shared_ptr<RSFontMgr> fDynamicFontManager{nullptr};
    std::shared_ptr<RSFontMgr> fTestFontManager{nullptr};
#else
    skia_private::THashMap<FamilyKey, std::vector<sk_sp<SkTypeface>>, FamilyKey::Hasher> fTypefaces;
    sk_sp<SkFontMgr> fDefaultFontManager;
    sk_sp<SkFontMgr> fAssetFontManager;
    sk_sp<SkFontMgr> fDynamicFontManager;
    sk_sp<SkFontMgr> fTestFontManager;
#endif
    std::vector<SkString> fDefaultFamilyNames;
    ParagraphCache fParagraphCache;

#ifdef ENABLE_TEXT_ENHANCE
    std::mutex fMutex;
    mutable std::shared_mutex mutex_;
#endif
};
}  // namespace textlayout
}  // namespace skia

#endif  // FontCollection_DEFINED
