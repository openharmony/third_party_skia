/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifdef OHOS_SUPPORT
#include <algorithm>
#ifdef _WIN32
#include <cstdlib>
#else
#include <cstddef>
#include <climits>
#endif
#include <iostream>
#include <fstream>
#include <unicode/utf.h>
#include <unicode/utf8.h>
#include <unordered_set>

#include "include/Hyphenator.h"
#include "log.h"

namespace skia {
namespace textlayout {
std::once_flag Hyphenator::initFlag;
const std::map<std::string, std::string> HPB_FILE_NAMES = {
    {"as", "hyph-as.hpb"},                 // Assamese
    {"be", "hyph-be.hpb"},                 // Belarusian
    {"bg", "hyph-bg.hpb"},                 // Bulgarian
    {"bn", "hyph-bn.hpb"},                 // Bengali
    {"cs", "hyph-cs.hpb"},                 // Czech
    {"cy", "hyph-cy.hpb"},                 // Welsh
    {"da", "hyph-da.hpb"},                 // Danish
    {"de-1996", "hyph-de-1996.hpb"},       // German,1996orthography
    {"de-1901", "hyph-de-1901.hpb"},       // German,1901orthography
    {"de-ch-1901", "hyph-de-ch-1901.hpb"}, // SwissGerman,1901orthography
    {"el-monoton", "hyph-el-monoton.hpb"}, // ModernGreek,monotonic
    {"el-polyton", "hyph-el-polyton.hpb"}, // ModernGreek,polytonic
    {"en-latn", "hyph-en-gb.hpb"},         // Latin English
    {"en-gb", "hyph-en-gb.hpb"},           // British English
    {"en-us", "hyph-en-us.hpb"},           // American English
    {"es", "hyph-es.hpb"},                 // Spanish
    {"et", "hyph-et.hpb"},                 // Estonian
    {"fr", "hyph-fr.hpb"},                 // French
    {"ga", "hyph-ga.hpb"},                 // Irish
    {"gl", "hyph-gl.hpb"},                 // Galician
    {"gu", "hyph-gu.hpb"},                 // Gujarati
    {"hi", "hyph-hi.hpb"},                 // Hindi
    {"hr", "hyph-hr.hpb"},                 // Croatian
    {"hu", "hyph-hu.hpb"},                 // Hungarian
    {"hy", "hyph-hy.hpb"},                 // Armenian
    {"id", "hyph-id.hpb"},                 // Indonesian
    {"is", "hyph-is.hpb"},                 // Icelandic
    {"it", "hyph-it.hpb"},                 // Italian
    {"ka", "hyph-ka.hpb"},                 // Georgian
    {"kn", "hyph-kn.hpb"},                 // Kannada
    {"la", "hyph-la.hpb"},                 // Latin
    {"lt", "hyph-lt.hpb"},                 // Lithuanian
    {"lv", "hyph-lv.hpb"},                 // Latvian
    {"mk", "hyph-mk.hpb"},                 // Macedonian
    {"ml", "hyph-ml.hpb"},                 // Malayalam
    {"mn-cyrl", "hyph-mn-cyrl.hpb"},       // Mongolian,Cyrillicscript
    {"mr", "hyph-mr.hpb"},                 // Marathi
    {"mul-ethi", "hyph-mul-ethi.hpb"},     // Ethiopic
    {"nl", "hyph-nl.hpb"},                 // Dutch
    {"or", "hyph-or.hpb"},                 // Oriya
    {"pa", "hyph-pa.hpb"},                 // Punjabi
    {"pl", "hyph-pl.hpb"},                 // Polish
    {"pt", "hyph-pt.hpb"},                 // Portuguese
    {"rm", "hyph-rm.hpb"},                 // Romansh
    {"ru", "hyph-ru.hpb"},                 // Russian
    {"sh-cyrl", "hyph-sh-cyrl.hpb"},       // Serbo-Croatian,Cyrillicscript
    {"sh-latn", "hyph-sh-latn.hpb"},       // Serbo-Croatian,Latinscript
    {"sk", "hyph-sk.hpb"},                 // Slovak
    {"sl", "hyph-sl.hpb"},                 // Slovenian
    {"sr-cyrl", "hyph-sr-cyrl.hpb"},       // Serbian,Cyrillicscript
    {"sv", "hyph-sv.hpb"},                 // Swedish
    {"ta", "hyph-ta.hpb"},                 // Tamil
    {"te", "hyph-te.hpb"},                 // Telugu
    {"th", "hyph-th.hpb"},                 // Thai
    {"tk", "hyph-tk.hpb"},                 // Turkmen
    {"tr", "hyph-tr.hpb"},                 // Turkish
    {"uk", "hyph-uk.hpb"},                 // Ukrainian
    {"pinyin", "hyph-zh-latn-pinyin.hpb"}, // Chinese,Pinyin. language code ‘pinyin’ is not right,will be repair later
};

// in hyphenation, when a word ends with below chars, the char(s) is stripped during hyphenation.
const std::unordered_set<uint16_t> EXCLUDED_WORD_ENDING_CHARS = {
    0x21, // !
    0x22, // "
    0x23, // #
    0x24, // $
    0x25, // %
    0x26, // &
    0x27, // '
    0x28, // (
    0x29, // )
    0x2A, // *
    0x2B, // +
    0x2C, // ,
    0x2D, // -
    0x2e, // .
    0x2f, // /
    0x3A, // :
    0x3b, // ;
    0x3C, // <
    0x3D, // =
    0x3E, // >
    0x3F  // ?
};

struct HyphenTableInfo {
    const HyphenatorHeader* header{nullptr};
    const uint32_t* maindict{nullptr};
    const ArrayOf16bits* mappings{nullptr};

