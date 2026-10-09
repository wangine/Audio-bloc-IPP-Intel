#pragma once

#include <entropy/egress_clock_abi.h>

#include <atomic>
#include <cstdint>

namespace entropy::host
{
struct EgressProgramIdentity
{
    std::uint64_t musicSessionRevision = 0;
    std::uint64_t musicGeneration = 0;
    std::uint64_t auxGeneration = 0;
    std::uint64_t cinemaGeneration = 0;
    bool cinemaProgram = false;
    std::uint64_t programRevision = 0;
    std::uint64_t programSession = 0;
    std::uint64_t producerRevision = 0;

    friend constexpr bool operator==(const EgressProgramIdentity&,
                                     const EgressProgramIdentity&) noexcept = default;
};

// Audio-thread-owned detector. The first observation after ARM is a baseline;
// subsequent identity changes are exact programme-stream boundaries.
class EgressProgramBoundary final
{
public:
    bool observe(const EgressProgramIdentity& identity) noexcept
    {
        if (!initialized_)
        {
            identity_ = identity;
            initialized_ = true;
            return false;
        }
        // AUX is an independently mixed overlay.  Its endpoint generation
        // changes when a short notification starts or stops, but that must
        // not declare a discontinuity in the continuous MUSIC/CINEMA
        // programme clock shared by the egress domains. Likewise, an input
        // outside the selected programme cannot reset that programme's DACs.
        const bool sameProgramme = identity.producerRevision == identity_.producerRevision &&
            identity.programRevision == identity_.programRevision &&
            identity.programSession == identity_.programSession &&
            identity.cinemaProgram == identity_.cinemaProgram &&
            (identity.cinemaProgram
                ? identity.cinemaGeneration == identity_.cinemaGeneration
                : identity.musicSessionRevision == identity_.musicSessionRevision &&
                  identity.musicGeneration == identity_.musicGeneration);
        if (sameProgramme)
        {
            identity_ = identity;
            return false;
        }
        identity_ = identity;
        return true;
    }

    void reset() noexcept
    {
        identity_ = {};
        initialized_ = false;
    }

private:
    EgressProgramIdentity identity_{};
    bool initialized_ = false;
};

// One latch belongs to each independent DAC domain. This guarantees both
// ClockSync instances receive the same boundary exactly once without sharing
// consumer state or coupling the DLL instances.
class EgressPushFlagLatch final
{
public:
    void markDiscontinuity() noexcept
    {
        pending_.store(true, std::memory_order_release);
    }

    std::uint32_t consume() noexcept
    {
        return pending_.exchange(false, std::memory_order_acq_rel)
            ? ENTROPY_EGRESS_CLOCK_DISCONTINUITY
            : 0u;
    }

    void reset() noexcept
    {
        pending_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> pending_{false};
};
} // namespace entropy::host
