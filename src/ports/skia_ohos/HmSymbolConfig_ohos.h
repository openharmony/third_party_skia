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
#include <json/json.h>

#include "SkStream.h"
#include "SkString.h"
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

    std::vector<std::vector<PiecewiseParameter>>* getGroupParameters(AnimationType type,
        AnimationSubType subType, int animationMode);

    int parseConfigOfHmSymbol(const char* fname, SkString fontDir);

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

    const uint32_t defaultColorHexLen = 9;
    const uint32_t defaultColorStrLen = 7;
    const uint32_t hexFlag = 16;
    const uint32_t twoBytesBitsLen = 16;
    const uint32_t oneByteBitsLen = 8;

    AnimationInfo* getAnimationInfo(AnimationType type);

    int checkConfigFile(const char* fname, Json::Value& root);

    void parseSymbolAnimations(const Json::Value& root, const char* key);
    void parseSymbolAnimations(const Json::Value& root, std::vector<AnimationInfo>* animationInfos);
    void parseSymbolAnimationParas(const Json::Value& root, std::vector<AnimationPara>& animationParas);
    void parseSymbolAnimationPara(const Json::Value& root, AnimationPara& animationPara);
    void parseSymbolGroupParas(const Json::Value& root, std::vector<std::vector<PiecewiseParameter>>& groupParameters);
    void parseSymbolPiecewisePara(const Json::Value& root, PiecewiseParameter& piecewiseParameter);
    void parseSymbolAnimationSubType(const char* subTypeStr, AnimationSubType& subType);
    void parseSymbolCurveArgs(const Json::Value& root, std::map<std::string, double_t>& curveArgs);
    void parseSymbolProperties(const Json::Value& root, std::map<std::string, std::vector<double_t>>& properties);

    void parseSymbolLayersGrouping(const Json::Value& root);
    void parseOneSymbol(const Json::Value& root, std::unordered_map<uint32_t, SymbolLayersGroups>* hmSymbolConfig);
    void parseLayers(const Json::Value& root, std::vector<std::vector<size_t>>& layers);
    void parseRenderModes(const Json::Value& root, std::map<SymbolRenderingStrategy,
        std::vector<RenderGroup>>& renderModesGroups);
    void parseComponets(const Json::Value& root, std::vector<size_t>& components);
    void parseRenderGroups(const Json::Value& root, std::vector<RenderGroup>& renderGroups);
    void parseGroupIndexes(const Json::Value& root, std::vector<GroupInfo>& groupInfos);
    void parseLayerOrMaskIndexes(const Json::Value& root, std::vector<size_t>& indexes);
    void parseDefaultColor(const char* defaultColorStr, RenderGroup& renderGroup);
    void parseAnimationSettings(const Json::Value& root, std::vector<AnimationSetting>& animationSettings);
    void parseAnimationSetting(const Json::Value& root, AnimationSetting& animationSetting);
    void parseAnimationType(const char* animationTypeStr, AnimationType& animationType);
    void parseGroupSettings(const Json::Value& root, std::vector<GroupSetting>& groupSettings);
    void parseGroupSetting(const Json::Value& root, GroupSetting& groupSetting);

    void parseOneSymbolNativeCase(const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups,
        uint32_t& nativeGlyphId);
    void parseOneSymbolLayerCase(const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups);
    void parseOneSymbolRenderCase(const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups);
    void parseOneSymbolAnimateCase(const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups);
};

#endif