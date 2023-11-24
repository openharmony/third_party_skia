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
    VK_STRUCTURE_TYPE_RENDER_PASS_DAMAGE_REGION_BEGIN_INFO_TYPE = VK_STRUCTURE_TYPE_MAX_ENUM - 7
} VkstructureTypeHUAWEI;

typedef struct VkRenderPassDamageRegionBeginInfo {
    VkStructureTypeHUAWEI       sType;
    const void*                 pNext;
    uint32_t                    regionCount;
    const VkRect2D*             regions;
} VkRenderPassDamageRegionBeginInfo;

#ifdef __cplusplus
}
#endif