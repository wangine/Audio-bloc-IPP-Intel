#pragma once

#include "EgressClockEstimator.h"
#include <entropy/egress_clock_abi.h>
#if ENTROPY_A06_R04_DIAG
#include <entropy/r04_diagnostics_abi.h>
#endif

#include <cstdint>
#include <atomic>
#include <algorithm>
#include <cmath>

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
    struct Quality {
        double requestedPpm=0, appliedPpm=0, deltaApplied=0, maxAbsDeltaApplied=0;
        double maxAbsPpmPerSecond=0, maxResetJumpPpm=0;
        std::uint64_t calls=0, resets=0;
        bool coherent=false;
    };
    // Only the owner calls update/reset. Observation never steers the ratio.
    void reset() noexcept {
        const double jump=std::abs(appliedPpm_);
        resetImpl();
        qualitySequence_.fetch_add(1,std::memory_order_acq_rel);
        maxResetJump_.store((std::max)(maxResetJump_.load(std::memory_order_relaxed),jump),std::memory_order_relaxed);
        resets_.fetch_add(1,std::memory_order_relaxed);
        requested_.store(0,std::memory_order_relaxed);applied_.store(0,std::memory_order_relaxed);delta_.store(0,std::memory_order_relaxed);
        qualitySequence_.fetch_add(1,std::memory_order_release);
    }
    RatioDecision update(const ClockObservation& observation,const ClockEstimate& estimate) noexcept {
        const double previous=appliedPpm_;
        const double dt=haveHost_&&observation.host100ns>lastHost100ns_
            ? double(observation.host100ns-lastHost100ns_)*1e-7 : 0;
        const auto decision=updateImpl(observation,estimate);
        const double delta=decision.appliedPpm-previous;
        qualitySequence_.fetch_add(1,std::memory_order_acq_rel);
        requested_.store(decision.requestedPpm,std::memory_order_relaxed);
        applied_.store(decision.appliedPpm,std::memory_order_relaxed);delta_.store(delta,std::memory_order_relaxed);
        maxDelta_.store((std::max)(maxDelta_.load(std::memory_order_relaxed),std::abs(delta)),std::memory_order_relaxed);
        maxSlope_.store((std::max)(maxSlope_.load(std::memory_order_relaxed),dt>0?std::abs(delta)/dt:0),std::memory_order_relaxed);
        calls_.fetch_add(1,std::memory_order_relaxed);
        qualitySequence_.fetch_add(1,std::memory_order_release);
        return decision;
    }
    Quality quality() const noexcept {
        Quality q;
        for(unsigned attempt=0;attempt<4;++attempt){
            const auto before=qualitySequence_.load(std::memory_order_acquire);if(before&1u)continue;
            q.requestedPpm=requested_.load(std::memory_order_relaxed);q.appliedPpm=applied_.load(std::memory_order_relaxed);
            q.deltaApplied=delta_.load(std::memory_order_relaxed);q.maxAbsDeltaApplied=maxDelta_.load(std::memory_order_relaxed);
            q.maxAbsPpmPerSecond=maxSlope_.load(std::memory_order_relaxed);q.maxResetJumpPpm=maxResetJump_.load(std::memory_order_relaxed);
            q.calls=calls_.load(std::memory_order_relaxed);q.resets=resets_.load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
        if(before==qualitySequence_.load(std::memory_order_acquire)){q.coherent=true;break;}
        }
        return q;
    }

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
    void resetImpl() noexcept;
    RatioDecision updateImpl(const ClockObservation&,const ClockEstimate&) noexcept;
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> qualitySequence_{0}, calls_{0}, resets_{0};
    std::atomic<double> requested_{0}, applied_{0}, delta_{0}, maxDelta_{0}, maxSlope_{0}, maxResetJump_{0};
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
