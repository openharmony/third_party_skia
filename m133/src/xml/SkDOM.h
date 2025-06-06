/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkDOM_DEFINED
#define SkDOM_DEFINED

#include "include/core/SkScalar.h"
#include "include/core/SkTypes.h"
#include "include/private/base/SkNoncopyable.h"
#include "include/private/base/SkTemplates.h"
#include "src/base/SkArenaAlloc.h"

class SkDOMParser;
class SkStream;
class SkXMLParser;

struct SkDOMAttr {
    const char* fName;
    const char* fValue;
};

struct SkDOMNode {
    const char* fName;
    SkDOMNode*  fFirstChild;
    SkDOMNode*  fNextSibling;
    SkDOMAttr*  fAttrs;
    uint16_t    fAttrCount;
    uint8_t     fType;
    uint8_t     fPad;

    const SkDOMAttr* attrs() const {
        return fAttrs;
    }

    SkDOMAttr* attrs() {
        return fAttrs;
    }
};

class SK_API SkDOM : public SkNoncopyable {
public:
    SkDOM();
    virtual ~SkDOM();

    typedef SkDOMNode Node;
    typedef SkDOMAttr Attr;
    static void walk_dom(const SkDOM& dom, const SkDOM::Node* node, SkXMLParser* parser);

    /** Returns null on failure
    */
    virtual const Node* build(SkStream&);

    virtual const Node* copy(const SkDOM& dom, const Node* node);
    virtual SkXMLParser* beginParsing();
    virtual const Node* finishParsing();

    const Node* getRootNode() const;

    enum Type {
        kElement_Type,
        kText_Type
    };
    Type getType(const Node*) const;

    const char* getName(const Node*) const;
    const Node* getFirstChild(const Node*, const char elem[] = nullptr) const;
    const Node* getNextSibling(const Node*, const char elem[] = nullptr) const;

    const char* findAttr(const Node*, const char attrName[]) const;
    const Attr* getFirstAttr(const Node*) const;
    const Attr* getNextAttr(const Node*, const Attr*) const;
    const char* getAttrName(const Node*, const Attr*) const;
    const char* getAttrValue(const Node*, const Attr*) const;

    // helpers for walking children
    int countChildren(const Node* node, const char elem[] = nullptr) const;

    // helpers for calling SkParse
    bool findS32(const Node*, const char name[], int32_t* value) const;
    bool findScalars(const Node*, const char name[], SkScalar value[], int count) const;
    bool findHex(const Node*, const char name[], uint32_t* value) const;
    bool findBool(const Node*, const char name[], bool*) const;
    int  findList(const Node*, const char name[], const char list[]) const;

    bool findScalar(const Node* node, const char name[], SkScalar value[]) const {
        return this->findScalars(node, name, value, 1);
    }

    bool hasAttr(const Node*, const char name[], const char value[]) const;
    bool hasS32(const Node*, const char name[], int32_t value) const;
    bool hasScalar(const Node*, const char name[], SkScalar value) const;
    bool hasHex(const Node*, const char name[], uint32_t value) const;
    bool hasBool(const Node*, const char name[], bool value) const;

    class AttrIter {
    public:
        AttrIter(const SkDOM&, const Node*);
        const char* next(const char** value);
    private:
        const Attr* fAttr;
        const Attr* fStop;
    };

protected:
    SkArenaAllocWithReset        fAlloc;
    Node*                        fRoot;
    std::unique_ptr<SkDOMParser> fParser;

    using INHERITED = SkNoncopyable;
};

#endif
