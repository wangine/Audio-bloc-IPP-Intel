#pragma once

#include <cstdint>

namespace entropy::sinkrecovery
{
class Watchdog
{
public:
    static constexpr std::uint32_t kWaitMilliseconds = 250;
    static constexpr std::uint32_t kTimeoutsBeforeReopen = 4;

    void opened() noexcept
    {
        consecutiveTimeouts_ = 0;
    }

    void audioEvent() noexcept
    {
        consecutiveTimeouts_ = 0;
    }

    bool waitTimeout() noexcept
    {
        if (consecutiveTimeouts_ < kTimeoutsBeforeReopen)
            ++consecutiveTimeouts_;
        return consecutiveTimeouts_ >= kTimeoutsBeforeReopen;
    }

    static constexpr bool waitFailure() noexcept { return true; }
    static constexpr bool bufferFailure() noexcept { return true; }

    std::uint32_t consecutiveTimeouts() const noexcept
    {
        return consecutiveTimeouts_;
    }

private:
    std::uint32_t consecutiveTimeouts_ = 0;
};
} // namespace entropy::sinkrecovery
