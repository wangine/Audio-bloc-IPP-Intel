#pragma once
#include "EndpointLossWindow.h"
#include "ModuleRuntime.h"
#include "SourceFifo192.h"
#include "SourceReadiness.h"
#include "SourceWorkerWake.h"
#include "SourceSignalFreshness.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace entropy::host {

struct SourceDomainConfig {
    std::uint32_t channels=0, destinationRate=192000, dspBlockFrames=1024;
    std::uint32_t fifoCapacityFrames=131072, prerollFrames=32768;
    std::uint32_t captureFifoCapacityFrames=65536, outputTargetFrames=65536;
    std::uint32_t srcEngine=0, endpointReadFrames=2048, srcInputFrames=256;
};

struct SourceDomainTelemetry {
    bool capture_failed=false;
    std::uint32_t state=0, source_rate=0, channels=0;
    std::uint32_t container_bits=0, valid_bits=0, discontinuity_flags=0;
    std::uint32_t src_engine=0, src_latency_frames=0;
    std::uint64_t generation=0, endpoint_frames_read=0, src_input_frames=0, src_output_frames=0;
    std::uint64_t endpoint_lost_frames=0, resets=0, faults=0;
    std::uint32_t endpoint_available_frames=0;
    double peak_left=0.0, peak_right=0.0;
    std::array<double, 8> channel_peaks{};
    SourceFifo192Telemetry fifo{};
    SourceFifo192Telemetry capture_fifo{};
    std::uint32_t preroll_frames=0, target_frames=0;
};

class SourceDomainStream final {
public:
    SourceDomainStream()=default;
    ~SourceDomainStream();
    SourceDomainStream(const SourceDomainStream&)=delete;
    SourceDomainStream& operator=(const SourceDomainStream&)=delete;

    static bool validConfig(const SourceDomainConfig&) noexcept;
    bool prepare(ModuleInstance& endpoint, ModuleInstance& src, const SourceDomainConfig&);
    bool start();
    void stop() noexcept;
    bool ready() const noexcept;
    bool pop1024(double* const* outputs, std::uint64_t& generation) noexcept;
    bool pop1024(double* const* outputs, SourceStreamDescriptor& descriptor) noexcept;
    SourceDomainTelemetry telemetry() const noexcept;

private:
    void captureWorker() noexcept;
    void srcWorker() noexcept;
    ModuleInstance* endpoint_=nullptr; ModuleInstance* src_=nullptr;
    SourceDomainConfig config_{};
    SourceFifo192 captureFifo_;
    SourceFifo192 fifo_;
    std::thread captureWorker_, srcWorker_;
    std::atomic<bool> stop_{false}, running_{false};
    SourceReadiness ready_;
    SourceWorkerWake wake_;
    std::atomic<std::uint64_t> activeGeneration_{0};
    std::atomic<std::uint64_t> endpointRead_{0}, srcIn_{0}, srcOut_{0}, endpointLost_{0}, resets_{0}, faults_{0};
    std::atomic<std::uint32_t> sourceRate_{0}, state_{0}, latency_{0}, endpointAvailable_{0};
    std::atomic<std::uint32_t> containerBits_{0}, validBits_{0}, discontinuityFlags_{0};
    std::atomic<double> peakLeft_{0.0}, peakRight_{0.0};
    EndpointLossWindow endpointLossWindow_;
    SourceSignalFreshness signalFreshness_;
};

} // namespace entropy::host

