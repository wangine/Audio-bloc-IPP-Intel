#pragma once

#include "RealtimeBufferPolicy.h"

#include <entropy/egress_clock_abi.h>

#include <algorithm>
#include <cstdint>

namespace entropy::host
{
struct EgressStartupPrefillResult final
{
    std::uint64_t inputEngineFrames = 0;
    std::uint64_t nominalSinkFrames = 0;
};

// Control-plane startup helper. It counts the nominal SRC output reported by
// the real ClockSync boundary instead of assuming input and sink frame units
// are interchangeable. No allocation, wait, or audio payload substitution is
// performed here; callers provide already allocated deterministic silence.
inline bool prefillEgressClockForStart(
    const EntropyEgressClockApiV1& api,
    EntropyModuleHandle handle,
    std::uint32_t channels,
    std::uint32_t engineRate,
    std::uint32_t sinkRate,
    std::uint32_t maximumInputFrames,
    std::uint32_t maximumRenderFrames,
    const double* const* silence,
    EgressStartupPrefillResult& result
#if ENTROPY_A06_R04_DIAG
    , void* observerContext=nullptr,
    void (*observer)(void*,const EntropyEgressClockPushBlockF64&)=nullptr
#endif
    ) noexcept
{
    result = {};
    if (api.push == nullptr || handle == nullptr || channels == 0 ||
        engineRate == 0 || sinkRate == 0 || maximumInputFrames == 0 ||
        maximumRenderFrames == 0 || silence == nullptr)
        return false;

    const auto requiredSinkFrames =
        RealtimeBufferPolicy::egressStartupPrefillSinkFrames(
            sinkRate, maximumRenderFrames);
    const auto nominalInputFrames =
        (static_cast<std::uint64_t>(requiredSinkFrames) * engineRate +
         sinkRate - 1u) / sinkRate;
    const auto inputBudget = nominalInputFrames +
        static_cast<std::uint64_t>(maximumInputFrames) * 8u;

    while (result.nominalSinkFrames < requiredSinkFrames)
    {
        if (result.inputEngineFrames >= inputBudget) return false;
        const auto remainingBudget = inputBudget - result.inputEngineFrames;
        const auto frames = static_cast<std::uint32_t>((std::min)(
            remainingBudget,
            static_cast<std::uint64_t>(maximumInputFrames)));

        EntropyEgressClockPushBlockF64 block{};
        block.struct_size = sizeof(block);
        block.channels = channels;
        block.frames = frames;
        block.inputs = silence;
#if ENTROPY_A06_R04_DIAG
        if(observer)observer(observerContext,block);
#endif
        if (api.push(handle, &block) != ENTROPY_OK ||
            block.consumed_frames != frames)
            return false;

        result.inputEngineFrames += block.consumed_frames;
        result.nominalSinkFrames += block.nominal_frames_written;
    }
    return true;
}
} // namespace entropy::host
