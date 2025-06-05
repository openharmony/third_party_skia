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
#include "HmSymbolConfig_ohos.h"

namespace skia::text {
std::function<int(const char* filePath)> HmSymbolConfig_OHOS::fLoadSymbolConfigFunc = nullptr;
void HmSymbolConfig_OHOS::SetLoadSymbolConfig(std::function<int(const char* filePath)>& loadSymbolConfigFunc)
{
    fLoadSymbolConfigFunc = loadSymbolConfigFunc;
}
int HmSymbolConfig_OHOS::LoadSymbolConfig(const char* fileName, SkString fileDir)
{
    if (fLoadSymbolConfigFunc == nullptr) {
        return ERROR_CONFIG_FUN_NOT_DEFINED;
    }
    if (fileName == nullptr || strlen(fileName) == 0) {
        return ERROR_CONFIG_NOT_FOUND;
    }
    if (fileDir.size() == 0) {
        fileDir.append(".");
    }
    std::string fullname(fileDir.c_str(), fileDir.size());
    char separator = '/';
#if defined(SK_BUILD_FONT_MGR_FOR_PREVIEW_WIN)
    separator = '\\';
#endif
    if (fileDir[fileDir.size() - 1] != separator) {
        fullname += separator;
    }
    std::string fNameStr(fileName, strlen(fileName));
    fullname += fNameStr;
    return fLoadSymbolConfigFunc(fullname.c_str());
}
} // namespace skia::text