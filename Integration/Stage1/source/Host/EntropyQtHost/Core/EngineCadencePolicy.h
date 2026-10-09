#pragma once

#include <chrono>
#include <cstdint>

namespace entropy::host
{
class EngineCadencePolicy final
{
public:
    static constexpr std::uint64_t blockFrames = 1024;
    static constexpr std::uint64_t sampleRate = 192000;

    static constexpr std::uint64_t deadlineNanoseconds(
        std::uint64_t epochNanoseconds,
        std::uint64_t completedBlocks) noexcept
    {
        // Split before multiplying: the naive intermediate wraps after ~27 h
        // although its final nanosecond deadline is still well within range.
        return epochNanoseconds +
            (completedBlocks / sampleRate) * blockFrames * 1'000'000'000ULL +
            ((completedBlocks % sampleRate) * blockFrames * 1'000'000'000ULL) / sampleRate;
    }

    static std::chrono::steady_clock::time_point rebasedEpoch(
        std::chrono::steady_clock::time_point resume,
        std::uint64_t completedBlocks) noexcept
    {
        // A blocking control-plane operation produces no authoritative audio
        // blocks. Its wall-clock duration is therefore not programme debt.
        // Keep the exact completed-frame index, but move its calendar anchor
        // to the instant audio production resumes.
        return resume - std::chrono::nanoseconds(
            deadlineNanoseconds(0, completedBlocks));
    }

    // Stop accelerated recovery once output has regained its transport cushion.
    // This changes only the calendar anchor, never the completed frame index.
    static std::chrono::steady_clock::time_point recoveryDeadline(
        std::chrono::steady_clock::time_point scheduled,
        std::chrono::steady_clock::time_point now,
        std::chrono::nanoseconds cycle, bool outputAtTarget) noexcept
    {
        if (outputAtTarget && cycle > std::chrono::nanoseconds::zero() &&
            now > scheduled && now - scheduled > cycle)
            return now + cycle;
        return scheduled;
    }

    static std::chrono::steady_clock::time_point boundedDeadline(
        std::chrono::steady_clock::time_point scheduledDeadline,
        std::chrono::steady_clock::time_point now,
        std::chrono::nanoseconds cycle,
        std::chrono::steady_clock::time_point blockBegin) noexcept
    {
        if (cycle <= std::chrono::nanoseconds::zero()) return now;

        // scheduledDeadline follows the exact frame-count calendar. Its anchor
        // can explicitly change through recoveryDeadline once output is full.
        // Do not otherwise replace it with wall time after a late block:
        // recurring recoverable delays would permanently underproduce audio.
        // Recover phase with at least 7/8 of a normal interval between starts
        // (maximum 8/7 production pace), never an unbounded catch-up loop.
        // No frame is repeated, skipped, or resampled by this scheduling rule.
        const auto earliest = blockBegin + cycle * 7 / 8;
        const auto paced = scheduledDeadline < earliest ? earliest : scheduledDeadline;
        return paced < now ? now : paced;
    }
};
}
