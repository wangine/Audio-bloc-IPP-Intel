#include "EgressRatioController.h"

#include <algorithm>
#include <cmath>

namespace entropy::egress
{
bool RatioController::prepare(const EntropyEgressClockConfig& config) noexcept
{
    if (config.engine_rate < 8000 || config.engine_rate > 768000 ||
        config.sink_rate < 8000 || config.sink_rate > 768000 ||
        config.target_fill_frames == 0 ||
        !std::isfinite(config.maximum_correction_ppm) ||
        config.maximum_correction_ppm <= 0.0)
    {
        prepared_ = false;
        return false;
    }

    config_ = config;
    prepared_ = true;
    reset();
    return true;
}

void RatioController::resetImpl() noexcept
{
#if ENTROPY_A06_R04_DIAG
    // Lifetime maximum is diagnostic-only and survives algorithm resets.
    const auto maximum=diagnosticDelta_.max_abs_delta_applied;
    diagnosticDelta_={};diagnosticDelta_.max_abs_delta_applied=maximum;
#endif
#if ENTROPY_EGRESS_FORENSIC_TRACE
    // B2-FF-007: retain monotonic diagnostic call sequence across algorithm resets.
    const auto calls=forensic_.controller_calls;forensic_={};forensic_.controller_calls=calls;
#endif
    appliedPpm_ = 0.0;
    requestedPpm_ = 0.0;
    lastHost100ns_ = 0;
    haveHost_ = false;
}

RatioDecision RatioController::updateImpl(const ClockObservation& observation,
                                      const ClockEstimate& estimate) noexcept
{
    RatioDecision decision {};
#if ENTROPY_A06_R04_DIAG
    const auto previousRequested=requestedPpm_;
    const auto previousApplied=appliedPpm_;
    const auto maximum=diagnosticDelta_.max_abs_delta_applied;
    diagnosticDelta_={};diagnosticDelta_.max_abs_delta_applied=maximum;
#endif
    decision.requestedPpm = requestedPpm_;
    decision.appliedPpm = appliedPpm_;
    decision.stepBegin = 1.0 + appliedPpm_ * 1.0e-6;
    decision.stepEnd = decision.stepBegin;
#if ENTROPY_EGRESS_FORENSIC_TRACE
    ++forensic_.controller_calls;forensic_.decision_updated=0;
    forensic_.base_ppm=forensic_.backlog_ppm=0.0; // unavailable while decision_updated=0
    forensic_.requested_ppm=decision.requestedPpm;forensic_.applied_ppm=decision.appliedPpm;
    forensic_.step_begin=decision.stepBegin;forensic_.step_end=decision.stepEnd;
    forensic_.fill_before=observation.fillBefore;
#endif

    if (!prepared_)
        return decision;

    double dt = 0.0;
    const bool clockOrderValid =
        observation.continuous &&
        observation.host100ns != 0 &&
        (!haveHost_ || observation.host100ns > lastHost100ns_);

    if (clockOrderValid)
    {
        if (haveHost_)
            dt = static_cast<double>(
                observation.host100ns - lastHost100ns_) * 1.0e-7;
        lastHost100ns_ = observation.host100ns;
        haveHost_ = true;
    }

    if (!clockOrderValid || !estimate.valid ||
        estimate.sinkFramesPerSecond <= 0.0)
        return decision;

    // The FIFO sample domain is sink_rate after nominal conversion. Producer publication
    // cadence is a host-scheduling observation, not a crystal measurement.
    // Using nominalPublished/time here turned scheduler/generation jitter into
    // false DAC drift and commanded multi-thousand-ppm ASRC motion.
    const double baseStep =
        static_cast<double>(config_.sink_rate) /
        estimate.sinkFramesPerSecond;
    const double basePpm = (baseStep - 1.0) * 1.0e6;

    // FIFO error is not a fast oscillator estimator. It asks only for the
    // extra rate needed to retire backlog over a long recovery horizon.
    const double fillError =
        observation.fillBefore -
        static_cast<double>(config_.target_fill_frames);
    const double rawBacklogPpm =
        fillError /
        (kRecoverySeconds * static_cast<double>(config_.sink_rate)) *
        1.0e6;
    // FIFO occupancy is a slow phase-centering term only. It must never
    // masquerade as oscillator error or drive the 96-tap ASRC through a
    // multi-thousand-ppm cache-thrashing sweep after a scheduler excursion.
    const double backlogPpm = std::clamp(
        rawBacklogPpm, -kBacklogTrimLimitPpm, kBacklogTrimLimitPpm);

    const double limit = config_.maximum_correction_ppm;
    requestedPpm_ = std::clamp(basePpm + backlogPpm, -limit, limit);

    decision.recoveryRequired =
        std::abs(fillError) >
        static_cast<double>((std::max)(observation.renderFrames, 1u));

    const double stepBeginPpm = appliedPpm_;
    if (dt > 0.0)
    {
        // A late callback increases the allowed change only in proportion to
        // elapsed real time. It does not turn FIFO fill into an instantaneous servo.
        const double boundedDt = (std::min)(dt, 0.5);
        const double maxDelta = kSlewPpmPerSecond * boundedDt;
        const double delta = std::clamp(
            requestedPpm_ - appliedPpm_, -maxDelta, maxDelta);
        appliedPpm_ += delta;
    }

    decision.requestedPpm = requestedPpm_;
    decision.appliedPpm = appliedPpm_;
    decision.stepBegin = 1.0 + stepBeginPpm * 1.0e-6;
    decision.stepEnd = 1.0 + appliedPpm_ * 1.0e-6;
#if ENTROPY_A06_R04_DIAG
    diagnosticDelta_.delta_requested=requestedPpm_-previousRequested;
    diagnosticDelta_.delta_applied=appliedPpm_-previousApplied;
    diagnosticDelta_.max_abs_delta_applied=(std::max)(maximum,std::abs(diagnosticDelta_.delta_applied));
    diagnosticDelta_.update_dt_seconds=dt;
    diagnosticDelta_.rate_valid=dt>0.0;
    diagnosticDelta_.applied_ppm_per_second=dt>0.0?diagnosticDelta_.delta_applied/dt:0.0;
#endif
#if ENTROPY_EGRESS_FORENSIC_TRACE
    forensic_.decision_updated=1;forensic_.base_ppm=basePpm;forensic_.backlog_ppm=backlogPpm;
    forensic_.requested_ppm=decision.requestedPpm;forensic_.applied_ppm=decision.appliedPpm;
    forensic_.step_begin=decision.stepBegin;forensic_.step_end=decision.stepEnd;
#endif
    return decision;
}
} // namespace entropy::egress
