/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkStubHeifDecoderAPI_DEFINED
#define SkStubHeifDecoderAPI_DEFINED

// This stub implementation of HeifDecoderAPI.h lets us compile SkHeifCodec.cpp
// even when libheif is not available.  It, of course, does nothing and fails to decode.

#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <vector>

enum SkHeifColorFormat {
    kHeifColorFormat_RGB565,
    kHeifColorFormat_RGBA_8888,
    kHeifColorFormat_BGRA_8888,
    kHeifColorFormat_NV12,
    kHeifColorFormat_NV21,
    kHeifColorFormat_RGBA_1010102,
    kHeifColorFormat_P010_NV12,
    kHeifColorFormat_P010_NV21,
};

struct HeifStream {
    virtual ~HeifStream() {}

    virtual size_t read(void*, size_t) = 0;
    virtual bool   rewind()            = 0;
    virtual bool   seek(size_t)        = 0;
    virtual bool   hasLength() const   = 0;
    virtual size_t getLength() const   = 0;
    virtual bool   hasPosition() const = 0;
    virtual size_t getPosition() const = 0;
};

struct HeifNclxColor {
    uint16_t colorPrimaries;
    uint16_t transferCharacteristics;
    uint16_t matrixCoefficients;
    uint8_t fullRangeFlag;
};

struct HeifFrameInfo {
    uint32_t mWidth;
    uint32_t mHeight;
    int32_t  mRotationAngle;           // Rotation angle, clockwise, should be multiple of 90
    uint32_t mBytesPerPixel;           // Number of bytes for one pixel
    int64_t mDurationUs;               // Duration of the frame in us
    std::vector<uint8_t> mIccData;     // ICC data array
    bool hasNclxColor = false;
    HeifNclxColor nclxColor;
};

enum class HeifImageHdrType {
    UNKNOWN = 0,
    VIVID_DUAL = 1,
    ISO_DUAL,
    VIVID_SINGLE,
    ISO_SINGLE,
};

struct HeifDecoder {
    HeifDecoder() {}

    virtual ~HeifDecoder() {}

    virtual bool init(HeifStream* stream, HeifFrameInfo* frameInfo) = 0;

    virtual bool getSequenceInfo(HeifFrameInfo* frameInfo, size_t *frameCount) = 0;

    virtual bool decode(HeifFrameInfo* frameInfo) = 0;

    virtual bool decodeSequence(int frameIndex, HeifFrameInfo* frameInfo) = 0;

    virtual bool setOutputColor(SkHeifColorFormat colorFormat) = 0;

    virtual void setDstBuffer(uint8_t *dstBuffer, size_t rowStride, void *context) = 0;

    virtual bool getScanline(uint8_t* dst) = 0;

    virtual size_t skipScanlines(int count) = 0;
    virtual bool getImageInfo(HeifFrameInfo *frameInfo) = 0;
    virtual bool decodeGainmap() = 0;
    virtual void setGainmapDstBuffer(uint8_t* dstBuffer, size_t rowStride) = 0;
    virtual bool getGainmapInfo(HeifFrameInfo* frameInfo) = 0;
    virtual bool getTmapInfo(HeifFrameInfo* frameInfo) = 0;
    virtual HeifImageHdrType getHdrType() = 0;
    virtual void getVividMetadata(std::vector<uint8_t>& uwaInfo, std::vector<uint8_t>& displayInfo,
        std::vector<uint8_t>& lightInfo) = 0;
    virtual void getISOMetadata(std::vector<uint8_t>& isoMetadata) = 0;
    virtual void getErrMsg(std::string& errMsg) = 0;
};

struct StubHeifDecoder : HeifDecoder {
    bool init(HeifStream* stream, HeifFrameInfo* frameInfo) override {
        delete stream;
        return false;
    }

    bool getSequenceInfo(HeifFrameInfo* frameInfo, size_t *frameCount) override {
        return false;
    }

    bool decode(HeifFrameInfo* frameInfo) override {
        return false;
    }

    bool decodeSequence(int frameIndex, HeifFrameInfo* frameInfo) override {
        return false;
    }

    bool setOutputColor(SkHeifColorFormat colorFormat) override {
        return false;
    }

    void setDstBuffer(uint8_t *dstBuffer, size_t rowStride, void *context) override {
        return;
    }

    bool getScanline(uint8_t* dst) override {
        return false;
    }

    size_t skipScanlines(int count) override {
        return 0;
    }

    bool getImageInfo(HeifFrameInfo *frameInfo) override {
        return false;
    }

    bool decodeGainmap() override {
        return false;
    }

    void setGainmapDstBuffer(uint8_t* dstBuffer, size_t rowStride) override {
        return;
    }

    bool getGainmapInfo(HeifFrameInfo* frameInfo) override {
        return false;
    }

    bool getTmapInfo(HeifFrameInfo* frameInfo) override {
        return false;
    } 

    HeifImageHdrType getHdrType() override {
        return HeifImageHdrType::UNKNOWN;
    }

    void getVividMetadata(std::vector<uint8_t>& uwaInfo, std::vector<uint8_t>& displayInfo,
        std::vector<uint8_t>& lightInfo) override {
        return;
    }

    void getISOMetadata(std::vector<uint8_t>& isoMetadata) override {
        return;
    }

    void getErrMsg(std::string& errMsg) override {
        return;
    }
};

#endif//SkStubHeifDecoderAPI_DEFINED
