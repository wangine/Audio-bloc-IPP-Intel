#pragma once

#include <atomic>
#include <cstdint>

namespace entropy::host {

struct MusicSrcSwitchRequest
{
    std::uint64_t sequence = 0;
    std::uint32_t engine = 0;
};

// Single-writer (audio command loop), single-reader (control observer) mailbox.
// Publishing is wait-free and never performs lifecycle work on the audio thread.
class MusicSrcSwitchMailbox final
{
public:
    void publish(std::uint64_t sequence, std::uint32_t engine) noexcept
    {
        const auto stamp = stamp_.load(std::memory_order_relaxed);
        stamp_.store(stamp + 1u, std::memory_order_relaxed); // write in progress
        sequence_.store(sequence, std::memory_order_relaxed);
        engine_.store(engine, std::memory_order_relaxed);
        stamp_.store(stamp + 2u, std::memory_order_release);
    }

    bool consume(std::uint64_t& lastSequence,
                 MusicSrcSwitchRequest& request) const noexcept
    {
        const auto before = stamp_.load(std::memory_order_acquire);
        if ((before & 1u) != 0u) return false;

        MusicSrcSwitchRequest candidate;
        candidate.sequence = sequence_.load(std::memory_order_relaxed);
        candidate.engine = engine_.load(std::memory_order_relaxed);

        const auto after = stamp_.load(std::memory_order_acquire);
        if (before != after || (after & 1u) != 0u ||
            candidate.sequence == 0 || candidate.sequence == lastSequence)
            return false;

        request = candidate;
        lastSequence = candidate.sequence;
        return true;
    }

private:
    mutable std::atomic<std::uint64_t> stamp_{0};
    std::atomic<std::uint64_t> sequence_{0};
    std::atomic<std::uint32_t> engine_{0};
};

} // namespace entropy::host
