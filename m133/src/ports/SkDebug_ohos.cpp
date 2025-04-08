/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * 2023.4.26 log adapt ohos.
 * Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
 */

#include "include/private/base/SkDebug.h"
#include <stdio.h>
#include <string>

#ifdef SKIA_OHOS_SHADER_REDUCE
#include <parameters.h>
#endif

#define LOG_TAG "skia"
#include "hilog/log.h"

#ifdef SKIA_OHOS_SINGLE_OWNER
#include "backtrace_local.h"
#include "include/core/SkLog.h"
#include <sstream>
#include <thread>
#include <fstream>
#endif

extern "C" {
    int HiLogPrintArgs(LogType type, LogLevel level, unsigned int domain, const char* tag, const char* fmt, va_list ap);
}

// Print debug output to stdout as well. This is useful for command line
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

#ifdef SKIA_OHOS_SINGLE_OWNER
static bool IsRenderService()
{
    std::ifstream procfile("/proc/self/cmdline");
    if (!procfile.is_open()) {
        SK_LOGE("IsRenderService open failed");
        return false;
    }
    std::string processName;
    std::getline(procfile, processName);
    procfile.close();
    static const std::string target = "/system/bin/render_service";
    bool result = processName.compare(0, target.size(), target) == 0;
    return result;
}

static bool IsBeta()
{
    static const bool isBeta = OHOS::system::GetParameter("const.logsystem.versiontype", "unknown") == "beta";
    return isBeta;
}

bool GetEnableSkiaSingleOwner()
{
    static const bool gIsEnableSingleOwner = IsRenderService() && IsBeta();
    return gIsEnableSingleOwner;
}

void PrintBackTrace(uint32_t tid)
{
    std::string msg = "";
    OHOS::HiviewDFX::GetBacktraceStringByTid(msg, tid, 0, false);
    if (!msg.empty()) {
        std::vector<std::string> out;
        std::stringstream ss(msg);
        std::string s;
        while (std::getline(ss, s, '\n')) {
            out.push_back(s);
        }
        SK_LOGE(" ======== tid:%{public}d", tid);
        for (auto const& line : out) {
            SK_LOGE(" callstack %{public}s", line.c_str());
        }
    }
}
#endif