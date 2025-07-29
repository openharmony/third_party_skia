/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef HISPEED_PLUGIN_H
#define HISPEED_PLUGIN_H

#ifdef USE_HISPEED_PLUGIN

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/dec/vp8i_dec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*HSDPlugin_VP8ParseResiduals)(const VP8BandProbas *(*const)[16 + 1], VP8MBData *const,
    const VP8QuantMatrix *const, int16_t *, VP8MB *const, VP8MB *const, VP8BitReader *const);
typedef int (*HSDPlugin_VP8ParseIntraModeRow)(
    VP8BitReader *const, uint8_t *, uint8_t *const, VP8MBData *, VP8Proba *, const uint8_t, const int, const int, int);
typedef void (*HSDPlugin_VFilter16i)(uint8_t *, int, int, int, int);
typedef void (*HSDPlugin_HFilter16)(uint8_t *, int, int, int, int);
typedef void (*HSDPlugin_HFilter16i)(uint8_t *, int, int, int, int);
typedef void (*HSDPlugin_HFilter8)(uint8_t *, uint8_t *, int, int, int, int);
typedef void (*HSDPlugin_HFilter8i)(uint8_t *, uint8_t *, int, int, int, int);
typedef void (*HSDPlugin_UpsampleYuvToRgbaLinePair)(const uint8_t *, const uint8_t *, const uint8_t *, const uint8_t *,
    const uint8_t *, const uint8_t *, uint8_t *, uint8_t *, int);

extern HSDPlugin_VP8ParseResiduals g_vp8ParseResidualsHandle;
extern HSDPlugin_VP8ParseIntraModeRow g_vp8ParseIntraModeRowHandle;
extern HSDPlugin_VFilter16i g_vFilter16iHandle;
extern HSDPlugin_HFilter16 g_hFilter16Handle;
extern HSDPlugin_HFilter16i g_hFilter16iHandle;
extern HSDPlugin_HFilter8 g_hFilter8Handle;
extern HSDPlugin_HFilter8i g_hFilter8iHandle;
extern HSDPlugin_UpsampleYuvToRgbaLinePair g_upsampleYuvToRgbaLinePairHandle;

void WebPLoadHispeedPlugin();
void WebPUnloadHispeedPlugin();

#ifdef __cplusplus
}
#endif

#endif
#endif /* HISPEED_PLUGIN_H */