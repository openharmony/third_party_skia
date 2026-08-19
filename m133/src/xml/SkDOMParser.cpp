/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: Implementation for Css style parser.
 * Create: 2023/4/25
 */

#include "src/xml/SkDOM.h"
#include "src/xml/SkDOMParser.h"

#ifdef SKIA_OHOS_SVG_PROTECTION
#include "include/core/SkLog.h"
#include "modules/svg/include/SkSVGResourceProtection.h"

#include <limits>
#endif

char* SkDOMParser::dupstr(SkArenaAlloc* chunk, const char src[], size_t srcLen) {
    SkASSERT(chunk && src);
    char* dst = chunk->makeArrayDefault<char>(srcLen + 1);
    memcpy(dst, src, srcLen);
    dst[srcLen] = '\0';
    return dst;
}

#ifdef SKIA_OHOS_SVG_PROTECTION
char* SkDOMParser::dupstrLimited(const char src[], size_t srcLen) {
    if (!fSVGResourceLimits) {
        return dupstr(fAlloc, src, srcLen);
    }
    if (fResourceLimitExceeded) {
        return nullptr;
    }
    if (srcLen == SIZE_MAX) {
        fResourceLimitExceeded = true;
        SK_LOGE("SVG resource limit exceeded: fMaxArenaAllocBytes "
                "actual=%{public}zu max=%{public}zu\n",
                SIZE_MAX, fSVGResourceLimits ? fSVGResourceLimits->fMaxArenaAllocBytes : 0);
        SK_SVG_RESOURCE_PROTECTION_REPORT();
        return nullptr;
    }
    const size_t bytes = srcLen + 1;
    // SkArenaAlloc represents allocation sizes with uint32_t and reserves internal block
    // metadata plus up to one 4 KiB rounding unit. Reject oversized requests before its
    // release assertions can terminate the process.
    constexpr size_t kMaxSingleArenaAllocation =
            static_cast<size_t>(std::numeric_limits<uint32_t>::max()) - (1u << 12) - 64u;
    if (bytes > kMaxSingleArenaAllocation) {
        fResourceLimitExceeded = true;
        SK_LOGE("SVG arena allocation unsupported: "
                "actual=%{public}zu max=%{public}zu\n",
                bytes, kMaxSingleArenaAllocation);
        SK_SVG_RESOURCE_PROTECTION_REPORT();
        return nullptr;
    }
    if (!this->consumeArenaBytes(bytes)) {
        return nullptr;
    }
    return dupstr(fAlloc, src, srcLen);
}

bool SkDOMParser::consumeArenaBytes(size_t bytes) {
    if (fResourceLimitExceeded) {
        return false;
    }
    if (fSVGResourceLimits && fSVGResourceLimits->fMaxArenaAllocBytes > 0 &&
        (fArenaAllocBytes > fSVGResourceLimits->fMaxArenaAllocBytes ||
         bytes > fSVGResourceLimits->fMaxArenaAllocBytes - fArenaAllocBytes)) {
        const size_t actual = bytes > SIZE_MAX - fArenaAllocBytes
                ? SIZE_MAX : fArenaAllocBytes + bytes;
        fResourceLimitExceeded = true;
        SK_LOGE("SVG resource limit exceeded: fMaxArenaAllocBytes "
                "actual=%{public}zu max=%{public}zu\n",
                actual, fSVGResourceLimits->fMaxArenaAllocBytes);
        SK_SVG_RESOURCE_PROTECTION_REPORT();
        return false;
    }
    fArenaAllocBytes += bytes;
    return true;
}
#endif

SkDOM::Node* SkDOMParser::getRoot() const {
    return fRoot;
}

void SkDOMParser::flushAttributes() {
    SkASSERT(fLevel > 0);

#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fResourceLimitExceeded) {
        return;
    }
    if (fSVGResourceLimits) {
        if (fSVGResourceLimits->fMaxDOMNodeCount > 0 &&
            fNodeCount >= fSVGResourceLimits->fMaxDOMNodeCount) {
            const size_t actual = fNodeCount == SIZE_MAX ? SIZE_MAX : fNodeCount + 1;
            fResourceLimitExceeded = true;
            SK_LOGE("SVG resource limit exceeded: fMaxDOMNodeCount "
                    "actual=%{public}zu max=%{public}zu\n",
                    actual, fSVGResourceLimits->fMaxDOMNodeCount);
            SK_SVG_RESOURCE_PROTECTION_REPORT();
            return;
        }
        ++fNodeCount;
    }
#endif

    int attrCount = fAttrs.size();

