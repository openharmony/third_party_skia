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
#include <functional>
#include "securec.h"

#ifdef SK_BUILD_FONT_MGR_FOR_OHOS
#include <parameters.h>
#endif

#ifdef ENABLE_DEBUG

#define LOGE(fmt, args...)        \
    printf("E %s:%d  %s - " fmt, basename(__FILE__), __LINE__, __FUNCTION__, ##args)
#define LOGI(fmt, args...)        \
    printf("I %s:%d - " fmt,  __FUNCTION__, __LINE__, ##args)
#define LOGW(fmt, args...)        \
    printf("W %s:%d  %s - " fmt, basename(__FILE__), __LINE__, __FUNCTION__, ##args)

#else

#define LOGE        SkDEBUGF
#define LOGI        SkDEBUGF
#define LOGW        SkDEBUGF

#endif

using PiecewiseParaKeyFunc = std::function<void(const char*, const Json::Value&, PiecewiseParameter&)>;
using SymbolKeyFunc = std::function<void(const char*, const Json::Value&, SymbolLayersGroups&)>;
using SymnolAniFunc = std::function<void(const char*, const Json::Value&, AnimationPara&)>;

const char SPECIAL_ANIMATIONS[] = "special_animations";
const char COMMON_ANIMATIONS[] = "common_animations";
const char SYMBOL_LAYERS_GROUPING[] = "symbol_layers_grouping";
const char ANIMATION_TYPE[] = "animation_type";
const char ANIMATION_PARAMETERS[] = "animation_parameters";
const char ANIMATION_MODE[] = "animation_mode";
const char SUB_TYPE[] = "sub_type";
const char GROUP_PARAMETERS[] = "group_parameters";
const char CURVE[] = "curve";
const char CURVE_ARGS[] = "curve_args";
const char DURATION[] = "duration";
const char DELAY[] = "delay";
const char NATIVE_GLYPH_ID[] = "native_glyph_id";
const char SYMBOL_GLYPH_ID[] = "symbol_glyph_id";
const char LAYERS[] = "layers";
const char RENDER_MODES[] = "render_modes";
const char MODE[] = "mode";
const char RENDER_GROUPS[] = "render_groups";
const char GROUP_INDEXES[] = "group_indexes";
const char DEFAULT_COLOR[] = "default_color";
const char FIX_ALPHA[] = "fix_alpha";
const char LAYER_INDEXES[] = "layer_indexes";
const char MASK_INDEXES[] = "mask_indexes";
const char GROUP_SETTINGS[] = "group_settings";
const char ANIMATION_INDEX[] = "animation_index";

const int NO_ERROR = 0; // no error
const int ERROR_CONFIG_NOT_FOUND = 1; // the configuration document is not found
const int ERROR_CONFIG_FORMAT_NOT_SUPPORTED = 2; // the formation of configuration is not supported
const int ERROR_CONFIG_INVALID_VALUE_TYPE = 4; // invalid value type in the configuration
const int ERROR_TYPE_COUNT = 11;

/*! To get the display text of an error
 * \param err the id of an error
 * \return The text to explain the error
 */
const char* errInfoToString(int err)
{
    const static std::array<const char*, ERROR_TYPE_COUNT> errInfoToString{
        "successful",                                                      // NO_ERROR = 0
        "config file is not found",                              // ERROR_CONFIG_NOT_FOUND
        "the format of config file is not supported", // ERROR_CONFIG_FORMAT_NOT_SUPPORTED
        "missing tag",                                         // ERROR_CONFIG_MISSING_TAG
        "invalid value type",                           // ERROR_CONFIG_INVALID_VALUE_TYPE
        "font file is not exist",                                  // ERROR_FONT_NOT_EXIST
        "invalid font stream",                                // ERROR_FONT_INVALID_STREAM
        "no font stream",                                          // ERROR_FONT_NO_STREAM
        "family is not found",                                   // ERROR_FAMILY_NOT_FOUND
        "no available family in the system",                   //ERROR_NO_AVAILABLE_FAMILY
        "no such directory"                                         // ERROR_DIR_NOT_FOUND
    };
    if (err >= 0 && err < ERROR_TYPE_COUNT) {
        return errInfoToString[err];
    }
    return "unknown error";
}

/*! To log the error information
 * \param err the id of an error
 * \param key the key which indicates the the part with the error
 * \param expected the expected type of json node.
 * \n     It's used only for err 'ERROR_CONFIG_INVALID_VALUE_TYPE'
 * \param actual the actual type of json node.
 * \n     It's used only for err 'ERROR_CONFIG_INVALID_VALUE_TYPE'
 * \return err
 */
int logErrInfo(int err, const char* key, Json::ValueType expected = Json::nullValue,
    Json::ValueType actual = Json::nullValue)
{
    if (err != ERROR_CONFIG_INVALID_VALUE_TYPE) {
        LOGE("%s : %s\n", errInfoToString(err), key);
    } else {
        const char* types[] = {
            "null",
            "int",
            "unit",
            "real",
            "string",
            "boolean",
            "array",
            "object",
        };
        int size = sizeof(types) / sizeof(char*);
        if ((expected >= 0 && expected < size) &&
            (actual >= 0 && actual < size)) {
            LOGE("%s : '%s' should be '%s', but here it's '%s'\n",
                errInfoToString(err), key, types[expected], types[actual]);
        } else {
            LOGE("%s : %s\n", errInfoToString(err), key);
        }
    }
    return err;
}

/*! To get the data of font configuration file
 * \param fname the full name of the font configuration file
 * \param[out] size the size of data returned to the caller
 * \return The pointer of content of the file
 * \note The returned pointer should be freed by the caller
 */
char* getDataFromFile(const char* fname, int& size)
{
    FILE* fp = fopen(fname, "r");
    if (fp == nullptr) {
        return nullptr;
    }
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp) + 1;
    rewind(fp);
    void* data = malloc(size);
    if (data == nullptr) {
        fclose(fp);
        return nullptr;
    }
    memset_s(data, size, 0, size);
    (void) fread(data, size, 1, fp);
    fclose(fp);
    return (char*)data;
}

bool HmSymbolConfig_OHOS::getHmSymbolEnable()
{
#ifdef SK_BUILD_FONT_MGR_FOR_OHOS
    static bool isHMSymbolEnable =
        (std::atoi(OHOS::system::GetParameter("persist.sys.graphic.hmsymbolcfg.enable", "1").c_str()) != 0);
    return isHMSymbolEnable;
#else
    return true;
#endif
}

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

std::vector<std::vector<PiecewiseParameter>>* HmSymbolConfig_OHOS::getGroupParameters(AnimationType type,
    AnimationSubType subType, int animationMode)
{
    auto info = getAnimationInfo(type);
    if (info == nullptr) {
        return nullptr;
    }
    for (auto &para : info->animationParas) {
        if (para.subType == subType && para.animationMode == animationMode) {
            return &para.groupParameters;
        }
    }
    return nullptr;
}

AnimationInfo* HmSymbolConfig_OHOS::getAnimationInfo(AnimationType type)
{
    for (auto &info : commonAnimationInfos_) {
        if (info.animationType == type) {
            return &info;
        }
    }
    for (auto &info : specialAnimationInfos_) {
        if (info.animationType == type) {
            return &info;
        }
    }
    return nullptr;
}

/*! check the system font configuration document
 * \param fname the full name of the font configuration document
 * \return NO_ERROR successful
 * \return ERROR_CONFIG_NOT_FOUND config document is not found
 * \return ERROR_CONFIG_FORMAT_NOT_SUPPORTED config document format is not supported
 */
int HmSymbolConfig_OHOS::checkConfigFile(const char* fname, Json::Value& root)
{
    int size = 0;
    char* data = getDataFromFile(fname, size);
    if (data == nullptr) {
        return logErrInfo(ERROR_CONFIG_NOT_FOUND, fname);
    }
    JSONCPP_STRING errs;
    Json::CharReaderBuilder charReaderBuilder;
    std::unique_ptr<Json::CharReader> jsonReader(charReaderBuilder.newCharReader());
    bool isJson = jsonReader->parse(data, data + size, &root, &errs);
    free((void*)data);
    data = nullptr;

    if (!isJson || !errs.empty()) {
        return logErrInfo(ERROR_CONFIG_FORMAT_NOT_SUPPORTED, fname);
    }
    return NO_ERROR;
}

int HmSymbolConfig_OHOS::parseConfigOfHmSymbol(const char* fname, SkString fontDir)
{
    std::lock_guard<std::mutex> lock(hmSymbolMut_);
    if (getInit()) {
        return NO_ERROR;
    }
    clear();
    const int fontEndSize = 2;
    int len = strlen(fname) + (fontDir.size() + fontEndSize);
    char fullname[len];
    if (memset_s(fullname, len, 0, len) != 0 ||
        strcpy_s(fullname, len, fontDir.c_str()) != 0) {
        return ERROR_CONFIG_NOT_FOUND;
    }
#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN)
    if (fontDir[fontDir.size() - 1] != '\\') {
        strcat_s(fullname, len, "\\");
    }
#else
    if (fontDir[fontDir.size() - 1] != '/') {
        strcat_s(fullname, len, "/");
    }
#endif
    if (strcat_s(fullname, len, fname) != 0) {
        return ERROR_CONFIG_NOT_FOUND;
    }

    Json::Value root;
    int err = checkConfigFile(fullname, root);
    if (err != NO_ERROR) {
        return err;
    }

    const char* key = nullptr;
    std::string tags[] = {COMMON_ANIMATIONS, SPECIAL_ANIMATIONS, SYMBOL_LAYERS_GROUPING};
    for (unsigned int i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        key = tags[i].c_str();
        if (!root.isMember(key) || !root[key].isArray()) {
            continue;
        }

        if (!strcmp(key, COMMON_ANIMATIONS) || !strcmp(key, SPECIAL_ANIMATIONS)) {
            parseSymbolAnimations(root[key], key);
        } else {
            parseSymbolLayersGrouping(root[key]);
        }
    }
    root.clear();
    setInit(true);
    return NO_ERROR;
}

void HmSymbolConfig_OHOS::parseSymbolAnimations(const Json::Value& root, const char* key)
{
    if (!strcmp(key, COMMON_ANIMATIONS)) {
        parseSymbolAnimations(root, getCommonAnimationInfos());
    } else {
        parseSymbolAnimations(root, getSpecialAnimationInfos());
    }
}

void HmSymbolConfig_OHOS::parseSymbolAnimations(const Json::Value& root, std::vector<AnimationInfo>* animationInfos)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("animation is not object!");
            continue;
        }

        if (!root[i].isMember(ANIMATION_TYPE) || !root[i].isMember(ANIMATION_PARAMETERS)) {
            continue;
        }
        AnimationInfo animationInfo;
        if (!root[i][ANIMATION_TYPE].isString()) {
            SkDebugf("animation_type is not string!");
            continue;
        }
        const char* animationType = root[i][ANIMATION_TYPE].asCString();
        parseAnimationType(animationType, animationInfo.animationType);

        if (!root[i][ANIMATION_PARAMETERS].isArray()) {
            SkDebugf("animation_parameters is not array!");
            continue;
        }
        parseSymbolAnimationParas(root[i][ANIMATION_PARAMETERS], animationInfo.animationParas);
        animationInfos->push_back(animationInfo);
    }
}

