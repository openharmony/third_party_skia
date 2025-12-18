/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef SK_HISPEED_PLUGIN_MANAGER_H
#define SK_HISPEED_PLUGIN_MANAGER_H

#include <cstdint>

typedef void (*HSDImageFunc_RGBA_to_rgbA)(
    uint32_t* dst, const uint32_t* src, int count);
typedef void (*HSDImageFunc_RGBA_to_bgrA)(
    uint32_t* dst, const uint32_t* src, int count);

class SkHispeedPluginManager {
public:
    static SkHispeedPluginManager& GetInstance();
    HSDImageFunc_RGBA_to_rgbA GetFunc_RGBA_to_rgbA();
    HSDImageFunc_RGBA_to_bgrA GetFunc_RGBA_to_bgrA();
    
private:
    SkHispeedPluginManager();
    ~SkHispeedPluginManager();
    SkHispeedPluginManager(const SkHispeedPluginManager&) = delete;
    SkHispeedPluginManager& operator=(const SkHispeedPluginManager&) = delete;
    
    bool LoadHispeedImageSo();
    void UnloadHispeedImageSo();
    
    bool isHispeedImageSoOpened_;
    void *hispeedImageSoHandle_;
    
    HSDImageFunc_RGBA_to_rgbA funcRGBA_to_rgbA_;
    HSDImageFunc_RGBA_to_bgrA funcRGBA_to_bgrA_;
};

#endif