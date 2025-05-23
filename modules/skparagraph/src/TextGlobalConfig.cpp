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
#include "include/TextGlobalConfig.h"

namespace skia {
namespace textlayout {
uint32_t TextGlobalConfig::bundleApiVersion_{0};
std::atomic<bool> TextGlobalConfig::noGlyphShowTofu_{false};

bool TextGlobalConfig::IsTargetApiVersion(uint32_t targetVersion)
{
    return bundleApiVersion_ >= targetVersion;
}

void TextGlobalConfig::SetNoGlyphShow(uint32_t noGlyphShow)
{
    noGlyphShowTofu_.store(noGlyphShow == 1);
};

bool TextGlobalConfig::NoGlyphShowUseTofu(const std::string& family)
{
    return noGlyphShowTofu_.load() && family == NOTDEF_FAMILY;
};
}  // namespace textlayout
}  // namespace skia