#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fSVGResourceLimits &&
        (attrCount < 0 || static_cast<size_t>(attrCount) >
                                  std::numeric_limits<uint16_t>::max())) {
        fResourceLimitExceeded = true;
        SK_LOGE("SVG DOM attribute count unsupported: "
                "actual=%{public}d max=%{public}u\n",
                attrCount, static_cast<unsigned>(std::numeric_limits<uint16_t>::max()));
        SK_SVG_RESOURCE_PROTECTION_REPORT();
        return;
    }
    const size_t attrCountSize = static_cast<size_t>(attrCount);
    if (attrCountSize > (SIZE_MAX - sizeof(SkDOM::Node)) / sizeof(SkDOMAttr)) {
        fResourceLimitExceeded = true;
        SK_LOGE("SVG resource limit exceeded: fMaxArenaAllocBytes "
                "actual=%{public}zu max=%{public}zu\n",
                SIZE_MAX, fSVGResourceLimits ? fSVGResourceLimits->fMaxArenaAllocBytes : 0);
        SK_SVG_RESOURCE_PROTECTION_REPORT();
        return;
    }
    const size_t nodeBytes = sizeof(SkDOMAttr) * attrCountSize + sizeof(SkDOM::Node);
    if (!this->consumeArenaBytes(nodeBytes)) {
        return;
    }
#endif

#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fRoot != nullptr && fParentStack.empty()) {
        fResourceLimitExceeded = true;
        SK_SVG_RESOURCE_PROTECTION_REPORT();
        return;
    }
#endif

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
        SkDOM::Node* parent = fParentStack.back();
        SkASSERT(fRoot && parent);
        node->fNextSibling = parent->fFirstChild;
        parent->fFirstChild = node;
    }
    *fParentStack.append() = node;

    sk_careful_memcpy(node->attrs(), fAttrs.begin(), attrCount * sizeof(SkDOM::Attr));
    fAttrs.reset();
}

bool SkDOMParser::onStartElement(const char elem[]) {
#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fResourceLimitExceeded) {
        return true;
    }
#endif
    this->startCommon(elem, strlen(elem), SkDOM::kElement_Type);
#ifdef SKIA_OHOS_SVG_PROTECTION
    return fResourceLimitExceeded;
#else
    return false;
#endif
}

bool SkDOMParser::onAddAttribute(const char name[], const char value[]) {
#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fResourceLimitExceeded) {
        return true;
    }
#endif
#ifdef SKIA_OHOS_SVG_PROTECTION
    char* attrName = dupstrLimited(name, strlen(name));
    if (!attrName) {
        return true;
    }
    char* attrValue = dupstrLimited(value, strlen(value));
    if (!attrValue) {
        return true;
    }
    SkDOM::Attr* attr = fAttrs.append();
    attr->fName = attrName;
    attr->fValue = attrValue;
#else
    SkDOM::Attr* attr = fAttrs.append();
    attr->fName = dupstr(fAlloc, name, strlen(name));
    attr->fValue = dupstr(fAlloc, value, strlen(value));
    return false;
#endif
    return false;
}

bool SkDOMParser::onEndElement(const char elem[]) {
#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fResourceLimitExceeded) {
        return true;
    }
    if (fLevel <= 0) {
        fResourceLimitExceeded = true;
        SK_SVG_RESOURCE_PROTECTION_REPORT();
        return true;
    }
#endif
    --fLevel;
    if (fNeedToFlush)
        flushAttributes();
    fNeedToFlush = false;

#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fResourceLimitExceeded) {
        return true;
    }
    if (fParentStack.empty()) {
        fResourceLimitExceeded = true;
        SK_SVG_RESOURCE_PROTECTION_REPORT();
        return true;
    }
#endif

    SkDOM::Node* parent;

    parent = fParentStack.back();
    fParentStack.pop_back();

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
#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fResourceLimitExceeded) {
        return true;
    }
#endif
    startCommon(text, len, SkDOM::kText_Type);
#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fResourceLimitExceeded) {
        return true;
    }
#endif
    SkDOMParser::onEndElement(fElemName);

    return false;
}

void SkDOMParser::startCommon(const char elem[], size_t elemSize, SkDOM::Type type) {
#ifdef SKIA_OHOS_SVG_PROTECTION
    if (fResourceLimitExceeded) {
        return;
    }
#endif
    if (fLevel > 0 && fNeedToFlush) {
        flushAttributes();
#ifdef SKIA_OHOS_SVG_PROTECTION
        if (fResourceLimitExceeded) {
            return;
        }
#endif
    }
    fElemName =
#ifdef SKIA_OHOS_SVG_PROTECTION
            dupstrLimited(elem, elemSize);
#else
            dupstr(fAlloc, elem, elemSize);
#endif
    if (!fElemName) {
#ifdef SKIA_OHOS_SVG_PROTECTION
        fResourceLimitExceeded = true;
#endif
        return;
    }
    fNeedToFlush = true;
    fElemType = type;
    ++fLevel;
}
