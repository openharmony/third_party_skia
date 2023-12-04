/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: Draw area manager for vulkan api partial render extended function.
 * Create: 2023/11/27
 */

#include "src/gpu/vk/GrVkDrawAreaManager.h"
#include "src/image/SkSurface_Gpu.h"
#include "src/gpu/BaseDevice.h"
#include "src/gpu/GrRenderTargetProxy.h"

void GrVkDrawAreaManager::bindDrawingArea(SkSurface* surface, const std::vector<SkIRect>& skIRects) {
    if (!surface) {
        return;
    }

    auto gpuDevice = static_cast<SkSurface_Gpu*>(surface)->getDevice();
    if (!gpuDevice) {
        return;
    }

    auto gpuDeviceProxy = gpuDevice->asGpuDevice()->targetProxy();
    if (!gpuDeviceProxy) {
        return;
    }

    mRtmap[gpuDeviceProxy->peekRenderTarget()] = skIRects;
}

std::vector<SkIRect>& GrVkDrawAreaManager::getDrawingArea(GrRenderTarget* rt) {
    std::map<GrRenderTarget*, std::vector<SkIRect>>::iterator iter = mRtmap.find(rt);
    if (iter != mRtmap.end()) {
        return iter->second;
    } else {
        static std::vector<SkIRect> emptyVec = {};
        return emptyVec;
    }
}

void GrVkDrawAreaManager::clearSurface(SkSurface* surface) {
    if (!surface) {
        return;
    }

    auto gpuDevice = static_cast<SkSurface_Gpu*>(surface)->getDevice();
    if (!gpuDevice) {
        return;
    }

    auto gpuDeviceProxy = gpuDevice->asGpuDevice()->targetProxy();
    if (!gpuDeviceProxy) {
        return;
    }

    mRtmap.erase(gpuDeviceProxy->peekRenderTarget());
}

void GrVkDrawAreaManager::clearAll() {
    mRtmap.clear();
}