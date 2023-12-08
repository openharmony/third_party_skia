/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "include/private/SkOnce.h"
#include "HmSymbolConfig_ohos.h"

HmSymbolConfig_OHOS* HmSymbolConfig_OHOS::getInstance()
{
    static SkOnce once;
    static HmSymbolConfig_OHOS* singleton;

    once([] {
        singleton = new HmSymbolConfig_OHOS();
    });
    return singleton;
}

std::unordered_map<uint32_t, SymbolLayersGroups>* HmSymbolConfig_OHOS::getHmSymbolConfig()
{
    return &hmSymbolConfig_;
}

std::vector<AnimationInfo>* HmSymbolConfig_OHOS::getCommonAnimationInfos()
{
    return &commonAnimationInfos_;
}

std::vector<AnimationInfo>* HmSymbolConfig_OHOS::getSpecialAnimationInfos()
{
    return &specialAnimationInfos_;
}

SymbolLayersGroups* HmSymbolConfig_OHOS::getSymbolLayersGroups(uint32_t glyphId)
{
    if (hmSymbolConfig_.size() == 0) {
        return nullptr;
    }
    std::unordered_map<uint32_t, SymbolLayersGroups>::iterator iter = hmSymbolConfig_.find(glyphId);
    if (iter != hmSymbolConfig_.end()) {
        return &(iter->second);
    }
    return nullptr;
}