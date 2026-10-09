#pragma once

#include "SourceDomainStream.h"
#include "CinemaWdlSrc.h"
#include "SourceReadiness.h"
#include "SourceWorkerWake.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <memory>
#include <entropy/src_abi.h>

namespace entropy::host {

class CinemaSourceDomainStream final {
public:
    CinemaSourceDomainStream() = default;
    ~CinemaSourceDomainStream();
    CinemaSourceDomainStream(const CinemaSourceDomainStream&) = delete;
    CinemaSourceDomainStream& operator=(const CinemaSourceDomainStream&) = delete;

    static bool validConfig(const SourceDomainConfig&) noexcept;
    static bool supportsReferenceRate(unsigned rate) noexcept {
        for(auto supported:referenceRates_) if(rate==supported) return true;
        return false;
    }
    bool prepare(ModuleInstance& endpoint, ModuleInstance& src,
                 const SourceDomainConfig&);
    bool start();
    // Idempotent acknowledgement only. Changing engines requires restartCinema().
    bool requestSrcEngine(std::uint32_t engine) noexcept;
    void stop() noexcept;
    bool ready() const noexcept;
    bool pop1024(double* const* outputs, std::uint64_t& generation) noexcept;
    bool pop1024(double* const* outputs, SourceStreamDescriptor& descriptor) noexcept;
    SourceDomainTelemetry telemetry() const noexcept;

private:
    void captureWorker() noexcept;
    void srcWorker() noexcept;

    ModuleInstance* endpoint_ = nullptr;
    ModuleInstance* src_ = nullptr;
    SourceDomainConfig config_{};
    CinemaWdlSrc cinemaSrc_;
    static constexpr std::array<unsigned,6> referenceRates_{44100,48000,88200,96000,176400,192000};
    std::array<std::unique_ptr<ModuleInstance>,6> referenceSrc_{};
    EntropySrcApiV1 referenceApi_{};
    unsigned outputCapacity_=0;
    // Retained solely for the frozen legacy test translation unit.
    std::atomic<std::uint32_t> requestedSrcEngine_{0};
    SourceFifo192 captureFifo_;
    SourceFifo192 fifo_;
    std::thread captureWorker_;
    std::thread srcWorker_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    SourceReadiness ready_;
    SourceWorkerWake wake_;
    std::atomic<std::uint64_t> activeGeneration_{0};
    std::atomic<std::uint64_t> endpointRead_{0};
    std::atomic<std::uint64_t> srcIn_{0};
    std::atomic<std::uint64_t> srcOut_{0};
    std::atomic<std::uint64_t> resets_{0};
    std::atomic<std::uint64_t> faults_{0};
    std::atomic<std::uint32_t> sourceRate_{0};
    std::atomic<std::uint32_t> state_{0};
    std::atomic<std::uint32_t> latency_{0};
    std::atomic<std::uint32_t> endpointAvailable_{0};
    std::atomic<std::uint32_t> validBits_{0};
    std::atomic<std::uint32_t> publishedChannels_{0};
    std::atomic<std::uint32_t> publishedSrcEngine_{0};
    std::array<std::atomic<double>, 8> peaks_{};
    SourceSignalFreshness signalFreshness_;
};

} // namespace entropy::host

