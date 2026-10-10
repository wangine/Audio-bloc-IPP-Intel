#pragma once

#include <entropy/dac_sink_abi.h>
#if ENTROPY_A06_R04_DIAG
#include <entropy/r04_diagnostics.h>
#endif

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

struct IAudioClient;
struct IAudioRenderClient;
struct IAudioClock;

namespace entropy::dac
{
class WasapiDacSink final
{
public:
#if ENTROPY_A06_R04_DIAG
    EntropyR04ClockQuality diagnosticOnRenderOwner() const noexcept { return quality_.value; }
#endif
    WasapiDacSink() noexcept;
    ~WasapiDacSink();
    WasapiDacSink(const WasapiDacSink&) = delete;
    WasapiDacSink& operator=(const WasapiDacSink&) = delete;

    void setCancelEvent(void* event) noexcept {cancelEvent_=event;} // borrowed; owner joined before release
    uint64_t clockFrequency() const noexcept {return clockFrequency_;}
    bool latencyValid() const noexcept {return latencyValid_;}
    uint64_t streamLatency100ns() const noexcept {return streamLatency100ns_;}
    double precisePositionFrames() const noexcept {return precisePositionFrames_;}
    bool freshClock() const noexcept {return freshClock_;}
    EntropyResult refreshDevices();
    std::uint32_t deviceCount() const noexcept;
    EntropyResult getDevice(std::uint32_t index, EntropyDacSinkDeviceInfo& info) const noexcept;
    EntropyResult open(const EntropyDacSinkConfig& config);
    EntropyResult close() noexcept;
    EntropyResult start() noexcept;
    EntropyResult stop() noexcept;
    EntropyResult waitBuffer(EntropyDacSinkBufferRequest& request) noexcept;
    EntropyResult submit(EntropyDacSinkPcmBlock& block) noexcept;
    EntropyDacSinkStatus status() const noexcept;

private:
#if ENTROPY_A06_R04_DIAG
    entropy::r04::ClockQuality quality_;
#endif
    struct Device { std::string id; std::string name; bool isDefault = false; };
    void releaseInterfaces() noexcept;
    void setDriverFailure(long result, bool deviceLost) noexcept;
    static bool isDeviceLoss(long result) noexcept;

    std::vector<Device> devices_;
    IAudioClient* audioClient_ = nullptr;
    IAudioRenderClient* renderClient_ = nullptr;
    IAudioClock* audioClock_ = nullptr;
    void* event_ = nullptr;
    void* cancelEvent_=nullptr;uint64_t streamLatency100ns_=0;bool latencyValid_=false;bool freshClock_=false;double precisePositionFrames_=0;
    EntropyDacSinkConfig config_{};
    std::uint32_t bufferFrames_ = 0;
    std::uint32_t blockAlignment_ = 0;
    std::uint64_t clockFrequency_ = 0;
    bool pending_ = false;
    bool ownsCom_ = false;
    std::atomic<std::uint32_t> state_{ENTROPY_DAC_SINK_CLOSED};
    std::atomic<std::uint32_t> faults_{0};
    std::atomic<std::int32_t> lastResult_{0};
    std::atomic<std::uint64_t> submittedFrames_{0};
    std::atomic<std::uint64_t> renderedBuffers_{0};
    std::atomic<std::uint64_t> xruns_{0};
    std::atomic<std::uint64_t> timeouts_{0};
    std::atomic<std::uint64_t> deviceLosses_{0};
    std::atomic<std::uint64_t> devicePosition_{0};
    std::atomic<std::uint64_t> deviceQpc_{0};
};
} // namespace entropy::dac
