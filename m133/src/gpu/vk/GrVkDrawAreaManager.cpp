/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/gpu/vk/GrVkDrawAreaManager.h"
#include "src/gpu/ganesh/Device.h"
#include "src/gpu/ganesh/GrRenderTargetProxy.h"
#include "src/gpu/ganesh/surface/SkSurface_Ganesh.h"

void GrVkDrawAreaManager::bindDrawingArea(SkSurface* surface, const std::vector<SkIRect>& skIRects)
{
    if (!surface) {
        return;
    }

    auto gpuDevice = static_cast<SkSurface_Ganesh*>(surface)->getDevice();
    if (!gpuDevice) {
        return;
    }

    auto gpuDeviceProxy = gpuDevice->targetProxy();
    if (!gpuDeviceProxy) {
        return;
    }

    SkAutoMutexExclusive lock(fMutex);
    fRtmap[gpuDeviceProxy->peekRenderTarget()] = skIRects;
}

std::vector<SkIRect>& GrVkDrawAreaManager::getDrawingArea(GrRenderTarget* rt)
{
    SkAutoMutexExclusive lock(fMutex);
    std::map<GrRenderTarget*, std::vector<SkIRect>>::iterator iter = fRtmap.find(rt);
    if (iter != fRtmap.end()) {
        return iter->second;
    } else {
        static std::vector<SkIRect> emptyVec = {};
        return emptyVec;
    }
}

void GrVkDrawAreaManager::clearSurface(SkSurface* surface)
{
    if (!surface) {
        return;
    }

    auto gpuDevice = static_cast<SkSurface_Ganesh*>(surface)->getDevice();
    if (!gpuDevice) {
        return;
    }

    auto gpuDeviceProxy = gpuDevice->targetProxy();
    if (!gpuDeviceProxy) {
        return;
    }

    SkAutoMutexExclusive lock(fMutex);
    fRtmap.erase(gpuDeviceProxy->peekRenderTarget());
}

void GrVkDrawAreaManager::clearAll()
{
    SkAutoMutexExclusive lock(fMutex);
    fRtmap.clear();
}