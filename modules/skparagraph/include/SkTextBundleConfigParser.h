/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.. All rights reserved.
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

#ifndef THIRD_PARTY_SKIA_BUNDLE_MANAGER_H
#define THIRD_PARTY_SKIA_BUNDLE_MANAGER_H

#include <cstddef>
#include <cstdint>

namespace skia {
namespace textlayout {
constexpr uint32_t SINCE_API18_VERSION = 18;

class SkTextBundleConfigParser {
public:
    static bool IsTargetApiVersion(uint32_t targetVersion);

    static void SetTargetVersion(uint32_t targetVersion)
    {
        bundleApiVersion_ = targetVersion;
    };

private:
    static uint32_t bundleApiVersion_;
};
} // namespace textlayout
} // namespace skia

#endif // THIRD_PARTY_SKIA_BUNDLE_MANAGER_H