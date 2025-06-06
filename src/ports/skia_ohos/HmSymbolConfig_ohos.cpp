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

#include "HmSymbolConfig_ohos.h"

#include <functional>
#include <fstream>

#include "securec.h"

#include "include/private/SkOnce.h"

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

using SymbolKeyFuncMap = std::unordered_map<std::string, SymbolKeyFunc>;
const char SPECIAL_ANIMATIONS[] = "special_animations";
const char COMMON_ANIMATIONS[] = "common_animations";
const char SYMBOL_LAYERS_GROUPING[] = "symbol_layers_grouping";
const char ANIMATION_TYPE[] = "animation_type";
const char ANIMATION_TYPES[] = "animation_types";
const char ANIMATION_PARAMETERS[] = "animation_parameters";
const char ANIMATION_MODE[] = "animation_mode";
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
const char COMMON_SUB_TYPE[] = "common_sub_type";
const char ANIMATION_SETTINGS[] = "animation_settings";
const char PROPERTIES[] = "properties";
const char SLOPE[] = "slope";

const int NO_ERROR = 0; // no error
const int ERROR_CONFIG_NOT_FOUND = 1; // the configuration document is not found
const int ERROR_CONFIG_FORMAT_NOT_SUPPORTED = 2; // the formation of configuration is not supported
const int ERROR_CONFIG_INVALID_VALUE_TYPE = 4; // invalid value type in the configuration
const int ERROR_TYPE_COUNT = 11;

static const std::map<std::string, AnimationType> ANIMATIONS_TYPES = {
    {"scale", AnimationType::SCALE_TYPE},
    {"appear", AnimationType::APPEAR_TYPE},
    {"disappear", AnimationType::DISAPPEAR_TYPE},
    {"bounce", AnimationType::BOUNCE_TYPE},
    {"variable_color", AnimationType::VARIABLE_COLOR_TYPE},
    {"pulse", AnimationType::PULSE_TYPE},
    {"replace_appear", AnimationType::REPLACE_APPEAR_TYPE},
    {"replace_disappear", AnimationType::REPLACE_DISAPPEAR_TYPE},
    {"disable", AnimationType::DISABLE_TYPE}
};

static const std::map<std::string, CurveType> CURVE_TYPES = {
    {"spring", CurveType::SPRING},
    {"linear", CurveType::LINEAR},
    {"friction", CurveType::FRICTION},
    {"sharp", CurveType::SHARP}
};

/* To get the display text of an error
 * \param err the id of an error
 * \return The text to explain the error
 */
const char* ErrInfoToString(int err)
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

/* To log the error information
 * \param err the id of an error
 * \param key the key which indicates the the part with the error
 * \param expected the expected type of json node.
 * \n     It's used only for err 'ERROR_CONFIG_INVALID_VALUE_TYPE'
 * \param actual the actual type of json node.
 * \n     It's used only for err 'ERROR_CONFIG_INVALID_VALUE_TYPE'
 * \return err
 */
int LogErrInfo(int err, const char* key, Json::ValueType expected = Json::nullValue,
    Json::ValueType actual = Json::nullValue)
{
    if (err != ERROR_CONFIG_INVALID_VALUE_TYPE) {
        LOGE("%s : %s\n", ErrInfoToString(err), key);
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
                ErrInfoToString(err), key, types[expected], types[actual]);
        } else {
            LOGE("%s : %s\n", ErrInfoToString(err), key);
        }
    }
    return err;
}

/* To get the data of font configuration file
 * \param fname the full name of the font configuration file
 * \param[out] size the size of data returned to the caller
 * \return The pointer of content of the file
 * \note The returned pointer should be freed by the caller
 */
std::unique_ptr<char[]> GetDataFromFile(const char* fname, int& size)
{
    auto file = std::make_unique<std::ifstream>(fname);
    if (file == nullptr || !file->is_open()) {
        size = 0;
        return nullptr;
    }

    file->seekg(0, std::ios::end);
    size = static_cast<int>(file->tellg()); // get the length of file
    if (size <= 0) {
        size = 0;
        return nullptr;
    }
    auto data = std::make_unique<char[]>(size);
    if (data == nullptr) {
        size = 0;
        return nullptr;
    }
    file->seekg(0, std::ios::beg);
    file->read(data.get(), size); // read data a block
    return data;
}

