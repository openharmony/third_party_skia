// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifdef OHOS_SUPPORT
#include <iostream>
#include <fstream>

#include "log.h"
#include "modules/skparagraph/include/Hyphenator.h"

namespace skia {
namespace textlayout {

    const std::unordered_map<std::string, std::string> HPB_FILE_NAMES = {
        {"as", "hyph-as.hpb"},
        {"be", "hyph-be.hpb"},
        {"bg", "hyph-bg.hpb"},
        {"bn", "hyph-bn.hpb"},
        {"cs", "hyph-cs.hpb"},
        {"cy", "hyph-cy.hpb"},
        {"da", "hyph-da.hpb"},
        {"de-1901", "hyph-de-1901.hpb"},
        {"de-1996", "hyph-de-1996.hpb"},
        {"de-ch-1901", "hyph-de-ch-1901.hpb"},
        {"el-monoton", "hyph-el-monoton.hpb"},
        {"el-polyton", "hyph-el-polyton.hpb"},
        {"en-gb", "hyph-en-gb.hpb"},
        {"es", "hyph-es.hpb"},
        {"et", "hyph-et.hpb"},
        {"fr", "hyph-fr.hpb"},
        {"ga", "hyph-ga.hpb"},
        {"gl", "hyph-gl.hpb"},
        {"grc-x-ibycus", "hyph-grc-x-ibycus.hpb"},
        {"gu", "hyph-gu.hpb"},
        {"hi", "hyph-hi.hpb"},
        {"hr", "hyph-hr.hpb"},
        {"hu", "hyph-hu.hpb"},
        {"hy", "hyph-hy.hpb"},
        {"id", "hyph-id.hpb"},
        {"is", "hyph-is.hpb"},
        {"it", "hyph-it.hpb"},
        {"ka", "hyph-ka.hpb"},
        {"kn", "hyph-kn.hpb"},
        {"la", "hyph-la.hpb"},
        {"lt", "hyph-lt.hpb"},
        {"lv", "hyph-lv.hpb"},
        {"mk", "hyph-mk.hpb"},
        {"ml", "hyph-ml.hpb"},
        {"mn-cyrl", "hyph-mn-cyrl.hpb"},
        {"mr", "hyph-mr.hpb"},
        {"mul-ethi", "hyph-mul-ethi.hpb"},
        {"nl", "hyph-nl.hpb"},
        {"or", "hyph-or.hpb"},
        {"pa", "hyph-pa.hpb"},
        {"pl", "hyph-pl.hpb"},
        {"pt", "hyph-pt.hpb"},
        {"rm", "hyph-rm.hpb"},
        {"ru", "hyph-ru.hpb"},
        {"sh-cyrl", "hyph-sh-cyrl.hpb"},
        {"sh-latn", "hyph-sh-latn.hpb"},
        {"sk", "hyph-sk.hpb"},
        {"sl", "hyph-sl.hpb"},
        {"sr-cyrl", "hyph-sr-cyrl.hpb"},
        {"sv", "hyph-sv.hpb"},
        {"ta", "hyph-ta.hpb"},
        {"te", "hyph-te.hpb"},
        {"th", "hyph-th.hpb"},
        {"tk", "hyph-tk.hpb"},
        {"tr", "hyph-tr.hpb"},
        {"uk", "hyph-uk.hpb"},
        {"zh-py", "hyph-zh-latn-pinyin.hpb"},
    };

    void ReadBinaryFile(const std::string filePath, std::vector<uint8_t>& buffer)
    {
        std::ifstream file(filePath, std::ifstream::binary);
        if (!file) {
            TEXT_LOGE("Hyphen,create stream fail:%{public}s", filePath.c_str());
            file.close();
            return;
        }

        if (!file.is_open()) {
            TEXT_LOGE("Hyphen,can't read file:%{public}s", filePath.c_str());
            file.close();
            return;
        }

        file.seekg(0, std::ios::end);
        std::streamsize length = file.tellg();
        file.seekg(0, std::ios::beg);

        buffer.resize(length);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), length)) {
            TEXT_LOGE("Hyphen,read file error:%{public}s", filePath.c_str());
        }
        file.close();
    }

    const std::string& ResolveHpbFile(const std::string& locale)
    {
        auto it = HPB_FILE_NAMES.find(locale);
        if (it != HPB_FILE_NAMES.end()) {
            return it->second;
        }
        return HPB_FILE_NAMES.at("en-gb");
    }

    const std::vector<uint8_t>& Hyphenator::GetHyphenatorData(const std::string& locale)
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        if (auto search = hyphMap.find(locale); search != hyphMap.end()) {
            TEXT_LOGE("## got hyphenator data for locale '%{public}s'", locale.c_str());
            return search->second;
        }
        std::string filename = "/system/usr/ohos_hyphen_data/" + ResolveHpbFile(locale);
        std::vector<uint8_t> fileBuffer;
        ReadBinaryFile(filename, fileBuffer);
        if (!fileBuffer.empty()) {
            hyphMap.emplace(locale, std::move(fileBuffer));
        }
        return hyphMap[locale];
    }

} // namespace textlayout
} // namespace skia
#endif