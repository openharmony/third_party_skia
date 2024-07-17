/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: vulkan api partial render extended function. Will delete when vulkan-headers heuristic
 * Create: 2023/11/27
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum VkStructureTypeHUAWEI {
    VK_STRUCTURE_TYPE_BLUR_COLOR_FILTER_INFO_HUAWEI = VK_STRUCTURE_TYPE_MAX_ENUM - 15,
    VK_STRUCTURE_TYPE_BLUR_NOISE_INFO_HUAWEI = VK_STRUCTURE_TYPE_MAX_ENUM - 14,
    VK_STRUCTURE_TYPE_DRAW_BLUR_IMAGE_INFO_HUAWEI = VK_STRUCTURE_TYPE_MAX_ENUM - 13,
    VK_STRUCTURE_TYPE_RENDER_PASS_DAMAGE_REGION_BEGIN_INFO_TYPE = VK_STRUCTURE_TYPE_MAX_ENUM - 7
} VkstructureTypeHUAWEI;

typedef struct VkRenderPassDamageRegionBeginInfo {
    VkStructureTypeHUAWEI       sType;
    const void*                 pNext;
    uint32_t                    regionCount;
    const VkRect2D*             regions;
} VkRenderPassDamageRegionBeginInfo;

#define VK_HUAWEI_draw_blur_image 1
#define VK_HUAWEI_DRAW_BLUR_IMAGE_SPEC_VERSION 10
#define VK_HUAWEI_DRAW_BLUR_IMAGE_EXTENSION_NAME "VK_HUAWEI_draw_blur_image"

typedef struct VkDrawBlurImageInfoHUAWEI {
    VkStructureTypeHUAWEI             sType;
    const void*                       pNext;
    float                             sigma;
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

typedef void (VKAPI_PTR *PFN_vkCmdDrawBlurImageHUAWEI)(VkCommandBuffer commandBuffer, const VkDrawBlurImageInfoHUAWEI *drawBlurImageInfo);

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

#ifdef __cplusplus
}
#endif