void HmSymbolConfig_OHOS::parseSymbolAnimationParas(const Json::Value& root, std::vector<AnimationPara>& animationParas)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("animation_parameter is not object!");
            continue;
        }
        AnimationPara animationPara;
        parseSymbolAnimationPara(root[i], animationPara);
        animationParas.push_back(animationPara);
    }
}

void HmSymbolConfig_OHOS::parseSymbolAnimationPara(const Json::Value& root, AnimationPara& animationPara)
{
    const char* key = nullptr;
    std::string tags[] = {ANIMATION_MODE, SUB_TYPE, GROUP_PARAMETERS};
    using SymnolAniFuncMap = std::unordered_map<std::string, SymnolAniFunc>;
    SymnolAniFuncMap funcMap = {
        {ANIMATION_MODE, [](const char* key, const Json::Value& root, AnimationPara& animationPara)
            {
                if (!root[key].isInt()) {
                    SkDebugf("animation_mode is not int!");
                    return;
                }
                animationPara.animationMode = root[key].asInt();
            }
        },
        {SUB_TYPE, [this](const char* key, const Json::Value& root, AnimationPara& animationPara)
            {
                if (!root[key].isString()) {
                    SkDebugf("sub_type is not string!");
                    return;
                }
                const char* subTypeStr = root[key].asCString();
                parseSymbolAnimationSubType(subTypeStr, animationPara.subType);
            }
        },
        {GROUP_PARAMETERS, [this](const char* key, const Json::Value& root, AnimationPara& animationPara)
            {
                if (!root[key].isArray()) {
                    SkDebugf("group_parameters is not array!");
                    return;
                }
                parseSymbolGroupParas(root[key], animationPara.groupParameters);
            }
        }
    };
    for (unsigned int i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        key = tags[i].c_str();
        if (!root.isMember(key)) {
            continue;
        }
        if (funcMap.count(key) > 0) {
            funcMap[key](key, root, animationPara);
        }
    }
}

