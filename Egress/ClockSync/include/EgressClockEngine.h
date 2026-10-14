#pragma once

#include "EntropyBandlimitedAsrc.h"
#include "EgressClockEstimator.h"
#include "EgressRatioController.h"
#include "MultiChannelSrcEngine.h"

#include <entropy/egress_clock_abi.h>

#ifndef ENTROPY_EGRESS_FIRST_FAULT
#define ENTROPY_EGRESS_FIRST_FAULT 0
#endif
#if ENTROPY_EGRESS_FIRST_FAULT
#include "EgressFirstFaultCapture.h"
#endif

#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <vector>

namespace entropy::egress
{
class EgressClockEngine
{
public:
#if ENTROPY_A06_R04_DIAG
    EntropyR04ClockSnapshot diagnosticOnRenderOwner() const noexcept {
        auto v=diagnostic_;v.struct_size=sizeof(v);
        v.read_position_end=readPosition_;v.fractional_phase_end=readPosition_-std::floor(readPosition_);
        v.state=state_.load(std::memory_order_relaxed);
        v.phase_valid=v.state==ENTROPY_EGRESS_CLOCK_LOCKED;
        v.input_frames=inputFrames_.load(std::memory_order_relaxed);
        v.nominal_frames=nominalFrames_.load(std::memory_order_relaxed);
        v.output_frames=outputFrames_.load(std::memory_order_relaxed);
        if(!v.control_called){v.control=forensicControlOnRenderThread();v.control.decision_updated=0;v.control.estimate_reason=ENTROPY_EST_NOT_CALLED;v.control.estimate_valid=0;v.control.fill_before=v.target_fill+v.fill_error;v.delta={};v.delta.max_abs_delta_applied=ratioController_.diagnosticDelta().max_abs_delta_applied;}
        return v;
    }
#endif
    // Render owner, before the first render only. Select a position from the
    // host's presentation contract; no change to the estimator/sinc/servo law.
    unsigned lastResetReasonOnOwner() const noexcept {return lastResetReason_;}
    bool observeSilentClock(uint64_t position,uint64_t qpc,unsigned frames) noexcept;
    ClockEstimate clockEstimateOnOwner() const noexcept {return lastEstimate_;}
    bool primeNominalPosition(double offset, unsigned frames) noexcept;
    double nominalReadPosition() const noexcept {return readPosition_-kGuardFrames;}
    bool configure(const EntropyEgressClockConfig& requested);
    void reset() noexcept;
    EntropyResult push(EntropyEgressClockPushBlockF64& block) noexcept;
    EntropyResult render(EntropyEgressClockRenderBlockF64& block) noexcept;
    EntropyResult clearFault() noexcept;
    bool configured() const noexcept { return configured_; }
    const EntropyEgressClockConfig& config() const noexcept { return config_; }
    ClockEstimator::Quality clockQuality() const noexcept {return clockEstimator_.quality();}
    RatioController::Quality ratioQuality() const noexcept {return ratioController_.quality();}
    EntropyEgressClockTelemetry telemetry() const noexcept;
    std::uint32_t latencySinkFrames() const noexcept;
#if ENTROPY_EGRESS_FIRST_FAULT
    EntropyEgressDiagnosticSnapshot diagnostics() const noexcept { return firstFault_.snapshot(); }
#endif
#if ENTROPY_EGRESS_FORENSIC_TRACE
    // B2-FF-007: only the render adapter on the render OWNER thread may call this.
    // The collector receives copies through its queue, never reads these members.
    EntropyForensicControl forensicControlOnRenderThread() const noexcept {
        auto value=clockEstimator_.forensic();ratioController_.copyForensic(value);return value;
    }
#endif

private:
#if ENTROPY_A06_R04_DIAG
    EntropyR04ClockSnapshot diagnostic_{};
#endif
#if ENTROPY_EGRESS_FIRST_FAULT
    // B2-FF-001: opt-in compact recorder; no TEMP_FORENSIC storage/worker.
    EgressFirstFaultCapture firstFault_;
#endif
    static constexpr std::uint32_t kGuardFrames =
        static_cast<std::uint32_t>(asrc::PolyphaseSinc::kTaps + 4);

    void resetStream() noexcept;
    void writeNominal(const double* const* source, std::uint32_t frames) noexcept;
    void silence(double* const* outputs, std::uint32_t frames) noexcept;
    bool finite(const double* const* inputs, std::uint32_t frames) noexcept;
    double interpolate(std::uint32_t channel, std::int64_t lower,
                       const asrc::PolyphaseSinc::Kernel& kernel) const noexcept;
    EntropyResult starve(EntropyEgressClockRenderBlockF64& block,
                         EntropyEgressClockState state) noexcept;

    src::MultiChannelSrcEngine nominalSrc_;
    ClockEstimator clockEstimator_;
    RatioController ratioController_;
    EntropyEgressClockConfig config_ {};
    std::vector<double> ring_;
    std::vector<double> nominalStorage_;
    std::array<double*, ENTROPY_EGRESS_CLOCK_MAX_CHANNELS> nominalOutputs_ {};
    // SPSC ownership:
    // producer(push) owns writeFrame_ and publishes discontinuity epoch/boundary;
    // consumer(render) owns publishedReadFrame_, phase, active state and servo state.
    std::atomic<std::int64_t> writeFrame_ {0};
    std::atomic<std::int64_t> publishedReadFrame_ {0};
    std::atomic<std::int64_t> discontinuityBoundaryFrame_ {0};
    std::atomic<std::uint64_t> producerEpoch_ {0};
    std::uint64_t consumerEpoch_ = 0;
    std::int64_t consumerBoundaryFrame_ = 0;
    double readPosition_ = 0.0;
    unsigned lastResetReason_=0;ClockEstimate lastEstimate_{};
    double correctionPpm_ = 0.0;
    bool configured_ = false;
    bool active_ = false;
    std::atomic<std::uint32_t> faultFlags_ {ENTROPY_EGRESS_CLOCK_FAULT_NONE};
    std::atomic<std::uint32_t> state_ {ENTROPY_EGRESS_CLOCK_PREROLL};
    std::atomic<std::uint64_t> inputFrames_ {0};
    std::atomic<std::uint64_t> nominalFrames_ {0};
    std::atomic<std::uint64_t> outputFrames_ {0};
    std::atomic<std::uint64_t> renderCallbacks_ {0};
    std::atomic<std::uint64_t> starvedCallbacks_ {0};
    std::atomic<std::uint64_t> resyncCount_ {0};
    std::atomic<std::uint64_t> nonfiniteSamples_ {0};
    std::atomic<std::uint64_t> lastSinkPosition_ {0};
    std::atomic<std::uint64_t> lastHostTime100ns_ {0};
    std::atomic<double> lastFill_ {0.0};
    std::atomic<double> publishedCorrectionPpm_ {0.0};
    std::atomic<double> requestedCorrectionPpm_ {0.0};
};
} // namespace entropy::egress
