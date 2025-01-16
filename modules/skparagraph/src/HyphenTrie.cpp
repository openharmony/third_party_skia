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
#include "include/HyphenTrie.h"

namespace skia {
namespace textlayout {
void HyphenTrie::insert(const std::string& key, const std::string& value)
{
    std::shared_ptr<TrieNode> node = root;
    for (char c : key) {
        if (node->children.count(c) == 0) {
            node->children.emplace(c, std::make_shared<TrieNode>());
        }
        node = node->children[c];
    }
    node->value = value;
}

std::string HyphenTrie::findPartialMatch(const std::string& keyPart)
{
    std::shared_ptr<TrieNode> node = root;
    for (char c : keyPart) {
        if (node->children.find(c) == node->children.end()) {
            return "";
        }
        node = node->children[c];
    }
    return collectValues(node);
}

std::string HyphenTrie::collectValues(const std::shared_ptr<TrieNode>& node)
{
    if (node == nullptr) {
        return "";
    }
    if (!node->value.empty()) {
        return node->value;
    }
    for (const auto& child : node->children) {
        std::string value = collectValues(child.second);
        if (!value.empty()) {
            return value;
        }
    }
    return "";
}
} // namespace textlayout
} // namespace skia
#endif
