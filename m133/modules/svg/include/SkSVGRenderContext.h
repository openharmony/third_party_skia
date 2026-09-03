/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGRenderContext_DEFINED
#define SkSVGRenderContext_DEFINED

#include "include/core/SkFontMgr.h"
#include "include/core/SkFourByteTag.h"
#include "include/core/SkM44.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSize.h"
#include "include/core/SkTypes.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_factory.h"
#include "modules/svg/include/SkSVGAttribute.h"
#include "modules/svg/include/SkSVGIDMapper.h"
#include "modules/svg/include/SkSVGNode.h"
#ifdef SKIA_OHOS_SVG_PROTECTION
#include "modules/svg/include/SkSVGResourceLimits.h"
#endif
#include "modules/svg/include/SkSVGTypes.h"
#include "src/base/SkTLazy.h"
#include "src/core/SkTHash.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

static constexpr float DEFAULT_RESIZE_PERCENTAGE = 100;
class SkCanvas;
class SkPaint;
class SkString;
namespace skresources { class ResourceProvider; }

class SK_API SkSVGLengthContext {
public:
    SkSVGLengthContext(const SkSize& viewport, float resizePercentage = DEFAULT_RESIZE_PERCENTAGE, SkScalar dpi = 90)
        : fViewport(viewport), fResizePercentage(resizePercentage), fDPI(dpi) {}

    enum class LengthType {
        kHorizontal,
        kVertical,
        kOther,
    };

    const float& getResizePercentage() const { return fResizePercentage; }
    const SkSize& viewPort() const { return fViewport; }
    void setViewPort(const SkSize& viewport) { fViewport = viewport; }

    SkScalar resolve(const SkSVGLength&, LengthType) const;
    SkScalar resolveForSVG(const SkSVGLength&, LengthType) const;
    SkRect   resolveRect(const SkSVGLength& x, const SkSVGLength& y,
                         const SkSVGLength& w, const SkSVGLength& h) const;
    SkRect   resolveRectForSVG(const SkSVGLength& x, const SkSVGLength& y,
                         const SkSVGLength& w, const SkSVGLength& h) const;

private:
    SkSize   fViewport;
    float  fResizePercentage;
    SkScalar fDPI;
};

struct SK_API SkSVGPresentationContext {
    SkSVGPresentationContext();
    SkSVGPresentationContext(const SkSVGPresentationContext&)            = default;
    SkSVGPresentationContext& operator=(const SkSVGPresentationContext&) = default;

    const skia_private::THashMap<SkString, SkSVGColorType>* fNamedColors = nullptr;

    // Inherited presentation attributes, computed for the current node.
    SkSVGPresentationAttributes fInherited;
};

class SK_API SkSVGRenderContext {
public:
#ifdef SKIA_OHOS_SVG_PROTECTION
    struct ResourceState {
        const SkSVGResourceLimits* fLimits;
        size_t fLayerEffectPixels = 0;
        size_t fRecursionDepth = 0;
        bool fFailed = false;
    };
#endif
    // Captures data required for object bounding box resolution.
    struct OBBScope {
        const SkSVGNode*          fNode;
        const SkSVGRenderContext* fCtx;
    };

    SkSVGRenderContext(SkCanvas*,
                       const sk_sp<SkFontMgr>&,
                       const sk_sp<skresources::ResourceProvider>&,
                       const SkSVGIDMapper&,
                       const SkSVGLengthContext&,
                       const SkSVGPresentationContext&,
                       const OBBScope&,
                       const sk_sp<SkShapers::Factory>&
#ifdef SKIA_OHOS_SVG_PROTECTION
                       , ResourceState* = nullptr
#endif
                       );
    SkSVGRenderContext(const SkSVGRenderContext&);
    SkSVGRenderContext(const SkSVGRenderContext&, SkCanvas*);
    // Establish a new OBB scope.  Normally used when entering a node's render scope.
    SkSVGRenderContext(const SkSVGRenderContext&, const SkSVGNode*);
    ~SkSVGRenderContext();

    const SkSVGLengthContext& lengthContext() const { return *fLengthContext; }
    SkSVGLengthContext* writableLengthContext() { return fLengthContext.writable(); }

    const SkSVGPresentationContext& presentationContext() const { return *fPresentationContext; }

    SkCanvas* canvas() const { return fCanvas; }
    void saveOnce();
#ifdef SKIA_OHOS_SVG_PROTECTION
    bool resourceLimitExceeded() const { return fResourceState && fResourceState->fFailed; }

    // Tracks one actual SVG traversal step. The scope is shared by all render
    // contexts belonging to a render operation, so href expansion and regular
    // child traversal consume the same depth budget.
    class RecursionScope {
    public:
        explicit RecursionScope(const SkSVGRenderContext&);
        RecursionScope(const SkSVGRenderContext&, size_t initialDepth);
        ~RecursionScope();

        explicit operator bool() const { return fValid; }
        bool enter();

    private:
        RecursionScope(const RecursionScope&) = delete;
        RecursionScope& operator=(const RecursionScope&) = delete;

        ResourceState* fState;
        size_t fEnteredDepth = 0;
        bool fValid = true;
    };
#endif

    enum ApplyFlags {
        kLeaf = 1 << 0, // the target node doesn't have descendants
    };
    void applyPresentationAttributes(const SkSVGPresentationAttributes&, uint32_t flags);

