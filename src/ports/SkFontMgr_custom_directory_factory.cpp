/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_directory.h"

#ifndef SK_FONT_FILE_PREFIX
#  if defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)
#    define SK_FONT_FILE_PREFIX "/System/Library/Fonts/"
#  else
#    define SK_FONT_FILE_PREFIX "/usr/share/fonts/"
#  endif
#endif

#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_LINUX)
std::string SkFontMgr::containerFontPath = "";
std::string SkFontMgr::runtimeOS = "OHOS";
SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS(const char *path);
sk_sp<SkFontMgr> SkFontMgr::Factory()
{
    if (SkFontMgr::runtimeOS == "OHOS") {
        return SkFontMgr_New_OHOS(nullptr);
    }
    return SkFontMgr_New_Custom_Directory(SK_FONT_FILE_PREFIX);
}
#else
sk_sp<SkFontMgr> SkFontMgr::Factory()
{
    return SkFontMgr_New_Custom_Directory(SK_FONT_FILE_PREFIX);
}
#endif
