/*
 * Copyright 2024 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkFourByteTag_DEFINED
#define SkFourByteTag_DEFINED

#include <cstdint>
#ifdef ENABLE_TEXT_ENHANCE
#include <string>
#endif

using SkFourByteTag = uint32_t;

static inline constexpr SkFourByteTag SkSetFourByteTag(char a, char b, char c, char d) {
    return (((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d);
}

#ifdef ENABLE_TEXT_ENHANCE
static inline std::string SkFourByteTagToString(SkFourByteTag tag) {
    char chars[4] = {
        static_cast<char>((tag >> 24) & 0xFF),
        static_cast<char>((tag >> 16) & 0xFF),
        static_cast<char>((tag >> 8) & 0xFF),
        static_cast<char>(tag & 0xFF),
    };
    return std::string(chars, 4);
}
#endif

#endif
