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

#ifndef TEXT_PARAMETER_DEFINED
#define TEXT_PARAMETER_DEFINED

#ifdef ENABLE_TEXT_ENHANCE
#ifdef ENABLE_OHOS_ENHANCE
#include "parameters.h"
#endif

namespace skia {
namespace textlayout {
class TextParameter {
public:
    static bool GetAutoSpacingEnable()
    {
#ifdef ENABLE_OHOS_ENHANCE
        static bool autoSpacingEnable = OHOS::system::GetBoolParameter("persist.sys.text.autospacing.enable", false);
        return autoSpacingEnable;
#else
        return false;
#endif
    }
    static uint32_t GetUnicodeSizeLimitForParagraphCache()
    {
        const uint32_t unicodeSizeDefaultLimit = 16000;
#ifdef ENABLE_OHOS_ENHANCE
        static uint32_t unicodeSizeLimit = OHOS::system::GetUintParameter(
            "persist.sys.text.paragraph_cache.unicode_size_limit", unicodeSizeDefaultLimit);
        return unicodeSizeLimit;
#else
        return unicodeSizeDefaultLimit;
#endif
    }
};
}  // namespace textlayout
}  // namespace skia

#endif // ENABLE_TEXT_ENHANCE
#endif // TEXT_PARAMETER_DEFINED