    bool initHyphenTableInfo(const std::vector<uint8_t>& hyphenatorData)
    {
        if (hyphenatorData.size() < sizeof(HyphenatorHeader)) {
            return false;
        }
        header = reinterpret_cast<const HyphenatorHeader*>(hyphenatorData.data());
        // get master table, it always is in direct mode
        maindict = (uint32_t*)(hyphenatorData.data() + header->toc);
        mappings = reinterpret_cast<const ArrayOf16bits*>(hyphenatorData.data() + header->mappings);
        // this is actually beyond the real 32 bit address, but just to have an offset that
        // is clearly out of bounds without recalculating it again
        return !(header->minCp == header->maxCp && mappings->count == 0);
    }
};

struct HyphenSubTable {
    uint16_t* staticOffset{nullptr};
    uint32_t nextOffset{0};
    PathType type{PathType::PATTERN};

    bool initHyphenSubTableInfo(uint16_t& code, uint16_t& offset, HyphenTableInfo& hyphenInfo)
    {
        auto header = hyphenInfo.header;
        if (offset == header->maxCount(hyphenInfo.mappings)) {
            code = 0;
            return false;
        }

        uint32_t baseOffset = *(hyphenInfo.maindict + offset - 1); // previous entry end
        uint32_t initialValue = *(hyphenInfo.maindict + offset);
        this->type = (PathType)(initialValue >> HYPHEN_SHIFT_BITS_30);
        // direct and pairs need to have offset different from zero
        if (initialValue == 0 && (type == PathType::DIRECT || type == PathType::PAIRS)) {
            return false;
        }
        // base offset is 16 bit
        auto address = reinterpret_cast<const uint8_t*>(header);
        this->staticOffset = (uint16_t*)(address + HYPHEN_BASE_CODE_SHIFT * baseOffset);

        // get a subtable according character
        // once: read as 32bit, the rest of the access will be 16bit (13bit for offsets)
        this->nextOffset = (initialValue & 0x3fffffff);
        return true;
    }
};

struct HyphenFindBreakParam {
    const HyphenatorHeader* header{nullptr};
    HyphenSubTable hyphenSubTable;
    uint16_t code{0};
    uint16_t offset{0};
};

void ReadBinaryFile(const std::string& filePath, std::vector<uint8_t>& buffer)
{
    char tmpPath[PATH_MAX] = {0};
    if (filePath.size() > PATH_MAX) {
        TEXT_LOGE("File name is too long");
        return;
    }
#ifdef _WIN32
    auto ret = _fullpath(tmpPath, filePath.c_str(), sizeof(tmpPath));
#else
    auto ret = realpath(filePath.c_str(), tmpPath);
#endif
    if (ret == nullptr) {
        TEXT_LOGE("Invalid file %{public}s", filePath.c_str());
        return;
    }
    std::ifstream file(filePath, std::ifstream::binary);
    if (!file.is_open()) {
        TEXT_LOGE("Failed to open %{public}s", filePath.c_str());
        return;
    }

    file.seekg(0, std::ios::end);
    std::streamsize length = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize(length);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), length)) {
        TEXT_LOGE("Failed to read %{public}s", filePath.c_str());
    }
    file.close();
}

