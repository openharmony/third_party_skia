// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_HW_SYMBOL_CONFIG_H_
#define LIB_HW_SYMBOL_CONFIG_H_

#include <unordered_map>
#include "include/core/SkTypes.h"
#include "include/core/SkHMSymbol.h"

class SK_API HmSymbolConfig_OHOS
{
public:
    static HmSymbolConfig_OHOS* getInstance();

    std::unordered_map<uint32_t, SymbolLayersGroups>* getHmSymbolConfig();

    std::vector<AnimationInfo>* getCommonAnimationInfos();

    std::vector<AnimationInfo>* getSpecialAnimationInfos();

    SymbolLayersGroups* getSymbolLayersGroups(uint32_t glyphId);
private:
    std::unordered_map<uint32_t, SymbolLayersGroups> hmSymbolConfig_;
    std::vector<AnimationInfo> commonAnimationInfos_;
    std::vector<AnimationInfo> specialAnimationInfos_;
};

#endif