void HmSymbolConfig_OHOS::parseSymbolAnimationSubType(const char* subTypeStr, AnimationSubType& subType)
{
    using SymbolAniSubFuncMap = std::unordered_map<std::string, std::function<void(AnimationSubType&)>>;
    SymbolAniSubFuncMap funcMap = {
        {
            "unit",
            [](AnimationSubType& subType) {
                subType = AnimationSubType::UNIT;
            }
        },
        {
            "variable_3_group",
            [](AnimationSubType& subType) {
                subType = AnimationSubType::VARIABLE_3_GROUP;
            }
        },
        {
            "variable_4_group",
            [](AnimationSubType& subType) {
                subType = AnimationSubType::VARIABLE_4_GROUP;
            }
        }
    };
    if (funcMap.count(subTypeStr) > 0) {
        funcMap[subTypeStr](subType);
        return;
    }
    SkDebugf("%{public}s is invalid value!", subTypeStr);
}

void HmSymbolConfig_OHOS::parseSymbolGroupParas(const Json::Value& root,
    std::vector<std::vector<PiecewiseParameter>>& groupParameters)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isArray()) {
            SkDebugf("group_parameter is not array!");
            continue;
        }
        std::vector<PiecewiseParameter> piecewiseParameters;
        for (unsigned int j = 0; j < root[i].size(); j++) {
            if (!root[i][j].isObject()) {
                SkDebugf("piecewise_parameter is not object!");
                continue;
            }
            PiecewiseParameter piecewiseParameter;
            parseSymbolPiecewisePara(root[i][j], piecewiseParameter);
            piecewiseParameters.push_back(piecewiseParameter);
        }

        groupParameters.push_back(piecewiseParameters);
    }
}


