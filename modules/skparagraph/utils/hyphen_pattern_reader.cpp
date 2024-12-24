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
#include "hyphen_pattern.h"

#include <codecvt>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unicode/utf.h>
#include <unicode/utf8.h>
#include <unistd.h>

namespace OHOS::Hyphenate {
constexpr size_t SHIFT_BITS_14 = 14;
constexpr size_t SHIFT_BITS_30 = 30;
constexpr size_t PADDING_SIZE = 4;

vector<uint16_t> ConvertToUtf16(const string& utf8Str)
{
    int32_t i = 0;
    uint32_t c = 0;
    vector<uint16_t> target;
    const int32_t textLength = utf8Str.size();

    while (i < textLength) {
        U8_NEXT(utf8Str.c_str(), i, textLength, c);
        if (U16_LENGTH(c) == 1) {
            target.push_back(c);
        } else {
            target.push_back(U16_LEAD(c));
            target.push_back(U16_TRAIL(c));
        }
    }
    return target;
}

struct Pattern {
    uint16_t code;
    uint16_t count;
    uint8_t patterns[4]; // dynamic
};

struct ArrayOf16bits {
    uint16_t count;
    uint16_t codes[3]; // dynamic
};

struct Header {
    uint8_t magic1;
    uint8_t magic2;
    uint8_t minCp;
    uint8_t maxCp;
    uint32_t toc;
    uint32_t mappings;
    uint32_t version;

    inline uint16_t CodeOffset(uint16_t code, const ArrayOf16bits* maps = nullptr) const
    {
        if (maps && (code < minCp || code > maxCp)) {
            for (size_t i = maps->count; i != 0;) {
                i -= HYPHEN_BASE_CODE_SHIFT;
                if (maps->codes[i] == code) {
                    // cout << "resolved mapping ix: " << static_cast<int>(m->codes[i + 1]) << endl;
                    auto offset = maps->codes[i + 1];
                    return (maxCp - minCp) * HYPHEN_BASE_CODE_SHIFT + (offset - maxCp) * HYPHEN_BASE_CODE_SHIFT + 1;
                }
            }
            return MaxCount(maps);
        }
        if (maps) {
            // + 1 because previous end is before next start
            // 2x because every second value to beginning addres
            return (code - minCp) * HYPHEN_BASE_CODE_SHIFT + 1;
        } else {
            if (code < minCp || code > maxCp) {
                return maxCp + 1;
            }
            return (code - minCp);
        }
    }

    inline static void ToLower(uint16_t& code)
    {
        code = tolower(code);
        cout << "tolower: " << hex << static_cast<int>(code) << endl;
    }

    inline uint16_t MaxCount(const ArrayOf16bits* maps) const
    {
        // need to write this in binary provider !!
        return (maxCp - minCp) * HYPHEN_BASE_CODE_SHIFT + maps->count;
    }
};

struct CodeInfo {
    int32_t OpenPatFile(const char* filePath);
    int32_t GetHeader();
    int32_t GetCodeInfo(uint16_t code);
    void ProcessPattern(const std::vector<uint16_t>& target, size_t offset, vector<uint8_t>& result);
    bool ProcessDirect(const std::vector<uint16_t>& target, size_t& offset);
    void ProcessLinear(const std::vector<uint16_t>& target, size_t offset, vector<uint8_t>& result);
    bool ProcessNextCode(const std::vector<uint16_t>& target, size_t& offset);
    void ClearResource();
    Header* header_{nullptr};
    uint8_t* address_{nullptr};
    FILE* file_{nullptr};
    size_t fileSize{0};
    uint16_t maxCount{0};
    PathType type{PathType::PATTERN};
    uint16_t offset_{0};
    uint16_t code_{0};
    uint32_t index_{0};
    uint32_t nextOffset_;
    uint16_t* staticOffset_{nullptr};
    ArrayOf16bits* mappings{nullptr};
};

int32_t CodeInfo::OpenPatFile(const char* filePath)
{
    cout << "Attempt to mmap " << filePath << endl;

    FILE* file = fopen(filePath, "r");
    if (file == nullptr) {
        cerr << "FATAL: " << errno << endl;
        return FAILED;
    }

    struct stat st;
    if (fstat(fileno(file), &st) != 0) {
        cerr << "FATAL: fstat" << endl;
        fclose(file);
        return FAILED;
    }
    size_t length = st.st_size;
    uint8_t* address = static_cast<uint8_t*>(mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fileno(file), 0u));
    if (address == MAP_FAILED) {
        cerr << "FATAL: mmap" << endl;
        fclose(file);
        return FAILED;
    }

