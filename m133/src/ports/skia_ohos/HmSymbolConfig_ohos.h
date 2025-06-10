/*
* Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef LIB_HW_SYMBOL_CONFIG_H
#define LIB_HW_SYMBOL_CONFIG_H
#include <functional>

#include "include/core/SkString.h"
namespace skia::text {

const int NO_ERROR = 0;                          // no error
const int ERROR_CONFIG_NOT_FOUND = 1;            // the configuration document is not found
const int ERROR_CONFIG_FORMAT_NOT_SUPPORTED = 2; // the formation of configuration is not supported
const int ERROR_CONFIG_INVALID_VALUE_TYPE = 4;   // invalid value type in the configuration
const int ERROR_TYPE_COUNT = 11;
const int ERROR_CONFIG_FUN_NOT_DEFINED = 12;

class SK_API HmSymbolConfig_OHOS {
public:
    static void SetLoadSymbolConfig(std::function<int(const char* filePath)>& loadSymbolConfigFunc);
    static int LoadSymbolConfig(const char* fileName, SkString fileDir);

private:
    static std::function<int(const char* filePath)> fLoadSymbolConfigFunc;
};
} // namespace skia::text
#endif