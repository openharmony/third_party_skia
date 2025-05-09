/*
 * Copyright 2011 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkStream.h"
#include "include/core/SkTypeface.h"
#include "include/private/SkMutex.h"
#include "include/private/SkOnce.h"
#include "include/utils/SkCustomTypeface.h"
#include "src/core/SkAdvancedTypefaceMetrics.h"
#include "src/core/SkEndian.h"
#include "src/core/SkFontDescriptor.h"
#include "src/core/SkFontPriv.h"
#include "src/core/SkLRUCache.h"
#include "src/core/SkScalerContext.h"
#include "src/core/SkSurfacePriv.h"
#include "src/core/SkTypefaceCache.h"
#include "src/sfnt/SkOTTable_OS_2.h"
#include "src/utils/SkUTF.h"

#include <mutex>

SkTypeface::SkTypeface(const SkFontStyle& style, bool isFixedPitch)
    : fUniqueID(SkTypefaceCache::NewFontID()), fStyle(style), fIsFixedPitch(isFixedPitch) { }

SkTypeface::~SkTypeface() { }

///////////////////////////////////////////////////////////////////////////////

namespace {

class SkEmptyTypeface : public SkTypeface {
public:
    static sk_sp<SkTypeface> Make() { return sk_sp<SkTypeface>(new SkEmptyTypeface); }
protected:
    SkEmptyTypeface() : SkTypeface(SkFontStyle(), true) { }

    std::unique_ptr<SkStreamAsset> onOpenStream(int* ttcIndex) const override { return nullptr; }
    sk_sp<SkTypeface> onMakeClone(const SkFontArguments& args) const override {
        return sk_ref_sp(this);
    }
    std::unique_ptr<SkScalerContext> onCreateScalerContext(
        const SkScalerContextEffects& effects, const SkDescriptor* desc) const override
    {
        return SkScalerContext::MakeEmpty(
                sk_ref_sp(const_cast<SkEmptyTypeface*>(this)), effects, desc);
    }
    void onFilterRec(SkScalerContextRec*) const override { }
    std::unique_ptr<SkAdvancedTypefaceMetrics> onGetAdvancedMetrics() const override {
        return nullptr;
    }
    void onGetFontDescriptor(SkFontDescriptor*, bool*) const override { }
    void onCharsToGlyphs(const SkUnichar* chars, int count, SkGlyphID glyphs[]) const override {
        sk_bzero(glyphs, count * sizeof(glyphs[0]));
    }
    int onCountGlyphs() const override { return 0; }
    void getPostScriptGlyphNames(SkString*) const override {}
    void getGlyphToUnicodeMap(SkUnichar*) const override {}
    int onGetUPEM() const override { return 0; }
    class EmptyLocalizedStrings : public SkTypeface::LocalizedStrings {
    public:
        bool next(SkTypeface::LocalizedString*) override { return false; }
    };
    void onGetFamilyName(SkString* familyName) const override {
        familyName->reset();
    }
    bool onGetPostScriptName(SkString*) const override {
        return false;
    }
    SkTypeface::LocalizedStrings* onCreateFamilyNameIterator() const override {
        return new EmptyLocalizedStrings;
    }
    bool onGlyphMaskNeedsCurrentColor() const override {
        return false;
    }
    int onGetVariationDesignPosition(SkFontArguments::VariationPosition::Coordinate coordinates[],
                                     int coordinateCount) const override
    {
        return 0;
    }
    int onGetVariationDesignParameters(SkFontParameters::Variation::Axis parameters[],
                                       int parameterCount) const override
    {
        return 0;
    }
    int onGetTableTags(SkFontTableTag tags[]) const override { return 0; }
    size_t onGetTableData(SkFontTableTag, size_t, size_t, void*) const override {
        return 0;
    }
};

constexpr int MAX_VARFONT_CACHE_SIZE = 64;

class SkVarFontCache {
public:
    SkVarFontCache() : fLRUCache(MAX_VARFONT_CACHE_SIZE) {}

    static SkVarFontCache& Instance()
    {
        static SkVarFontCache cache;
        return cache;
    }

    sk_sp<SkTypeface> GetVarFont(sk_sp<SkTypeface> typeface, const SkFontArguments& args)
    {
        if (!typeface) {
            return nullptr;
        }

        size_t hash = 0;
        hash ^= std::hash<uint32_t>()(typeface->uniqueID());
#ifdef OHOS_SUPPORT
        uint32_t typefaceHash = typeface->GetHash();
        hash ^= std::hash<uint32_t>()(typefaceHash);
#endif
        hash ^= std::hash<int>()(args.getCollectionIndex());
        const auto& positions = args.getVariationDesignPosition();
        for (int i = 0; i < positions.coordinateCount; i++) {
            const auto& coord = positions.coordinates[i];
            hash ^= std::hash<SkFourByteTag>()(coord.axis);
            hash ^= std::hash<float>()(coord.value);
        }

        std::lock_guard<std::mutex> lock(fMutex);
        auto cached = fLRUCache.find(hash);
        if (!cached) {
            int ttcIndex = args.getCollectionIndex();
            auto varTypeface = SkFontMgr::RefDefault()->makeFromStream(typeface->openStream(&ttcIndex), args);
            if (!varTypeface) {
                return typeface;
            } else {
                fLRUCache.insert(hash, varTypeface);
                return varTypeface;
            }
        }

        return *cached;
    }

private:
    SkLRUCache<uint32_t, sk_sp<SkTypeface>> fLRUCache;
    std::mutex fMutex;
};

}  // namespace

SkFontStyle SkTypeface::FromOldStyle(Style oldStyle) {
    return SkFontStyle((oldStyle & SkTypeface::kBold) ? SkFontStyle::kBold_Weight
                                                      : SkFontStyle::kNormal_Weight,
                       SkFontStyle::kNormal_Width,
                       (oldStyle & SkTypeface::kItalic) ? SkFontStyle::kItalic_Slant
                                                        : SkFontStyle::kUpright_Slant);
}

SkTypeface* SkTypeface::GetDefaultTypeface(Style style) {
    static SkOnce once[4];
    static sk_sp<SkTypeface> defaults[4];

    SkASSERT((int)style < 4);
    once[style]([style] {
        sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
        auto t = fm->legacyMakeTypeface(nullptr, FromOldStyle(style));
        defaults[style] = t ? t : SkEmptyTypeface::Make();
    });
    return defaults[style].get();
}

sk_sp<SkTypeface> SkTypeface::MakeDefault() {
    return sk_ref_sp(GetDefaultTypeface());
}

uint32_t SkTypeface::UniqueID(const SkTypeface* face) {
    if (nullptr == face) {
        face = GetDefaultTypeface();
    }
    return face->uniqueID();
}

bool SkTypeface::Equal(const SkTypeface* facea, const SkTypeface* faceb) {
    return facea == faceb || SkTypeface::UniqueID(facea) == SkTypeface::UniqueID(faceb);
}

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkTypeface> SkTypeface::MakeFromName(const char name[],
                                           SkFontStyle fontStyle) {
    if (nullptr == name && (fontStyle.slant() == SkFontStyle::kItalic_Slant ||
                            fontStyle.slant() == SkFontStyle::kUpright_Slant) &&
                           (fontStyle.weight() == SkFontStyle::kBold_Weight ||
                            fontStyle.weight() == SkFontStyle::kNormal_Weight)) {
        return sk_ref_sp(GetDefaultTypeface(static_cast<SkTypeface::Style>(
            (fontStyle.slant() == SkFontStyle::kItalic_Slant ? SkTypeface::kItalic :
                                                               SkTypeface::kNormal) |
            (fontStyle.weight() == SkFontStyle::kBold_Weight ? SkTypeface::kBold :
                                                               SkTypeface::kNormal))));
    }
    return SkFontMgr::RefDefault()->legacyMakeTypeface(name, fontStyle);
}

#ifdef OHOS_SUPPORT
std::vector<sk_sp<SkTypeface>> SkTypeface::GetSystemFonts()
{
    return SkFontMgr::RefDefault()->getSystemFonts();
}
#endif

sk_sp<SkTypeface> SkTypeface::MakeFromStream(std::unique_ptr<SkStreamAsset> stream, int index) {
    if (!stream) {
        return nullptr;
    }
    return SkFontMgr::RefDefault()->makeFromStream(std::move(stream), index);
}

sk_sp<SkTypeface> SkTypeface::MakeFromData(sk_sp<SkData> data, int index) {
    if (!data) {
        return nullptr;
    }
    return SkFontMgr::RefDefault()->makeFromData(std::move(data), index);
}

sk_sp<SkTypeface> SkTypeface::MakeFromFile(const char path[], int index) {
    return SkFontMgr::RefDefault()->makeFromFile(path, index);
}

sk_sp<SkTypeface> SkTypeface::makeClone(const SkFontArguments& args) const {
    return this->onMakeClone(args);
}

///////////////////////////////////////////////////////////////////////////////

void SkTypeface::serialize(SkWStream* wstream, SerializeBehavior behavior) const {
    bool isLocalData = false;
    SkFontDescriptor desc;
    this->onGetFontDescriptor(&desc, &isLocalData);

    bool shouldSerializeData = false;

#ifdef ARKUI_X_ENABLE
    // Prohibiting serializes typeface when arkui-x use custom's font
    isLocalData = false;
#endif

    switch (behavior) {
        case SerializeBehavior::kDoIncludeData:      shouldSerializeData = true;        break;
        case SerializeBehavior::kDontIncludeData:    shouldSerializeData = false;       break;
        case SerializeBehavior::kIncludeDataIfLocal: shouldSerializeData = isLocalData; break;
    }

    if (shouldSerializeData) {
        int index;
        desc.setStream(this->openStream(&index));
        if (desc.hasStream()) {
            desc.setCollectionIndex(index);
        }

        int numAxes = this->getVariationDesignPosition(nullptr, 0);
        if (0 < numAxes) {
            numAxes = this->getVariationDesignPosition(desc.setVariationCoordinates(numAxes), numAxes);
            if (numAxes <= 0) {
                desc.setVariationCoordinates(0);
            }
        }
    }
    desc.serialize(wstream);
}

sk_sp<SkData> SkTypeface::serialize(SerializeBehavior behavior) const {
    SkDynamicMemoryWStream stream;
    this->serialize(&stream, behavior);
    return stream.detachAsData();
}

sk_sp<SkTypeface> SkTypeface::MakeDeserialize(SkStream* stream) {
    SkFontDescriptor desc;
    if (!SkFontDescriptor::Deserialize(stream, &desc)) {
        return nullptr;
    }

    if (desc.hasStream()) {
        if (auto tf = SkCustomTypefaceBuilder::Deserialize(desc.dupStream().get())) {
            return tf;
        }
    }

    if (desc.hasStream()) {
        SkFontArguments args;
        args.setCollectionIndex(desc.getCollectionIndex());
        args.setVariationDesignPosition({desc.getVariation(), desc.getVariationCoordinateCount()});
        sk_sp<SkFontMgr> defaultFm = SkFontMgr::RefDefault();
        sk_sp<SkTypeface> typeface = defaultFm->makeFromStream(desc.detachStream(), args);
        if (typeface) {
            return typeface;
        }
    }

    auto typeface = SkTypeface::MakeFromName(desc.getFamilyName(), desc.getStyle());
    if (desc.getVariationCoordinateCount() > 0 && typeface) {
        SkFontArguments args;
        args.setCollectionIndex(desc.getCollectionIndex());
        args.setVariationDesignPosition({desc.getVariation(), desc.getVariationCoordinateCount()});
        typeface = SkVarFontCache::Instance().GetVarFont(typeface, args);
    }

    return typeface;
}

bool SkTypeface::isCustomTypeface() const {
    return fIsCustom;
}

void SkTypeface::setIsCustomTypeface(bool isCustom) {
    fIsCustom = isCustom;
}

#ifdef OHOS_SUPPORT
bool SkTypeface::isThemeTypeface() const
{
    return fIsTheme;
}

void SkTypeface::setIsThemeTypeface(bool isTheme)
{
    fIsTheme = isTheme;
}
#endif

std::unique_ptr<SkStreamAsset> SkTypeface::openExistingStream(int* ttcIndex) const {
    int ttcIndexStorage;
    if (nullptr == ttcIndex) {
        // So our subclasses don't need to check for null param
        ttcIndex = &ttcIndexStorage;
    }
    return this->onOpenExistingStream(ttcIndex);
}

std::unique_ptr<SkStreamAsset> SkTypeface::onOpenExistingStream(int* ttcIndex) const {
    return this->onOpenStream(ttcIndex);
}

///////////////////////////////////////////////////////////////////////////////

bool SkTypeface::glyphMaskNeedsCurrentColor() const {
    return this->onGlyphMaskNeedsCurrentColor();
}

int SkTypeface::getVariationDesignPosition(
        SkFontArguments::VariationPosition::Coordinate coordinates[], int coordinateCount) const
{
    return this->onGetVariationDesignPosition(coordinates, coordinateCount);
}

int SkTypeface::getVariationDesignParameters(
        SkFontParameters::Variation::Axis parameters[], int parameterCount) const
{
    return this->onGetVariationDesignParameters(parameters, parameterCount);
}

int SkTypeface::countTables() const {
    return this->onGetTableTags(nullptr);
}

int SkTypeface::getTableTags(SkFontTableTag tags[]) const {
    return this->onGetTableTags(tags);
}

size_t SkTypeface::getTableSize(SkFontTableTag tag) const {
    return this->onGetTableData(tag, 0, ~0U, nullptr);
}

size_t SkTypeface::getTableData(SkFontTableTag tag, size_t offset, size_t length,
                                void* data) const {
    return this->onGetTableData(tag, offset, length, data);
}

sk_sp<SkData> SkTypeface::copyTableData(SkFontTableTag tag) const {
    return this->onCopyTableData(tag);
}

sk_sp<SkData> SkTypeface::onCopyTableData(SkFontTableTag tag) const {
    size_t size = this->getTableSize(tag);
    if (size) {
        sk_sp<SkData> data = SkData::MakeUninitialized(size);
        (void)this->getTableData(tag, 0, size, data->writable_data());
        return data;
    }
    return nullptr;
}

std::unique_ptr<SkStreamAsset> SkTypeface::openStream(int* ttcIndex) const {
    int ttcIndexStorage;
    if (nullptr == ttcIndex) {
        // So our subclasses don't need to check for null param
        ttcIndex = &ttcIndexStorage;
    }
    return this->onOpenStream(ttcIndex);
}

std::unique_ptr<SkScalerContext> SkTypeface::createScalerContext(
        const SkScalerContextEffects& effects, const SkDescriptor* desc) const {
    std::unique_ptr<SkScalerContext> scalerContext = this->onCreateScalerContext(effects, desc);
    SkASSERT(scalerContext);
    return scalerContext;
}

void SkTypeface::unicharsToGlyphs(const SkUnichar uni[], int count, SkGlyphID glyphs[]) const {
    if (count > 0 && glyphs && uni) {
        this->onCharsToGlyphs(uni, count, glyphs);
    }
}

SkGlyphID SkTypeface::unicharToGlyph(SkUnichar uni) const {
    SkGlyphID glyphs[1] = { 0 };
    this->onCharsToGlyphs(&uni, 1, glyphs);
    return glyphs[0];
}

namespace {
class SkConvertToUTF32 {
public:
    SkConvertToUTF32() {}

    const SkUnichar* convert(const void* text, size_t byteLength, SkTextEncoding encoding) {
        const SkUnichar* uni;
        switch (encoding) {
            case SkTextEncoding::kUTF8: {
                uni = fStorage.reset(byteLength);
                const char* ptr = (const char*)text;
                const char* end = ptr + byteLength;
                for (int i = 0; ptr < end; ++i) {
                    fStorage[i] = SkUTF::NextUTF8(&ptr, end);
                }
            } break;
            case SkTextEncoding::kUTF16: {
                uni = fStorage.reset(byteLength);
                const uint16_t* ptr = (const uint16_t*)text;
                const uint16_t* end = ptr + (byteLength >> 1);
                for (int i = 0; ptr < end; ++i) {
                    fStorage[i] = SkUTF::NextUTF16(&ptr, end);
                }
            } break;
            case SkTextEncoding::kUTF32:
                uni = (const SkUnichar*)text;
                break;
            default:
                SK_ABORT("unexpected enum");
        }
        return uni;
    }

private:
    SkAutoSTMalloc<256, SkUnichar> fStorage;
};
}

int SkTypeface::textToGlyphs(const void* text, size_t byteLength, SkTextEncoding encoding,
                             SkGlyphID glyphs[], int maxGlyphCount) const {
    if (0 == byteLength) {
        return 0;
    }

    SkASSERT(text);

    int count = SkFontPriv::CountTextElements(text, byteLength, encoding);
    if (!glyphs || count > maxGlyphCount) {
        return count;
    }

    if (encoding == SkTextEncoding::kGlyphID) {
        memcpy(glyphs, text, count << 1);
        return count;
    }

    SkConvertToUTF32 storage;
    const SkUnichar* uni = storage.convert(text, byteLength, encoding);

    this->unicharsToGlyphs(uni, count, glyphs);
    return count;
}

int SkTypeface::countGlyphs() const {
    return this->onCountGlyphs();
}

int SkTypeface::getUnitsPerEm() const {
    // should we try to cache this in the base-class?
    return this->onGetUPEM();
}

bool SkTypeface::getKerningPairAdjustments(const uint16_t glyphs[], int count,
                                           int32_t adjustments[]) const {
    SkASSERT(count >= 0);
    // check for the only legal way to pass a nullptr.. everything is 0
    // in which case they just want to know if this face can possibly support
    // kerning (true) or never (false).
    if (nullptr == glyphs || nullptr == adjustments) {
        SkASSERT(nullptr == glyphs);
        SkASSERT(0 == count);
        SkASSERT(nullptr == adjustments);
    }
    return this->onGetKerningPairAdjustments(glyphs, count, adjustments);
}

SkTypeface::LocalizedStrings* SkTypeface::createFamilyNameIterator() const {
    return this->onCreateFamilyNameIterator();
}

void SkTypeface::getFamilyName(SkString* name) const {
    SkASSERT(name);
    this->onGetFamilyName(name);
}

#ifdef OHOS_SUPPORT
void SkTypeface::getFontPath(SkString* path) const
{
    SkASSERT(path);
    this->onGetFontPath(path);
}
#endif

bool SkTypeface::getPostScriptName(SkString* name) const {
    return this->onGetPostScriptName(name);
}

void SkTypeface::getGlyphToUnicodeMap(SkUnichar* dst) const {
    sk_bzero(dst, sizeof(SkUnichar) * this->countGlyphs());
}

std::unique_ptr<SkAdvancedTypefaceMetrics> SkTypeface::getAdvancedMetrics() const {
    std::unique_ptr<SkAdvancedTypefaceMetrics> result = this->onGetAdvancedMetrics();
    if (result && result->fPostScriptName.isEmpty()) {
        result->fPostScriptName = result->fFontName;
    }
    if (result && result->fType == SkAdvancedTypefaceMetrics::kTrueType_Font) {
        SkOTTableOS2::Version::V2::Type::Field fsType;
        constexpr SkFontTableTag os2Tag = SkTEndian_SwapBE32(SkOTTableOS2::TAG);
        constexpr size_t fsTypeOffset = offsetof(SkOTTableOS2::Version::V2, fsType);
        if (this->getTableData(os2Tag, fsTypeOffset, sizeof(fsType), &fsType) == sizeof(fsType)) {
            if (fsType.Bitmap || (fsType.Restricted && !(fsType.PreviewPrint || fsType.Editable))) {
                result->fFlags |= SkAdvancedTypefaceMetrics::kNotEmbeddable_FontFlag;
            }
            if (fsType.NoSubsetting) {
                result->fFlags |= SkAdvancedTypefaceMetrics::kNotSubsettable_FontFlag;
            }
        }
    }
    return result;
}

bool SkTypeface::onGetKerningPairAdjustments(const uint16_t glyphs[], int count,
                                             int32_t adjustments[]) const {
    return false;
}

///////////////////////////////////////////////////////////////////////////////

#include "include/core/SkPaint.h"
#include "src/core/SkDescriptor.h"

SkRect SkTypeface::getBounds() const {
    fBoundsOnce([this] {
        if (!this->onComputeBounds(&fBounds)) {
            fBounds.setEmpty();
        }
    });
    return fBounds;
}

bool SkTypeface::onComputeBounds(SkRect* bounds) const {
    // we use a big size to ensure lots of significant bits from the scalercontext.
    // then we scale back down to return our final answer (at 1-pt)
    const SkScalar textSize = 2048;
    const SkScalar invTextSize = 1 / textSize;

    SkFont font;
    font.setTypeface(sk_ref_sp(const_cast<SkTypeface*>(this)));
    font.setSize(textSize);
    font.setLinearMetrics(true);

    SkScalerContextRec rec;
    SkScalerContextEffects effects;

    SkScalerContext::MakeRecAndEffectsFromFont(font, &rec, &effects);

    SkAutoDescriptor ad;
    SkScalerContextEffects noeffects;
    SkScalerContext::AutoDescriptorGivenRecAndEffects(rec, noeffects, &ad);

    std::unique_ptr<SkScalerContext> ctx = this->createScalerContext(noeffects, ad.getDesc());

    SkFontMetrics fm;
    ctx->getFontMetrics(&fm);
    if (!fm.hasBounds()) {
        return false;
    }
    bounds->setLTRB(fm.fXMin * invTextSize, fm.fTop * invTextSize,
                    fm.fXMax * invTextSize, fm.fBottom * invTextSize);
    return true;
}

std::unique_ptr<SkAdvancedTypefaceMetrics> SkTypeface::onGetAdvancedMetrics() const {
    SkDEBUGFAIL("Typefaces that need to work with PDF backend must override this.");
    return nullptr;
}

#ifdef OHOS_SUPPORT
uint32_t SkTypeface::GetHash() const
{
    if (hash_ != 0) {
        return hash_;
    }
    auto skData = serialize(SkTypeface::SerializeBehavior::kDontIncludeData);
    if (skData == nullptr) {
        return hash_;
    }
    std::unique_ptr<SkStreamAsset> ttfStream = openExistingStream(0);
    uint32_t seed = ttfStream.get() != nullptr ? ttfStream->getLength() : 0;
    hash_ = SkOpts::hash_fn(skData->data(), skData->size(), seed);
    return hash_;
}

void SkTypeface::SetHash(uint32_t hash)
{
    hash_ = hash;
}
#endif
