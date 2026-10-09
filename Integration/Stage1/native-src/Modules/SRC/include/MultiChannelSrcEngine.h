#pragma once

#include <entropy/src_abi.h>

#include <cstdint>
#include <memory>
#include <vector>

struct soxr;

namespace r8b { class CDSPResampler; }

namespace entropy::src
{
class MultiChannelSrcEngine
{
public:
    MultiChannelSrcEngine();
    ~MultiChannelSrcEngine();
    MultiChannelSrcEngine(const MultiChannelSrcEngine&) = delete;
    MultiChannelSrcEngine& operator=(const MultiChannelSrcEngine&) = delete;

    bool configure(const EntropySrcConfig& config);
    // Internal static-library use only: the existing output nominal stage.
    // Does not broaden the input SRC DLL ABI (which remains -> 192 kHz).
    bool configureNominalEgress(const EntropySrcConfig& config);
    void reset();
    EntropyResult process(EntropySrcBlockF64& block) noexcept;
    const EntropySrcConfig& config() const noexcept { return config_; }
    std::uint32_t latencyOutputFrames() const noexcept;
    static std::uint32_t requiredOutputCapacity(
        std::uint32_t sourceRate, std::uint32_t destinationRate,
        std::uint32_t maximumInputFrames) noexcept;

private:
    bool configureValidated(const EntropySrcConfig& config);
    struct Impl;
    std::unique_ptr<Impl> impl_;
    EntropySrcConfig config_ {};
};
}