static void PiecewiseParaCurveCase(const char* key, const Json::Value& root, PiecewiseParameter& piecewiseParameter)
{
    if (!root[key].isString()) {
        SkDebugf("curve is not string!");
        return;
    }
    if (!strcmp(root[key].asCString(), "spring")) {
        piecewiseParameter.curveType = CurveType::SPRING;
        return;
    } 
    if (!strcmp(root[key].asCString(), "linear")) {
        piecewiseParameter.curveType = CurveType::LINEAR;
        return;
    }
    SkDebugf("curve is invalid value!");
}

static void PiecewiseParaDurationCase(const char* key, const Json::Value& root,
    PiecewiseParameter& piecewiseParameter)
{
    if (!root[key].isInt()) {
        SkDebugf("duration is not int!");
        return;
    }
    piecewiseParameter.duration = root[key].asInt();
}

static void PiecewiseParaDelayCase(const char* key, const Json::Value& root, PiecewiseParameter& piecewiseParameter)
{
    if (!root[key].isInt()) {
        SkDebugf("delay is not int!");
        return;
    }
    piecewiseParameter.delay = root[key].asInt();
}

void HmSymbolConfig_OHOS::parseSymbolPiecewisePara(const Json::Value& root, PiecewiseParameter& piecewiseParameter)
{
    const char* key = nullptr;
    std::string tags[] = {CURVE, CURVE_ARGS, DURATION, DELAY, "properties"};
    using PiecewiseFuncMap = std::unordered_map<std::string, PiecewiseParaKeyFunc>;
    PiecewiseFuncMap funcMap = {
        {CURVE, PiecewiseParaCurveCase},
        {CURVE_ARGS, [this](const char* key, const Json::Value& root, PiecewiseParameter& piecewiseParameter)
            {
                if (!root[key].isObject()) {
                    SkDebugf("curve_args is not object!");
                    return;
                }
                parseSymbolCurveArgs(root[key], piecewiseParameter.curveArgs);
            }
        },
        {DURATION, PiecewiseParaDurationCase},
        {DELAY, PiecewiseParaDelayCase},
        {"properties", [this](const char* key, const Json::Value& root, PiecewiseParameter& piecewiseParameter)
            {
                if (!root[key].isObject()) {
                    SkDebugf("properties is not object!");
                    return;
                }
                parseSymbolProperties(root[key], piecewiseParameter.properties);
            }
        }
    };

    for (unsigned int i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        key = tags[i].c_str();
        if (!root.isMember(key)) {
            continue;
        }
        if (funcMap.count(key) > 0) {
            funcMap[key](key, root, piecewiseParameter);
        }
    }
}

