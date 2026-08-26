/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2023/4/25
 */

#include "include/core/SkColor.h"
#ifdef SKIA_OHOS_SVG_PROTECTION
#include "include/core/SkLog.h"
#endif
#include "src/xml/SkDOM.h"
#include "src/xml/SkDOMParser.h"

#include "modules/svg/include/CssStyleParser.h"
#ifdef SKIA_OHOS_SVG_PROTECTION
#include "modules/svg/include/SkSVGResourceProtection.h"
#endif
#include "modules/svg/include/SkSVGXMLDOM.h"

#ifdef SKIA_OHOS_SVG_PROTECTION
#include "include/private/base/SkDebug.h"
#endif


class SkSVGDOMParser : public SkDOMParser {
public:
    SkSVGDOMParser(SkArenaAllocWithReset* chunk) : SkDOMParser(chunk) {}
#ifdef SKIA_OHOS_SVG_PROTECTION
    void setSVGResourceLimits(const SkSVGResourceLimits* limits) {
        fSVGResourceLimits = limits;
        SkDOMParser::setSVGResourceLimits(limits);
        fStyleParser.setSVGResourceLimits(limits);
    }
#endif
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
#ifdef SKIA_OHOS_SVG_PROTECTION
        if (fResourceLimitExceeded) {
            return true;
        }
#endif
        this->startCommon(elem, strlen(elem), SkDOM::kElement_Type);
#ifdef SKIA_OHOS_SVG_PROTECTION
        if (fResourceLimitExceeded) {
            return true;
        }
#endif
        if (!strcmp(elem, "style")) {
            fProcessingStyle = true;
        }
#ifdef SKIA_OHOS_SVG_PROTECTION
        return fResourceLimitExceeded;
#else
        return false;
#endif
    }

    bool setSVGColor(
        SkDOM::Attr* attr, const char name[], const char value[], const SkColorEx& svgThemeColor) {
        if (svgThemeColor.valid && (((strcmp(name, "fill") == 0) && (strcmp(value, "none") != 0)) ||
            ((strcmp(name, "stroke") == 0) && (strcmp(value, "none") != 0))) && isPureColor(value)) {
            char colorBuffer[8];
            int res = snprintf(colorBuffer, sizeof(colorBuffer), "#%06x", (svgThemeColor.color & 0xFFFFFF));
            if (res < 0) {
                attr->fValue =
#ifdef SKIA_OHOS_SVG_PROTECTION
                        dupstrLimited(value, strlen(value));
#else
                        dupstr(fAlloc, value, strlen(value));
#endif
            } else {
                attr->fValue =
#ifdef SKIA_OHOS_SVG_PROTECTION
                        dupstrLimited(colorBuffer, strlen(colorBuffer));
#else
                        dupstr(fAlloc, colorBuffer, strlen(colorBuffer));
#endif
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
                attr->fValue =
#ifdef SKIA_OHOS_SVG_PROTECTION
                        dupstrLimited(value, strlen(value));
#else
                        dupstr(fAlloc, value, strlen(value));
#endif
            } else {
                attr->fValue =
#ifdef SKIA_OHOS_SVG_PROTECTION
                        dupstrLimited(opacityBuffer, strlen(opacityBuffer));
#else
                        dupstr(fAlloc, opacityBuffer, strlen(opacityBuffer));
#endif
            }
            return false;
        }
        return true;
    }

    bool onAddAttribute(const char name[], const char value[]) override {
#ifdef SKIA_OHOS_SVG_PROTECTION
        if (fResourceLimitExceeded) {
            return true;
        }
        char* attrName = dupstrLimited(name, strlen(name));
        if (!attrName) {
            return true;
        }
        SkColorEx svgThemeColor;
        svgThemeColor.value = fSvgThemeColor;

        SkDOM::Attr pendingAttr = { attrName, nullptr };
        if (setSVGColor(&pendingAttr, name, value, svgThemeColor)) {
            pendingAttr.fValue = dupstrLimited(value, strlen(value));
        }
        if (!pendingAttr.fValue) {
            return true;
        }
        *fAttrs.append() = pendingAttr;

        // add attributes in style classes.
        if (!strcmp(pendingAttr.fName, "class")) {
            const auto& styleClassMap = fStyleParser.getArributesMap(pendingAttr.fValue);
            if (fSVGResourceLimits && fSVGResourceLimits->fMaxClassFanOut > 0) {
                const size_t maxClassFanOut = fSVGResourceLimits->fMaxClassFanOut;
                if (fClassFanOut > maxClassFanOut ||
                    styleClassMap.size() > maxClassFanOut - fClassFanOut) {
                    const size_t actual = styleClassMap.size() > SIZE_MAX - fClassFanOut
                            ? SIZE_MAX : fClassFanOut + styleClassMap.size();
                    fResourceLimitExceeded = true;
                    SK_LOGE("SVG resource limit exceeded: fMaxClassFanOut "
                            "actual=%{public}zu max=%{public}zu\n",
                            actual, fSVGResourceLimits->fMaxClassFanOut);
                    SK_SVG_RESOURCE_PROTECTION_REPORT();
                    return true;
                }
                fClassFanOut += styleClassMap.size();
            }
            for (auto& arr : styleClassMap) {
                char* classAttrName = dupstrLimited(arr.first.c_str(), arr.first.size());
                if (!classAttrName) {
                    return true;
                }
                SkDOM::Attr pendingClassAttr = { classAttrName, nullptr };
                if (setSVGColor(&pendingClassAttr, classAttrName,
                                arr.second.c_str(), svgThemeColor)) {
                    pendingClassAttr.fValue =
                            dupstrLimited(arr.second.c_str(), arr.second.size());
                }
                if (!pendingClassAttr.fValue) {
                    return true;
                }
                *fAttrs.append() = pendingClassAttr;
            }
        }
        return false;
#else
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
#endif
    }

    bool onEndElement(const char elem[]) override {
#ifdef SKIA_OHOS_SVG_PROTECTION
        if (fResourceLimitExceeded) {
            return true;
        }
#endif
        if (SkDOMParser::onEndElement(elem)) {
            return true;
        }
        if (!strcmp(elem, "style")) {
            fProcessingStyle = false;
        }
#ifdef SKIA_OHOS_SVG_PROTECTION
        return fResourceLimitExceeded;
#else
        return false;
#endif
    }

    static std::string RemoveEmptyChar(const char text[], int len) {
        std::vector<char> textVector(text, text + len);
        std::vector<char> output;
        for (auto i = textVector.begin(); i != textVector.end(); i++) {
            if (!((*i == ' ') || (*i == '\n') || (*i == '\t'))) {
                output.push_back(*i);
            }
        }
        return std::string(output.begin(), output.end());
    }

    bool onText(const char text[], int len) override {
#ifdef SKIA_OHOS_SVG_PROTECTION
        if (fResourceLimitExceeded) {
            return true;
        }
        if (fProcessingStyle && fSVGResourceLimits &&
            fSVGResourceLimits->fMaxStyleTextLen > 0) {
            const size_t textLen = static_cast<size_t>(len > 0 ? len : 0);
            const size_t maxLen = fSVGResourceLimits->fMaxStyleTextLen;
            if (fStyleTextBytes > maxLen || textLen > maxLen - fStyleTextBytes) {
                const size_t actual = textLen > SIZE_MAX - fStyleTextBytes
                        ? SIZE_MAX : fStyleTextBytes + textLen;
                fResourceLimitExceeded = true;
                SK_LOGE("SVG resource limit exceeded: fMaxStyleTextLen "
                        "actual=%{public}zu max=%{public}zu\n",
                        actual, maxLen);
                SK_SVG_RESOURCE_PROTECTION_REPORT();
                return true;
            }
            fStyleTextBytes += textLen;
        }
#endif
        std::string style = RemoveEmptyChar(text, len);
        this->startCommon(style.c_str(), style.size(), SkDOM::kText_Type);
#ifdef SKIA_OHOS_SVG_PROTECTION
        if (fResourceLimitExceeded) {
            return true;
        }
#endif
        this->SkSVGDOMParser::onEndElement(style.c_str());
        if (fProcessingStyle && !style.empty() && style.front() == '.') {
            fStyleParser.parseCssStyle(style);
#ifdef SKIA_OHOS_SVG_PROTECTION
            if (fStyleParser.resourceLimitExceeded()) {
                fResourceLimitExceeded = true;
            }
#endif
        }
#ifdef SKIA_OHOS_SVG_PROTECTION
        return fResourceLimitExceeded;
#else
        return false;
#endif
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
#ifdef SKIA_OHOS_SVG_PROTECTION
    const SkSVGResourceLimits* fSVGResourceLimits = nullptr;
    size_t fStyleTextBytes = 0;
    // Total number of style attributes expanded from all class attributes in this DOM.
    size_t fClassFanOut = 0;
#endif
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

#ifdef SKIA_OHOS_SVG_PROTECTION
const SkSVGXMLDOM::Node* SkSVGXMLDOM::build(
        SkStream& docStream, uint64_t svgThemeColor, const SkSVGResourceLimits& limits) {
    fSvgThemeColor = svgThemeColor;
    SkSVGDOMParser parser(&fAlloc);
    parser.setSVGResourceLimits(&limits);
    if (!parser.parse(docStream, svgThemeColor) || parser.resourceLimitExceeded()) {
        if (parser.resourceLimitExceeded()) {
            SK_LOGE("SVG XML construction failed due to resource limit\n");
        }
        fRoot = nullptr;
        fAlloc.reset();
        return nullptr;
    }
    fRoot = parser.getRoot();
    return fRoot;
}
#endif

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