bool HmSymbolConfig_OHOS::GetHmSymbolEnable()
{
#ifdef SK_BUILD_FONT_MGR_FOR_OHOS
    static bool isHMSymbolEnable =
        (std::atoi(OHOS::system::GetParameter("persist.sys.graphic.hmsymbolcfg.enable", "1").c_str()) != 0);
    return isHMSymbolEnable;
#else
    return true;
#endif
}

HmSymbolConfig_OHOS* HmSymbolConfig_OHOS::GetInstance()
{
    static HmSymbolConfig_OHOS singleton;
    return &singleton;
}

SymbolLayersGroups HmSymbolConfig_OHOS::GetSymbolLayersGroups(uint16_t glyphId)
{
    SymbolLayersGroups symbolLayersGroups;
    if (hmSymbolConfig_.size() == 0) {
        return symbolLayersGroups;
    }
    auto iter = hmSymbolConfig_.find(glyphId);
    if (iter != hmSymbolConfig_.end()) {
        symbolLayersGroups = iter->second;
    }
    return symbolLayersGroups;
}

std::vector<std::vector<PiecewiseParameter>> HmSymbolConfig_OHOS::GetGroupParameters(AnimationType type,
    uint16_t groupSum, uint16_t animationMode, CommonSubType commonSubType)
{
    std::vector<std::vector<PiecewiseParameter>> groupParametersOut;
    if (animationInfos_.empty()) {
        return groupParametersOut;
    }
    std::unordered_map<AnimationType, AnimationInfo>::iterator iter = animationInfos_.find(type);
    if (iter == animationInfos_.end()) {
        return groupParametersOut;
    }
    auto key = EncodeAnimationAttribute(groupSum, animationMode, commonSubType);
    auto para = iter->second.animationParas.find(key);
    if (para != iter->second.animationParas.end()) {
        groupParametersOut = para->second.groupParameters;
    }
    return groupParametersOut;
}

/* check the system font configuration document
 * \param fname the full name of the font configuration document
 * \return NO_ERROR successful
 * \return ERROR_CONFIG_NOT_FOUND config document is not found
 * \return ERROR_CONFIG_FORMAT_NOT_SUPPORTED config document format is not supported
 */
int HmSymbolConfig_OHOS::CheckConfigFile(const char* fname, Json::Value& root)
{
    int size = 0;
    auto data = GetDataFromFile(fname, size);
    if (data == nullptr) {
        return LogErrInfo(ERROR_CONFIG_NOT_FOUND, fname);
    }
    JSONCPP_STRING errs;
    Json::CharReaderBuilder charReaderBuilder;
    std::unique_ptr<Json::CharReader> jsonReader(charReaderBuilder.newCharReader());
    bool isJson = jsonReader->parse(data.get(), data.get() + size, &root, &errs);
    if (!isJson || !errs.empty()) {
        return LogErrInfo(ERROR_CONFIG_FORMAT_NOT_SUPPORTED, fname);
    }
    return NO_ERROR;
}

int HmSymbolConfig_OHOS::ParseConfigOfHmSymbol(const char* fname, SkString fontDir)
{
    std::lock_guard<std::mutex> lock(hmSymbolMut_);
    if (GetInit()) {
        return NO_ERROR;
    }
    if (fname == nullptr || strlen(fname) == 0) {
        return ERROR_CONFIG_NOT_FOUND;
    }
    Clear();
    if (fontDir.size() == 0) {
        fontDir.append(".");
    }
    std::string fullname(fontDir.c_str(), fontDir.size());
    char separator = '/';
#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN)
    separator = '\\';
#endif
    if (fontDir[fontDir.size() - 1] != separator) {
        fullname += separator;
    }
    std::string fnameStr(fname, strlen(fname));
    fullname += fnameStr;
    Json::Value root;
    int err = CheckConfigFile(fullname.c_str(), root);
    if (err != NO_ERROR) {
        return err;
    }

    const char* key = nullptr;
    std::vector<std::string> tags = {COMMON_ANIMATIONS, SPECIAL_ANIMATIONS, SYMBOL_LAYERS_GROUPING};
    for (unsigned int i = 0; i < tags.size(); i++) {
        key = tags[i].c_str();
        if (!root.isMember(key) || !root[key].isArray()) {
            continue;
        }
        if (!strcmp(key, COMMON_ANIMATIONS) || !strcmp(key, SPECIAL_ANIMATIONS)) {
            ParseSymbolAnimations(root[key], &animationInfos_);
        } else {
            ParseSymbolLayersGrouping(root[key]);
        }
    }
    root.clear();
    SetInit(true);
    return NO_ERROR;
}