void HmSymbolConfig_OHOS::parseSymbolCurveArgs(const Json::Value& root, std::map<std::string, double_t>& curveArgs)
{
    if (root.empty()) {
        return;
    }

    for (Json::Value::const_iterator iter = root.begin(); iter != root.end(); ++iter) {
        std::string name = iter.name();
        const char* memberName = name.c_str();
        if (!root[memberName].isNumeric()) {
            SkDebugf("%{public}s is not numeric!", memberName);
            continue;
        }
        curveArgs.insert({std::string(memberName), root[memberName].asDouble()});
    }
}

void HmSymbolConfig_OHOS::parseSymbolProperties(const Json::Value& root,
    std::map<std::string, std::vector<double_t>>& properties)
{
    for (Json::Value::const_iterator iter = root.begin(); iter != root.end(); ++iter) {
        std::string name = iter.name();
        const char* memberName = name.c_str();
        if (!root[memberName].isArray()) {
            SkDebugf("%{public}s is not array!", memberName);
            continue;
        }

        std::vector<double_t> propertyValues;
        for (unsigned int i = 0; i < root[memberName].size(); i++) {
            if (!root[memberName][i].isNumeric()) {
                SkDebugf("property value is not numeric!");
                continue;
            }
            propertyValues.push_back(root[memberName][i].asDouble());
        }

        properties.insert({std::string(memberName), propertyValues});
    }
}

void HmSymbolConfig_OHOS::parseSymbolLayersGrouping(const Json::Value& root)
{
    std::unordered_map<uint32_t, SymbolLayersGroups>* hmSymbolConfig
        = getHmSymbolConfig();
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("symbol_layers_grouping[%{public}d] is not object!", i);
            continue;
        }
        parseOneSymbol(root[i], hmSymbolConfig);
    }
}

static void SymbolGlyphCase(const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups)
{
    if (!root[key].isInt()) {
        SkDebugf("symbol_glyph_id is not int!");
        return;
    }
    symbolLayersGroups.symbolGlyphId = root[key].asInt();
}

void HmSymbolConfig_OHOS::parseOneSymbol(const Json::Value& root,
    std::unordered_map<uint32_t, SymbolLayersGroups>* hmSymbolConfig)
{
    const char* key = nullptr;
    std::string tags[] = {NATIVE_GLYPH_ID, SYMBOL_GLYPH_ID, LAYERS, RENDER_MODES, "animation_settings"};
    uint32_t nativeGlyphId;
    SymbolLayersGroups symbolLayersGroups;
    using SymbolKeyFuncMap = std::unordered_map<std::string, SymbolKeyFunc>;
    SymbolKeyFuncMap funcMap = {
        {NATIVE_GLYPH_ID, [this, &nativeGlyphId](const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups)
            {
                if (!root[key].isInt()) {
                    SkDebugf("native_glyph_id is not int!");
                    return;
                }
                nativeGlyphId = root[key].asInt();
            }
        },
        {SYMBOL_GLYPH_ID, SymbolGlyphCase},
        {LAYERS, [this](const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups)
            {
                if (!root[key].isArray()) {
                    SkDebugf("layers is not array!");
                    return;
                }
                parseLayers(root[key], symbolLayersGroups.layers);
            }
        },
        {RENDER_MODES, [this](const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups)
            {
                if (!root[key].isArray()) {
                    SkDebugf("render_modes is not array!");
                    return;
                }
                parseRenderModes(root[key], symbolLayersGroups.renderModeGroups);
            }
        },
        {"animation_settings", [this](const char* key, const Json::Value& root,
            SymbolLayersGroups& symbolLayersGroups)
            {
                if (!root[key].isArray()) {
                    SkDebugf("animation_settings is not array!");
                    return;
                }
                parseAnimationSettings(root[key], symbolLayersGroups.animationSettings);
            }
        }
    };
    for (unsigned int i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        key = tags[i].c_str();
        if (!root.isMember(key)) {
            continue;
        }
        if (funcMap.count(key) > 0) {
            funcMap[key](key, root, symbolLayersGroups);
        }
    }
    hmSymbolConfig->insert({nativeGlyphId, symbolLayersGroups});
}

