/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: vulkan api partial render extended function. Will delete when vulkan-headers heuristic
 * Create: 2023/11/27
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

const uint32_t VK_DEVICE_CREATE_HTS_ENABLE_BIT = 0x80000000; // for device using hts
typedef enum VkStructureTypeHUAWEI {
    VK_STRUCTURE_TYPE_BLUR_COLOR_FILTER_INFO_HUAWEI = VK_STRUCTURE_TYPE_MAX_ENUM - 15,
    VK_STRUCTURE_TYPE_BLUR_NOISE_INFO_HUAWEI = VK_STRUCTURE_TYPE_MAX_ENUM - 14,
    VK_STRUCTURE_TYPE_DRAW_BLUR_IMAGE_INFO_HUAWEI = VK_STRUCTURE_TYPE_MAX_ENUM - 13,
    /* for hts/ffts semaphore */
    VK_STRUCTURE_TYPE_SEMAPHORE_EXT_CREATE_INFO_HUAWEI = VK_STRUCTURE_TYPE_MAX_ENUM - 11,
    VK_STRUCTURE_TYPE_RENDER_PASS_DAMAGE_REGION_BEGIN_INFO_TYPE = VK_STRUCTURE_TYPE_MAX_ENUM - 7
} VkstructureTypeHUAWEI;

typedef struct VkRenderPassDamageRegionBeginInfo {
    VkStructureTypeHUAWEI       sType;
    const void*                 pNext;
    uint32_t                    regionCount;
    const VkRect2D*             regions;
} VkRenderPassDamageRegionBeginInfo;

#define VK_HUAWEI_DRAW_BLUR_IMAGE 1
#define VK_HUAWEI_DRAW_BLUR_IMAGE_SPEC_VERSION 10
#define VK_HUAWEI_DRAW_BLUR_IMAGE_EXTENSION_NAME "VK_HUAWEI_draw_blur_image"
#define VK_HUAWEI_GPU_ERROR_RECOVER (-199999999)

typedef enum VkBlurOriginTypeHUAWEI {
    BLUR_ORIGIN_NONE_FLIP_HUAWEI = 0,
    BLUR_ORIGIN_Y_AXIS_FLIP_HUAWEI = 1,
} VkBlurOriginTypeHUAWEI;

typedef struct VkDrawBlurImageInfoHUAWEI {
    VkStructureTypeHUAWEI             sType;
    const void*                       pNext;
    float                             sigma;
    VkBlurOriginTypeHUAWEI            origin;
    VkRect2D                          srcRegion;
    VkRect2D                          dstRegion;
    VkImageView                       srcImageView;
} VkDrawBlurImageInfoHUAWEI;

typedef struct VkBlurNoiseInfoHUAWEI {
    VkStructureTypeHUAWEI             sType;
    const void*                       pNext;
    float                             noiseRatio;
} VkBlurNoiseInfoHUAWEI;

typedef struct VkBlurColorFilterInfoHUAWEI {
    VkStructureTypeHUAWEI             sType;
    const void*                       pNext;
    float                             saturation;
    float                             brightness;
} VkBlurColorFilterInfoHUAWEI;

typedef void (VKAPI_PTR *PFN_vkCmdDrawBlurImageHUAWEI)(VkCommandBuffer commandBuffer,\
    const VkDrawBlurImageInfoHUAWEI* drawBlurImageInfo);

typedef VkResult (VKAPI_PTR *PFN_vkGetBlurImageSizeHUAWEI)(VkDevice device, \
    const VkDrawBlurImageInfoHUAWEI *drawBlurImageInfo, VkRect2D *pSize);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR void VKAPI_CALL vkCmdDrawBlurImageHUAWEI(
    VkCommandBuffer                   commandBuffer,
    const VkDrawBlurImageInfoHUAWEI*  drawBlurImageInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkGetBlurImageSizeHUAWEI(
    VkDevice                          device,
    const VkDrawBlurImageInfoHUAWEI*  drawBlurImageInfo,
    VkRect2D*                         pSize);
#endif

#ifdef SUPPORT_OPAQUE_OPTIMIZATION
typedef void (VKAPI_PTR* PFN_vkCmdSetOpaqueRegionHUAWEI)(VkCommandBuffer commandBuffer,\
    uint32_t scissorCount, VkRect2D* pScissors);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR void VKAPI_CALL vkCmdSetOpaqueRegionHUAWEI(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    scissorCount,
    VkRect2D                                    *pScissors);
#endif
#endif

#ifdef __cplusplus
}
#endif