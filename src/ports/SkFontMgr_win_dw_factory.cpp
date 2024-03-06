/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkTypes.h"
#if defined(SK_BUILD_FOR_WIN)  // And !SKIA_GDI?

#include "include/core/SkFontMgr.h"
#include "include/ports/SkTypeface_win.h"
#include "src/ports/SkFontMgr_preview.h"

std::string SkFontMgr::runtimeOS = "OHOS";
std::string containerFontPath = "";
SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS(const char *path);

sk_sp<SkFontMgr> SkFontMgr::Factory() {
    if (SkFontMgr::runtimeOS == "OHOS") {
        return SkFontMgr_New_OHOS(nullptr);
    }
    if (SkFontMgr::runtimeOS == "OHOS_Container") {
        return SkFontMgr_New_OHOS(containerFontPath.c_str());
    }
    return SkFontMgr_New_DirectWrite();
}

#endif//defined(SK_BUILD_FOR_WIN)
