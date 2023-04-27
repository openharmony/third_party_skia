/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2023/4/25
 */
#ifndef SkDOMPARSER_DEFINED
#define SkDOMPARSER_DEFINED

#include "include/private/SkTDArray.h"
#include "src/core/SkArenaAlloc.h"
#include "src/xml/SkXMLParser.h"

class SkDOMParser : public SkXMLParser {
public:
    SkDOMParser(SkArenaAllocWithReset* chunk) : SkXMLParser(&fParserError), fAlloc(chunk) {
        fAlloc->reset();
        fRoot = nullptr;
        fLevel = 0;
        fNeedToFlush = true;
    }
    SkDOM::Node* getRoot() const;

    static char* dupstr(SkArenaAlloc* chunk, const char src[], size_t srcLen);

    SkXMLParserError fParserError;

protected:
    void flushAttributes();
    bool onStartElement(const char elem[]) override;
    bool onAddAttribute(const char name[], const char value[]) override;
    bool onEndElement(const char elem[]) override;
    bool onText(const char text[], int len) override;
    void startCommon(const char elem[], size_t elemSize, SkDOM::Type type);

    SkTDArray<SkDOM::Node*> fParentStack;
    SkArenaAllocWithReset*  fAlloc;
    SkDOM::Node*            fRoot;
    bool                    fNeedToFlush;

    // state needed for flushAttributes()
    SkTDArray<SkDOM::Attr>  fAttrs;
    char*                   fElemName;
    SkDOM::Type             fElemType;
    int                     fLevel;
};
#endif
