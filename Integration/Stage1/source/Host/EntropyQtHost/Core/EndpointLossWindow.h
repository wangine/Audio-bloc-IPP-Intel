#pragma once

#include <cstdint>

namespace entropy::host {

// Converts a provider's lifetime-cumulative loss counter into losses observed
// by this Host capture session. A generation change or provider counter reset
// opens a new baseline; subsequent increments remain fully visible.
class EndpointLossWindow final
{
public:
    std::uint64_t observe(std::uint64_t generation,
                          std::uint64_t cumulative) noexcept
    {
        if (!initialized_ || generation != generation_ || cumulative < last_)
        {
            initialized_ = true;
            generation_ = generation;
            baseline_ = cumulative;
            last_ = cumulative;
            return 0;
        }
        last_ = cumulative;
        return cumulative - baseline_;
    }

    void reset() noexcept
    {
        initialized_ = false;
        generation_ = 0;
        baseline_ = 0;
        last_ = 0;
    }

private:
    bool initialized_ = false;
    std::uint64_t generation_ = 0;
    std::uint64_t baseline_ = 0;
    std::uint64_t last_ = 0;
};

} // namespace entropy::host