void HmSymbolConfig_OHOS::ParseSymbolAnimations(const Json::Value& root,
    std::unordered_map<AnimationType, AnimationInfo>* animationInfos)
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
        const std::string animationType = root[i][ANIMATION_TYPE].asString();
        ParseAnimationType(animationType, animationInfo.animationType);

        if (!root[i][ANIMATION_PARAMETERS].isArray()) {
            SkDebugf("animation_parameters is not array!");
            continue;
        }
        ParseSymbolAnimationParas(root[i][ANIMATION_PARAMETERS], animationInfo.animationParas);
        animationInfos->insert({animationInfo.animationType, animationInfo});
    }
}

uint32_t HmSymbolConfig_OHOS::EncodeAnimationAttribute(uint16_t groupSum, uint16_t animationMode,
    CommonSubType commonSubType)
{
    uint32_t result = static_cast<uint32_t>(groupSum);
    result = (result << oneByteBitsLen) + static_cast<uint32_t>(animationMode);
    result = (result << oneByteBitsLen) + static_cast<uint32_t>(commonSubType);
    return result;
}

void HmSymbolConfig_OHOS::ParseSymbolAnimationParas(const Json::Value& root,
    std::map<uint32_t, AnimationPara>& animationParas)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("animation_parameter is not object!");
            continue;
        }
        AnimationPara animationPara;
        ParseSymbolAnimationPara(root[i], animationPara);
        uint32_t attributeKey = EncodeAnimationAttribute(animationPara.groupParameters.size(),
            animationPara.animationMode, animationPara.commonSubType);
        animationParas.insert({attributeKey, animationPara});
    }
}

void HmSymbolConfig_OHOS::ParseSymbolAnimationPara(const Json::Value& root, AnimationPara& animationPara)
{
    const char* key = nullptr;
    std::vector<std::string> tags = {ANIMATION_MODE, COMMON_SUB_TYPE, GROUP_PARAMETERS};
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
        {COMMON_SUB_TYPE, [this](const char* key, const Json::Value& root, AnimationPara& animationPara)
            {
                if (!root[key].isString()) {
                    SkDebugf("sub_type is not string!");
                    return;
                }
                const std::string subTypeStr = root[key].asString();
                ParseSymbolCommonSubType(subTypeStr, animationPara.commonSubType);
            }
        },
        {GROUP_PARAMETERS, [this](const char* key, const Json::Value& root, AnimationPara& animationPara)
            {
                if (!root[key].isArray()) {
                    SkDebugf("group_parameters is not array!");
                    return;
                }
                ParseSymbolGroupParas(root[key], animationPara.groupParameters);
            }
        }
    };
    for (unsigned int i = 0; i < tags.size(); i++) {
        key = tags[i].c_str();
        if (!root.isMember(key)) {
            continue;
        }
        if (funcMap.count(key) > 0) {
            funcMap[key](key, root, animationPara);
        }
    }
}

void HmSymbolConfig_OHOS::ParseSymbolCommonSubType(const std::string& subTypeStr, CommonSubType& commonSubType)
{
    using SymbolAniSubFuncMap = std::unordered_map<std::string, std::function<void(CommonSubType&)>>;
    SymbolAniSubFuncMap funcMap = {
        {
            "up",
            [](CommonSubType& commonSubType) {
                commonSubType = CommonSubType::UP;
            }
        },
        {
            "down",
            [](CommonSubType& commonSubType) {
                commonSubType = CommonSubType::DOWN;
            }
        }
    };
    if (funcMap.count(subTypeStr) > 0) {
        funcMap[subTypeStr](commonSubType);
        return;
    }
    SkDebugf("%{public}s is invalid value!", subTypeStr.c_str());
}

void HmSymbolConfig_OHOS::ParseSymbolGroupParas(const Json::Value& root,
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
            ParseSymbolPiecewisePara(root[i][j], piecewiseParameter);
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
    const std::string curveTypeStr = root[key].asString();
    auto iter = CURVE_TYPES.find(curveTypeStr);
    if (iter != CURVE_TYPES.end()) {
        piecewiseParameter.curveType = iter->second;
        return;
    }
    SkDebugf("curve is invalid value!");
}

static void PiecewiseParaDurationCase(const char* key, const Json::Value& root,
    PiecewiseParameter& piecewiseParameter)
{
    if (!root[key].isNumeric()) {
        SkDebugf("duration is not numeric!");
        return;
    }
    if (root[key].isConvertibleTo(Json::uintValue)) {
        piecewiseParameter.duration = root[key].asUInt();
    }
}

