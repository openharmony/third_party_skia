// Copyright 2019 Google LLC.
#ifndef FontCollection_DEFINED
#define FontCollection_DEFINED

#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include "include/core/SkFontMgr.h"
#include "include/core/SkRefCnt.h"
#include "modules/skparagraph/include/FontArguments.h"
#include "modules/skparagraph/include/ParagraphCache.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "include/private/SkTHash.h"
#include "drawing.h"

namespace skia {
namespace textlayout {

class TextStyle;
class Paragraph;
class FontCollection : public SkRefCnt {
public:
    FontCollection();

    size_t getFontManagersCount() const;

#ifndef USE_SKIA_TXT
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
    sk_sp<SkTypeface> defaultFallback();
#else
    void setAssetFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setDynamicFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setTestFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager);
    void setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager, const char defaultFamilyName[]);
    void setDefaultFontManager(std::shared_ptr<RSFontMgr> fontManager, const std::vector<SkString>& defaultFamilyNames);

    std::shared_ptr<RSFontMgr> getFallbackManager() const { return fDefaultFontManager; }

    std::vector<std::shared_ptr<RSTypeface>> findTypefaces(const std::vector<SkString>& familyNames, RSFontStyle fontStyle);
    std::vector<std::shared_ptr<RSTypeface>> findTypefaces(const std::vector<SkString>& familyNames, RSFontStyle fontStyle, const std::optional<FontArguments>& fontArgs);

    std::shared_ptr<RSTypeface> defaultFallback(SkUnichar unicode, RSFontStyle fontStyle, const SkString& locale);
    std::shared_ptr<RSTypeface> defaultFallback();
#endif

    void disableFontFallback();
    void enableFontFallback();
    bool fontFallbackEnabled() { return fEnableFontFallback; }

    ParagraphCache* getParagraphCache() { return &fParagraphCache; }

    void clearCaches();

private:
#ifndef USE_SKIA_TXT
    std::vector<sk_sp<SkFontMgr>> getFontManagerOrder() const;

    sk_sp<SkTypeface> matchTypeface(const SkString& familyName, SkFontStyle fontStyle);
#else
    std::vector<std::shared_ptr<RSFontMgr>> getFontManagerOrder() const;

    std::shared_ptr<RSTypeface> matchTypeface(const SkString& familyName, RSFontStyle fontStyle);
#endif

    struct FamilyKey {
#ifndef USE_SKIA_TXT
        FamilyKey(const std::vector<SkString>& familyNames, SkFontStyle style, const std::optional<FontArguments>& args)
                : fFamilyNames(familyNames), fFontStyle(style), fFontArguments(args) {}
#else
        FamilyKey(const std::vector<SkString>& familyNames, RSFontStyle style, const std::optional<FontArguments>& args)
                : fFamilyNames(familyNames), fFontStyle(style), fFontArguments(args) {}
#endif

        FamilyKey() {}

        std::vector<SkString> fFamilyNames;
#ifndef USE_SKIA_TXT
        SkFontStyle fFontStyle;
#else
        RSFontStyle fFontStyle;
#endif
        std::optional<FontArguments> fFontArguments;

        bool operator==(const FamilyKey& other) const;

        struct Hasher {
            size_t operator()(const FamilyKey& key) const;
        };
    };

    bool fEnableFontFallback;
#ifndef USE_SKIA_TXT
    SkTHashMap<FamilyKey, std::vector<sk_sp<SkTypeface>>, FamilyKey::Hasher> fTypefaces;
    sk_sp<SkFontMgr> fDefaultFontManager;
    sk_sp<SkFontMgr> fAssetFontManager;
    sk_sp<SkFontMgr> fDynamicFontManager;
    sk_sp<SkFontMgr> fTestFontManager;
#else
    std::unordered_map<FamilyKey, std::vector<std::shared_ptr<RSTypeface>>, FamilyKey::Hasher> fTypefaces;
    std::shared_ptr<RSFontMgr> fDefaultFontManager;
    std::shared_ptr<RSFontMgr> fAssetFontManager;
    std::shared_ptr<RSFontMgr> fDynamicFontManager;
    std::shared_ptr<RSFontMgr> fTestFontManager;
#endif
    std::vector<SkString> fDefaultFamilyNames;
    ParagraphCache fParagraphCache;
};
}  // namespace textlayout
}  // namespace skia

#endif  // FontCollection_DEFINED
