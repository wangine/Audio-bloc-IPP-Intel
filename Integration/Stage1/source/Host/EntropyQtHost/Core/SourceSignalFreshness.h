#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>

namespace entropy::host {
class SourceSignalFreshness final {
public:
    using Clock = std::chrono::steady_clock;
    void reset() noexcept { last_.store(0, std::memory_order_release); }
    void captured(Clock::time_point now = Clock::now()) noexcept {
        last_.store(ticks(now), std::memory_order_release);
    }
    bool fresh(Clock::time_point now = Clock::now()) const noexcept {
        const auto last = last_.load(std::memory_order_acquire);
        const auto age = ticks(now) - last;
        // Bridge reads can return no frames or block after a client stops.
        // Expire on the observer, without changing capture/SRC/audio cadence.
        return last != 0 && age >= 0 && age <= 250;
    }
private:
    static std::int64_t ticks(Clock::time_point time) noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
    }
    std::atomic<std::int64_t> last_{0};
};
}