static void PiecewiseParaDelayCase(const char* key, const Json::Value& root, PiecewiseParameter& piecewiseParameter)
{
    if (!root[key].isNumeric()) {
        SkDebugf("delay is not numeric!");
        return;
    }
    if (root[key].isConvertibleTo(Json::intValue)) {
        piecewiseParameter.delay = root[key].asInt();
    }
}

void HmSymbolConfig_OHOS::ParseSymbolPiecewisePara(const Json::Value& root, PiecewiseParameter& piecewiseParameter)
{
    const char* key = nullptr;
    std::vector<std::string> tags = {CURVE, CURVE_ARGS, DURATION, DELAY, PROPERTIES};
    using PiecewiseFuncMap = std::unordered_map<std::string, PiecewiseParaKeyFunc>;
    PiecewiseFuncMap funcMap = {
        {CURVE, PiecewiseParaCurveCase},
        {CURVE_ARGS, [this](const char* key, const Json::Value& root, PiecewiseParameter& piecewiseParameter)
            {
                if (!root[key].isObject()) {
                    SkDebugf("curve_args is not object!");
                    return;
                }
                ParseSymbolCurveArgs(root[key], piecewiseParameter.curveArgs);
            }
        },
        {DURATION, PiecewiseParaDurationCase},
        {DELAY, PiecewiseParaDelayCase},
        {PROPERTIES, [this](const char* key, const Json::Value& root, PiecewiseParameter& piecewiseParameter)
            {
                if (!root[key].isObject()) {
                    SkDebugf("properties is not object!");
                    return;
                }
                ParseSymbolProperties(root[key], piecewiseParameter.properties);
            }
        }
    };

    for (unsigned int i = 0; i < tags.size(); i++) {
        key = tags[i].c_str();
        if (!root.isMember(key)) {
            continue;
        }
        if (funcMap.count(key) > 0) {
            funcMap[key](key, root, piecewiseParameter);
        }
    }
}

void HmSymbolConfig_OHOS::ParseSymbolCurveArgs(const Json::Value& root, std::map<std::string, float>& curveArgs)
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
        curveArgs.insert({std::string(memberName), root[memberName].asFloat()});
    }
}

void HmSymbolConfig_OHOS::ParseSymbolProperties(const Json::Value& root,
    std::map<std::string, std::vector<float>>& properties)
{
    for (Json::Value::const_iterator iter = root.begin(); iter != root.end(); ++iter) {
        std::string name = iter.name();
        const char* memberName = name.c_str();
        if (!root[memberName].isArray()) {
            SkDebugf("%{public}s is not array!", memberName);
            continue;
        }

        std::vector<float> propertyValues;
        for (unsigned int i = 0; i < root[memberName].size(); i++) {
            if (!root[memberName][i].isNumeric()) {
                SkDebugf("property value is not numeric!");
                continue;
            }
            propertyValues.push_back(root[memberName][i].asFloat());
        }

        properties.insert({std::string(memberName), propertyValues});
    }
}

void HmSymbolConfig_OHOS::ParseSymbolLayersGrouping(const Json::Value& root)
{
    std::unordered_map<uint16_t, SymbolLayersGroups>* hmSymbolConfig
        = &hmSymbolConfig_;
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("symbol_layers_grouping[%{public}d] is not object!", i);
            continue;
        }
        ParseOneSymbol(root[i], hmSymbolConfig);
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

void HmSymbolConfig_OHOS::ParseOneSymbolNativeCase(const char* key, const Json::Value& root,
    SymbolLayersGroups& symbolLayersGroups, uint16_t& nativeGlyphId)
{
    if (!root[key].isInt()) {
        SkDebugf("native_glyph_id is not int!");
        return;
    }
    nativeGlyphId = root[key].asInt();
}

void HmSymbolConfig_OHOS::ParseOneSymbolLayerCase(const char* key, const Json::Value& root,
    SymbolLayersGroups& symbolLayersGroups)
{
    if (!root[key].isArray()) {
        SkDebugf("layers is not array!");
        return;
    }
    ParseLayers(root[key], symbolLayersGroups.layers);
}

void HmSymbolConfig_OHOS::ParseOneSymbolRenderCase(const char* key, const Json::Value& root,
    SymbolLayersGroups& symbolLayersGroups)
{
    if (!root[key].isArray()) {
        SkDebugf("render_modes is not array!");
        return;
    }
    ParseRenderModes(root[key], symbolLayersGroups.renderModeGroups);
}