std::string getLanguageCode(std::string locale, int hyphenPos)
{
    // to lower case
    std::transform(locale.begin(), locale.end(), locale.begin(), ::tolower);

    // find '-',substring the locale
    size_t pos = std::string::npos;
    int count = 0;
    for (size_t i = 0; i < locale.size(); ++i) {
        if (locale[i] == '-') {
            ++count;
            if (count == hyphenPos) {
                pos = i;
                break;
            }
        }
    }

    if (pos != std::string::npos) {
        return locale.substr(0, pos);
    } else {
        return locale;
    }
}

void Hyphenator::initTrieTree()
{
    for (const auto& item : HPB_FILE_NAMES) {
        fTrieTree.insert(item.first, item.second);
    }
}

const std::vector<uint8_t>& Hyphenator::getHyphenatorData(const std::string& locale)
{
    const std::vector<uint8_t>& firstResult =
        findHyphenatorData(getLanguageCode(locale, 2)); //num 2:sub string locale to the second '-'
    if (!firstResult.empty()) {
        return firstResult;
    } else {
        return findHyphenatorData(getLanguageCode(locale, 1));
    }
}

const std::vector<uint8_t>& Hyphenator::findHyphenatorData(const std::string& langCode)
{
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        auto search = fHyphenMap.find(langCode);
        if (search != fHyphenMap.end()) {
            return search->second;
        }
    }

    return loadPatternFile(langCode);
}

const std::vector<uint8_t>& Hyphenator::loadPatternFile(const std::string& langCode)
{
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    auto search = fHyphenMap.find(langCode);
    if (search != fHyphenMap.end()) {
        return search->second;
    }
    std::string hpbFileName = fTrieTree.findPartialMatch(langCode);
    if (!hpbFileName.empty()) {
        std::string filename = "/system/usr/ohos_hyphen_data/" + hpbFileName;
        std::vector<uint8_t> fileBuffer;
        ReadBinaryFile(filename, fileBuffer);
        if (!fileBuffer.empty()) {
            fHyphenMap.emplace(langCode, std::move(fileBuffer));
            return fHyphenMap[langCode];
        }
    }
    return fEmptyResult;
}

void formatTarget(std::vector<uint16_t>& target)
{
    while (EXCLUDED_WORD_ENDING_CHARS.find(target.back()) != EXCLUDED_WORD_ENDING_CHARS.end()) {
        target.pop_back();
    }
    target.insert(target.cbegin(), '.');
    target.push_back('.');

    for (auto& code : target) {
        HyphenatorHeader::toLower(code);
    }
}

void processPattern(const Pattern* p, size_t count, uint32_t index, std::vector<uint16_t>& word, std::vector<uint8_t>& res)
{
    TEXT_LOGD("Index:%{public}u", index);
    if (count > 0) {
        count *= 0x4; // patterns are padded to 4 byte arrays
        // when we reach pattern node (leaf), we need to increase index by one because of our
        // own code offset
        for (size_t currentIndex = 0; index < res.size() && currentIndex < count; index++) {
            TEXT_LOGD("Pattern info:%{public}zu, %{public}u, 0x%{public}x", count, index,
                      p->patterns[currentIndex]);
            res[index] = std::max(res[index], (p->patterns[currentIndex]));
            currentIndex++;
        }
    }
}

void processLinear(uint16_t* data, size_t index, HyphenFindBreakParam& param, std::vector<uint16_t>& word,
                   std::vector<uint8_t>& res)
{
    TEXT_LOGD("Index:%{public}zu", index);
    const ArrayOf16bits* p = reinterpret_cast<const ArrayOf16bits*>(data);
    uint16_t count = p->count;
    if (count > index + 1) {
        // the pattern is longer than the remaining word
        return;
    }
    index--;

    // check the rest of the string
    for (auto j = 0; j < count; j++) {
        if (p->codes[j] != word[index]) {
            return;
        } else {
            index--;
        }
    }
    uint32_t offset = 1 + count; // array size, code points, no padding for 16 bit
    uint16_t pOffset = *(data + offset);
    offset++; // move forward, after pattern
    if (!pOffset) {
        return;
    }

    const Pattern* matchPattern =
        reinterpret_cast<const Pattern*>(reinterpret_cast<const uint8_t*>(param.header) + (pOffset & 0xfff));
    index++; // matching peeks ahead
    processPattern(matchPattern, (pOffset >> 0xc), index, word, res);
    if (*(data + offset) != 0) { // peek if there is more to come
        return processLinear(data + offset, index, param, word, res);
    }
}

