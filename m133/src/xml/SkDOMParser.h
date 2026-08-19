/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2023/4/25
 */
#ifndef SkDOMPARSER_DEFINED
#define SkDOMPARSER_DEFINED

#include "include/private/base/SkTDArray.h"
#include "src/base/SkArenaAlloc.h"
#include "src/xml/SkXMLParser.h"

#ifdef SKIA_OHOS_SVG_PROTECTION
#include "modules/svg/include/SkSVGResourceLimits.h"
#endif

class SkDOMParser : public SkXMLParser {
public:
    SkDOMParser(SkArenaAllocWithReset* chunk) : SkXMLParser(&fParserError), fAlloc(chunk) {
        fAlloc->reset();
        fRoot = nullptr;
        fLevel = 0;
        fNeedToFlush = true;
    }
    SkDOM::Node* getRoot() const;

#ifdef SKIA_OHOS_SVG_PROTECTION
    void setSVGResourceLimits(const SkSVGResourceLimits* limits) { fSVGResourceLimits = limits; }
    bool resourceLimitExceeded() const { return fResourceLimitExceeded; }
#endif

    static char* dupstr(SkArenaAlloc* chunk, const char src[], size_t srcLen);
#ifdef SKIA_OHOS_SVG_PROTECTION
    char* dupstrLimited(const char src[], size_t srcLen);
    bool consumeArenaBytes(size_t bytes);
#endif

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
#ifdef SKIA_OHOS_SVG_PROTECTION
    const SkSVGResourceLimits* fSVGResourceLimits = nullptr;
    size_t                     fNodeCount = 0;
    bool                       fResourceLimitExceeded = false;
    size_t                     fArenaAllocBytes = 0;
#endif
};
#endif
