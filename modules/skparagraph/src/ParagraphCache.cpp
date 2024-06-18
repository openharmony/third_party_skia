// Copyright 2019 Google LLC.
#include <memory>

#include "modules/skparagraph/include/FontArguments.h"
#include "modules/skparagraph/include/ParagraphCache.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "log.h"

namespace skia {
namespace textlayout {

namespace {
    int32_t relax(SkScalar a) {
        // This rounding is done to match Flutter tests. Must be removed..
        if (SkScalarIsFinite(a)) {
          auto threshold = SkIntToScalar(1 << 12);
          return SkFloat2Bits(SkScalarRoundToScalar(a * threshold)/threshold);
        } else {
          return SkFloat2Bits(a);
        }
    }

    bool exactlyEqual(SkScalar x, SkScalar y) {
        return x == y || (x != x && y != y);
    }

}  // namespace

class ParagraphCacheKey {
public:
    ParagraphCacheKey(const ParagraphImpl* paragraph)
        : fText(paragraph->fText.c_str(), paragraph->fText.size())
        , fPlaceholders(paragraph->fPlaceholders)
        , fTextStyles(paragraph->fTextStyles)
        , fParagraphStyle(paragraph->paragraphStyle()) {
        fHash = computeHash(paragraph);
    }

    ParagraphCacheKey(const ParagraphCacheKey& other) = default;

    ParagraphCacheKey(ParagraphCacheKey&& other)
        : fText(std::move(other.fText))
        , fPlaceholders(std::move(other.fPlaceholders))
        , fTextStyles(std::move(other.fTextStyles))
        , fParagraphStyle(std::move(other.fParagraphStyle))
        , fHash(other.fHash) {
        other.fHash = 0;
    }

    // thin constructor suitable only for searching
    ParagraphCacheKey(uint32_t hash) : fHash(hash){}

    bool operator==(const ParagraphCacheKey& other) const;

    uint32_t hash() const { return fHash; }

    const SkString& text() const { return fText; }

private:
    static uint32_t mix(uint32_t hash, uint32_t data);
    uint32_t computeHash(const ParagraphImpl* paragraph) const;

    SkString fText;
    SkTArray<Placeholder, true> fPlaceholders;
    SkTArray<Block, true> fTextStyles;
    ParagraphStyle fParagraphStyle;
    uint32_t fHash;
};

class ParagraphCacheValue {
public:
    ParagraphCacheValue(ParagraphCacheKey&& key, const ParagraphImpl* paragraph)
        : fKey(std::move(key))
        , fRuns(paragraph->fRuns)
        , fClusters(paragraph->fClusters)
        , fClustersIndexFromCodeUnit(paragraph->fClustersIndexFromCodeUnit)
        , fCodeUnitProperties(paragraph->fCodeUnitProperties)
        , fWords(paragraph->fWords)
        , fBidiRegions(paragraph->fBidiRegions)
        , fHasLineBreaks(paragraph->fHasLineBreaks)
        , fHasWhitespacesInside(paragraph->fHasWhitespacesInside)
        , fTrailingSpaces(paragraph->fTrailingSpaces) { }

    // Input == key
    ParagraphCacheKey fKey;

