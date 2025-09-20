/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_SingleOwner_DEFINED
#define skgpu_SingleOwner_DEFINED

#include "include/private/base/SkDebug.h" // IWYU pragma: keep

#if defined(SK_DEBUG) || defined(SKIA_OHOS_SINGLE_OWNER)
#include "include/private/base/SkAssert.h"
#include "include/private/base/SkMutex.h"
#include "include/private/base/SkThreadAnnotations.h"
#include "include/private/base/SkThreadID.h"
#ifdef SKIA_OHOS_SINGLE_OWNER
#include <thread>
#include <string>
#include <sstream>
#include "include/core/SkLog.h"
#include <csignal>
#endif

#endif

namespace skgpu {

#if defined(SK_DEBUG)
#define SKGPU_ASSERT_SINGLE_OWNER(obj) \
    skgpu::SingleOwner::AutoEnforce debug_SingleOwner(obj, __FILE__, __LINE__);
#define SKGPU_ASSERT_SINGLE_OWNER_OHOS(obj)
#endif

#if defined(SKIA_OHOS_SINGLE_OWNER)
#define SKGPU_ASSERT_SINGLE_OWNER(obj)
#define SKGPU_ASSERT_SINGLE_OWNER_OHOS(obj) \
    skgpu::SingleOwner::AutoEnforce debug_SingleOwner(obj, __FILE__, __LINE__);
#endif

#if defined(SK_DEBUG) || defined(SKIA_OHOS_SINGLE_OWNER)
// This is a debug tool to verify an object is only being used from one thread at a time.
static const int SIGNO_FOR_OCEAN = 42;
class SingleOwner {
public:
     SingleOwner() : fOwner(kIllegalThreadID), fReentranceCount(0) {}

     struct AutoEnforce {
         AutoEnforce(SingleOwner* so, const char* file, int line)
                : fFile(file), fLine(line), fSO(so) {
             if(fSO) {
                fSO->enter(file, line);
             }
         }
         ~AutoEnforce() { 
            if(fSO) {
                fSO->exit(fFile, fLine);
            }
         }

         const char* fFile;
         int fLine;
         SingleOwner* fSO;
     };

private:
     void enter(const char* file, int line) {
#ifdef SKIA_OHOS_SINGLE_OWNER
         if (!GetEnableSkiaSingleOwner()) {
            return;
         }
#endif
         SkAutoMutexExclusive lock(fMutex);
         SkThreadID self = SkGetThreadID();
#ifdef SKIA_OHOS_SINGLE_OWNER
         bool assertCheck = (fOwner == self || fOwner == kIllegalThreadID);
         if (!assertCheck) {
            SK_LOGE("\n\n\n\n ========== BackTrace Start ==========");
            PrintBackTrace(fOwnerTid);
            PrintBackTrace(SkGetThreadID());
            SK_LOGE("========== BackTrace End ========== occur file:%{public}s line:%{public}d\n\n\n\n",
                file, line);
            raise(SIGNO_FOR_OCEAN); // report to Ocean
         }
         fReentranceCount++;
         fOwner = self;
         fOwnerTid = SkGetThreadID();
#else
         SkASSERTF(fOwner == self || fOwner == kIllegalThreadID, "%s:%d Single owner failure.",
                   file, line);
         fReentranceCount++;
         fOwner = self;
#endif
     }

     void exit(const char* file, int line) {
#ifdef SKIA_OHOS_SINGLE_OWNER
         if (!GetEnableSkiaSingleOwner()) {
            return;
         }
#endif
         SkAutoMutexExclusive lock(fMutex);
#ifdef SKIA_OHOS_SINGLE_OWNER
         SkThreadID self = SkGetThreadID();
         bool assertCheck = (fOwner == self || fOwner == kIllegalThreadID);
         if (!assertCheck) {
            SK_LOGE("\n\n\n\n ========== BackTrace Start ==========");
            PrintBackTrace(fOwnerTid);
            PrintBackTrace(SkGetThreadID());
            SK_LOGE("========== BackTrace End ========== occur file:%{public}s line:%{public}d\n\n\n\n",
                file, line);
            raise(SIGNO_FOR_OCEAN); // report to Ocean
         }
         fReentranceCount--;
         if (fReentranceCount <= 0) {
             fOwner = kIllegalThreadID;
             fOwnerTid = 0;
         }
#else
         SkASSERTF(fOwner == SkGetThreadID(), "%s:%d Single owner failure.", file, line);
         fReentranceCount--;
         if (fReentranceCount == 0) {
             fOwner = kIllegalThreadID;
         }
#endif
     }

#ifdef SKIA_OHOS_SINGLE_OWNER
     pid_t fOwnerTid SK_GUARDED_BY(fMutex);
#endif
     SkMutex fMutex;
     SkThreadID fOwner    SK_GUARDED_BY(fMutex);
     int fReentranceCount SK_GUARDED_BY(fMutex);
};
#else
#define SKGPU_ASSERT_SINGLE_OWNER(obj)
class SingleOwner {}; // Provide a no-op implementation so we can pass pointers to constructors
#endif

} // namespace skgpu

#endif
