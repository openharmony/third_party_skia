// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_SKIA_HYPHENATOR_H
#define THIRD_PARTY_SKIA_HYPHENATOR_H

#ifdef OHOS_SUPPORT
#include <cstring>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace skia {
namespace textlayout {

    class Hyphenator {
    public:
        static Hyphenator& GetInstance()
        {
            static Hyphenator instance;
            return instance;
        }
        const std::vector<uint8_t>& GetHyphenatorData(const std::string& locale);

    private:
        Hyphenator() = default;
        ~Hyphenator() = default;
        Hyphenator(const Hyphenator&) = delete;
        Hyphenator& operator=(const Hyphenator&) = delete;

        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, std::vector<uint8_t>> hyphMap;
    };

} // namespace textlayout
} // namespace skia
#endif
#endif  // THIRD_PARTY_SKIA_HYPHENATOR_H
