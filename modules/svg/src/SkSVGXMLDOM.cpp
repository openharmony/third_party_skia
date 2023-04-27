/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2023/4/25
 */

#include "include/core/SkColor.h"
#include "src/xml/SkDOM.h"
#include "src/xml/SkDOMParser.h"

#include "modules/svg/include/CssStyleParser.h"
#include "modules/svg/include/SkSVGXMLDOM.h"


class SkSVGDOMParser : public SkDOMParser {
public:
    SkSVGDOMParser(SkArenaAllocWithReset* chunk) : SkDOMParser(chunk) {}
    /** Returns true for success
    */
    bool parse(SkStream& docStream, uint64_t svgThemeColor) {
        fSvgThemeColor = svgThemeColor;
        return SkXMLParser::parse(docStream);
    };

protected:
    union SkColorEx {
        struct {
            SkColor  color    : 32;
            bool     valid    : 1;
            uint32_t reserved : 31; // reserved
        };
        uint64_t value = 0;
    };

    bool onStartElement(const char elem[]) override {
        this->startCommon(elem, strlen(elem), SkDOM::kElement_Type);
        if (!strcmp(elem, "style")) {
            fProcessingStyle = true;
        }
        return false;
    }

    bool setSVGColor(
        SkDOM::Attr* attr, const char name[], const char value[], const SkColorEx& svgThemeColor) {
        if (svgThemeColor.valid && (((strcmp(name, "fill") == 0) && (strcmp(value, "none") != 0)) ||
            ((strcmp(name, "stroke") == 0) && (strcmp(value, "none") != 0))) && isPureColor(value)) {
            char colorBuffer[8];
            int res = snprintf(colorBuffer, sizeof(colorBuffer), "#%06x", (svgThemeColor.color & 0xFFFFFF));
            if (res < 0) {
                attr->fValue = dupstr(fAlloc, value, strlen(value));
            } else {
                attr->fValue = dupstr(fAlloc, colorBuffer, strlen(colorBuffer));
            }

            return false;
        }
        if ((svgThemeColor.valid == 1) && (strcmp(name, "opacity") == 0)) {
            char opacityBuffer[4];
            // the opacity is stored in svgThemeColor[24:31], so shift right by 24 bits after extracting it,
            // for e.g., (0x33FFFFFF & 0xFF000000) >> 24 = 0x33.
            // the target string of opacity is like "0.1", so normalize 0x33 to 1, for e.g., 0x33 / 255 = 0.13.
            int res = snprintf(
                opacityBuffer, sizeof(opacityBuffer), "%2.1f", ((svgThemeColor.color & 0xFF000000) >> 24) / 255.0);
            if (res < 0) {
                attr->fValue = dupstr(fAlloc, value,  strlen(value));
            } else {
                attr->fValue = dupstr(fAlloc, opacityBuffer, strlen(opacityBuffer));
            }
            return false;
        }
        return true;
    }

    bool onAddAttribute(const char name[], const char value[]) override {
        SkDOM::Attr* attr = fAttrs.append();
        attr->fName = dupstr(fAlloc, name, strlen(name));
        SkColorEx svgThemeColor;
        svgThemeColor.value = fSvgThemeColor;
        if (!setSVGColor(attr, name, value, svgThemeColor)) {
            return false;
        }
        attr->fValue = dupstr(fAlloc, value, strlen(value));
        // add attributes in style classes.
        if (!strcmp(attr->fName, "class")) {
            auto styleClassMap = fStyleParser.getArributesMap(attr->fValue);
            if (!styleClassMap.empty()) {
                for (auto& arr: styleClassMap) {
                    SkDOM::Attr* attr = fAttrs.append();
                    attr->fName = dupstr(fAlloc, arr.first.c_str(), strlen(arr.first.c_str()));
                    if (!setSVGColor(attr, attr->fName, arr.second.c_str(), svgThemeColor)) {
                        continue;
                    }
                    attr->fValue = dupstr(fAlloc, arr.second.c_str(), strlen(arr.second.c_str()));
                }
            }
        }
        return false;
    }

    bool onEndElement(const char elem[]) override {
        if (SkDOMParser::onEndElement(elem)) {
            return true;
        }
        if (!strcmp(elem, "style")) {
            fProcessingStyle = false;
        }
        return false;
    }

    bool onText(const char text[], int len) override {
        std::string style(text, len);
        this->startCommon(style.c_str(), len, SkDOM::kText_Type);
        this->SkSVGDOMParser::onEndElement(style.c_str());
        if (fProcessingStyle && !style.empty() && style.front() == '.') {
            fStyleParser.parseCssStyle(style);
        }
        return false;
    }

    // check is pure color svg
    bool isPureColor(const char value[]) const {
        std::string color(value);
        if (color.empty()) {
            return true;
        }

        auto pos = color.find_first_not_of(' ');
        if (pos != std::string::npos) {
            color = color.substr(pos);
        }
        // 6 is least length of "url(#..." of a valid color value, 5 is to get the "url(#"
        if (color.length() > 6 && color.substr(0, 5) == "url(#") {
            return false;
        }
        return true;
    }
private:
    // for parse css style svg files.
    bool fProcessingStyle = false;
    CssStyleParser fStyleParser;
    uint64_t fSvgThemeColor = 0;
};


const SkSVGXMLDOM::Node* SkSVGXMLDOM::build(SkStream& docStream) {
    SkSVGDOMParser parser(&fAlloc);
    if (!parser.parse(docStream, fSvgThemeColor))
    {
        SkDEBUGCODE(SkDebugf("xml parse error, line %d\n", parser.fParserError.getLineNumber());)
        fRoot = nullptr;
        fAlloc.reset();
        return nullptr;
    }
    fRoot = parser.getRoot();
    return fRoot;
}

const SkSVGXMLDOM::Node* SkSVGXMLDOM::build(SkStream& docStream, uint64_t svgThemeColor) {
    fSvgThemeColor = svgThemeColor;
    return SkSVGXMLDOM::build(docStream);
}

const SkSVGXMLDOM::Node* SkSVGXMLDOM::copy(const SkDOM& dom, const SkSVGXMLDOM::Node* node) {
    SkSVGDOMParser parser(&fAlloc);

    SkDOM::walk_dom(dom, node, &parser);

    fRoot = parser.getRoot();
    return fRoot;
}

SkXMLParser* SkSVGXMLDOM::beginParsing() {
    SkASSERT(!fParser);
    fParser = std::make_unique<SkSVGDOMParser>(&fAlloc);

    return fParser.get();
}