    // Scoped wrapper that temporarily clears the original node reference.
    class BorrowedNode {
    public:
        explicit BorrowedNode(sk_sp<SkSVGNode>* node)
            : fOwner(node) {
            if (fOwner) {
                fBorrowed = std::move(*fOwner);
                *fOwner = nullptr;
            }
        }

        ~BorrowedNode() {
            if (fOwner) {
                *fOwner = std::move(fBorrowed);
            }
        }

        const SkSVGNode* get() const { return fBorrowed.get(); }
        const SkSVGNode* operator->() const { return fBorrowed.get(); }
        const SkSVGNode& operator*() const { return *fBorrowed; }

        explicit operator bool() const { return !!fBorrowed; }

    private:
        // noncopyable
        BorrowedNode(const BorrowedNode&)      = delete;
        BorrowedNode& operator=(BorrowedNode&) = delete;

        sk_sp<SkSVGNode>* fOwner;
        sk_sp<SkSVGNode>  fBorrowed;
    };

    // Note: the id->node association is cleared for the lifetime of the returned value
    // (effectively breaks reference cycles, assuming appropriate return value scoping).
    BorrowedNode findNodeById(const SkSVGIRI&) const;

    SkTLazy<SkPaint> fillPaint() const;
    SkTLazy<SkPaint> strokePaint() const;

    SkSVGColorType resolveSvgColor(const SkSVGColor&) const;

    // The local computed clip path (not inherited).
    const SkPath* clipPath() const { return fClipPath.getMaybeNull(); }

    const sk_sp<skresources::ResourceProvider>& resourceProvider() const {
        return fResourceProvider;
    }

    sk_sp<SkFontMgr> fontMgr() const {
        // It is probably an oversight to try to render <text> without having set the SkFontMgr.
        // We will assert this in debug mode, but fallback to an empty fontmgr in release builds.
        SkASSERT(fFontMgr);
        return fFontMgr ? fFontMgr : SkFontMgr::RefDefault();
    }

    // Returns the translate/scale transformation required to map into the current OBB scope,
    // with the specified units.
    struct OBBTransform {
        SkV2 offset, scale;
    };
    OBBTransform transformForCurrentOBB(SkSVGObjectBoundingBoxUnits) const;

    SkRect resolveOBBRect(const SkSVGLength& x, const SkSVGLength& y,
                          const SkSVGLength& w, const SkSVGLength& h,
                          SkSVGObjectBoundingBoxUnits) const;

    const OBBScope& currentOBBScope() const { return fOBBScope; }

    std::unique_ptr<SkShaper> makeShaper() const {
        SkASSERT(fTextShapingFactory);
        return fTextShapingFactory->makeShaper(this->fontMgr());
    }

    std::unique_ptr<SkShaper::BiDiRunIterator> makeBidiRunIterator(const char* utf8,
                                                                   size_t utf8Bytes,
                                                                   uint8_t bidiLevel) const {
        SkASSERT(fTextShapingFactory);
        return fTextShapingFactory->makeBidiRunIterator(utf8, utf8Bytes, bidiLevel);
    }

    std::unique_ptr<SkShaper::ScriptRunIterator> makeScriptRunIterator(const char* utf8,
                                                                       size_t utf8Bytes) const {
        SkASSERT(fTextShapingFactory);
        constexpr SkFourByteTag unknownScript = SkSetFourByteTag('Z', 'z', 'z', 'z');
        return fTextShapingFactory->makeScriptRunIterator(utf8, utf8Bytes, unknownScript);
    }

private:
    // Stack-only
    void* operator new(size_t)                               = delete;
    void* operator new(size_t, void*)                        = delete;
    SkSVGRenderContext& operator=(const SkSVGRenderContext&) = delete;

    void applyOpacity(SkScalar opacity, uint32_t flags, bool hasFilter);
    void applyFilter(const SkSVGFuncIRI&);
    void applyClip(const SkSVGFuncIRI&);
    void applyMask(const SkSVGFuncIRI&);
#ifdef SKIA_OHOS_SVG_PROTECTION
    bool consumeLayerEffectPixels(const SkRect* bounds, size_t layerCount = 1);
#endif

    SkTLazy<SkPaint> commonPaint(const SkSVGPaint&, float opacity) const;

    const sk_sp<SkFontMgr>&                       fFontMgr;
    const sk_sp<SkShapers::Factory>&              fTextShapingFactory;
    const sk_sp<skresources::ResourceProvider>&   fResourceProvider;
    const SkSVGIDMapper&                          fIDMapper;
    SkTCopyOnFirstWrite<SkSVGLengthContext>       fLengthContext;
    SkTCopyOnFirstWrite<SkSVGPresentationContext> fPresentationContext;
    SkCanvas*                                     fCanvas;
    // The save count on 'fCanvas' at construction time.
    // A restoreToCount() will be issued on destruction.
    int                                           fCanvasSaveCount;

    // clipPath, if present for the current context (not inherited).
    SkTLazy<SkPath>                               fClipPath;

    // Deferred opacity optimization for leaf nodes.
    float                                         fDeferredPaintOpacity = 1;

    // Current object bounding box scope.
    const OBBScope                                fOBBScope;
#ifdef SKIA_OHOS_SVG_PROTECTION
    ResourceState* const                          fResourceState;
#endif
};

#endif // SkSVGRenderContext_DEFINED