    cout << "Magic: " << hex << *reinterpret_cast<uint32_t*>(address) << dec << endl;
    this->file_ = file;
    this->fileSize = length;
    this->address_ = address;
    return SUCCEED;
}

static std::vector<uint16_t> GetInputWord(const char* input)
{
    const std::string utf8Str = "." + std::string(input) + ".";
    std::vector<uint16_t> target = ConvertToUtf16(utf8Str);
    for (auto& code : target) {
        Header::ToLower(code);
    }
    return target;
}

int32_t CodeInfo::GetHeader()
{
    header_ = reinterpret_cast<Header*>(address_);
    uint16_t minCp = header_->minCp;
    uint16_t maxCp = header_->maxCp;
    // get master table, it always is in direct mode
    mappings = reinterpret_cast<ArrayOf16bits*>(reinterpret_cast<uint32_t*>(address_ + header_->mappings));
    // this is actually beyond the real 32 bit address, but just to have an offset that
    // is clearly out of bounds without recalculating it again
    maxCount = header_->MaxCount(mappings);
    cout << "min/max: " << minCp << "/" << maxCp << " count " << static_cast<int>(maxCount) << endl;
    cout << "size of top level mappings: " << static_cast<int>(mappings->count) << endl;
    if (minCp == maxCp && mappings->count == 0) {
        cerr << "### unexpected min/max in input file-> exit" << endl;
        return FAILED;
    }
    return SUCCEED;
}

void CodeInfo::ClearResource()
{
    (void)munmap(address_, fileSize);
    address_ = nullptr;
    (void)fclose(file_);
    file_ = nullptr;
    fileSize = 0;
}

int32_t CodeInfo::GetCodeInfo(uint16_t code)
{
    type = PathType::PATTERN;
    this->code_ = code;
    this->index_ = 0;
    offset_ = header_->CodeOffset(code, mappings);
    if (offset_ == maxCount) {
        cout << hex << char(code) << " unable to map, contiue straight" << endl;
        return FAILED;
    }

    // previous entry end
    uint32_t baseOffset =
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint32_t*>(address_ + header_->toc) + offset_ - 1);
    uint32_t initialValue = *(reinterpret_cast<uint32_t*>(address_ + header_->toc) + offset_);
    if (initialValue == 0) { // 0 is never valid offset from maindict
        cout << char(code) << " is not in main dict, contiue straight" << endl;
        return FAILED;
    }
    // base offset is 16 bit
    staticOffset_ = reinterpret_cast<uint16_t*>(address_ + HYPHEN_BASE_CODE_SHIFT * baseOffset);

    // get a subtable according character
    // once: read as 32bit, the rest of the access will be 16bit (13bit for offsets)
    nextOffset_ = (initialValue & 0x3fffffff);
    type = static_cast<PathType>(initialValue >> SHIFT_BITS_30);

    cout << hex << baseOffset << " top level code: 0x" << hex << static_cast<int>(code) << " starting with offset: 0x"
         << hex << offset_ << " table-offset 0x" << nextOffset_ << endl;
    return SUCCEED;
}

