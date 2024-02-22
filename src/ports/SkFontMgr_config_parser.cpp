/*
 * Copyright 2011 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * 2021.9.9 SkFontMgr_config_parser on previewer of ohos.
 *           Copyright (c) 2021 Huawei Device Co., Ltd. All rights reserved.
 */

// Despite the name and location, this is portable code.

#include "src/ports/SkFontMgr_config_parser.h"
#include "src/ports/skia_ohos/HmSymbolConfig_ohos.h"

#include <expat.h>

#include "include/core/SkStream.h"
#include "include/private/SkFixed.h"
#include "src/core/SkTSearch.h"

#define SK_FONT_CONFIG_FILE_NAME "fonts.xml"
std::string SkFontMgr::containerFontPath = "";

namespace {

std::string g_lmpSystemFontsFile = "INVALID_FILE_PATH";

}

/**
 * This parser file for android contains TWO 'familyset' handlers:
 * One for JB and earlier and the other for LMP and later.
 * For previewer, we only use the fonts.xml in LMP, so the LMP 'familyset' handler is only needed.
 */

struct FamilyData;

struct TagHandler {
    /** Called at the start tag.
     *  Called immediately after the parent tag retuns this handler from a call to 'tag'.
     *  Allows setting up for handling the tag content and processing attributes.
     *  If nullptr, will not be called.
     */
    void (*start)(FamilyData* data, const char* tag, const char** attributes);

    /** Called at the end tag.
     *  Allows post-processing of any accumulated information.
     *  This will be the last call made in relation to the current tag.
     *  If nullptr, will not be called.
     */
    void (*end)(FamilyData* data, const char* tag);

    /** Called when a nested tag is encountered.
     *  This is responsible for determining how to handle the tag.
     *  If the tag is not recognized, return nullptr to skip the tag.
     *  If nullptr, all nested tags will be skipped.
     */
    const TagHandler* (*tag)(FamilyData* data, const char* tag, const char** attributes);

    /** The character handler for this tag.
     *  This is only active for character data contained directly in this tag (not sub-tags).
     *  The first parameter will be castable to a FamilyData*.
     *  If nullptr, any character data in this tag will be ignored.
     */
    XML_CharacterDataHandler chars;
};

/** Represents the current parsing state. */
struct FamilyData {
    FamilyData(XML_Parser parser, SkTDArray<FontFamily*>& families,
               const SkString& basePath, bool isFallback, const char* filename,
               const TagHandler* topLevelHandler)
        : fParser(parser)
        , fFamilies(families)
        , fCurrentFamily(nullptr)
        , fCurrentFontInfo(nullptr)
        , fVersion(0)
        , fBasePath(basePath)
        , fIsFallback(isFallback)
        , fFilename(filename)
        , fDepth(1)
        , fSkip(0)
        , fHandler(&topLevelHandler, 1)
    { }

    XML_Parser fParser;                         // The expat parser doing the work, owned by caller
    SkTDArray<FontFamily*>& fFamilies;          // The array to append families, owned by caller
    std::unique_ptr<FontFamily> fCurrentFamily; // The family being created, owned by this
    FontFileInfo* fCurrentFontInfo;             // The info being created, owned by fCurrentFamily
    int fVersion;                               // The version of the file parsed.
    const SkString& fBasePath;                  // The current base path.
    const bool fIsFallback;                     // The file being parsed is a fallback file
    const char* fFilename;                      // The name of the file currently being parsed.

    int fDepth;                                 // The current element depth of the parse.
    int fSkip;                                  // The depth to stop skipping, 0 if not skipping.
    SkTDArray<const TagHandler*> fHandler;      // The stack of current tag handlers.
};

static bool memeq(const char *s1, const char *s2, size_t n1, size_t n2)
{
    return n1 == n2 && 0 == memcmp(s1, s2, n1);
}
#define MEMEQ(c, s, n) memeq(c, s, sizeof(c) - 1, n)

#define ATTS_NON_NULL(a, i) (a[i] != nullptr && a[i + 1] != nullptr)

#define SK_FONTMGR_CONFIG_PARSER_PREFIX "[SkFontMgr Config Parser] "

