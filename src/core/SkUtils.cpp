/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkUtils.h"
#ifdef NOT_BUILD_FOR_OHOS_SDK
#include <parameters.h>
#endif

const char SkHexadecimalDigits::gUpper[16] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
const char SkHexadecimalDigits::gLower[16] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

// vma cache
static thread_local bool g_vmaCacheFlag = false;

bool SkGetMemoryOptimizedFlag()
{
    // global flag for vma cache
#ifdef NOT_BUILD_FOR_OHOS_SDK
    static bool g_memoryOptimizeFlag = OHOS::system::GetBoolParameter("sys.graphic.vma.opt", false);
#else
    static bool g_memoryOptimizeFlag = false;
#endif
    return g_memoryOptimizeFlag;
}

bool SkGetVmaCacheFlag()
{
    if (!SkGetMemoryOptimizedFlag()) {
        return false;
    }
    return g_vmaCacheFlag;
}

void SkSetVmaCacheFlag(bool flag)
{
    g_vmaCacheFlag = flag;
}

#ifdef NOT_BUILD_FOR_OHOS_SDK
int GetIntParamWithDefault(int paramValue, int maxValue, int defaultValue)
{
    if (paramValue <= 0 || paramValue > maxValue) {
        paramValue = defaultValue; // default value
    }
    return paramValue;
}

bool GetBoolParamWithFlag(bool paramValue)
{
    if (!SkGetMemoryOptimizedFlag()) {
        return false;
    }
    return paramValue;
}
#endif

int SkGetVmaBlockSizeMB()
{
#ifdef NOT_BUILD_FOR_OHOS_SDK
    constexpr int MAX_VMA_BLOCK_SIZE = 256;
    constexpr int DEFAULT_VMA_BLOCK_SIZE = 64;
    static int g_vmaBlockSize = GetIntParamWithDefault(
        std::atoi(OHOS::system::GetParameter("sys.graphic.vma.blockSize", "64").c_str()),
        MAX_VMA_BLOCK_SIZE, DEFAULT_VMA_BLOCK_SIZE);
#else
    static int g_vmaBlockSize = 4; // default value
#endif
    return g_vmaBlockSize;
}

int SkGetNeedCachedMemroySize()
{
#ifdef NOT_BUILD_FOR_OHOS_SDK
    constexpr int MAX_VMA_CACHE_MEMORY_SIZE = 512 * 1024 * 1024;
    constexpr int DEFAULT_VMA_CACHE_MEMORY_SIZE = 9000000;
    static int g_vmaCacheMemorySize = GetIntParamWithDefault(
        std::atoi(OHOS::system::GetParameter("sys.graphic.vma.minCachedSize", "9000000").c_str()),
        MAX_VMA_CACHE_MEMORY_SIZE, DEFAULT_VMA_CACHE_MEMORY_SIZE);
#else
    static int g_vmaCacheMemorySize = 0; // default value
#endif
    return g_vmaCacheMemorySize;
}

bool SkGetVmaDefragmentOn()
{
#ifdef NOT_BUILD_FOR_OHOS_SDK
    static bool g_vmaDefragmentFlag =
        GetBoolParamWithFlag(OHOS::system::GetBoolParameter("sys.graphic.vma.defragment", true));
    return g_vmaDefragmentFlag;
#else
    return false;
#endif
}

size_t SkGetVmaBlockCountMax()
{
#ifdef NOT_BUILD_FOR_OHOS_SDK
    constexpr int MAX_VMA_BLOCK_COUNT_MAX = 4096;
    constexpr int DEFAULT_VMA_BLOCK_COUNT_MAX = 10;
    static int g_vmaBlockCountMax = GetIntParamWithDefault(
        std::atoi(OHOS::system::GetParameter("sys.graphic.vma.maxBlockCount", "10").c_str()),
        MAX_VMA_BLOCK_COUNT_MAX, DEFAULT_VMA_BLOCK_COUNT_MAX);
    return g_vmaBlockCountMax;
#else
    return SIZE_MAX; // default value
#endif
}

bool SkGetVmaDebugFlag()
{
#ifdef NOT_BUILD_FOR_OHOS_SDK
    static bool g_vmaDebugFlag =
        GetBoolParamWithFlag(std::atoi(OHOS::system::GetParameter("sys.graphic.vma.debug", "0").c_str()) != 0);
    return g_vmaDebugFlag;
#else
    return false;
#endif
}