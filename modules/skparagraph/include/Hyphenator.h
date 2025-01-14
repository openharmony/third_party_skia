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

    uint16_t codeOffset(uint16_t code, const ArrayOf16bits* maps = nullptr) const
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

class TrieNode {
public:
    std::unordered_map<char, TrieNode*> children;
    std::string value;

    TrieNode() : value("") {}
};

class Trie {
public:
    Trie() : root(new TrieNode) {}
    ~Trie() { destroy(root); }

    void insert(const std::string& key, const std::string& value)
    {
        TrieNode* node = root;
        for (char c : key) {
            if (node->children.find(c) == node->children.end()) {
                node->children[c] = new TrieNode();
            }
            node = node->children[c];
        }
        node->value = value;
    }

    std::string findPartialMatch(const std::string& keyPart)
    {
        TrieNode* node = root;
        for (char c : keyPart) {
            if (node->children.find(c) == node->children.end()) {
                return "";
            }
            node = node->children[c];
        }

        return collectValues(node);
    }

private:
    TrieNode* root;

    void destroy(TrieNode* node)
    {
        if (!node) {
            return;
        }
        for (auto& child : node->children) {
            destroy(child.second);
        }
        delete node;
    }
    std::string collectValues(TrieNode* node)
    {
        if (!node) {
            return "";
        }
        if (!node->value.empty()) {
            return node->value;
        }
        for (auto& child : node->children) {
            std::string value = collectValues(child.second);
            if (!value.empty()) {
                return value;
            }
        }
        return "";
    }
};

class Hyphenator {
public:
    static Hyphenator& GetInstance()
    {
        static Hyphenator instance;
        static bool isInitialized = false;
        if (!isInitialized) {
            instance.initTrieTree();
            isInitialized = true;
        }
        return instance;
    }
    const std::vector<uint8_t>& getHyphenatorData(const std::string& locale);
    const std::vector<uint8_t>& findHyphenatorData(const std::string& langCode);
    const std::vector<uint8_t>& loadPatternFile(const std::string& langCode);
    std::vector<uint8_t> FindBreakPositions(const std::vector<uint8_t>& hyphenatorData, const SkString& text,
                                            size_t startPos, size_t endPos);

private:
    Hyphenator() = default;
    ~Hyphenator() = default;
    Hyphenator(const Hyphenator&) = delete;
    Hyphenator& operator=(const Hyphenator&) = delete;

    void initTrieTree();

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::vector<uint8_t>> fHyphenMap;
    Trie fTrieTree;
    const std::vector<uint8_t> fEmptyResult;
};
} // namespace textlayout
} // namespace skia
#endif
#endif  // THIRD_PARTY_SKIA_HYPHENATOR_H