void HmSymbolConfig_OHOS::ParseOneSymbolAnimateCase(const char* key, const Json::Value& root,
    SymbolLayersGroups& symbolLayersGroups)
{
    if (!root[key].isArray()) {
        SkDebugf("animation_settings is not array!");
        return;
    }
    ParseAnimationSettings(root[key], symbolLayersGroups.animationSettings);
}

void HmSymbolConfig_OHOS::ParseOneSymbol(const Json::Value& root,
    std::unordered_map<uint16_t, SymbolLayersGroups>* hmSymbolConfig)
{
    const char* key = nullptr;
    std::vector<std::string> tags = {NATIVE_GLYPH_ID, SYMBOL_GLYPH_ID, LAYERS, RENDER_MODES, ANIMATION_SETTINGS};
    uint16_t nativeGlyphId;
    SymbolLayersGroups symbolLayersGroups;

    SymbolKeyFuncMap funcMap = {
        {NATIVE_GLYPH_ID, [this, &nativeGlyphId](const char* key, const Json::Value& root,
            SymbolLayersGroups& symbolLayersGroups)
            {
                ParseOneSymbolNativeCase(key, root, symbolLayersGroups, nativeGlyphId);
            }
        },
        {SYMBOL_GLYPH_ID, SymbolGlyphCase},
        {LAYERS, [this](const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups)
            {
                ParseOneSymbolLayerCase(key, root, symbolLayersGroups);
            }
        },
        {RENDER_MODES, [this](const char* key, const Json::Value& root, SymbolLayersGroups& symbolLayersGroups)
            {
                ParseOneSymbolRenderCase(key, root, symbolLayersGroups);
            }
        },
        {ANIMATION_SETTINGS, [this](const char* key, const Json::Value& root,
            SymbolLayersGroups& symbolLayersGroups)
            {
                ParseOneSymbolAnimateCase(key, root, symbolLayersGroups);
            }
        }
    };
    for (unsigned int i = 0; i < tags.size(); i++) {
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

void HmSymbolConfig_OHOS::ParseLayers(const Json::Value& root, std::vector<std::vector<size_t>>& layers)
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
        ParseComponets(root[i][key], components);
        layers.push_back(components);
    }
}

void HmSymbolConfig_OHOS::ParseComponets(const Json::Value& root, std::vector<size_t>& components)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isInt()) {
            SkDebugf("component is not int!");
            continue;
        }
        components.push_back(root[i].asInt());
    }
}

void HmSymbolConfig_OHOS::ParseRenderModes(const Json::Value& root, std::map<SymbolRenderingStrategy,
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
            std::string modeValue = root[i][MODE].asString();
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
            ParseRenderGroups(root[i][RENDER_GROUPS], renderGroups);
        }
        renderModesGroups.insert({renderingStrategy, renderGroups});
    }
}

void HmSymbolConfig_OHOS::ParseRenderGroups(const Json::Value& root, std::vector<RenderGroup>& renderGroups)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("render_group is not object!");
            continue;
        }

        RenderGroup renderGroup;
        if (root[i].isMember(GROUP_INDEXES) && root[i][GROUP_INDEXES].isArray()) {
            ParseGroupIndexes(root[i][GROUP_INDEXES], renderGroup.groupInfos);
        }
        if (root[i].isMember(DEFAULT_COLOR) && root[i][DEFAULT_COLOR].isString()) {
            const std::string defaultColor = root[i][DEFAULT_COLOR].asString();
            ParseDefaultColor(defaultColor, renderGroup);
        }
        if (root[i].isMember(FIX_ALPHA) && root[i][FIX_ALPHA].isDouble()) {
            renderGroup.color.a = static_cast<float>(root[i][FIX_ALPHA].asDouble());
        }
        renderGroups.push_back(renderGroup);
    }
}

void HmSymbolConfig_OHOS::ParseGroupIndexes(const Json::Value& root, std::vector<GroupInfo>& groupInfos)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        GroupInfo groupInfo;
        if (root[i].isMember(LAYER_INDEXES)) {
            if (!root[i][LAYER_INDEXES].isArray()) {
                SkDebugf("layer_indexes is not array!");
                continue;
            }
            ParseLayerOrMaskIndexes(root[i][LAYER_INDEXES], groupInfo.layerIndexes);
        }
        if (root[i].isMember(MASK_INDEXES)) {
            if (!root[i][MASK_INDEXES].isArray()) {
                SkDebugf("mask_indexes is not array!");
                continue;
            }
            ParseLayerOrMaskIndexes(root[i][MASK_INDEXES], groupInfo.maskIndexes);
        }
        groupInfos.push_back(groupInfo);
    }
}

