/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2023/4/25
 */

#include "src/xml/SkDOM.h"
#include "src/xml/SkDOMParser.h"

char* SkDOMParser::dupstr(SkArenaAlloc* chunk, const char src[], size_t srcLen) {
    SkASSERT(chunk && src);
    char* dst = chunk->makeArrayDefault<char>(srcLen + 1);
    memcpy(dst, src, srcLen);
    dst[srcLen] = '\0';
    return dst;
}

SkDOM::Node* SkDOMParser::getRoot() const {
    return fRoot;
}

void SkDOMParser::flushAttributes() {
    SkASSERT(fLevel > 0);

    int attrCount = fAttrs.count();

    SkDOMAttr* attrs = fAlloc->makeArrayDefault<SkDOMAttr>(attrCount);
    SkDOM::Node* node = fAlloc->make<SkDOM::Node>();

    node->fName = fElemName;
    node->fFirstChild = nullptr;
    node->fAttrCount = SkToU16(attrCount);
    node->fAttrs = attrs;
    node->fType = fElemType;

    if (fRoot == nullptr) {
        node->fNextSibling = nullptr;
        fRoot = node;
    } else { // this adds siblings in reverse order. gets corrected in onEndElement()
        SkDOM::Node* parent = fParentStack.top();
        SkASSERT(fRoot && parent);
        node->fNextSibling = parent->fFirstChild;
        parent->fFirstChild = node;
    }
    *fParentStack.push() = node;

    sk_careful_memcpy(node->attrs(), fAttrs.begin(), attrCount * sizeof(SkDOM::Attr));
    fAttrs.reset();
}

bool SkDOMParser::onStartElement(const char elem[]) {
    this->startCommon(elem, strlen(elem), SkDOM::kElement_Type);
    return false;
}

bool SkDOMParser::onAddAttribute(const char name[], const char value[]) {
    SkDOM::Attr* attr = fAttrs.append();
    attr->fName = dupstr(fAlloc, name, strlen(name));
    attr->fValue = dupstr(fAlloc, value, strlen(value));
    return false;
}

bool SkDOMParser::onEndElement(const char elem[]) {
    --fLevel;
    if (fNeedToFlush)
        flushAttributes();
    fNeedToFlush = false;

    SkDOM::Node* parent;

    fParentStack.pop(&parent);

    SkDOM::Node* child = parent->fFirstChild;
    SkDOM::Node* prev = nullptr;
    while (child) {
        SkDOM::Node* next = child->fNextSibling;
        child->fNextSibling = prev;
        prev = child;
        child = next;
    }
    parent->fFirstChild = prev;
    return false;
}

bool SkDOMParser::onText(const char text[], int len) {
    startCommon(text, len, SkDOM::kText_Type);
    SkDOMParser::onEndElement(fElemName);

    return false;
}

void SkDOMParser::startCommon(const char elem[], size_t elemSize, SkDOM::Type type) {
    if (fLevel > 0 && fNeedToFlush) {
        flushAttributes();
    }
    fNeedToFlush = true;
    fElemName = dupstr(fAlloc, elem, elemSize);
    fElemType = type;
    ++fLevel;
}