/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2020/03/31
 */

#ifndef CSS_STYLE_PARSER_H
#define CSS_STYLE_PARSER_H

#include <string>
#include <unordered_map>
#include <vector>

using ClassStyleMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

class CssStyleParser {
public:
    void parseCssStyle(const std::string& style);
    const std::unordered_map<std::string, std::string>& getArributesMap(const std::string& key) const;
    static std::vector<std::string> splitString(const std::string& srcString, const std::string& splitString);

private:
    ClassStyleMap fStyleMap;
};
#endif
