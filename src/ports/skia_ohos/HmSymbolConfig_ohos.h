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

#ifndef LIB_HW_SYMBOL_CONFIG_H_
#define LIB_HW_SYMBOL_CONFIG_H_

#include <unordered_map>
#include <mutex>
#include "include/core/SkTypes.h"
#include "include/core/HMSymbol.h"

class SK_API HmSymbolConfig_OHOS
{
public:
    static HmSymbolConfig_OHOS* getInstance();

    std::unordered_map<uint32_t, SymbolLayersGroups>* getHmSymbolConfig();

    std::vector<AnimationInfo>* getCommonAnimationInfos();

    std::vector<AnimationInfo>* getSpecialAnimationInfos();

    SymbolLayersGroups* getSymbolLayersGroups(uint32_t glyphId);

    bool getHmSymbolEnable();

    bool getInit() const
    {
        return isInit_;
    }

    void setInit(const bool init)
    {
        isInit_ = init;
    }

    void lock()
    {
        hmSymbolMut_.lock();
    }

    void unlock()
    {
        hmSymbolMut_.unlock();
    }

    void clear()
    {
        hmSymbolConfig_.clear();
        commonAnimationInfos_.clear();
        specialAnimationInfos_.clear();
        isInit_ = false;
    }
private:
    std::unordered_map<uint32_t, SymbolLayersGroups> hmSymbolConfig_;
    std::vector<AnimationInfo> commonAnimationInfos_;
    std::vector<AnimationInfo> specialAnimationInfos_;
    std::mutex hmSymbolMut_;
    bool isInit_ = false;
};

#endif