bool processDirect(uint16_t* data, HyphenFindBreakParam& param, uint32_t& nextOffset, PathType& type)
{
    TEXT_LOGD("");
    param.offset = param.header->codeOffset(param.code);
    if (param.header->minCp != param.header->maxCp && param.offset > param.header->maxCp) {
        return false;
    }
    uint16_t nextValue = *(data + nextOffset + param.offset);
    nextOffset = nextValue & 0x3fff; // Use mask 0x3fff to extract the lower 14 bits of nextValue
    type = (PathType)(nextValue >> HYPHEN_SHIFT_BITS_14);
    return true;
}

bool processPairs(const ArrayOf16bits* data, HyphenFindBreakParam& param, uint16_t code, uint32_t& nextOffset,
                  PathType& type)
{
    TEXT_LOGD("Code:0x%{public}x", code);
    uint16_t count = data->count;
    bool match = false;
    for (size_t j = 0; j < count; j += HYPHEN_BASE_CODE_SHIFT) {
        if (data->codes[j] == code) {
            nextOffset = data->codes[j + 1] & 0x3fff;
            type = (PathType)(data->codes[j + 1] >> HYPHEN_SHIFT_BITS_14);
            match = true;
            break;
        } else if (data->codes[j] > code) {
            break;
        }
    }
    return match;
}

void findBreakByType(HyphenFindBreakParam& param, const size_t& targetIndex, std::vector<uint16_t>& target,
                     std::vector<uint8_t>& result)
{
    TEXT_LOGD("TopLevel:%{public}zu", targetIndex);
    auto [staticOffset, nextOffset, type] = param.hyphenSubTable;
    uint32_t index = 0; // used in inner loop to traverse path further (backwards)
    while (true) {
        TEXT_LOGD("Loop:%{public}zu %{public}u", targetIndex, index);
        // there is always at 16bit of pattern address before next node data
        uint16_t pOffset = *(staticOffset + nextOffset);
        // from binary version 2 onwards, we have common nodes with 16bit offset (not bound to code points)
        if (type == PathType::PATTERN && (param.header->version >> 0x18) > 1) {
            pOffset =
                *(reinterpret_cast<const uint16_t*>(param.header) + nextOffset + (param.header->version & 0xffff));
        }
        nextOffset++;
        if (pOffset) {
            // if we have reached pattern, apply it to result
            uint16_t count = (pOffset >> 0xc);
            pOffset = 0xfff & pOffset;
            auto p = reinterpret_cast<const Pattern*>(reinterpret_cast<const uint8_t*>(param.header) + pOffset);
            processPattern(p, count, targetIndex - index, target, result);
        }
        if (type == PathType::PATTERN) {
            // just break the loop
            break;
        } else if (type == PathType::DIRECT) {
            if (index == targetIndex) {
                break;
            }
            index++; // resolve new code point (on the left)
            param.code = target[targetIndex - index];
            if (!processDirect(staticOffset, param, nextOffset, type)) {
                break;
            }
        } else if (type == PathType::LINEAR) {
            processLinear((staticOffset + nextOffset), targetIndex - index, param, target, result);
            // when a linear element has been processed, we always break and move to next top level index
            break;
        } else {
            if (index == targetIndex) {
                break;
            }
            index++;
            auto p = reinterpret_cast<const ArrayOf16bits*>(staticOffset + nextOffset);
            if (!processPairs(p, param, target[targetIndex - index], nextOffset, type)) {
                break;
            }
        }
    }
}

void findBreaks(const std::vector<uint8_t>& hyphenatorData, std::vector<uint16_t>& target, std::vector<uint8_t>& result)
{
    HyphenTableInfo hyphenInfo;
    if (!hyphenInfo.initHyphenTableInfo(hyphenatorData)) {
        return;
    }

    if (target.size() > 0) {
        for (size_t i = target.size() - 1; i >= 1; --i) {
            HyphenSubTable hyphenSubTable;
            auto header = hyphenInfo.header;
            auto code = target[i];
            auto offset = header->codeOffset(code, hyphenInfo.mappings);
            if (!hyphenSubTable.initHyphenSubTableInfo(code, offset, hyphenInfo)) {
                continue;
            }
            HyphenFindBreakParam param{header, hyphenSubTable, code, offset};
            findBreakByType(param, i, target, result);
        }
    }
}

