/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2023/4/25
 */

#ifndef SkSVGXMLDOM_DEFINED
#define SkSVGXMLDOM_DEFINED

#include "src/xml/SkDOM.h"

class SkSVGXMLDOM : public SkDOM {
public:
    using SkDOMNode = Node;
    using SkDOMAttr = Attr;

    SkSVGXMLDOM() = default;
    ~SkSVGXMLDOM() override = default;

    const Node* build(SkStream& docStream, uint64_t svgThemeColor);

    // override SkDom functions
    const Node* build(SkStream& docStream) override;
    const Node* copy(const SkDOM& dom, const Node* node) override;
    SkXMLParser* beginParsing() override;
private:
    // for pure color svg
    uint64_t fSvgThemeColor {0};
};
#endif
