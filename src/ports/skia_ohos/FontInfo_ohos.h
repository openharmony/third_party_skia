// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FONTINFO_OHOS_H
#define FONTINFO_OHOS_H

#include "securec.h"

#include "SkFixed.h"
#include "SkFontDescriptor.h"
#include "SkFontHost_FreeType_common.h"

/*!
 * \brief To manage the font information
 */
struct FontInfo {
public:
    /*! Constructor
     *
     */
    FontInfo() : familyName(""), fname(""), index(0),
        style(SkFontStyle::Normal()), isFixedWidth(false), stream(nullptr)
    {
        memset_s(&axisSet, sizeof(AxisSet), 0, sizeof(AxisSet));
    }
    /*! Copy Constructor
     * \param font an object of FontInfo
     */
    explicit FontInfo(const FontInfo& font)
        : familyName(font.familyName), fname(font.fname), index(font.index),
          style(font.style), isFixedWidth(font.isFixedWidth), stream(nullptr)
    {
        axisSet.axis = font.axisSet.axis;
        axisSet.range = font.axisSet.range;
        if (font.stream) {
            stream = font.stream->duplicate();
        }
    }

    /*! Move Constructor
     * \param font an object of FontInfo
     */
    explicit FontInfo(FontInfo&& font)
        : familyName(std::move(font.familyName)), fname(std::move(font.fname)), index(font.index),
          style(font.style), isFixedWidth(font.isFixedWidth), stream(nullptr)
    {
        axisSet.axis = std::move(font.axisSet.axis);
        axisSet.range = std::move(font.axisSet.range);
        if (font.stream) {
            stream = std::move(font.stream);
        }
    }

    /*! Constructor
     * \param fname the fullname of font file
     * \param index the index of the typeface in the font file
     */
    FontInfo(const char* fname, int index)
        : familyName(""), fname(""), index(index),
          style(SkFontStyle::Normal()), isFixedWidth(false), stream(nullptr)
    {
        if (fname) {
            this->fname.set(fname);
        }
        memset_s(&axisSet, sizeof(axisSet), 0, sizeof(axisSet));
    }

    /*! Destructor
     *
     */
    virtual ~FontInfo() = default;

    /*! Copy assignment operator
     * \param font an object of FontInfo
     */
    FontInfo& operator = (const FontInfo& font)
    {
        if (this == &font) {
            return *this;
        }
        familyName = font.familyName;
        fname = font.fname;
        index = font.index;
        style = font.style;
        isFixedWidth = font.isFixedWidth;
        axisSet.axis = font.axisSet.axis;
        axisSet.range = font.axisSet.range;
        if (font.stream) {
            stream = font.stream->duplicate();
        }
        return *this;
    }

    /*! The move assignment operator
     * \param font an object of FontInfo
     */
    FontInfo& operator = (FontInfo&& font)
    {
        if (this == &font) {
            return *this;
        }
        familyName = std::move(font.familyName);
        fname = std::move(font.fname);
        index = font.index;
        style = font.style;
        isFixedWidth = font.isFixedWidth;
        axisSet.axis = std::move(font.axisSet.axis);
        axisSet.range = std::move(font.axisSet.range);
        if (font.stream) {
            stream = std::move(font.stream);
        }
        return *this;
    }

    /*! To set axis values
     * \param count the count of axis
     * \param axis an array of SkFixed value
     * \param range an array of AxisDefinition
     */
    void setAxisSet(int count, const SkFixed* axis,
        const SkTypeface_FreeType::Scanner::AxisDefinition* range)
    {
        axisSet.axis.clear();
        axisSet.range.clear();
        for (int i = 0; i < count; i++) {
            axisSet.axis.emplace_back(axis[i]);
            axisSet.range.emplace_back(range[i]);
        }
    }

    SkString familyName;  // the real family name of the font
    SkString fname; // the full name of font file
    int index; // the index of the font in a ttc font
    SkFontStyle style; // the font style
    bool isFixedWidth; // the flag to indicate if the font has fixed width or not
    /*!
     * \brief To manage the axis values for variable font
     */
    struct AxisSet {
        std::vector<SkFixed> axis;  // the axis values
        std::vector<SkTypeface_FreeType::Scanner::AxisDefinition> range; // the axis ranges
    } axisSet; // the axis values for a variable font
    std::unique_ptr<SkStreamAsset> stream; // the data stream of font file
};

#endif /* FONTINFO_OHOS_H */
