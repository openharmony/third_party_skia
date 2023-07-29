/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkTypes.h"
#if defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)

#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_mac_ct.h"
#include "src/ports/SkFontMgr_preview.h"

std::string SkFontMgr::runtimeOS = "OHOS";
SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS(const char *path);

sk_sp<SkFontMgr> SkFontMgr::Factory() {
#if !defined(USE_DEFAULT_FONT)
    if (SkFontMgr::runtimeOS == "OHOS") {
        return SkFontMgr_New_OHOS(nullptr);
    }
    if (SkFontMgr::runtimeOS == "OHOS_Container") {
        return SkFontMgr_New_Preview();
    }
#endif
    return SkFontMgr_New_CoreText(nullptr);
}

#endif//defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)
