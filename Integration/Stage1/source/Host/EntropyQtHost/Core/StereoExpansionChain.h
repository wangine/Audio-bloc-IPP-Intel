#pragma once

#include "OptionalAudioStageRuntime.h"

#include <array>
#include <cstdint>

namespace entropy::host
{
class R05BlockRecorder;
struct StereoExpansionResult
{
    std::array<const double*, 2> channels{};
};

class StereoExpansionChain final
{
public:
    void bindSlot(std::size_t index, OptionalAudioStageRuntime* stage) noexcept;
    OptionalStageState slotState(std::size_t index) const noexcept;
    StereoExpansionResult process(const double* const* inputs,
                                  std::uint32_t frames, R05BlockRecorder* diagnostic=nullptr) noexcept;

private:
    static constexpr std::uint32_t maximumFrames = 1024;
    std::array<OptionalAudioStageRuntime*, 3> stages_{};
    std::array<std::array<std::array<double, maximumFrames>, 2>, 3> scratch_{};
};
} // namespace entropy::host