void HmSymbolConfig_OHOS::parseLayers(const Json::Value& root, std::vector<std::vector<size_t>>& layers)
{
    const char* key = "components";
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("layer is not object!");
            continue;
        }

        if (!root[i].isMember(key)) {
            continue;
        }
        if (!root[i][key].isArray()) {
            SkDebugf("components is not array!");
            continue;
        }
        std::vector<size_t> components;
        parseComponets(root[i][key], components);
        layers.push_back(components);
    }
}

void HmSymbolConfig_OHOS::parseComponets(const Json::Value& root, std::vector<size_t>& components)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isInt()) {
            SkDebugf("component is not int!");
            continue;
        }
        components.push_back(root[i].asInt());
    }
}

void HmSymbolConfig_OHOS::parseRenderModes(const Json::Value& root, std::map<SymbolRenderingStrategy,
    std::vector<RenderGroup>>& renderModesGroups)
{
    std::unordered_map<std::string, SymbolRenderingStrategy> strategeMap = {
        {"monochrome", SymbolRenderingStrategy::SINGLE},
        {"multicolor", SymbolRenderingStrategy::MULTIPLE_COLOR},
        {"hierarchical", SymbolRenderingStrategy::MULTIPLE_OPACITY},
    };
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("render_mode is not object!");
            continue;
        }

        SymbolRenderingStrategy renderingStrategy;
        std::vector<RenderGroup> renderGroups;
        if (root[i].isMember(MODE)) {
            if (!root[i][MODE].isString()) {
                SkDebugf("mode is not string!");
                continue;
            }
            std::string modeValue = root[i][MODE].asCString();
            if (strategeMap.count(modeValue) > 0) {
                renderingStrategy = strategeMap[modeValue];
            } else {
                SkDebugf("%{public}s is invalid value!", modeValue.c_str());
                continue;
            }
        }
        if (root[i].isMember(RENDER_GROUPS)) {
            if (!root[i][RENDER_GROUPS].isArray()) {
                SkDebugf("render_groups is not array!");
                continue;
            }
            parseRenderGroups(root[i][RENDER_GROUPS], renderGroups);
        }
        renderModesGroups.insert({renderingStrategy, renderGroups});
    }
}

void HmSymbolConfig_OHOS::parseRenderGroups(const Json::Value& root, std::vector<RenderGroup>& renderGroups)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("render_group is not object!");
            continue;
        }

        RenderGroup renderGroup;
        if (root[i].isMember(GROUP_INDEXES) && root[i][GROUP_INDEXES].isArray()) {
            parseGroupIndexes(root[i][GROUP_INDEXES], renderGroup.groupInfos);
        }
        if (root[i].isMember(DEFAULT_COLOR) && root[i][DEFAULT_COLOR].isString()) {
            parseDefaultColor(root[i][DEFAULT_COLOR].asCString(), renderGroup);
        }
        if (root[i].isMember(FIX_ALPHA) && root[i][FIX_ALPHA].isDouble()) {
            renderGroup.color.a = root[i][FIX_ALPHA].asFloat();
        }
        renderGroups.push_back(renderGroup);
    }
}

void HmSymbolConfig_OHOS::parseGroupIndexes(const Json::Value& root, std::vector<GroupInfo>& groupInfos)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        GroupInfo groupInfo;
        if (root[i].isMember(LAYER_INDEXES)) {
            if (!root[i][LAYER_INDEXES].isArray()) {
                SkDebugf("layer_indexes is not array!");
                continue;
            }
            parseLayerOrMaskIndexes(root[i][LAYER_INDEXES], groupInfo.layerIndexes);
        }
        if (root[i].isMember(MASK_INDEXES)) {
            if (!root[i][MASK_INDEXES].isArray()) {
                SkDebugf("mask_indexes is not array!");
                continue;
            }
            parseLayerOrMaskIndexes(root[i][MASK_INDEXES], groupInfo.maskIndexes);
        }
        groupInfos.push_back(groupInfo);
    }
}

