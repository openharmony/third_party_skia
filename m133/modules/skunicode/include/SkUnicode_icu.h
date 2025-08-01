/*
 * Copyright 2024 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkUnicode_icu_DEFINED
#define SkUnicode_icu_DEFINED

#include "include/core/SkRefCnt.h"
#include "modules/skunicode/include/SkUnicode.h"

#ifdef ENABLE_DRAWING_ADAPTER
namespace SkiaRsText {
namespace SkUnicodes::ICU {
SKUNICODE_API sk_sp<SkUnicode> Make();
}
}
namespace SkUnicodes::ICU {
using namespace SkiaRsText::SkUnicodes::ICU;
}
#else
namespace SkUnicodes::ICU {
SKUNICODE_API sk_sp<SkUnicode> Make();
}
#endif
#endif //SkUnicode_icu_DEFINED
