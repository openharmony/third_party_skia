/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "src/plugin/hispeed_plugin.h"

#ifdef USE_HISPEED_PLUGIN

#if (defined(__aarch64__))
static const char *HISPEED_IMAGE_SO_PATH = "/system/lib64/libhispeed_image.so";
#else
static const char *HISPEED_IMAGE_SO_PATH = "/system/lib/libhispeed_image.so";
#endif
static void *hispeedImageHandle = NULL;
HSDPlugin_VP8ParseResiduals g_vp8ParseResidualsHandle = NULL;                  // HSDImage_VP8ParseResiduals
HSDPlugin_VP8ParseIntraModeRow g_vp8ParseIntraModeRowHandle = NULL;            // HSDImage_VP8ParseIntraModeRow
HSDPlugin_VFilter16i g_vFilter16iHandle = NULL;                                // HSDImage_VFilter16i
HSDPlugin_HFilter16 g_hFilter16Handle = NULL;                                  // HSDImage_HFilter16
HSDPlugin_HFilter16i g_hFilter16iHandle = NULL;                                // HSDImage_HFilter16i
HSDPlugin_HFilter8 g_hFilter8Handle = NULL;                                    // HSDImage_HFilter8
HSDPlugin_HFilter8i g_hFilter8iHandle = NULL;                                  // HSDImage_HFilter8i
HSDPlugin_UpsampleYuvToRgbaLinePair g_upsampleYuvToRgbaLinePairHandle = NULL;  // HSDImage_UpsampleYuvToRgbaLinePair

void WebPLoadHispeedPlugin()
{
    hispeedImageHandle = dlopen(HISPEED_IMAGE_SO_PATH, RTLD_LAZY);
    if (hispeedImageHandle == NULL) {
        return;
    }
    g_vp8ParseResidualsHandle = (HSDPlugin_VP8ParseResiduals)dlsym(hispeedImageHandle, "HSDImage_VP8ParseResiduals");
    g_vp8ParseIntraModeRowHandle =
        (HSDPlugin_VP8ParseIntraModeRow)dlsym(hispeedImageHandle, "HSDImage_VP8ParseIntraModeRow");
    g_vFilter16iHandle = (HSDPlugin_VFilter16i)dlsym(hispeedImageHandle, "HSDImage_VFilter16i");
    g_hFilter16Handle = (HSDPlugin_HFilter16)dlsym(hispeedImageHandle, "HSDImage_HFilter16");
    g_hFilter16iHandle = (HSDPlugin_HFilter16i)dlsym(hispeedImageHandle, "HSDImage_HFilter16i");
    g_hFilter8Handle = (HSDPlugin_HFilter8)dlsym(hispeedImageHandle, "HSDImage_HFilter8");
    g_hFilter8iHandle = (HSDPlugin_HFilter8i)dlsym(hispeedImageHandle, "HSDImage_HFilter8i");
    g_upsampleYuvToRgbaLinePairHandle = 
        (HSDPlugin_UpsampleYuvToRgbaLinePair)dlsym(hispeedImageHandle, "HSDImage_UpsampleYuvToRgbaLinePair");
}

void WebPUnloadHispeedPlugin()
{
    if (hispeedImageHandle != NULL) {
        dlclose(hispeedImageHandle);
        hispeedImageHandle = NULL;
        g_vp8ParseResidualsHandle = NULL;
        g_vp8ParseIntraModeRowHandle = NULL;
        g_vFilter16iHandle = NULL;
        g_hFilter16Handle = NULL;
        g_hFilter16iHandle = NULL;
        g_hFilter8Handle = NULL;
        g_hFilter8iHandle = NULL;
        g_upsampleYuvToRgbaLinePairHandle = NULL;
    }
}

#endif