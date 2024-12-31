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
#include <iostream>
#include <fstream>
#include <unicode/utf.h>
#include <unicode/utf8.h>

#include "include/Hyphenator.h"
#include "log.h"

namespace skia {
namespace textlayout {
const std::unordered_map<std::string, std::string> HPB_FILE_NAMES = {
    {"as", "hyph-as.hpb"},
    {"be", "hyph-be.hpb"},
    {"bg", "hyph-bg.hpb"},
    {"bn", "hyph-bn.hpb"},
    {"cs", "hyph-cs.hpb"},
    {"cy", "hyph-cy.hpb"},
    {"da", "hyph-da.hpb"},
    {"de-1901", "hyph-de-1901.hpb"},
    {"de-1996", "hyph-de-1996.hpb"},
    {"de-ch-1901", "hyph-de-ch-1901.hpb"},
    {"el-monoton", "hyph-el-monoton.hpb"},
    {"el-polyton", "hyph-el-polyton.hpb"},
    {"en-gb", "hyph-en-gb.hpb"},
    {"es", "hyph-es.hpb"},
    {"et", "hyph-et.hpb"},
    {"fr", "hyph-fr.hpb"},
    {"ga", "hyph-ga.hpb"},
    {"gl", "hyph-gl.hpb"},
    {"grc-x-ibycus", "hyph-grc-x-ibycus.hpb"},
    {"gu", "hyph-gu.hpb"},
    {"hi", "hyph-hi.hpb"},
    {"hr", "hyph-hr.hpb"},
    {"hu", "hyph-hu.hpb"},
    {"hy", "hyph-hy.hpb"},
    {"id", "hyph-id.hpb"},
    {"is", "hyph-is.hpb"},
    {"it", "hyph-it.hpb"},
    {"ka", "hyph-ka.hpb"},
    {"kn", "hyph-kn.hpb"},
    {"la", "hyph-la.hpb"},
    {"lt", "hyph-lt.hpb"},
    {"lv", "hyph-lv.hpb"},
    {"mk", "hyph-mk.hpb"},
    {"ml", "hyph-ml.hpb"},
    {"mn-cyrl", "hyph-mn-cyrl.hpb"},
    {"mr", "hyph-mr.hpb"},
    {"mul-ethi", "hyph-mul-ethi.hpb"},
    {"nl", "hyph-nl.hpb"},
    {"or", "hyph-or.hpb"},
    {"pa", "hyph-pa.hpb"},
    {"pl", "hyph-pl.hpb"},
    {"pt", "hyph-pt.hpb"},
    {"rm", "hyph-rm.hpb"},
    {"ru", "hyph-ru.hpb"},
    {"sh-cyrl", "hyph-sh-cyrl.hpb"},
    {"sh-latn", "hyph-sh-latn.hpb"},
    {"sk", "hyph-sk.hpb"},
    {"sl", "hyph-sl.hpb"},
    {"sr-cyrl", "hyph-sr-cyrl.hpb"},
    {"sv", "hyph-sv.hpb"},
    {"ta", "hyph-ta.hpb"},
    {"te", "hyph-te.hpb"},
    {"th", "hyph-th.hpb"},
    {"tk", "hyph-tk.hpb"},
    {"tr", "hyph-tr.hpb"},
    {"uk", "hyph-uk.hpb"},
    {"zh-py", "hyph-zh-latn-pinyin.hpb"},
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
        if (initialValue == 0) {
            // 0 is never valid offset from maindict
            return false;
        }
        // base offset is 16 bit
        auto address = reinterpret_cast<const uint8_t*>(header);
        this->staticOffset = (uint16_t*)(address + HYPHEN_BASE_CODE_SHIFT * baseOffset);

        // get a subtable according character
        // once: read as 32bit, the rest of the access will be 16bit (13bit for offsets)
        this->nextOffset = (initialValue & 0x3fffffff);
        this->type = (PathType)(initialValue >> HYPHEN_SHIFT_BITS_30);
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
    std::ifstream file(filePath, std::ifstream::binary);
    if (!file) {
        TEXT_LOGE("Failed to open %{public}s", filePath.c_str());
        file.close();
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

const std::string& ResolveHpbFile(const std::string& locale)
{
    auto it = HPB_FILE_NAMES.find(locale);
    if (it != HPB_FILE_NAMES.end()) {
        return it->second;
    }
    return HPB_FILE_NAMES.at("en-gb");
}

const std::vector<uint8_t>& Hyphenator::GetHyphenatorData(const std::string& locale)
{
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        auto search = hyphenMap.find(locale);
        if (search != hyphenMap.end()) {
            return search->second;
        }
    }

    LoadHyphenatorData(locale);
   
    return hyphenMap[locale];
}

bool Hyphenator::LoadHyphenatorData(const std::string& locale)
{
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    std::string filename = "/system/usr/ohos_hyphen_data/" + ResolveHpbFile(locale);
    std::vector<uint8_t> fileBuffer;
    ReadBinaryFile(filename, fileBuffer);
    if (!fileBuffer.empty()) {
        hyphenMap.emplace(locale, std::move(fileBuffer));
        return true;
    }
    return false;
}

void formatTarget(std::vector<uint16_t>& target)
{
    for (auto& code : target) {
        HyphenatorHeader::toLower(code);
    }

    target.insert(target.cbegin(), '.');
    target.push_back('.');
}

void processPattern(const Pattern* p, size_t i, uint32_t& ix, std::vector<uint16_t>& word, std::vector<uint8_t>& res)
{
    uint16_t code = p->code;
    // if the code point is defined, the sub index refers to code next to this node
    if (code) {
        if (code != word[i - ix]) {
            return;
        }
    } else {
        // so qe need to substract if this is the direct ref
        ix--;
    }
    uint16_t count = p->count;
    size_t currentIndex{0};
    // when we reach pattern node (leaf), we need to increase ix by one because of our
    // own code offset
    for (size_t j = i - ix; j <= i && currentIndex < count; j++) {
        res[j] = std::max(res[j], (p->patterns[currentIndex]));
        currentIndex++;
    }
}

void processLinear(uint16_t* data, size_t i, uint32_t& ix, std::vector<uint16_t>& word, std::vector<uint8_t>& res)
{
    const ArrayOf16bits* p = reinterpret_cast<const ArrayOf16bits*>(data);
    uint16_t count = p->count;
    uint32_t origPos = ix;
    ix++;
    if (count > i - ix + 1) {
        // the pattern is longer than the remaining word
        return;
    }

    bool match = true;
    // check the rest of the string
    for (auto j = 0; j < count; j++) {
        if (p->codes[j] != word[i - ix]) {
            match = false;
            break;
        } else {
            ix++;
        }
    }
    // if we reach the end, apply pattern
    if (!match) {
        return;
    }
    uint32_t offset = count + (count & 0x1);
    const Pattern* matchPattern = reinterpret_cast<const Pattern*>(data + offset);
    size_t index{0};
    for (size_t j = i - origPos - count; j <= i && index < matchPattern->count; j++) {
        res[j] = std::max(res[j], matchPattern->patterns[index]);
        index++;
    }
}

bool processDirect(uint16_t* data, HyphenFindBreakParam& param, uint32_t& nextOffset, PathType& type)
{
    param.offset = param.header->codeOffset(param.code);
    if (param.header->minCp != param.header->maxCp && param.offset > param.header->maxCp) {
        return false;
    }
    uint16_t nextValue = *(data + nextOffset + param.offset);
    nextOffset = nextValue & 0x3fff; // Use mask 0x3fff to extract the lower 14 bits of nextValue
    type = (PathType)(nextValue >> HYPHEN_SHIFT_BITS_14);
    return true;
}

bool processOtherType(const ArrayOf16bits* data, HyphenFindBreakParam& param, uint16_t subWord, uint32_t& nextOffset,
                      PathType& type)
{
    uint16_t count = data->count;
    bool match = false;
    for (size_t j = 0; j < count; j += HYPHEN_BASE_CODE_SHIFT) {
        if (data->codes[j] == subWord) {
            param.code = subWord;
            param.offset = param.header->codeOffset(subWord);
            if (param.header->minCp != param.header->maxCp && param.offset > param.header->maxCp) {
                break;
            }
            nextOffset = data->codes[j + 1] & 0x3fff;
            type = (PathType)(data->codes[j + 1] >> HYPHEN_SHIFT_BITS_14);
            match = true;
            break;
        } else if (data->codes[j] > subWord) {
            break;
        }
    }
    return match;
}

void findBreakByType(HyphenFindBreakParam& param, const size_t& index, std::vector<uint16_t>& target,
                     std::vector<uint8_t>& result)
{
    auto [staticOffset, nextOffset, type] = param.hyphenSubTable;
    uint32_t ix = 0;
    // enter cycle, we break when we find something that either matches or conflicts with a code
    // point
    while (true) {
        //   read access type (header) or null (break)
        //   move (remove mask bits and add static offset)
        if (type == PathType::PATTERN) {
            // if we have reached pattern, apply it to result
            auto p = reinterpret_cast<const Pattern*>(staticOffset + nextOffset);
            processPattern(p, index, ix, target, result);
            // loop breaks
            break;
            // else if we can directly point to next entry
        } else if (type == PathType::DIRECT) {
            // resolve new code point
            if (ix == index) {
                break;
            }
            ix++;
            param.code = target[index - ix];
            if (!processDirect(staticOffset, param, nextOffset, type)) {
                break;
            }
        } else if (type == PathType::LINEAR) {
            processLinear((staticOffset + nextOffset), index, ix, target, result);
            // either way, break
            break;
        } else {
            // resolve new code point
            if (ix == index) {
                break;
            }
            ix++;
            auto p = reinterpret_cast<const ArrayOf16bits*>(staticOffset + nextOffset);
            if (!processOtherType(p, param, target[index - ix], nextOffset, type)) {
                break;
            }
        }
    }
}

std::vector<uint8_t> findBreaks(const std::vector<uint8_t>& hyphenatorData, std::vector<uint16_t>& target)
{
    std::vector<uint8_t> result;
    result.resize(target.size());

    if (hyphenatorData.empty()) {
        return result;
    }

    formatTarget(target);
    result.resize(target.size(), 0);

    HyphenTableInfo hyphenInfo;
    if (!hyphenInfo.initHyphenTableInfo(hyphenatorData)) {
        return result;
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

    result.erase(result.cbegin());
    result.pop_back();
    return result;
}

std::vector<uint8_t> Hyphenator::FindBreakPositions(const std::vector<uint8_t>& hyphenatorData, const SkString &text,
                                        size_t startPos, size_t endPos)
{
    const auto lastword = std::string(text.c_str() + startPos, text.c_str() + endPos);
    std::vector<uint8_t> result;
    // resolve potential break positions
    if (!hyphenatorData.empty() && startPos + HYPHEN_WORD_SHIFT < endPos) {
        // need to have at least 4 characters for hyphenator to process
        // This a tricky bit, hyphenator expetcs an array of utf16, we have utf8 in fText and
        // unicode in fUnicodeText Best match currently is obtained running a conversion from utf8
        // toutf16 but it wastes a bit of memory & cpu clean this up once the dust settles a bit
        const auto lastword = std::string(text.c_str() + startPos, text.c_str() + endPos);
        std::vector<uint16_t> word;
        int32_t i = 0;
        const int32_t textLength = endPos - startPos;
        uint32_t c = 0;
        while (i < textLength) {
            U8_NEXT(lastword.c_str(), i, textLength, c);
            if (U16_LENGTH(c) == 1) {
                word.push_back(c);
            } else {
                word.push_back(U16_LEAD(c));
                word.push_back(U16_TRAIL(c));
            }
        }

        result = findBreaks(hyphenatorData, word);
    }
    return result;
}
} // namespace textlayout
} // namespace skia
#endif