    // Shaped results
    SkTArray<Run, false> fRuns;
    SkTArray<Cluster, true> fClusters;
    SkTArray<size_t, true> fClustersIndexFromCodeUnit;
    // ICU results
    SkTArray<SkUnicode::CodeUnitFlags, true> fCodeUnitProperties;
    std::vector<size_t> fWords;
    std::vector<SkUnicode::BidiRegion> fBidiRegions;
    bool fHasLineBreaks;
    bool fHasWhitespacesInside;
    TextIndex fTrailingSpaces;
    SkTArray<TextLine, false> fLines;
    SkScalar fHeight;
    SkScalar fWidth;
    SkScalar fMaxIntrinsicWidth;
    SkScalar fMinIntrinsicWidth;
    SkScalar fAlphabeticBaseline;
    SkScalar fIdeographicBaseline;
    SkScalar fLongestLine;
    bool fExceededMaxLines;
    // criteria to apply the layout cache, should presumably hash it, same
    // hash could be used to check if the entry has cached layout available
    LineBreakStrategy linebreakStrategy;
    WordBreakType wordBreakType;
    std::vector<SkScalar> indents;
    SkScalar fLayoutRawWidth;
};

uint32_t ParagraphCacheKey::mix(uint32_t hash, uint32_t data) {
    hash += data;
    hash += (hash << 10);
    hash ^= (hash >> 6);
    return hash;
}

uint32_t ParagraphCacheKey::computeHash(const ParagraphImpl* paragraph) const {
uint32_t hash = 0;
    for (auto& ph : fPlaceholders) {
        if (ph.fRange.width() == 0) {
            continue;
        }
        hash = mix(hash, SkGoodHash()(ph.fRange));
        hash = mix(hash, SkGoodHash()(relax(ph.fStyle.fHeight)));
        hash = mix(hash, SkGoodHash()(relax(ph.fStyle.fWidth)));
        hash = mix(hash, SkGoodHash()(ph.fStyle.fAlignment));
        hash = mix(hash, SkGoodHash()(ph.fStyle.fBaseline));
        if (ph.fStyle.fAlignment == PlaceholderAlignment::kBaseline) {
            hash = mix(hash, SkGoodHash()(relax(ph.fStyle.fBaselineOffset)));
        }
    }

    for (auto& ts : fTextStyles) {
        if (ts.fStyle.isPlaceholder()) {
            continue;
        }
        hash = mix(hash, SkGoodHash()(relax(ts.fStyle.getLetterSpacing())));
        hash = mix(hash, SkGoodHash()(relax(ts.fStyle.getWordSpacing())));
        hash = mix(hash, SkGoodHash()(ts.fStyle.getLocale()));
        hash = mix(hash, SkGoodHash()(relax(ts.fStyle.getHeight())));
        hash = mix(hash, SkGoodHash()(relax(ts.fStyle.getBaselineShift())));
        for (auto& ff : ts.fStyle.getFontFamilies()) {
            hash = mix(hash, SkGoodHash()(ff));
        }
        for (auto& ff : ts.fStyle.getFontFeatures()) {
            hash = mix(hash, SkGoodHash()(ff.fValue));
            hash = mix(hash, SkGoodHash()(ff.fName));
        }
        hash = mix(hash, std::hash<std::optional<FontArguments>>()(ts.fStyle.getFontArguments()));
        hash = mix(hash, SkGoodHash()(ts.fStyle.getFontStyle()));
        hash = mix(hash, SkGoodHash()(relax(ts.fStyle.getFontSize())));
        hash = mix(hash, SkGoodHash()(ts.fRange));
    }

    hash = mix(hash, SkGoodHash()(relax(fParagraphStyle.getHeight())));
    hash = mix(hash, SkGoodHash()(fParagraphStyle.getTextDirection()));
    hash = mix(hash, SkGoodHash()(fParagraphStyle.getReplaceTabCharacters() ? 1 : 0));

    auto& strutStyle = fParagraphStyle.getStrutStyle();
    if (strutStyle.getStrutEnabled()) {
        hash = mix(hash, SkGoodHash()(relax(strutStyle.getHeight())));
        hash = mix(hash, SkGoodHash()(relax(strutStyle.getLeading())));
        hash = mix(hash, SkGoodHash()(relax(strutStyle.getFontSize())));
        hash = mix(hash, SkGoodHash()(strutStyle.getHeightOverride()));
        hash = mix(hash, SkGoodHash()(strutStyle.getFontStyle()));
        hash = mix(hash, SkGoodHash()(strutStyle.getForceStrutHeight()));
        for (auto& ff : strutStyle.getFontFamilies()) {
            hash = mix(hash, SkGoodHash()(ff));
        }
    }

    hash = mix(hash, SkGoodHash()(paragraph->fLayoutRawWidth));
    hash = mix(hash, SkGoodHash()(fText));
    return hash;
}

uint32_t ParagraphCache::KeyHash::operator()(const ParagraphCacheKey& key) const {
    return key.hash();
}

bool ParagraphCacheKey::operator==(const ParagraphCacheKey& other) const {
    if (fText.size() != other.fText.size()) {
        return false;
    }
    if (fPlaceholders.size() != other.fPlaceholders.size()) {
        return false;
    }
    if (fText != other.fText) {
        return false;
    }
    if (fTextStyles.size() != other.fTextStyles.size()) {
        return false;
    }

    // There is no need to compare default paragraph styles - they are included into fTextStyles
    if (!exactlyEqual(fParagraphStyle.getHeight(), other.fParagraphStyle.getHeight())) {
        return false;
    }
    if (fParagraphStyle.getTextDirection() != other.fParagraphStyle.getTextDirection()) {
        return false;
    }

    if (!(fParagraphStyle.getStrutStyle() == other.fParagraphStyle.getStrutStyle())) {
        return false;
    }

    if (!(fParagraphStyle.getReplaceTabCharacters() == other.fParagraphStyle.getReplaceTabCharacters())) {
        return false;
    }

    for (int i = 0; i < fTextStyles.size(); ++i) {
        auto& tsa = fTextStyles[i];
        auto& tsb = other.fTextStyles[i];
        if (tsa.fStyle.isPlaceholder()) {
            continue;
        }
        if (!(tsa.fStyle.equalsByFonts(tsb.fStyle))) {
            return false;
        }
        if (tsa.fRange.width() != tsb.fRange.width()) {
            return false;
        }
        if (tsa.fRange.start != tsb.fRange.start) {
            return false;
        }
    }
    for (int i = 0; i < fPlaceholders.size(); ++i) {
        auto& tsa = fPlaceholders[i];
        auto& tsb = other.fPlaceholders[i];
        if (tsa.fRange.width() == 0 && tsb.fRange.width() == 0) {
            continue;
        }
        if (!(tsa.fStyle.equals(tsb.fStyle))) {
            return false;
        }
        if (tsa.fRange.width() != tsb.fRange.width()) {
            return false;
        }
        if (tsa.fRange.start != tsb.fRange.start) {
            return false;
        }
    }

    return true;
}

struct ParagraphCache::Entry {

