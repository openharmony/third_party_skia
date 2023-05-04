/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkFontMgrPriv.h"
#include "tools/flags/CommandLineFlags.h"
#include "tools/flags/CommonFlags.h"
#include "tools/fonts/TestFontMgr.h"

#if defined(SK_BUILD_FOR_WIN)
#include "include/ports/SkTypeface_win.h"
#endif
SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS();

namespace CommonFlags {


static DEFINE_bool(nativeFonts,
                   true,
                   "If true, use native font manager and rendering. "
                   "If false, fonts will draw as portably as possible.");
#if defined(SK_BUILD_FOR_WIN)
static DEFINE_bool(gdi, false, "Use GDI instead of DirectWrite for font rendering.");
#endif

void SetDefaultFontMgr() {
    gSkFontMgr_DefaultFactory = &SkFontMgr_New_OHOS;
}

}
