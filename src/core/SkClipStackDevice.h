/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkClipStackDevice_DEFINED
#define SkClipStackDevice_DEFINED

#include "include/core/SkBlurTypes.h"
#include "src/core/SkClipStack.h"
#include "src/core/SkDevice.h"

class SkClipStackDevice : public SkBaseDevice {
public:
    SkClipStackDevice(const SkImageInfo& info, const SkSurfaceProps& props)
        : SkBaseDevice(info, props)
        , fClipStack(fStorage, sizeof(fStorage))
    {}

    SkClipStack& cs() { return fClipStack; }
    const SkClipStack& cs() const { return fClipStack; }

protected:
    void onSave() override;
    void onRestore() override;
    void onClipRect(const SkRect& rect, SkClipOp, bool aa) override;
    void onClipRRect(const SkRRect& rrect, SkClipOp, bool aa) override;
    void onClipPath(const SkPath& path, SkClipOp, bool aa) override;
    void onClipShader(sk_sp<SkShader>) override;
    void onClipRegion(const SkRegion& deviceRgn, SkClipOp) override;
    void onReplaceClip(const SkIRect& rect) override;
    bool onClipIsAA() const override;
    bool onClipIsWideOpen() const override;
    void onAsRgnClip(SkRegion*) const override;
    ClipType onGetClipType() const override;
    SkIRect onDevClipBounds() const override;

    bool drawBlurImage(const SkImage* image, const SkBlurArg& blurArg) override { return false; }

private:
    enum {
        kPreallocCount = 16 // empirically determined, adjust as needed to reduce mallocs
    };
    intptr_t fStorage[kPreallocCount * sizeof(SkClipStack::Element) / sizeof(intptr_t)];
    SkClipStack fClipStack;

    using INHERITED = SkBaseDevice;
};

#endif
