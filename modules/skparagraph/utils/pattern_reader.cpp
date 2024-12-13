// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <codecvt>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <unicode/utf.h>
#include <unicode/utf8.h>

using namespace std;

enum class PathType : uint8_t {
    PATTERN = 0,
    LINEAR = 1,
    PAIRS = 2,
    DIRECT = 3
};

int main(int argc, char** argv)
{
    if (argc != 3) {
        cout << "usage: './hyphen hyph-en-us.pat.hib <mytestword>' " << endl;
        return 0;
    }

    const std::string filePath = argv[1];
    // begin and end markers could be optimized further for sure, but have them just supported as of now
    const std::string target_u8 = "." + std::string(argv[2]) + ".";
    // const std::string target_u8 = argv[2];

    std::vector<uint16_t> target;
    int32_t i = 0;
    const int32_t textLength = target_u8.size();
    uint32_t c = 0;
    while (i < textLength) {
        U8_NEXT(target_u8.c_str(), i, textLength, c);
        if (U16_LENGTH(c) == 1) {
            target.push_back(c);
        } else {
            target.push_back(U16_LEAD(c));
            target.push_back(U16_TRAIL(c));
        }
    }
    // const std::string target = "";
    size_t length { 0 };
    FILE* file {};
    cout << "Attempt to mmap " << filePath << endl;
    file = fopen(filePath.c_str(), "r");
    if (!file) {
        cerr << "FATAL: " << errno << endl;
        return -1;
    }

    struct stat st;
    if (fstat(fileno(file), &st) != 0) {
        cerr << "FATAL: fstat" << endl;
        fclose(file);
        return -1;
    }
    length = st.st_size;
    const uint8_t* address = static_cast<const uint8_t*>(mmap(NULL, length, PROT_READ, MAP_PRIVATE, fileno(file), 0u));

    if (address == MAP_FAILED) {
        cerr << "FATAL: mmap" << endl;
        fclose(file);
        return -1;
    }

    cout << "Magic: " << hex << *(uint32_t*)address << dec << endl;

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

        inline uint16_t codeOffset(uint16_t code, const ArrayOf16bits* maps = 0) const
        {
            if (maps && (code < minCp || code > maxCp)) {
                for (size_t ii = maps->count; ii != 0;) {
                    ii -= 2;
                    if (maps->codes[ii] == code) {
                        // cout << "resolved mapping ix: " << (int)m->codes[ii + 1] << endl;
                        auto offset = maps->codes[ii + 1];
                        return (maxCp - minCp) * 2 + (offset - maxCp) * 2 + 1;
                    }
                }
                return maxCount(maps);
            }
            if (maps) {
                return (code - minCp) * 2 + 1;
                // + 1 because previous end is before next start
                // 2x because every second value to beginning addres
            } else {
                if (code < minCp || code > maxCp) {
                    return maxCp + 1;
                }
                return (code - minCp);
            }
        }
        inline static void toLower(uint16_t& code)
        {
            code = tolower(code); // todo, should use tabled value or rather something from icu
            cout << "tolower: " << hex << (int)code << endl;
        }
        inline uint16_t maxCount(const ArrayOf16bits* maps) const
        {
            // need to write this in binary provider !!
            return (maxCp - minCp) * 2 + maps->count;
        }
    };

    for(auto& code : target) {
           Header::toLower(code);
    }

    vector<uint8_t> result;
    result.resize(target.size(), 0);

    auto header = reinterpret_cast<const Header*>(address);

    uint16_t minCp = header->minCp;
    uint16_t maxCp = header->maxCp;
    uint32_t toc_offset = header->toc;

    // get master table, it always is in direct mode
    const uint32_t* maindict = (uint32_t*)(address + toc_offset);
    const uint32_t* pMappings = (uint32_t*)(address + header->mappings);
    const ArrayOf16bits* mappings = reinterpret_cast<const ArrayOf16bits*>(pMappings);
    // this is actually beyond the real 32 bit address, but just to have an offset that
    // is clearly out of bounds without recalculating it again
    const uint16_t maxcount = header->maxCount(mappings);

    cout << "min/max: " << minCp << "/" << maxCp << " count " << (int)maxcount << endl;

    cout << "size of top level mappings: " << (int)mappings->count << endl;

    if (minCp == maxCp && mappings->count == 0) {
        cerr << "### unexpected min/max in input file-> exit" << endl;
        return -1;
    }

    for (size_t ii = target.size(); ii != 1; --ii) {
        auto code = target[ii];

        auto offset = header->codeOffset(code, mappings);
        if (offset == maxcount) {
            cout << hex << char(code) << " unable to map, contiue straight" << endl;
            result[ii] = 0;
            continue;
        }

        uint32_t baseOffset = *(maindict + offset - 1); // previous entry end
        uint32_t initialValue = *(maindict + offset);
        if (initialValue == 0) { // 0 is never valid offset from maindict
            cout << char(code) << " is not in main dict, contiue straight" << endl;
            continue;
        }
        // base offset is 16 bit
        auto staticOffset = (uint16_t*)(address + 2 * baseOffset);

        // get a subtable according character
        // once: read as 32bit, the rest of the access will be 16bit (13bit for offsets)
        auto nextOffset = (initialValue & 0x3fffffff);
        PathType type = (PathType)(initialValue >> 30);

        cout << hex << baseOffset << " top level code: 0x" << hex << (int)code << " starting with offset: 0x" << hex << offset << " table-offset 0x" << nextOffset << endl;

        uint32_t ix = 0;
        // enter cycle, we break when we find something that either matches or conflicts with a code point
        while (true) {
            //   read access type (header) or null (break)
            //   move (remove mask bits and add offset)
            // uint16_t next = *(staticOffset + nextOffset);
            // cout << "next offset: " << nextOffset << " value: " << hex << (uint32_t)next << dec << endl;
            cout << "#loop c: '" << code << "' starting with offset: 0x" << hex << offset << " table-offset 0x" << nextOffset << " ix: " << ix << endl;

            if (type == PathType::PATTERN) {
                //   if we have reached pattern, apply it to result
                auto p = reinterpret_cast<const Pattern*>(staticOffset + nextOffset);
                uint16_t code = p->code;
                // if the code point is defined, the sub index refers to code next to this node
                if (code) {
                    if (code != target[ii - ix]) {
                        cout << "break on pattern: " << hex << (int)code << endl;
                        break;
                    }
                } else {
                    // so qe need to substract if this is the direct ref
                    ix--;
                }
                uint16_t count = p->count;
                cout << "  found pattern with size: " << count << " ix: " << ix << endl;
                size_t i { 0 };
                // when we reach pattern node (leaf), we need to increase ix by one because of our own code offset
                for (size_t jj = ii - ix; jj <= ii && i < count; jj++) { // Todo: don't place markers to two last slots
                    cout << "    pattern index: " << i << " value: 0x" << hex << (int)(p->patterns[i]) << endl;
                    result[jj] = std::max(result[jj], (p->patterns[i]));
                    i++;
                }
                // loop breaks
                cout << "break on pattern" << endl;
                break;
                //   else if we can directly point to next entry
            } else if (type == PathType::DIRECT) {
                // resolve new code point
                if (ix == ii) { // should never be the case
                    cout << "# break loop on direct" << endl;
                    break;
                }

                ix++;
                code = target[ii - ix];
                offset = header->codeOffset(code);
                if (offset > maxCp) {
                    cout << "# break loop on direct" << endl;
                    break;
                }

                auto nextValue = *(staticOffset + nextOffset + offset);
                nextOffset = nextValue & 0x3fff;
                type = (PathType)(nextValue >> 14);
                cout << "  found direct: " << char(code) << " : " << hex << nextValue << " with offset: " << nextOffset << endl;
                // continue looping;
            } else if (type == PathType::LINEAR) {
                auto p = reinterpret_cast<const ArrayOf16bits*>(staticOffset + nextOffset);
                auto count = p->count;
                auto origPos = ix;
                ix++;
                cout << "  found linear with size: " << count << " looking next " << (int)target[ii - ix] << endl;
                if (count > ii - ix + 1) {
                    // the pattern is longer than the remaining word
                    cout << "# break loop on linear " << ii << " " << ix << endl;
                    break;
                }
                bool match = true;
                //     check the rest of the string
                for (auto jj = 0; jj < count; jj++) {
                    cout << "    linear index: " << jj << " value: " << hex << (int)p->codes[jj] << " vs " << (int)target[ii - ix] << endl;
                    if (p->codes[jj] != target[ii - ix]) {
                        match = false;
                        break;
                    } else {
                        ix++;
                    }
                }

                //     if we reach the end, apply pattern
                if (match) {
                    nextOffset += count + (count & 0x1);
                    auto p = reinterpret_cast<const Pattern*>(staticOffset + nextOffset);
                    cout << "    found match, needed to pad " << (int)(count & 0x1) << " pat count: " << (int)p->count << endl;
                    size_t i { 0 };
                    for (size_t jj = ii - origPos - count; jj <= ii && i < p->count; jj++) {
                        cout << "       pattern index: " << i << " value: " << hex << (int)p->patterns[i] << endl;
                        result[jj] = std::max(result[jj], p->patterns[i]);
                        i++;
                    }
                }
                // either way, break
                cout << "# break loop on linear" << endl;
                break;
            } else {
                // resolve new code point
                if (ix == ii) { // should detect this sooner
                    cout << "# break loop on pairs" << endl;
                    break;
                }
                auto p = reinterpret_cast<const ArrayOf16bits*>(staticOffset + nextOffset);
                uint16_t count = p->count;
                ix++;
                cout << "  continue to value pairs with size: " << count << " and code '" << (int)target[ii - ix] << "'" << endl;

                //     check pairs, array is sorted (but small)
                bool match = false;
                for (size_t jj = 0; jj < count; jj += 2) {
                    cout << "    checking pair: " << jj << " value: " << hex << (int)p->codes[jj] << " vs " << (int)target[ii - ix] << endl;
                    if (p->codes[jj] == target[ii - ix]) {
                        code = target[ii - ix];
                        cout << "      new value pair in : 0x" << jj << " with code 0x" << hex << (int)code << "'" << endl;
                        offset = header->codeOffset(code);
                        if (offset > maxCp) {
                            cout << "# break loop on pairs" << endl;
                            break;
                        }

                        nextOffset = p->codes[jj + 1] & 0x3fff;
                        type = (PathType)(p->codes[jj + 1] >> 14);
                        match = true;
                        break;
                    } else if (p->codes[jj] > target[ii - ix]) {
                        break;
                    }
                }
                if (!match) {
                    cout << "# break loop on pairs" << endl;
                    break;
                }
            }
        }
    }
    fclose(file);
    cout << "result size: " << result.size() << " while expecting " << target.size() << endl;
    if (result.size() <= target.size() + 1) {
        size_t ii = 0;
        for (auto bp : result) {
            cout << hex << (int)target[ii++] << ": " << to_string(bp) << endl;
        }
    }
    return 0;
}
