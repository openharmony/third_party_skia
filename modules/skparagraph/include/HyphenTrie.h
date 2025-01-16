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

#ifndef THIRD_PARTY_SKIA_HYPHENTRIE_H
#define THIRD_PARTY_SKIA_HYPHENTRIE_H

#ifdef OHOS_SUPPORT
#include <iostream>
#include <map>
#include <memory>
#include <string>

namespace skia {
namespace textlayout {
class TrieNode {
public:
    std::map<char, std::shared_ptr<TrieNode>> children;
    std::string value;
};

class HyphenTrie {
public:
    HyphenTrie() { root = std::make_shared<TrieNode>(); };
    ~HyphenTrie() = default;
    HyphenTrie(HyphenTrie&&) = delete;
    HyphenTrie& operator=(HyphenTrie&&) = delete;
    HyphenTrie(const HyphenTrie&) = delete;
    HyphenTrie& operator=(const HyphenTrie&) = delete;

    void insert(const std::string& key, const std::string& value);
    std::string findPartialMatch(const std::string& keyPart);

private:
    std::shared_ptr<TrieNode> root = nullptr;

    std::string collectValues(const std::shared_ptr<TrieNode>& node);
};
} // namespace textlayout
} // namespace skia
#endif
#endif // THIRD_PARTY_SKIA_HYPHENTRIE_H
