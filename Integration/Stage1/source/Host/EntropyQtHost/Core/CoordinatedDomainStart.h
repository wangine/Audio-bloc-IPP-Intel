#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace entropy::host
{
// Control-plane barrier used while arming independent DAC domains. Every
// worker opens its endpoint first; no endpoint is started until all enabled
// domains have reached the same point. This prevents the first DAC from
// draining its startup reserve while a later endpoint is still opening.
class CoordinatedDomainStart final
{
public:
    explicit CoordinatedDomainStart(std::uint32_t participants) noexcept
        : participants_(participants)
    {
    }

    bool arriveAndWait() noexcept
    {
        std::unique_lock lock(mutex_);
        if (cancelled_ || participants_ == 0) return false;
        ++arrived_;
        if (arrived_ == participants_)
        {
            released_ = true;
            condition_.notify_all();
            return true;
        }
        condition_.wait(lock, [this] { return released_ || cancelled_; });
        return released_ && !cancelled_;
    }

    void cancel() noexcept
    {
        {
            std::lock_guard lock(mutex_);
            cancelled_ = true;
        }
        condition_.notify_all();
    }

private:
    const std::uint32_t participants_ = 0;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::uint32_t arrived_ = 0;
    bool released_ = false;
    bool cancelled_ = false;
};
} // namespace entropy::host