void HmSymbolConfig_OHOS::ParseLayerOrMaskIndexes(const Json::Value& root, std::vector<size_t>& indexes)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isInt()) {
            SkDebugf("index is not int!");
            continue;
        }
        indexes.push_back(root[i].asInt());
    }
}

void HmSymbolConfig_OHOS::ParseDefaultColor(const std::string& defaultColorStr, RenderGroup& renderGroup)
{
    char defaultColorHex[defaultColorHexLen];
    defaultColorHex[0] = '0';
    defaultColorHex[1] = 'X';
    if (defaultColorStr.length() == defaultColorStrLen) {
        for (unsigned int i = 1; i < defaultColorStrLen; i++) {
            defaultColorHex[i + 1] = defaultColorStr[i];
        }
        defaultColorHex[defaultColorHexLen - 1] = '\0';
    } else {
        SkDebugf("%{public}s is invalid value!", defaultColorStr.c_str());
        return;
    }

    char* endPtr = nullptr;
    int defaultColorInt = strtol(defaultColorHex, &endPtr, hexFlag);
    renderGroup.color.r = (defaultColorInt >> twoBytesBitsLen) & 0xFF;
    renderGroup.color.g = (defaultColorInt >> oneByteBitsLen) & 0xFF;
    renderGroup.color.b = defaultColorInt & 0xFF;
}

void HmSymbolConfig_OHOS::ParseAnimationSettings(const Json::Value& root,
    std::vector<AnimationSetting>& animationSettings)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("animation_setting is not object!");
            continue;
        }
        AnimationSetting animationSetting;
        ParseAnimationSetting(root[i], animationSetting);
        animationSettings.push_back(animationSetting);
    }
}

void HmSymbolConfig_OHOS::ParseAnimationSetting(const Json::Value& root, AnimationSetting& animationSetting)
{
    if (root.isMember(ANIMATION_TYPES) && root[ANIMATION_TYPES].isArray()) {
        ParseAnimationTypes(root[ANIMATION_TYPES], animationSetting.animationTypes);
    }

    if (root.isMember(GROUP_SETTINGS) && root[GROUP_SETTINGS].isArray()) {
        ParseGroupSettings(root[GROUP_SETTINGS], animationSetting.groupSettings);
    }

    if (root.isMember(COMMON_SUB_TYPE) && root[COMMON_SUB_TYPE].isString()) {
        const std::string subTypeStr = root[COMMON_SUB_TYPE].asString();
        ParseSymbolCommonSubType(subTypeStr, animationSetting.commonSubType);
    }

    if (root.isMember(SLOPE) && root[SLOPE].isDouble()) {
        animationSetting.slope = root[SLOPE].asDouble();
    }
}

void HmSymbolConfig_OHOS::ParseAnimationTypes(const Json::Value& root, std::vector<AnimationType>& animationTypes)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isString()) {
            SkDebugf("animation_types is not string!");
            continue;
        }
        const std::string animationTypeStr = root[i].asString();
        AnimationType animationType;
        ParseAnimationType(animationTypeStr, animationType);
        animationTypes.push_back(animationType);
    }
}

void HmSymbolConfig_OHOS::ParseAnimationType(const std::string& animationTypeStr, AnimationType& animationType)
{
    auto iter = ANIMATIONS_TYPES.find(animationTypeStr);
    if (iter != ANIMATIONS_TYPES.end()) {
        animationType = iter->second;
    } else {
        SkDebugf("%{public}s is invalid value!", animationTypeStr.c_str());
    }
}

void HmSymbolConfig_OHOS::ParseGroupSettings(const Json::Value& root, std::vector<GroupSetting>& groupSettings)
{
    for (unsigned int i = 0; i < root.size(); i++) {
        if (!root[i].isObject()) {
            SkDebugf("group_setting is not object!");
            continue;
        }
        GroupSetting groupSetting;
        ParseGroupSetting(root[i], groupSetting);
        groupSettings.push_back(groupSetting);
    }
}

void HmSymbolConfig_OHOS::ParseGroupSetting(const Json::Value& root, GroupSetting& groupSetting)
{
    if (root.isMember(GROUP_INDEXES)) {
        if (!root[GROUP_INDEXES].isArray()) {
            SkDebugf("group_indexes is not array!");
        } else {
            ParseGroupIndexes(root[GROUP_INDEXES], groupSetting.groupInfos);
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