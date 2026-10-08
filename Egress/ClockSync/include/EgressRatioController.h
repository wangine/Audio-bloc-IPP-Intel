#pragma once

#include "EgressClockEstimator.h"
#include <entropy/egress_clock_abi.h>
#if ENTROPY_A06_R04_DIAG
#include <entropy/r04_diagnostics_abi.h>
#endif

#include <cstdint>

namespace entropy::egress
{
struct RatioDecision
{
    double requestedPpm = 0.0;
    double appliedPpm = 0.0;
    double stepBegin = 1.0;
    double stepEnd = 1.0;
    bool recoveryRequired = false;
};

class RatioController
{
public:
#if ENTROPY_A06_R04_DIAG
    EntropyR04ControlDelta diagnosticDelta() const noexcept { return diagnosticDelta_; }
#endif
    bool prepare(const EntropyEgressClockConfig& config) noexcept;
    void reset() noexcept;
    RatioDecision update(const ClockObservation& observation,
                         const ClockEstimate& estimate) noexcept;

#if ENTROPY_EGRESS_FORENSIC_TRACE
    // B2-FF-007 owner-thread only. Copies already-computed decisions.
    void copyForensic(EntropyForensicControl& out) const noexcept {
        out.controller_calls=forensic_.controller_calls;
        out.decision_updated=forensic_.decision_updated;
        out.base_ppm=forensic_.base_ppm; out.backlog_ppm=forensic_.backlog_ppm;
        out.requested_ppm=forensic_.requested_ppm; out.applied_ppm=forensic_.applied_ppm;
        out.step_begin=forensic_.step_begin; out.step_end=forensic_.step_end;
        out.fill_before=forensic_.fill_before;
    }
#endif
private:
#if ENTROPY_A06_R04_DIAG
    EntropyR04ControlDelta diagnosticDelta_{};
#endif
#if ENTROPY_EGRESS_FORENSIC_TRACE
    EntropyForensicControl forensic_ {};
#endif
    static constexpr double kRecoverySeconds = 30.0;
    static constexpr double kBacklogTrimLimitPpm = 100.0;
    static constexpr double kSlewPpmPerSecond = 50.0;

    EntropyEgressClockConfig config_ {};
    double appliedPpm_ = 0.0;
    double requestedPpm_ = 0.0;
    std::uint64_t lastHost100ns_ = 0;
    bool haveHost_ = false;
    bool prepared_ = false;
};
} // namespace entropy::egress