size_t getLanguagespecificLeadingBounds(const std::string& locale)
{
    static const std::unordered_set<std::string> specialLocales = {"ka", "hy", "pinyin", "el-monoton", "el-polyton"};
    size_t lead = 2; // hardcoded for the most of the language pattern files
    if (specialLocales.count(locale)) {
        lead = 1;
    }
    return lead + 1; // we pad the target with surrounding marks ('.'), thus +1
}

size_t getLanguagespecificTrailingBounds(const std::string& locale)
{
    static const std::unordered_set<std::string> threeCharLocales = {"en-gb", "et", "th", "pt", "ga",
                                                                     "cs", "cy", "sk", "en-us"};
    static const std::unordered_set<std::string> oneCharLocales = {"el-monoton", "el-polyton"};

    size_t trail = 2; // hardcoded for the most of the language pattern files
    if (threeCharLocales.count(locale)) {
        trail = 3; // 3: At least three characters
    } else if (oneCharLocales.count(locale)) {
        trail = 1;
    }
    return trail; // we break before, so we don't add extra for end marker
}

inline void formatResult(std::vector<uint8_t>& result, const size_t& leadingHyphmins, const size_t& trailingHyphmins,
                         std::vector<uint8_t>& offsets)
{
    for (size_t i = 0; i < leadingHyphmins; i++) {
        result[i] = 0;
    }

    // remove front marker
    result.erase(result.cbegin());

    // move indices to match input multi chars
    size_t pad = 0;
    for (size_t i = 0; i < offsets.size(); i++) {
        while (offsets[i] != 0) {
            result.insert(result.begin() + i + pad, result[i + pad]);
            TEXT_LOGD("Padding %{public}zu", i + pad);
            offsets[i]--;
            pad++;
        }
    }
    // remove end marker and uncertain results
    result.erase(result.cbegin() + result.size() - trailingHyphmins, result.cend());
}

std::vector<uint8_t> Hyphenator::findBreakPositions(const SkString& locale, const SkString& text, size_t startPos,
                                                    size_t endPos)
{
    TEXT_LOGD("Find break pos:%{public}zu %{public}zu %{public}zu", text.size(), startPos, endPos);
    const std::string dummy(locale.c_str());
    auto hyphenatorData = getHyphenatorData(dummy);
    std::vector<uint8_t> result;

    if (startPos > text.size() || endPos > text.size() || startPos > endPos) {
        TEXT_LOGE("Hyphen error pos %{public}zu %{public}zu %{public}zu", text.size(), startPos, endPos);
        return result;
    }
    const auto leadingHyphmins = getLanguagespecificLeadingBounds(dummy);
    const auto trailingHyphmins = getLanguagespecificTrailingBounds(dummy);
    // resolve potential break positions
    if (!hyphenatorData.empty() && startPos + std::max(leadingHyphmins, trailingHyphmins) <= endPos) {
        // typically need to have at least 4 characters for hyphenator to process
        const auto lastword = std::string(text.c_str() + startPos, text.c_str() + endPos);
        std::vector<uint16_t> word;
        std::vector<uint8_t> offsets;
        int32_t i = 0;
        const int32_t textLength = static_cast<int32_t>(endPos - startPos);
        UChar32 c = 0;
        int32_t prev = i;
        while (i < textLength) {
            U8_NEXT(reinterpret_cast<const uint8_t*>(lastword.c_str()), i, textLength, c);
            offsets.push_back(i - prev - U16_LENGTH(c));
            if (U16_LENGTH(c) == 1) {
                word.push_back(c);
            } else {
                word.push_back(U16_LEAD(c));
                word.push_back(U16_TRAIL(c));
            }
            prev = i;
        }

        formatTarget(word);
        // Bulgarian pattern file tells only the positions where
        // breaking is not allowed, we need to initialize defaults to allow breaking
        const uint8_t defaultValue = (dummy == "bg") ? 1 : 0; // 0: break is not allowed, 1: break level 1
        result.resize(word.size(), defaultValue);
        findBreaks(hyphenatorData, word, result);
        formatResult(result, leadingHyphmins, trailingHyphmins, offsets);
    }
    return result;
}
} // namespace textlayout
} // namespace skia
#endif
