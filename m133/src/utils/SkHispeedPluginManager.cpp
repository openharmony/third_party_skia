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

#include "SkHispeedPluginManager.h"

#include <dlfcn.h>
#include <string>
static const std::string HISPEED_IMAGE_SO = "libhispeed_image.so";

SkHispeedPluginManager& SkHispeedPluginManager::GetInstance() {
    static SkHispeedPluginManager instance;
    return instance;
}

SkHispeedPluginManager::SkHispeedPluginManager() {
    hispeedImageSoHandle_ = nullptr;
    funcRGBA_to_rgbA_ = nullptr;
    funcRGBA_to_bgrA_ = nullptr;
    isHispeedImageSoOpened_ = false;
    LoadHispeedImageSo();
}

SkHispeedPluginManager::~SkHispeedPluginManager() {
    UnloadHispeedImageSo();
}

bool SkHispeedPluginManager::LoadHispeedImageSo() {
    if (!isHispeedImageSoOpened_) {
        hispeedImageSoHandle_ = dlopen(HISPEED_IMAGE_SO.c_str(), RTLD_LAZY);
        if (hispeedImageSoHandle_ == nullptr) {
            return false;
        }

        funcRGBA_to_rgbA_ = reinterpret_cast<HSDImageFunc_RGBA_to_rgbA>(
            dlsym(hispeedImageSoHandle_, "HSD_Image_RGBA_to_rgbA"));
        if (funcRGBA_to_rgbA_ == nullptr) {
            UnloadHispeedImageSo();
            return false;
        }

        funcRGBA_to_bgrA_ = reinterpret_cast<HSDImageFunc_RGBA_to_bgrA>(
            dlsym(hispeedImageSoHandle_, "HSD_Image_RGBA_to_bgrA"));
        if (funcRGBA_to_bgrA_ == nullptr) {
            UnloadHispeedImageSo();
            return false;
        }

        isHispeedImageSoOpened_ = true;
    }
    return true;
}

void SkHispeedPluginManager::UnloadHispeedImageSo() {
    if (hispeedImageSoHandle_ != nullptr) {
        dlclose(hispeedImageSoHandle_);
    }

    hispeedImageSoHandle_ = nullptr;
    funcRGBA_to_rgbA_ = nullptr;
    funcRGBA_to_bgrA_ = nullptr;

    isHispeedImageSoOpened_ = false;
}

HSDImageFunc_RGBA_to_rgbA SkHispeedPluginManager::GetFunc_RGBA_to_rgbA() {
    if (!isHispeedImageSoOpened_ || hispeedImageSoHandle_ == nullptr) {
        return nullptr;
    }
    return funcRGBA_to_rgbA_;
}

HSDImageFunc_RGBA_to_bgrA SkHispeedPluginManager::GetFunc_RGBA_to_bgrA() {
    if (!isHispeedImageSoOpened_ || hispeedImageSoHandle_ == nullptr) {
        return nullptr;
    }
    return funcRGBA_to_bgrA_;
}
