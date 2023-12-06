/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: Draw area manager for vulkan api partial render extended function.
 * Create: 2023/11/27
 */

#ifndef GrVkDrawAreaManager_DEFINED
#define GrVkDrawAreaManager_DEFINED

#include <map>
#include "include/core/SkSurface.h"

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

    std::map<GrRenderTarget*, std::vector<SkIRect>> mRtmap;
};
#endif