/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef SkSVGStackGuard_DEFINED
#define SkSVGStackGuard_DEFINED

#include <cstddef>

#if defined(SKIA_OHOS) && defined(__aarch64__)
#include <cstdint>
#include <limits>
#include <pthread.h>
#endif

namespace sksvg {

static constexpr size_t kMinRecursionStackBytes = 64 * 1024;

#if defined(SKIA_OHOS) && defined(__aarch64__)
namespace detail {

struct StackBounds {
    uintptr_t fLow = 0;
    size_t fSize = 0;
};

inline StackBounds QueryCurrentThreadStackBounds() {
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) != 0) {
        return {};
    }

    void* stackAddress = nullptr;
    size_t stackSize = 0;
    const int getStackResult = pthread_attr_getstack(&attr, &stackAddress, &stackSize);
    (void)pthread_attr_destroy(&attr);
    if (getStackResult != 0 || !stackAddress || stackSize == 0) {
        return {};
    }

    return {reinterpret_cast<uintptr_t>(stackAddress), stackSize};
}

inline const StackBounds& CurrentThreadStackBounds() {
    // A pthread's stack mapping is fixed for its lifetime. Cache both a
    // successful query and an unavailable result, so recursion checks only
    // need to read the current stack pointer.
    static thread_local const StackBounds stackBounds = QueryCurrentThreadStackBounds();
    return stackBounds;
}

}  // namespace detail
#endif

inline bool HasSufficientStackForRecursion(size_t* remainingStackBytes) {
#if defined(SKIA_OHOS) && defined(__aarch64__)
    const detail::StackBounds& stackBounds = detail::CurrentThreadStackBounds();
    if (stackBounds.fLow == 0 || stackBounds.fSize == 0) {
        return true;
    }

    uintptr_t stackPointer;
    __asm__ volatile("mov %0, sp" : "=r"(stackPointer));

    // pthread_attr_getstack() returns the low address of the stack mapping.
    // AArch64 stacks grow down from stackHigh towards stackLow.
    const uintptr_t stackLow = stackBounds.fLow;
    const size_t stackSize = stackBounds.fSize;
    if (stackSize > std::numeric_limits<uintptr_t>::max() - stackLow) {
        return true;
    }
    const uintptr_t stackHigh = stackLow + stackSize;
    if (stackPointer < stackLow || stackPointer > stackHigh) {
        return true;
    }

    const size_t usedStackBytes = static_cast<size_t>(stackHigh - stackPointer);
    const size_t remaining = stackSize - usedStackBytes;
    if (remainingStackBytes) {
        *remainingStackBytes = remaining;
    }
    return remaining >= kMinRecursionStackBytes;
#else
    (void)remainingStackBytes;
    return true;
#endif
}

}  // namespace sksvg

#endif  // SkSVGStackGuard_DEFINED
