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

#ifndef GrVkDrawAreaManager_DEFINED
#define GrVkDrawAreaManager_DEFINED

#include <map>
#include "include/core/SkSurface.h"
#include "include/private/base/SkMutex.h"
#include "src/gpu/ganesh/GrRenderTarget.h"

class SK_API GrVkDrawAreaManager {
public:
    static GrVkDrawAreaManager &getInstance() {
        static GrVkDrawAreaManager instance;
        return instance;
    }

    GrVkDrawAreaManager(const GrVkDrawAreaManager &) = delete;
    GrVkDrawAreaManager &operator = (const GrVkDrawAreaManager &) = delete;
    GrVkDrawAreaManager(GrVkDrawAreaManager &&) = delete;
    GrVkDrawAreaManager &operator = (GrVkDrawAreaManager &&) = delete;

    void bindDrawingArea(SkSurface* surface, const std::vector<SkIRect>& skIRects);

    std::vector<SkIRect>& getDrawingArea(GrRenderTarget* rt);

    void clearSurface(SkSurface* surface);

    void clearAll();

private:
    GrVkDrawAreaManager() = default;
    virtual ~GrVkDrawAreaManager() = default;

    std::map<GrRenderTarget*, std::vector<SkIRect>> fRtmap;

    SkMutex fMutex;
};
#endif