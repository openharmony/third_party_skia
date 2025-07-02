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
#include <memory>
#include <mutex>

#include "include/core/SkString.h"
namespace skia::text {

const int NO_ERROR = 0;
const int ERROR_CONFIG_NOT_FOUND = 1;            // the configuration document is not found
const int ERROR_CONFIG_FUN_NOT_DEFINED = 12;
const int ERROR_CONFIG_FILE_PATH_ERROR = 13;

class SK_API HmSymbolConfig_OHOS {
public:
    static void SetLoadSymbolConfig(std::function<int(const char* filePath)>& loadSymbolConfigFunc);
    static void ClearLoadSymbolConfig();
    static int LoadSymbolConfig(const char* fileName, SkString fileDir);

private:
    static std::unique_ptr<std::function<int(const char* filePath)>> fLoadSymbolConfigFunc;
    static std::mutex fMutex;
};
} // namespace skia::text
#endif