void CodeInfo::ProcessPattern(const std::vector<uint16_t>& target, size_t offset, vector<uint8_t>& result)
{
    //   if we have reached pattern, apply it to result
    auto p = reinterpret_cast<const Pattern*>(staticOffset_ + nextOffset_);
    uint16_t codeValue = p->code;
    // if the code point is defined, the sub index refers to code next to this node
    if (codeValue) {
        if (codeValue != target[offset - index_]) {
            cout << "break on pattern: " << hex << codeValue << endl;
            return;
        }
    } else {
        // so qe need to substract if this is the direct ref
        index_--;
    }
    uint16_t count = p->count;
    cout << "  found pattern with size: " << count << " ix: " << index_ << endl;
    size_t i = 0;
    // when we reach pattern node (leaf), we need to increase ix by one because of our
    // own code offset
    for (size_t j = offset - index_; j <= offset && i < count; j++) {
        cout << "    pattern index: " << i << " value: 0x" << hex << static_cast<int>(p->patterns[i]) << endl;
        result[j] = std::max(result[j], (p->patterns[i]));
        i++;
    }
    // loop breaks
    cout << "break on pattern" << endl;
    //   else if we can directly point to next entry
}

bool CodeInfo::ProcessDirect(const std::vector<uint16_t>& target, size_t& offset)
{
    // resolve new code point
    if (index_ == offset) { // should never be the case
        cout << "# break loop on direct" << endl;
        return true;
    }

    index_++;
    code_ = target[offset - index_];
    offset = header_->CodeOffset(code_);
    if (offset > header_->maxCp) {
        cout << "# break loop on direct" << endl;
        return true;
    }

    auto nextValue = *(staticOffset_ + nextOffset_ + offset);
    nextOffset_ = nextValue & 0x3fff;
    type = static_cast<PathType>(nextValue >> SHIFT_BITS_14);
    cout << "  found direct: " << char(code_) << " : " << hex << nextValue << " with offset: " << nextOffset_ << endl;
    return false;
}

void CodeInfo::ProcessLinear(const std::vector<uint16_t>& target, size_t offset, vector<uint8_t>& result)
{
    auto p = reinterpret_cast<const ArrayOf16bits*>(staticOffset_ + nextOffset_);
    auto count = p->count;
    auto origPos = index_;

    index_++;
    cout << "  found linear with size: " << count << " looking next " << static_cast<int>(target[offset - index_])
         << endl;
    if (count > (offset - index_ + 1)) {
        // the pattern is longer than the remaining word
        cout << "# break loop on linear " << offset << " " << index_ << endl;
        return;
    }
    bool match = true;
    //     check the rest of the string
    for (auto j = 0; j < count; j++) {
        cout << "    linear index: " << j << " value: " << hex << static_cast<int>(p->codes[j]) << " vs "
             << static_cast<int>(target[offset - index_]) << endl;
        if (p->codes[j] != target[offset - index_]) {
            match = false;
            return;
        } else {
            index_++;
        }
    }

    //  if we reach the end, apply pattern
    if (match) {
        nextOffset_ += count + (count & 0x1);
        auto matchPattern = reinterpret_cast<const Pattern*>(staticOffset_ + nextOffset_);
        cout << "    found match, needed to pad " << static_cast<int>(count & 0x1)
             << " pat count: " << static_cast<int>(matchPattern->count) << endl;
        size_t i = 0;
        for (size_t j = offset - origPos - count; j <= offset && i < matchPattern->count; j++) {
            cout << "       pattern index: " << i << " value: " << hex << static_cast<int>(matchPattern->patterns[i])
                 << endl;
            result[j] = std::max(result[j], matchPattern->patterns[i]);
            i++;
        }
    }
    // either way, break
    cout << "# break loop on linear" << endl;
}

