// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_SKIA_HYPHENATOR_H
#define THIRD_PARTY_SKIA_HYPHENATOR_H

#ifdef OHOS_SUPPORT
#include <cctype>
#include <cstring>
#include <memory>
#include <shared_mutex>
#include <ucase.h>
#include <unordered_map>
#include <vector>

#include "include/core/SkString.h"

namespace skia {
namespace textlayout {
constexpr size_t HYPHEN_WORD_SHIFT = 4;
constexpr size_t HYPHEN_BASE_CODE_SHIFT = 2;
constexpr size_t HYPHEN_SHIFT_BITS_14 = 14;
constexpr size_t HYPHEN_SHIFT_BITS_30 = 30;

enum class PathType : uint8_t {
    PATTERN = 0,
    LINEAR = 1,
    PAIRS = 2,
    DIRECT = 3
};

struct Pattern {
    uint16_t code;
    uint16_t count;
    uint8_t patterns[4]; // dynamic
};

struct ArrayOf16bits {
    uint16_t count;
    uint16_t codes[3]; // dynamic
};

struct HyphenatorHeader {
    uint8_t magic1{0};
    uint8_t magic2{0};
    uint8_t minCp{0};
    uint8_t maxCp{0};
    uint32_t toc{0};
    uint32_t mappings{0};
    uint32_t version{0};

    inline uint16_t codeOffset(uint16_t code, const ArrayOf16bits* maps = nullptr) const
    {
        // need still reconsider what we want to do with a nodes in the middle of graph
        if (maps != nullptr && (code < minCp || code > maxCp)) {
            // we could assert that count is even
            for (size_t i = maps->count; i != 0;) {
                i -= HYPHEN_BASE_CODE_SHIFT;
                if (maps->codes[i] == code) {
                    auto offset = maps->codes[i + 1];
                    return (maxCp - minCp) * HYPHEN_BASE_CODE_SHIFT + (offset - maxCp) * HYPHEN_BASE_CODE_SHIFT + 1;
                }
            }
            return maxCount(maps);
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

    inline static void toLower(uint16_t& code)
    {
        // Open Harmony seems to have this even before C++20
        code = ucase_tolower(code);
    }
    
    inline uint16_t maxCount(const ArrayOf16bits* maps) const
    {
        // need to write this in binary provider !!
        return (maxCp - minCp) * HYPHEN_BASE_CODE_SHIFT + maps->count;
    }
};

class Hyphenator {
public:
    static Hyphenator& GetInstance()
    {
        static Hyphenator instance;
        return instance;
    }
    const std::vector<uint8_t>& GetHyphenatorData(const std::string& locale);
    bool LoadHyphenatorData(const std::string& locale);
    std::vector<uint8_t> FindBreakPositions(const std::vector<uint8_t>& hyphenatorData, const SkString& text,
                                            size_t startPos, size_t endPos);

private:
    Hyphenator() = default;
    ~Hyphenator() = default;
    Hyphenator(const Hyphenator&) = delete;
    Hyphenator& operator=(const Hyphenator&) = delete;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::vector<uint8_t>> hyphenMap;
};
} // namespace textlayout
} // namespace skia
#endif
#endif  // THIRD_PARTY_SKIA_HYPHENATOR_H
