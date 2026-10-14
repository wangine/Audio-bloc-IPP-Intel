#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>

namespace entropy::input {

// One producer and one Graph consumer. Steady-state publication never takes
// the consumer's reservation: it uses only release/acquire SPSC cursors.
// A reservation exists solely to pin the session/queue during a paired read.
// Maintenance (reconnect, stopped reset, inactive keep-latest) requests that
// reservation and waits OUTSIDE the Graph. Graph acquisition tries once.
template<class T, unsigned Capacity>
class IngressSpscQueue final {
    static_assert(Capacity && (Capacity & (Capacity - 1)) == 0);
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static constexpr unsigned readerBit = 1, maintenanceBit = 2;
    std::array<T, Capacity> slots_{};
    alignas(64) std::atomic<uint64_t> write_{0};
    alignas(64) std::atomic<uint64_t> read_{0};
    alignas(64) std::atomic<unsigned> access_{0};

public:
    T* producerSlot() noexcept {
        const auto w = write_.load(std::memory_order_relaxed);
        const auto r = read_.load(std::memory_order_acquire);
        return w - r < Capacity ? &slots_[w & (Capacity - 1)] : nullptr;
    }
    void commitProducer() noexcept {
        write_.store(write_.load(std::memory_order_relaxed) + 1,
                     std::memory_order_release);
    }
    bool acquireConsumer() noexcept {
        unsigned expected = 0;
        return access_.compare_exchange_strong(expected, readerBit,
            std::memory_order_acquire, std::memory_order_relaxed);
    }
    void releaseConsumer() noexcept {
        // Preserve a maintenance request posted while this reader was pinned.
        access_.fetch_and(~readerBit, std::memory_order_release);
    }
    const T* front() const noexcept {
        const auto r = read_.load(std::memory_order_relaxed);
        return r != write_.load(std::memory_order_acquire)
            ? &slots_[r & (Capacity - 1)] : nullptr;
    }
    void popConsumer() noexcept {
        read_.store(read_.load(std::memory_order_relaxed) + 1,
                    std::memory_order_release);
    }
    bool beginMaintenance() noexcept {
        // Producer/lifecycle owner only, never concurrent maintainers.
        // Posting the bit prevents new readers from starving a pending reset.
        return !(access_.fetch_or(maintenanceBit, std::memory_order_acq_rel)
                 & readerBit);
    }
    uint64_t discardExclusive() noexcept {
        // Only after beginMaintenance() succeeds; producer is also quiescent.
        // Never called by telemetry or as a normal active-overflow recovery.
        const auto w = write_.load(std::memory_order_relaxed);
        const auto n = w - read_.load(std::memory_order_relaxed);
        read_.store(w, std::memory_order_release);
        return n;
    }
    void endMaintenance() noexcept {
        access_.store(0, std::memory_order_release);
    }
    unsigned readableApprox() const noexcept {
        // Observation only: a reader may advance between these two loads.
        const auto w = write_.load(std::memory_order_acquire);
        const auto r = read_.load(std::memory_order_acquire);
        return w >= r && w - r <= Capacity ? static_cast<unsigned>(w - r) : 0;
    }
};

// Three preallocated snapshots. Publisher owns back_, serialized non-audio
// observers own front_; ownership of the third slot changes by atomic exchange.
// The publisher NEVER touches readerMutex_, waits for readers, allocates, or
// skips an audio block. A paused observer only keeps an older snapshot pinned.
template<class T>
class LatestIngressTelemetry final {
    static_assert(std::is_nothrow_copy_assignable_v<T>);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static constexpr unsigned dirtyBit = 4, indexMask = 3;
    std::array<T, 3> slots_{};
    unsigned back_ = 2;
    mutable unsigned front_ = 0;
    mutable std::atomic<unsigned> middle_{1};
    mutable std::mutex readerMutex_;

public:
    void publish(const T& value) noexcept {
        slots_[back_] = value;
        back_ = middle_.exchange(back_ | dirtyBit, std::memory_order_acq_rel)
                & indexMask;
    }
    T read() const noexcept {
        // Only control/telemetry callers; neither audio owner uses this mutex.
        std::lock_guard<std::mutex> lock(readerMutex_);
        if (middle_.load(std::memory_order_acquire) & dirtyBit)
            front_ = middle_.exchange(front_, std::memory_order_acq_rel)
                     & indexMask;
        return slots_[front_];
    }
};
} // namespace entropy::input
