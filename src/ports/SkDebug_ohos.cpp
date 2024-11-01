/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * 2023.4.26 log adapt ohos.
 * Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
 */

#include "include/core/SkTypes.h"
#include <stdio.h>

#ifdef SKIA_OHOS_SHADER_REDUCE
#include <parameters.h>
#endif

#define LOG_TAG "skia"
#include "hilog/log.h"

extern "C" {
    int HiLogPrintArgs(LogType type, LogLevel level, unsigned int domain, const char* tag, const char* fmt, va_list ap);
}

// Print debug output to stdout as well.  This is useful for command line
// applications (e.g. skia_launcher).
bool gSkDebugToStdOut = false;

void SkDebugf(const char format[], ...) {
    va_list args1, args2;
    va_start(args1, format);

    if (gSkDebugToStdOut) {
        va_copy(args2, args1);
        vprintf(format, args2);
        va_end(args2);
    }

    HiLogPrintArgs(LOG_CORE, LogLevel::LOG_DEBUG, 0xD001406, LOG_TAG, format, args1);

    va_end(args1);
}

#ifdef SKIA_OHOS_SHADER_REDUCE
bool SkShaderReduceProperty()
{
    static bool debugProp = std::atoi(OHOS::system::GetParameter("persist.sys.skia.shader.reduce", "1").c_str()) != 0;
    return debugProp;
}
#endif