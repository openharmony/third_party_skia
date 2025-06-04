/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2020/03/31
 */

#include "CssStyleParser.h"

const std::unordered_map<std::string, std::string>& CssStyleParser::getArributesMap(const std::string& key) const
{
    auto styleClassIter = fStyleMap.find(key);
    if (styleClassIter != fStyleMap.end()) {
        return styleClassIter->second;
    } else {
        static std::unordered_map<std::string, std::string> fEmptyMap;
        return fEmptyMap;
    }
}

void CssStyleParser::parseCssStyle(const std::string& style)
{
    // begin from 1 to skip first '.'for example: ".cls-1{fill:#843dd1;}"
    auto styles = splitString(style.substr(1), "}.");
    for (auto& style : styles) {
        auto nameEnd = style.find_first_of('{');
        if (nameEnd != std::string::npos) {
            auto names = style.substr(0, nameEnd);
            if (names.empty()) {
                return;
            }
            auto splitNames = splitString(names, ",.");
            auto attributesString = style.substr(nameEnd + 1);
            auto attributesVector = splitString(attributesString, ";");
            for (auto& splitName : splitNames) {
                for (auto& attribute : attributesVector) {
                    auto arrPair = splitString(attribute, ":");
                    // 2 means stye is a kind of key: value, for example a color stype: "fill:#843dd1"
                    if (arrPair.size() == 2) {
                        auto arrMapIter = fStyleMap.find(splitName);
                        if (arrMapIter == fStyleMap.end()) {
                            std::unordered_map<std::string, std::string> arrMap;
                            arrMap.emplace(std::make_pair(arrPair[0], arrPair[1]));
                            fStyleMap.emplace(std::make_pair(splitName, arrMap));
                        } else {
                            arrMapIter->second.emplace(std::make_pair(arrPair[0], arrPair[1]));
                        }
                    }
                }
            }
        }
    }
}

std::vector<std::string> CssStyleParser::splitString(const std::string& srcString, const std::string& splitString)
{
    std::string::size_type pos1;
    std::string::size_type pos2;
    std::vector<std::string> res;
    pos2 = srcString.find(splitString);
    pos1 = 0;
    while (std::string::npos != pos2) {
        res.push_back(srcString.substr(pos1, pos2 - pos1));

        pos1 = pos2 + splitString.size();
        pos2 = srcString.find(splitString, pos1);
    }
    if (pos1 != srcString.length()) {
        res.push_back(srcString.substr(pos1));
    }
    return res;
}