bool CodeInfo::ProcessNextCode(const std::vector<uint16_t>& target, size_t& offset)
{
    // resolve new code point
    if (index_ == offset) { // should detect this sooner
        cout << "# break loop on pairs" << endl;
        return true;
    }
    auto p = reinterpret_cast<const ArrayOf16bits*>(staticOffset_ + nextOffset_);
    uint16_t count = p->count;
    index_++;
    cout << "  continue to value pairs with size: " << count << " and code '"
         << static_cast<int>(target[offset - index_]) << "'" << endl;

    //     check pairs, array is sorted (but small)
    bool match = false;
    for (size_t j = 0; j < count; j += HYPHEN_BASE_CODE_SHIFT) {
        cout << "    checking pair: " << j << " value: " << hex << static_cast<int>(p->codes[j]) << " vs "
             << static_cast<int>(target[offset - index_]) << endl;
        if (p->codes[j] == target[offset - index_]) {
            code_ = target[offset - index_];
            cout << "      new value pair in : 0x" << j << " with code 0x" << hex << static_cast<int>(code_) << "'"
                 << endl;
            offset = header_->CodeOffset(code_);
            if (offset > header_->maxCp) {
                cout << "# break loop on pairs" << endl;
                break;
            }

            nextOffset_ = p->codes[j + 1] & 0x3fff;
            type = static_cast<PathType>(p->codes[j + 1] >> SHIFT_BITS_14);
            match = true;
            break;
        } else if (p->codes[j] > target[offset - index_]) {
            break;
        }
    }
    if (!match) {
        cout << "# break loop on pairs" << endl;
        return true;
    }
    return false;
}

void PrintResult(const vector<uint8_t>& result, const vector<uint16_t>& target)
{
    cout << "result size: " << result.size() << " while expecting " << target.size() << endl;
    if (result.size() <= target.size() + 1) {
        size_t i = 0;
        for (auto bp : result) {
            cout << hex << static_cast<int>(target[i++]) << ": " << to_string(bp) << endl;
        }
    }
}
} // namespace OHOS::Hyphenate

constexpr size_t ARG_NUM = 2;

std::vector<uint16_t> CheckArgs(int argc, char** argv)
{
    std::vector<uint16_t> target;
    if (argc != 3) { // 3: valid argument number
        cout << "usage: './hyphen hyph-en-us.pat.hib <mytestword>' " << endl;
        return target;
    }
    target = OHOS::Hyphenate::GetInputWord(argv[ARG_NUM]);
    if (target.empty()) {
        cout << "usage: './hyphen hyph-en-us.pat.hib <mytestword>' " << endl;
    }
    return target;
}

bool InitializeCodeInfo(OHOS::Hyphenate::CodeInfo& codeInfo, char* filePath)
{
    if (codeInfo.OpenPatFile(filePath) != SUCCEED) {
        return false;
    }
    if (codeInfo.GetHeader() != SUCCEED) {
        codeInfo.ClearResource();
        return false;
    }
    return true;
}

void ProcessCodeInfo(OHOS::Hyphenate::CodeInfo& codeInfo, const std::vector<uint16_t>& target,
                     std::vector<uint8_t>& result)
{
    for (size_t i = target.size() - 1; i != 0; --i) {
        if (codeInfo.GetCodeInfo(target[i]) != SUCCEED) {
            result[i] = 0;
            continue;
        }

        while (true) {
            std::cout << "#loop c: '" << codeInfo.code_ << "' starting with offset: 0x" << std::hex << codeInfo.offset
                      << " table-offset 0x" << codeInfo.nextOffset_ << " index: " << codeInfo.index_ << std::endl;

            if (codeInfo.type == OHOS::Hyphenate::PathType::PATTERN) {
                codeInfo.ProcessPattern(target, i, result);
                break;
            } else if (codeInfo.type == OHOS::Hyphenate::PathType::DIRECT && codeInfo.ProcessDirect(target, i)) {
                break;
            } else if (codeInfo.type == OHOS::Hyphenate::PathType::LINEAR) {
                codeInfo.ProcessLinear(target, i, result);
                break;
            } else if (codeInfo.ProcessNextCode(target, i)) {
                break;
            }
        }
    }
}

int main(int argc, char** argv)
{
    std::vector<uint16_t> target = CheckArgs(argc, argv);
    if (target.empty()) {
        return FAILED;
    }

    OHOS::Hyphenate::CodeInfo codeInfo;
    if (!InitializeCodeInfo(codeInfo, argv[1])) {
        return FAILED;
    }

    std::vector<uint8_t> result(target.size(), 0);
    ProcessCodeInfo(codeInfo, target, result);

    codeInfo.ClearResource();
    OHOS::Hyphenate::PrintResult(result, target);
    return SUCCEED;
}