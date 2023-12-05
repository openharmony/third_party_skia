// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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