void HmSymbolConfig_OHOS::parseLayerOrMaskIndexes(const Json::Value& root, std::vector<size_t>& indexes)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isInt()) {
            SkDebugf("index is not int!");
            continue;
        }
        indexes.push_back(root[i].asInt());
    }
}

void HmSymbolConfig_OHOS::parseDefaultColor(const char* defaultColorStr, RenderGroup& renderGroup)
{
    char defaultColorHex[defaultColorHexLen];
    defaultColorHex[0] = '0';
    defaultColorHex[1] = 'X';
    if (strlen(defaultColorStr) == defaultColorStrLen) {
        for (unsigned int i = 1; i < defaultColorStrLen; i++) {
            defaultColorHex[i + 1] = defaultColorStr[i];
        }
    } else {
        SkDebugf("%{public}s is invalid value!", defaultColorStr);
        return;
    }

    char* endPtr;
    int defaultColorInt = strtol(defaultColorHex, &endPtr, hexFlag);
    renderGroup.color.r = (defaultColorInt >> twoBytesBitsLen) & 0xFF;
    renderGroup.color.g = (defaultColorInt >> oneByteBitsLen) & 0xFF;
    renderGroup.color.b = defaultColorInt & 0xFF;
}

void HmSymbolConfig_OHOS::parseAnimationSettings(const Json::Value& root, std::vector<AnimationSetting>& animationSettings)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("animation_setting is not object!");
            continue;
        }
        AnimationSetting animationSetting;
        parseAnimationSetting(root[i], animationSetting);
        animationSettings.push_back(animationSetting);
    }
}

void HmSymbolConfig_OHOS::parseAnimationSetting(const Json::Value& root, AnimationSetting& animationSetting)
{
    if (root.isMember(ANIMATION_TYPE) && root[ANIMATION_TYPE].isString()) {
        parseAnimationType(root[ANIMATION_TYPE].asCString(), animationSetting.animationType);
    }

    if (root.isMember(SUB_TYPE) && root[SUB_TYPE].isString()) {
        parseSymbolAnimationSubType(root[SUB_TYPE].asCString(), animationSetting.animationSubType);
    }

    if (root.isMember(ANIMATION_MODE) && root[ANIMATION_MODE].isInt()) {
        animationSetting.animationMode = root[ANIMATION_MODE].asInt();
    }

    if (root.isMember(GROUP_SETTINGS) && root[GROUP_SETTINGS].isArray()) {
        parseGroupSettings(root[GROUP_SETTINGS], animationSetting.groupSettings);
    }
}

void HmSymbolConfig_OHOS::parseAnimationType(const char* animationTypeStr, AnimationType& animationType)
{
    if (!strcmp(animationTypeStr, "scale")) {
        animationType = AnimationType::SCALE_EFFECT;
        return;
    }
    if (!strcmp(animationTypeStr, "variable_color")) {
        animationType = AnimationType::VARIABLE_COLOR;
        return;
    }
    SkDebugf("%{public}s is invalid value!", animationTypeStr);
}

void HmSymbolConfig_OHOS::parseGroupSettings(const Json::Value& root, std::vector<GroupSetting>& groupSettings)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("group_setting is not object!");
            continue;
        }
        GroupSetting groupSetting;
        parseGroupSetting(root[i], groupSetting);
        groupSettings.push_back(groupSetting);
    }
}

void HmSymbolConfig_OHOS::parseGroupSetting(const Json::Value& root, GroupSetting& groupSetting)
{
    if (root.isMember(GROUP_INDEXES)) {
        if (!root[GROUP_INDEXES].isArray()) {
            SkDebugf("group_indexes is not array!");
        } else {
            parseGroupIndexes(root[GROUP_INDEXES], groupSetting.groupInfos);
        }
    }

    if (root.isMember(ANIMATION_INDEX)) {
        if (!root[ANIMATION_INDEX].isInt()) {
            SkDebugf("animation_index is not int!");
        } else {
            groupSetting.animationIndex = root[ANIMATION_INDEX].asInt();
        }
    }
}