#pragma once
#include "../model/Types.h"
#ifndef ENTROPY_KERNEL
#include <atomic>
#endif
namespace entropy::music {
class OperationGate {
#ifdef ENTROPY_KERNEL
    volatile LONG owner_=0;
#else
    std::atomic<int32_t> owner_{0};
#endif
public:
    bool tryEnter() noexcept {
#ifdef ENTROPY_KERNEL
        return InterlockedCompareExchange(&owner_,1,0)==0;
#else
        int32_t expected=0;return owner_.compare_exchange_strong(expected,1,std::memory_order_acquire);
#endif
    }
    void leave() noexcept {
#ifdef ENTROPY_KERNEL
        InterlockedExchange(&owner_,0);
#else
        owner_.store(0,std::memory_order_release);
#endif
    }
};
}
