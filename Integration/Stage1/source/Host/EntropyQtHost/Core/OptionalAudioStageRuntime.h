#pragma once

#include <entropy/module_abi.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace entropy::host
{
enum class OptionalStageState
{
    Active,
    Bypassed,
    UnavailableWire,
    FaultForcedBypass,
    EmptySlotWire
};

enum class OptionalFallback
{
    ExactWire,
    SilenceOutputs
};

class OptionalAudioStageRuntime
{
public:
    explicit OptionalAudioStageRuntime(
        std::uint32_t inputChannels = 2,
        std::uint32_t outputChannels = 2,
        OptionalFallback fallback = OptionalFallback::ExactWire);

    void configureFormat(std::uint32_t inputChannels,
                         std::uint32_t outputChannels,
                         OptionalFallback fallback) noexcept;

    void bind(EntropyModuleHandle handle, const EntropyModuleApiV1& api) noexcept;
    void unbind() noexcept;
    void setBypassed() noexcept;
    void setUnavailable() noexcept;
    void setEmptySlot() noexcept;
    void latchFault(int faultCode) noexcept;

    bool available() const noexcept;
    bool faulted() const noexcept;
    int faultCode() const noexcept;
    OptionalStageState state() const noexcept;

    bool process(const double* const* inputs, double* const* outputs,
                 std::uint32_t frames) noexcept;
    bool process(const double* left, const double* right,
                 double* outputLeft, double* outputRight,
                 unsigned frames) noexcept;

private:
    void applyFallback(double* const* outputs, std::uint32_t frames) noexcept;

    static constexpr std::uint32_t maximumChannels = 16;
    static constexpr std::uint32_t maximumFrames = 1024;
    std::uint32_t inputChannels_ = 2;
    std::uint32_t outputChannels_ = 2;
    OptionalFallback fallback_ = OptionalFallback::ExactWire;
    EntropyModuleHandle handle_ = nullptr;
    decltype(EntropyModuleApiV1{}.process_f64) process_ = nullptr;
    std::atomic<OptionalStageState> state_{OptionalStageState::UnavailableWire};
    std::atomic<int> fault_{0};
    std::unique_ptr<double[]> pristine_;
};
} // namespace entropy::host
