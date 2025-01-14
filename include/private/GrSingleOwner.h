/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrSingleOwner_DEFINED
#define GrSingleOwner_DEFINED

#include "include/core/SkTypes.h"

#if defined(SK_DEBUG) || defined(SKIA_OHOS_SINGLE_OWNER)
#include "include/private/SkMutex.h"
#include "include/private/SkThreadID.h"

#ifdef SKIA_OHOS_SINGLE_OWNER
#include <thread>
#include <string>
#include <sstream>
#include "include/core/SkLog.h"
#include <csignal>
#endif


#define GR_ASSERT_SINGLE_OWNER(obj) \
    GrSingleOwner::AutoEnforce debug_SingleOwner(obj, __FILE__, __LINE__);

static const int SIGNO_FOR_OCEAN = 42;
// This is a debug tool to verify an object is only being used from one thread at a time.
class GrSingleOwner {
public:
     GrSingleOwner() : fOwner(kIllegalThreadID), fReentranceCount(0) {}

     struct AutoEnforce {
         AutoEnforce(GrSingleOwner* so, const char* file, int line)
                : fFile(file), fLine(line), fSO(so) {
             fSO->enter(file, line);
         }
         ~AutoEnforce() { fSO->exit(fFile, fLine); }

         const char* fFile;
         int fLine;
         GrSingleOwner* fSO;
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
            SK_LOGE("========== BackTrace End ========== occur file:%{public}s line:%{public}d\n\n\n\n",
                file, line);
            raise(SIGNO_FOR_OCEAN); // report to Ocean
         }
         fReentranceCount++;
         fOwner = self;
         fOwnerTid = gettid();
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
#define GR_ASSERT_SINGLE_OWNER(obj)
class GrSingleOwner {}; // Provide a no-op implementation so we can pass pointers to constructors
#endif

#endif
