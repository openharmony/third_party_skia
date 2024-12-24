/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef HYPHENATE_PATTERN_H
#define HYPHENATE_PATTERN_H

#include <cinttypes>
#include <string>
#include <vector>

using namespace std;

namespace OHOS::Hyphenate {
#define SUCCEED (0)
#define FAILED (-1)

constexpr size_t HYPHEN_DEFAULT_INDENT = 10;
constexpr size_t HYPHEN_INDENT_INCREMENT = 2;
constexpr size_t HYPHEN_BASE_CODE_SHIFT = 2;
constexpr size_t ROOT_INDENT = 12;
constexpr size_t LARGE_PATH_SIZE = 8;
constexpr size_t BYTES_PRE_WORD = 4;
constexpr size_t SHIFT_BITS_14 = 14;
constexpr size_t SHIFT_BITS_16 = 16;
constexpr size_t SHIFT_BITS_30 = 30;
constexpr size_t PADDING_SIZE = 4;
constexpr int32_t BREAK_FLAG = 9;
constexpr int32_t NO_BREAK_FLAG = 8;

// We make assumption that 14 bytes is enough to represent offset
// so we get two first bits in the array for path type
// we have two bytes on the offset arrays
// for these
enum class PathType : uint8_t {
    PATTERN = 0,
    LINEAR = 1,
    PAIRS = 2,
    DIRECT = 3
};

vector<uint16_t> ConvertToUtf16(const string& utf8Str);
}
#endif