#define SK_FONTCONFIGPARSER_WARNING(message, ...)                                                 \
    SkDebugf(SK_FONTMGR_CONFIG_PARSER_PREFIX "%s:%d:%d: warning: " message "\n", self->fFilename, \
             XML_GetCurrentLineNumber(self->fParser), XML_GetCurrentColumnNumber(self->fParser), ##__VA_ARGS__)

static bool is_whitespace(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static void trim_string(SkString* s)
{
    char* str = s->writable_str();
    const char* start = str;  // start is inclusive
    const char* end = start + s->size();  // end is exclusive
    while (is_whitespace(*start)) { ++start; }
    if (start != end) {
        --end;  // make end inclusive
        while (is_whitespace(*end)) {
            --end;
        }
        ++end;  // make end exclusive
    }
    size_t len = end - start;
    memmove(str, start, len);
    s->resize(len);
}

namespace lmpParser {

static const TagHandler axisHandler = {
    /*start*/[](FamilyData* self, const char* tag, const char** attributes) {
        FontFileInfo& file = *self->fCurrentFontInfo;
        SkFourByteTag axisTag = SkSetFourByteTag('\0', '\0', '\0', '\0');
        SkFixed axisStyleValue = 0;
        bool axisTagIsValid = false;
        bool axisStyleValueIsValid = false;
        for (size_t i = 0; ATTS_NON_NULL(attributes, i); i += 2) {
            const char* name = attributes[i];
            const char* value = attributes[i + 1];
            size_t nameLen = strlen(name);
            if (MEMEQ("tag", name, nameLen)) {
                size_t valueLen = strlen(value);
                if (valueLen == 4) {
                    axisTag = SkSetFourByteTag(value[0], value[1], value[2], value[3]);
                    axisTagIsValid = true;
                    for (int j = 0; j < file.fVariationDesignPosition.count() - 1; ++j) {
                        if (file.fVariationDesignPosition[j].axis == axisTag) {
                            axisTagIsValid = false;
                            SK_FONTCONFIGPARSER_WARNING("'%c%c%c%c' axis specified more than once",
                                                        (axisTag >> 24) & 0xFF,
                                                        (axisTag >> 16) & 0xFF,
                                                        (axisTag >>  8) & 0xFF,
                                                        (axisTag      ) & 0xFF);
                        }
                    }
                } else {
                    SK_FONTCONFIGPARSER_WARNING("'%s' is an invalid axis tag", value);
                }
            } else if (MEMEQ("stylevalue", name, nameLen)) {
                if (parse_fixed<16>(value, &axisStyleValue)) {
                    axisStyleValueIsValid = true;
                } else {
                    SK_FONTCONFIGPARSER_WARNING("'%s' is an invalid axis stylevalue", value);
                }
            }
        }
        if (axisTagIsValid && axisStyleValueIsValid) {
            auto& coordinate = file.fVariationDesignPosition.push_back();
            coordinate.axis = axisTag;
            coordinate.value = SkFixedToScalar(axisStyleValue);
        }
    },
    /*end*/nullptr,
    /*tag*/nullptr,
    /*chars*/nullptr,
};

static const TagHandler fontHandler = {
    /*start*/[](FamilyData* self, const char* tag, const char** attributes) {
        // 'weight' (non-negative integer) [default 0]
        // 'style' ("normal", "italic") [default "auto"]
        // 'index' (non-negative integer) [default 0]
        // The character data should be a filename.
        FontFileInfo& file = self->fCurrentFamily->fFonts.push_back();
        self->fCurrentFontInfo = &file;
        SkString fallbackFor;
        for (size_t i = 0; ATTS_NON_NULL(attributes, i); i += 2) {
            const char* name = attributes[i];
            const char* value = attributes[i + 1];
            size_t nameLen = strlen(name);
            if (MEMEQ("weight", name, nameLen)) {
                if (!parse_non_negative_integer(value, &file.fWeight)) {
                    SK_FONTCONFIGPARSER_WARNING("'%s' is an invalid weight", value);
                }
            } else if (MEMEQ("style", name, nameLen)) {
                size_t valueLen = strlen(value);
                if (MEMEQ("normal", value, valueLen)) {
                    file.fStyle = FontFileInfo::Style::kNormal;
                } else if (MEMEQ("italic", value, valueLen)) {
                    file.fStyle = FontFileInfo::Style::kItalic;
                }
            } else if (MEMEQ("index", name, nameLen)) {
                if (!parse_non_negative_integer(value, &file.fIndex)) {
                    SK_FONTCONFIGPARSER_WARNING("'%s' is an invalid index", value);
                }
            } else if (MEMEQ("fallbackFor", name, nameLen)) {
                /** fallbackFor specifies a family fallback and should have been on family. */
                fallbackFor = value;
            }
        }
        if (!fallbackFor.isEmpty()) {
            std::unique_ptr<FontFamily>* fallbackFamily =
                self->fCurrentFamily->fallbackFamilies.find(fallbackFor);
            if (!fallbackFamily) {
                std::unique_ptr<FontFamily> newFallbackFamily(
                    new FontFamily(self->fCurrentFamily->fBasePath, true));
                fallbackFamily = self->fCurrentFamily->fallbackFamilies.set(
                    fallbackFor, std::move(newFallbackFamily));
                (*fallbackFamily)->fLanguages = self->fCurrentFamily->fLanguages;
                (*fallbackFamily)->fVariant = self->fCurrentFamily->fVariant;
                (*fallbackFamily)->fOrder = self->fCurrentFamily->fOrder;
                (*fallbackFamily)->fFallbackFor = fallbackFor;
            }
            self->fCurrentFontInfo = &(*fallbackFamily)->fFonts.emplace_back(file);
            self->fCurrentFamily->fFonts.pop_back();
        }
    },
    /*end*/[](FamilyData* self, const char* tag) {
        trim_string(&self->fCurrentFontInfo->fFileName);
    },
    /*tag*/[](FamilyData* self, const char* tag, const char** attributes) -> const TagHandler* {
        size_t len = strlen(tag);
        if (MEMEQ("axis", tag, len)) {
            return &axisHandler;
        }
        return nullptr;
    },
    /*chars*/[](void* data, const char* s, int len) {
        FamilyData* self = static_cast<FamilyData*>(data);
        self->fCurrentFontInfo->fFileName.append(s, len);
    }
};

static const TagHandler familyHandler = {
    /*start*/[](FamilyData* self, const char* tag, const char** attributes) {
        // 'name' (string) [optional]
        // 'lang' (space separated string) [default ""]
        // 'variant' ("elegant", "compact") [default "default"]
        // If there is no name, this is a fallback only font.
        FontFamily* family = new FontFamily(self->fBasePath, true);
        self->fCurrentFamily.reset(family);
        for (size_t i = 0; ATTS_NON_NULL(attributes, i); i += 2) {
            const char* name = attributes[i];
            const char* value = attributes[i + 1];
            size_t nameLen = strlen(name);
            size_t valueLen = strlen(value);
            if (MEMEQ("name", name, nameLen)) {
                SkAutoAsciiToLC tolc(value);
                family->fNames.push_back().set(tolc.lc());
                family->fIsFallbackFont = false;
            } else if (MEMEQ("lang", name, nameLen)) {
                size_t i = 0;
                while (true) {
                    for (; i < valueLen && is_whitespace(value[i]); ++i) { }
                    if (i == valueLen) {
                        break;
                    }
                    size_t j;
                    for (j = i + 1; j < valueLen && !is_whitespace(value[j]); ++j) { }
                    family->fLanguages.emplace_back(value + i, j - i);
                    i = j;
                    if (i == valueLen) {
                        break;
                    }
                }
            } else if (MEMEQ("variant", name, nameLen)) {
                if (MEMEQ("elegant", value, valueLen)) {
                    family->fVariant = kElegant_FontVariant;
                } else if (MEMEQ("compact", value, valueLen)) {
                    family->fVariant = kCompact_FontVariant;
                }
            }
        }
    },
    /*end*/[](FamilyData* self, const char* tag) {
        *self->fFamilies.append() = self->fCurrentFamily.release();
    },
    /*tag*/[](FamilyData* self, const char* tag, const char** attributes) -> const TagHandler* {
        size_t len = strlen(tag);
        if (MEMEQ("font", tag, len)) {
            return &fontHandler;
        }
        return nullptr;
    },
    /*chars*/nullptr,
};

static FontFamily* find_family(FamilyData* self, const SkString& familyName)
{
    for (int i = 0; i < self->fFamilies.count(); i++) {
        FontFamily* candidate = self->fFamilies[i];
        for (int j = 0; j < candidate->fNames.count(); j++) {
            if (candidate->fNames[j] == familyName) {
                return candidate;
            }
        }
    }
    return nullptr;
}

static const TagHandler aliasHandler = {
    /*start*/[](FamilyData* self, const char* tag, const char** attributes) {
        // 'name' (string) introduces a new family name.
        // 'to' (string) specifies which (previous) family to alias
        // 'weight' (non-negative integer) [optional]
        // If it *does not* have a weight, 'name' is an alias for the entire 'to' family.
        // If it *does* have a weight, 'name' is a new family consisting of
        // the font(s) with 'weight' from the 'to' family.

        SkString aliasName;
        SkString to;
        int weight = 0;
        for (size_t i = 0; ATTS_NON_NULL(attributes, i); i += 2) {
            const char* name = attributes[i];
            const char* value = attributes[i + 1];
            size_t nameLen = strlen(name);
            if (MEMEQ("name", name, nameLen)) {
                SkAutoAsciiToLC tolc(value);
                aliasName.set(tolc.lc());
            } else if (MEMEQ("to", name, nameLen)) {
                to.set(value);
            } else if (MEMEQ("weight", name, nameLen)) {
                if (!parse_non_negative_integer(value, &weight)) {
                    SK_FONTCONFIGPARSER_WARNING("'%s' is an invalid weight", value);
                }
            }
        }

        // Assumes that the named family is already declared
        FontFamily* targetFamily = find_family(self, to);
        if (!targetFamily) {
            SK_FONTCONFIGPARSER_WARNING("'%s' alias target not found", to.c_str());
            return;
        }

        if (weight) {
            FontFamily* family = new FontFamily(targetFamily->fBasePath, self->fIsFallback);
            family->fNames.push_back().set(aliasName);

            for (int i = 0; i < targetFamily->fFonts.count(); i++) {
                if (targetFamily->fFonts[i].fWeight == weight) {
                    family->fFonts.push_back(targetFamily->fFonts[i]);
                }
            }
            *self->fFamilies.append() = family;
        } else {
            targetFamily->fNames.push_back().set(aliasName);
        }
    },
    /*end*/nullptr,
    /*tag*/nullptr,
    /*chars*/nullptr,
};

static const TagHandler familySetHandler = {
    /*start*/[](FamilyData* self, const char* tag, const char** attributes) { },
    /*end*/nullptr,
    /*tag*/[](FamilyData* self, const char* tag, const char** attributes) -> const TagHandler* {
        size_t len = strlen(tag);
        if (MEMEQ("family", tag, len)) {
            return &familyHandler;
        } else if (MEMEQ("alias", tag, len)) {
            return &aliasHandler;
        }
        return nullptr;
    },
    /*chars*/nullptr,
};

} // namespace lmpParser

static const TagHandler topLevelHandler = {
    /*start*/nullptr,
    /*end*/nullptr,
    /*tag*/[](FamilyData* self, const char* tag, const char** attributes) -> const TagHandler* {
        size_t len = strlen(tag);
        if (MEMEQ("familyset", tag, len)) {
            // 'version' (non-negative integer) [default 0]
            for (size_t i = 0; ATTS_NON_NULL(attributes, i); i += 2) {
                const char* name = attributes[i];
                size_t nameLen = strlen(name);
                if (MEMEQ("version", name, nameLen)) {
                    const char* value = attributes[i + 1];
                    if (parse_non_negative_integer(value, &self->fVersion)) {
                        if (self->fVersion >= 21) {
                            return &lmpParser::familySetHandler;
                        }
                    }
                }
            }
        }
        return nullptr;
    },
    /*chars*/ nullptr,
};

static void XMLCALL start_element_handler(void *data, const char *tag, const char **attributes)
{
    FamilyData* self = static_cast<FamilyData*>(data);

    if (!self->fSkip) {
        const TagHandler* parent = self->fHandler.top();
        const TagHandler* child = parent->tag ? parent->tag(self, tag, attributes) : nullptr;
        if (child) {
            if (child->start) {
                child->start(self, tag, attributes);
            }
            self->fHandler.push_back(child);
            XML_SetCharacterDataHandler(self->fParser, child->chars);
        } else {
            SK_FONTCONFIGPARSER_WARNING("'%s' tag not recognized, skipping", tag);
            XML_SetCharacterDataHandler(self->fParser, nullptr);
            self->fSkip = self->fDepth;
        }
    }

    ++self->fDepth;
}

static void XMLCALL end_element_handler(void* data, const char* tag)
{
    FamilyData* self = static_cast<FamilyData*>(data);
    --self->fDepth;

    if (!self->fSkip) {
        const TagHandler* child = self->fHandler.top();
        if (child->end) {
            child->end(self, tag);
        }
        self->fHandler.pop();
        const TagHandler* parent = self->fHandler.top();
        XML_SetCharacterDataHandler(self->fParser, parent->chars);
    }

    if (self->fSkip == self->fDepth) {
        self->fSkip = 0;
        const TagHandler* parent = self->fHandler.top();
        XML_SetCharacterDataHandler(self->fParser, parent->chars);
    }
}

static void XMLCALL xml_entity_decl_handler(void *data,
                                            const XML_Char *entityName,
                                            int is_parameter_entity,
                                            const XML_Char *value,
                                            int value_length,
                                            const XML_Char *base,
                                            const XML_Char *systemId,
                                            const XML_Char *publicId,
                                            const XML_Char *notationName)
{
    FamilyData* self = static_cast<FamilyData*>(data);
    SK_FONTCONFIGPARSER_WARNING("'%s' entity declaration found, stopping processing", entityName);
    XML_StopParser(self->fParser, XML_FALSE);
}

static const XML_Memory_Handling_Suite sk_XML_alloc = {
    sk_malloc_throw,
    sk_realloc_throw,
    sk_free
};

/**
 * This function parses the given filename and stores the results in the given
 * families array. Returns the version of the file, negative if the file does not exist.
 */
static int parse_config_file(const char* filename, SkTDArray<FontFamily*>& families,
                             const SkString& basePath, bool isFallback)
{
    SkFILEStream file(filename);

    // Some of the files we attempt to parse (in particular, /vendor/etc/fallback_fonts.xml)
    // are optional - failure here is okay because one of these optional files may not exist.
    if (!file.isValid()) {
        SkDebugf(SK_FONTMGR_CONFIG_PARSER_PREFIX "'%s' could not be opened\n", filename);
        return -1;
    }

    SkAutoTCallVProc<std::remove_pointer_t<XML_Parser>, XML_ParserFree> parser(
        XML_ParserCreate_MM(nullptr, &sk_XML_alloc, nullptr));
    if (!parser) {
        SkDebugf(SK_FONTMGR_CONFIG_PARSER_PREFIX "could not create XML parser\n");
        return -1;
    }

    FamilyData self(parser, families, basePath, isFallback, filename, &topLevelHandler);
    XML_SetUserData(parser, &self);

    // Disable entity processing, to inhibit internal entity expansion. See expat CVE-2013-0340
    XML_SetEntityDeclHandler(parser, xml_entity_decl_handler);

    // Start parsing oldschool; switch these in flight if we detect a newer version of the file.
    XML_SetElementHandler(parser, start_element_handler, end_element_handler);

    // One would assume it would be faster to have a buffer on the stack and call XML_Parse.
    // But XML_Parse will call XML_GetBuffer anyway and memmove the passed buffer into it.
    // (Unless XML_CONTEXT_BYTES is undefined, but all users define it.)
    // In debug, buffer a small odd number of bytes to detect slicing in XML_CharacterDataHandler.
    static const int bufferSize = 512 SkDEBUGCODE(-507);
    bool done = false;
    while (!done) {
        void* buffer = XML_GetBuffer(parser, bufferSize);
        if (!buffer) {
            SkDebugf(SK_FONTMGR_CONFIG_PARSER_PREFIX "could not buffer enough to continue\n");
            return -1;
        }
        size_t len = file.read(buffer, bufferSize);
        done = file.isAtEnd();
        XML_Status status = XML_ParseBuffer(parser, len, done);
        if (XML_STATUS_ERROR == status) {
            XML_Error error = XML_GetErrorCode(parser);
            int line = XML_GetCurrentLineNumber(parser);
            int column = XML_GetCurrentColumnNumber(parser);
            const XML_LChar* errorString = XML_ErrorString(error);
            SkDebugf(SK_FONTMGR_CONFIG_PARSER_PREFIX "%s:%d:%d error %d: %s.\n",
                     filename, line, column, error, errorString);
            return -1;
        }
    }
    return self.fVersion;
}

/** Returns the version of the system font file actually found, negative if none. */
static int append_system_font_families(SkTDArray<FontFamily*>& fontFamilies,
                                       const SkString& basePath)
{
    int version = parse_config_file(g_lmpSystemFontsFile.c_str(), fontFamilies, basePath, false);
    return version;
}

void SkFontMgr_Config_Parser::GetSystemFontFamilies(SkTDArray<FontFamily*>& fontFamilies)
{
    std::string containerFontBasePath = SkFontMgr::containerFontPath;
    if (containerFontBasePath.empty()) {
        printf("error getting font base path '%s'\n", containerFontBasePath.c_str());
    }
    SkString basePath(containerFontBasePath.c_str());
    g_lmpSystemFontsFile = containerFontBasePath.append(SK_FONT_CONFIG_FILE_NAME);
    HmSymbolConfig_OHOS::getInstance()->parseConfigOfHmSymbol("hm_symbol_config.json", basePath);
    append_system_font_families(fontFamilies, basePath);
}

SkLanguage SkLanguage::getParent() const
{
    SkASSERT(!fTag.isEmpty());
    const char* tag = fTag.c_str();

    // strip off the rightmost "-.*"
    const char* parentTagEnd = strrchr(tag, '-');
    if (parentTagEnd == nullptr) {
        return SkLanguage();
    }
    size_t parentTagLen = parentTagEnd - tag;
    return SkLanguage(tag, parentTagLen);
}