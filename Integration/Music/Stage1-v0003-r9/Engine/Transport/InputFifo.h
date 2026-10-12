#pragma once
#include "SourceDomain.h"
#include <array>
#include <atomic>
#include <memory>
namespace entropy::music {
class InputFifo {
    struct Slot {
        StereoBlock block;
        uint64_t token{};
    };
    static constexpr uint64_t capacity = 16;
    std::unique_ptr<Slot[]> slots_{new Slot[capacity]};
    alignas(64) std::atomic<uint64_t> write_{0};
    alignas(64) std::atomic<uint64_t> read_{0};

  public:
    bool push(const StereoBlock &b, uint64_t token) noexcept {
        auto w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) >= capacity)
            return false;
        auto &s = slots_[w % capacity];
        s.block = b;
        s.token = token;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(StereoBlock &b, const std::atomic<uint64_t> &active, uint64_t expected=0) noexcept {
        if(!expected)expected=active.load(std::memory_order_acquire);
        if(!expected || expected!=active.load(std::memory_order_acquire))return false;
        auto r = read_.load(std::memory_order_relaxed);
        auto w = write_.load(std::memory_order_acquire);
        while (r < w) {
            if(expected!=active.load(std::memory_order_acquire))return false;
            auto &s = slots_[r % capacity];
            auto token = s.token;
            if (token == expected) {
                b = s.block;
                read_.store(r + 1, std::memory_order_release);
                return token == active.load(std::memory_order_acquire);
            }
            read_.store(++r, std::memory_order_release);
        }
        return false;
    }
    uint32_t fill() const noexcept {
        auto r = read_.load(std::memory_order_acquire);
        auto w = write_.load(std::memory_order_acquire);
        return static_cast<uint32_t>(w - r) * 1024;
    }
    // Consumer only: retire blocks from previous generations before priming.
    uint32_t currentFill(const std::atomic<uint64_t>& active,uint64_t expected) noexcept {
        auto r=read_.load(std::memory_order_relaxed);
        const auto w=write_.load(std::memory_order_acquire);
        if(!expected || expected!=active.load(std::memory_order_acquire))return 0;
        while(r<w && slots_[r%capacity].token!=expected) {
            if(expected!=active.load(std::memory_order_acquire))return 0;
            read_.store(++r,std::memory_order_release);
        }
        return expected==active.load(std::memory_order_acquire)?static_cast<uint32_t>(w-r)*1024:0;
    }
};
static_assert(std::atomic<uint64_t>::is_always_lock_free);
} // namespace entropy::music