    Entry(ParagraphCacheValue* value) : fValue(value) {}
    std::unique_ptr<ParagraphCacheValue> fValue;
};

ParagraphCache::ParagraphCache()
    : fChecker([](ParagraphImpl* impl, const char*, bool){ })
    , fLRUCacheMap(kMaxEntries)
    , fCacheIsOn(true)
    , fLastCachedValue(nullptr)
#ifdef PARAGRAPH_CACHE_STATS
    , fTotalRequests(0)
    , fCacheMisses(0)
    , fHashMisses(0)
#endif
{ }

ParagraphCache::~ParagraphCache() { }

void ParagraphCache::updateTo(ParagraphImpl* paragraph, const Entry* entry) {

    paragraph->fRuns.reset();
    paragraph->fRuns = entry->fValue->fRuns;
    paragraph->fClusters = entry->fValue->fClusters;
    paragraph->fClustersIndexFromCodeUnit = entry->fValue->fClustersIndexFromCodeUnit;
    paragraph->fCodeUnitProperties = entry->fValue->fCodeUnitProperties;
    paragraph->fWords = entry->fValue->fWords;
    paragraph->fBidiRegions = entry->fValue->fBidiRegions;
    paragraph->fHasLineBreaks = entry->fValue->fHasLineBreaks;
    paragraph->fHasWhitespacesInside = entry->fValue->fHasWhitespacesInside;
    paragraph->fTrailingSpaces = entry->fValue->fTrailingSpaces;
    for (auto& run : paragraph->fRuns) {
        run.setOwner(paragraph);
    }
    for (auto& cluster : paragraph->fClusters) {
        cluster.setOwner(paragraph);
    }
    paragraph->hash() = entry->fValue->fKey.hash();
}

void ParagraphCache::printStatistics() {
    SkDebugf("--- Paragraph Cache ---\n");
    SkDebugf("Total requests: %d\n", fTotalRequests);
    SkDebugf("Cache misses: %d\n", fCacheMisses);
    SkDebugf("Cache miss %%: %f\n", (fTotalRequests > 0) ? 100.f * fCacheMisses / fTotalRequests : 0.f);
    int cacheHits = fTotalRequests - fCacheMisses;
    SkDebugf("Hash miss %%: %f\n", (cacheHits > 0) ? 100.f * fHashMisses / cacheHits : 0.f);
    SkDebugf("---------------------\n");
}

void ParagraphCache::abandon() {
    this->reset();
}

void ParagraphCache::reset() {
    SkAutoMutexExclusive lock(fParagraphMutex);
#ifdef PARAGRAPH_CACHE_STATS
    fTotalRequests = 0;
    fCacheMisses = 0;
    fHashMisses = 0;
#endif
    fLRUCacheMap.reset();
    fLastCachedValue = nullptr;
}

bool ParagraphCache::useCachedLayout(const ParagraphImpl& paragraph, const ParagraphCacheValue* value) {
    if (value) {
        if (value->indents == paragraph.fIndents &&
            paragraph.getLineBreakStrategy() == value->linebreakStrategy &&
            paragraph.getWordBreakType() == value->wordBreakType) {

            return true;
        }
    }
    return false;
}

ParagraphCacheValue* ParagraphCache::resolveValue(ParagraphImpl& paragraph) {
    auto value = fLastCachedValue;
    if (fCacheIsOn) {
        // Check short path
        auto key = ParagraphCacheKey(&paragraph);
        if (!fLastCachedValue || fLastCachedValue->fKey.hash() == key.hash()) {
            std::unique_ptr<Entry>* entry = fLRUCacheMap.find(key);

            if (entry && *entry) {
                value = (*entry)->fValue.get();
            } else {
                value = nullptr;
            }
        }
        // Use last cached value
    } else {
        value = nullptr;
    }
    return value;
}

void ParagraphCache::SetStoredLayout(ParagraphImpl& paragraph) {
    if (auto value = resolveValue(paragraph)) {
        SetStoredLayoutImpl(paragraph, value);
    } else {
        if (auto value = cacheLayout(&paragraph)) {
            SetStoredLayoutImpl(paragraph, value);
        }
    }
}

void ParagraphCache::SetStoredLayoutImpl(ParagraphImpl& paragraph, ParagraphCacheValue* value) {
    value->fLines.reset();
    value->indents.clear();

    for( auto& line : paragraph.fLines) {
        value->fLines.emplace_back(line.CloneSelf());
    }
    paragraph.getSize(value->fHeight, value->fWidth, value->fLongestLine);
    paragraph.getIntrinsicSize(value->fMaxIntrinsicWidth, value->fMinIntrinsicWidth,
        value->fAlphabeticBaseline, value->fIdeographicBaseline,
        value->fExceededMaxLines );
    for (auto& indent : value->indents) {
        value->indents.push_back(indent);
    }
    value->linebreakStrategy = paragraph.getLineBreakStrategy();
    value->wordBreakType = paragraph.getWordBreakType();
    value->fLayoutRawWidth = paragraph.fLayoutRawWidth;
}

bool ParagraphCache::GetStoredLayout(ParagraphImpl& paragraph) {
    auto value = resolveValue(paragraph);
    // Check if we have a match, that should be pretty much only lentgh and wrapping modes
    // if the paragraph and text style match otherwise
    if (useCachedLayout(paragraph, value)) {
        // Need to ensure we have sufficient info for restoring
        // need some additionaö metrics
        if (value->fLines.empty()) {
            return false;
        }
        paragraph.fLines.reset();
        for ( auto& line : value->fLines) {
            paragraph.fLines.emplace_back(line.CloneSelf());
            paragraph.fLines.back().setParagraphImpl(&paragraph);
        }
        paragraph.setSize(value->fHeight, value->fWidth, value->fLongestLine);
        paragraph.setIntrinsicSize(value->fMaxIntrinsicWidth, value->fMinIntrinsicWidth,
            value->fAlphabeticBaseline, value->fIdeographicBaseline,
            value->fExceededMaxLines );
        return true;
    }
    return false;
}

bool ParagraphCache::findParagraph(ParagraphImpl* paragraph) {
    if (!fCacheIsOn) {
        return false;
    }
#ifdef PARAGRAPH_CACHE_STATS
    ++fTotalRequests;
#endif
    SkAutoMutexExclusive lock(fParagraphMutex);
    ParagraphCacheKey key(paragraph);
    std::unique_ptr<Entry>* entry = fLRUCacheMap.find(key);

    if (!entry) {
#ifdef USE_SKIA_TXT
        LOGD("ParagraphCache: cache miss, hash-%{public}d", key.hash());
#endif
        // We have a cache miss
#ifdef PARAGRAPH_CACHE_STATS
        ++fCacheMisses;
#endif
        fChecker(paragraph, "missingParagraph", true);
        return false;
    }
#ifdef USE_SKIA_TXT
    LOGD("ParagraphCache: cache hit, hash-%{public}d", key.hash());
#endif
    updateTo(paragraph, entry->get());
    fLastCachedValue = entry->get()->fValue.get();

    paragraph->hash() = key.hash();
    fChecker(paragraph, "foundParagraph", true);
    return true;
}

bool ParagraphCache::updateParagraph(ParagraphImpl* paragraph) {
    if (!fCacheIsOn) {
        return false;
    }
#ifdef PARAGRAPH_CACHE_STATS
    ++fTotalRequests;
#endif
    SkAutoMutexExclusive lock(fParagraphMutex);

    ParagraphCacheKey key(paragraph);
    std::unique_ptr<Entry>* entry = fLRUCacheMap.find(key);
    if (!entry) {
        // isTooMuchMemoryWasted(paragraph) not needed for now
        if (isPossiblyTextEditing(paragraph)) {
            // Skip this paragraph
            return false;
        }
        ParagraphCacheValue* value = new ParagraphCacheValue(std::move(key), paragraph);
        fLRUCacheMap.insert(value->fKey, std::make_unique<Entry>(value));
        fChecker(paragraph, "addedParagraph", true);
        fLastCachedValue = value;

        paragraph->hash() = key.hash();
        return true;
    } else {
        // We do not have to update the paragraph
        return false;
    }
}

ParagraphCacheValue* ParagraphCache::cacheLayout(ParagraphImpl* paragraph) {
    if (!fCacheIsOn) {
        return nullptr;
    }
#ifdef PARAGRAPH_CACHE_STATS
    ++fTotalRequests;
#endif
    SkAutoMutexExclusive lock(fParagraphMutex);

    ParagraphCacheKey key(paragraph);
    std::unique_ptr<Entry>* entry = fLRUCacheMap.find(key);
    if (!entry) {
        // isTooMuchMemoryWasted(paragraph) not needed for now
        if (isPossiblyTextEditing(paragraph)) {
            // Skip this paragraph
            return nullptr;
        }
        ParagraphCacheValue* value = new ParagraphCacheValue(std::move(key), paragraph);
        fLRUCacheMap.insert(value->fKey, std::make_unique<Entry>(value));
        fChecker(paragraph, "addedParagraph", true);
        fLastCachedValue = value;

        paragraph->hash() = key.hash();
        return value;
    } else {
        // Paragraph&layout already cached
        return nullptr;
    }
}

// Special situation: (very) long paragraph that is close to the last formatted paragraph
#define NOCACHE_PREFIX_LENGTH 40
bool ParagraphCache::isPossiblyTextEditing(ParagraphImpl* paragraph) {
    if (fLastCachedValue == nullptr) {
        return false;
    }

    auto& lastText = fLastCachedValue->fKey.text();
    auto& text = paragraph->fText;

    if ((lastText.size() < NOCACHE_PREFIX_LENGTH) || (text.size() < NOCACHE_PREFIX_LENGTH)) {
        // Either last text or the current are too short
        return false;
    }

    if (std::strncmp(lastText.c_str(), text.c_str(), NOCACHE_PREFIX_LENGTH) == 0) {
        // Texts have the same starts
        return true;
    }

    if (std::strncmp(lastText.c_str() + lastText.size() - NOCACHE_PREFIX_LENGTH, &text[text.size() - NOCACHE_PREFIX_LENGTH], NOCACHE_PREFIX_LENGTH) == 0) {
        // Texts have the same ends
        return true;
    }

    // It does not look like editing the text
    return false;
}
}  // namespace textlayout
}  // namespace skia
