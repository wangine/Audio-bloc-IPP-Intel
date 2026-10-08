#include "EgressClockEstimator.h"

#include <algorithm>
#include <cmath>

namespace entropy::egress
{
bool ClockEstimator::prepare(std::uint32_t sinkRate) noexcept
{
    if (sinkRate < 8000 || sinkRate > 768000)
    {
        sinkRate_ = 0;
        return false;
    }
    sinkRate_ = sinkRate;
    reset();
    return true;
}

void ClockEstimator::reset() noexcept
{
#if ENTROPY_EGRESS_FORENSIC_TRACE
    // B2-FF-007 TEMP_FORENSIC; reset_site=1 means reset() call, caller unspecified.
    const auto calls=forensic_.observe_calls, resets=forensic_.reset_count;
    forensic_={}; forensic_.observe_calls=calls; forensic_.reset_count=resets+1;
    forensic_.reset_site=1;
#endif
    next_ = 0;
    count_ = 0;
    lastSinkFrame_ = 0;
    lastHost100ns_ = 0;
    lastNominalPublished_ = 0;
    haveLast_ = false;
}

ClockEstimate ClockEstimator::observe(const ClockObservation& observation) noexcept
{
    ClockEstimate result {};
#if ENTROPY_EGRESS_FORENSIC_TRACE
    ++forensic_.observe_calls;
    forensic_.estimate_reason=ENTROPY_EST_NOT_CALLED;
    forensic_.estimate_valid=0; forensic_.window_seconds=0.0;
    forensic_.estimated_sink_hz=forensic_.estimated_nominal_hz=0.0;
    forensic_.estimate_confidence=0.0;
#endif
    if (sinkRate_ == 0)
    {
#if ENTROPY_EGRESS_FORENSIC_TRACE
        rejectForensic(ENTROPY_EST_NOT_PREPARED);
#endif
        return result;
    }

    if (!observation.continuous)
    {
        reset();
#if ENTROPY_EGRESS_FORENSIC_TRACE
        forensic_.reset_site=2; // observe(!continuous)
        rejectForensic(ENTROPY_EST_DISCONTINUOUS);
#endif
        return result;
    }

    if (observation.host100ns == 0 ||
        observation.sinkFrame == 0 ||
        observation.nominalPublished == 0)
    {
#if ENTROPY_EGRESS_FORENSIC_TRACE
        rejectForensic(ENTROPY_EST_ZERO_OBSERVATION);
#endif
        return result;
    }

    bool gapRebased = false;
    if (haveLast_)
    {
        // Sink position + device/QPC time define the oscillator observation.
        // Producer publication is allowed to pause: host scheduling throughput
        // is not a DAC-clock measurement. A backwards producer counter remains
        // a real discontinuity.
        if (observation.host100ns <= lastHost100ns_ ||
            observation.sinkFrame <= lastSinkFrame_ ||
            observation.nominalPublished < lastNominalPublished_)
        {
#if ENTROPY_EGRESS_FORENSIC_TRACE
            rejectForensic(ENTROPY_EST_NONMONOTONIC);
#endif
            return result;
        }

        const double dt = static_cast<double>(
            observation.host100ns - lastHost100ns_) * 1.0e-7;

        if (!(dt > 0.0))
        {
#if ENTROPY_EGRESS_FORENSIC_TRACE
            rejectForensic(ENTROPY_EST_NONMONOTONIC);
#endif
            return result;
        }

        // A >500 ms gap is not oscillator evidence, but rejecting against the
        // same stale anchor forever creates a permanent estimator freeze.
        // Rebase the regression window on the current observation; this sample
        // is accepted as the new anchor but cannot itself produce an estimate.
        if (dt > 0.5)
        {
            next_ = 0;
            count_ = 0;
            haveLast_ = false;
            gapRebased = true;
#if ENTROPY_EGRESS_FORENSIC_TRACE
            ++forensic_.reset_count;
            forensic_.reset_site = 3; // internal gap rebase
#endif
        }
    }

    samples_[next_] = Sample{
        observation.sinkFrame,
        observation.host100ns,
        observation.nominalPublished};
    next_ = (next_ + 1) % kCapacity;
    if (count_ < kCapacity) ++count_;

    lastSinkFrame_ = observation.sinkFrame;
    lastHost100ns_ = observation.host100ns;
    lastNominalPublished_ = observation.nominalPublished;
    haveLast_ = true;
#if ENTROPY_EGRESS_FORENSIC_TRACE
    ++forensic_.accepted_observations;
    forensic_.consecutive_rejections=0;
    forensic_.last_accepted_host100ns=lastHost100ns_;
#endif

    if (count_ < 2)
    {
#if ENTROPY_EGRESS_FORENSIC_TRACE
        forensic_.estimate_reason = gapRebased
            ? ENTROPY_EST_GAP_OVER_500MS
            : ENTROPY_EST_WINDOW_SHORT;
#endif
        return result;
    }

    const auto newestIndex = (next_ + kCapacity - 1) % kCapacity;
    const auto newestHost = samples_[newestIndex].host100ns;
    const std::uint64_t windowTicks =
        static_cast<std::uint64_t>(kWindowSeconds * 1.0e7);
    const auto threshold =
        newestHost > windowTicks ? newestHost - windowTicks : 0;

    const auto oldestStored = (next_ + kCapacity - count_) % kCapacity;
    std::size_t firstOffset = 0;
    while (firstOffset + 1 < count_)
    {
        const auto index = (oldestStored + firstOffset) % kCapacity;
        if (samples_[index].host100ns >= threshold)
            break;
        ++firstOffset;
    }

    const auto firstIndex = (oldestStored + firstOffset) % kCapacity;
    const auto& first = samples_[firstIndex];
    const double spanSeconds =
        static_cast<double>(newestHost - first.host100ns) * 1.0e-7;

#if ENTROPY_EGRESS_FORENSIC_TRACE
    forensic_.window_seconds=spanSeconds;
#endif
    // Do not declare a clock estimate before a real multi-second window exists.
    if (spanSeconds < kWindowSeconds * 0.975)
    {
#if ENTROPY_EGRESS_FORENSIC_TRACE
        forensic_.estimate_reason=ENTROPY_EST_WINDOW_SHORT;
#endif
        return result;
    }

    long double sumX = 0.0L;
    long double sumXX = 0.0L;
    long double sumSink = 0.0L;
    long double sumNominal = 0.0L;
    long double sumXSink = 0.0L;
    long double sumXNominal = 0.0L;
    std::size_t n = 0;

    for (std::size_t offset = firstOffset; offset < count_; ++offset)
    {
        const auto index = (oldestStored + offset) % kCapacity;
        const auto& sample = samples_[index];
        const long double x =
            static_cast<long double>(sample.host100ns - first.host100ns) * 1.0e-7L;
        const long double sink =
            static_cast<long double>(sample.sinkFrame - first.sinkFrame);
        const long double nominal =
            static_cast<long double>(sample.nominalPublished -
                                     first.nominalPublished);

        sumX += x;
        sumXX += x * x;
        sumSink += sink;
        sumNominal += nominal;
        sumXSink += x * sink;
        sumXNominal += x * nominal;
        ++n;
    }

    if (n < 32)
    {
#if ENTROPY_EGRESS_FORENSIC_TRACE
        forensic_.estimate_reason=ENTROPY_EST_TOO_FEW_POINTS;
#endif
        return result;
    }

    const long double denominator =
        static_cast<long double>(n) * sumXX - sumX * sumX;
    if (!(denominator > 0.0L))
    {
#if ENTROPY_EGRESS_FORENSIC_TRACE
        forensic_.estimate_reason=ENTROPY_EST_DEGENERATE;
#endif
        return result;
    }

    const long double sinkSlope =
        (static_cast<long double>(n) * sumXSink - sumX * sumSink) /
        denominator;
    const long double nominalSlope =
        (static_cast<long double>(n) * sumXNominal - sumX * sumNominal) /
        denominator;

    const double sinkRate = static_cast<double>(sinkSlope);
    const double nominalRate = static_cast<double>(nominalSlope);
    const double nominal = static_cast<double>(sinkRate_);

    // Only the sink slope is oscillator evidence used by the DAC ratio.
    // nominalRate is retained as producer-throughput telemetry; scheduler stalls
    // or generation transitions must not invalidate or steer the sink estimate.
    if (!std::isfinite(sinkRate) ||
        sinkRate < nominal * 0.5 || sinkRate > nominal * 1.5)
    {
#if ENTROPY_EGRESS_FORENSIC_TRACE
        forensic_.estimate_reason=ENTROPY_EST_RATE_INVALID;
#endif
        return result;
    }

    result.sinkFramesPerSecond = sinkRate;
    result.effectiveNominalFramesPerSecond =
        std::isfinite(nominalRate) && nominalRate >= 0.0 ? nominalRate : 0.0;
    result.confidence = std::clamp(spanSeconds / kWindowSeconds, 0.0, 1.0);
    result.valid = true;
#if ENTROPY_EGRESS_FORENSIC_TRACE
    forensic_.estimate_reason=ENTROPY_EST_VALID; forensic_.estimate_valid=1;
    forensic_.last_valid_host100ns=observation.host100ns;
    forensic_.estimated_sink_hz=result.sinkFramesPerSecond;
    forensic_.estimated_nominal_hz=result.effectiveNominalFramesPerSecond;
    forensic_.estimate_confidence=result.confidence;
#endif
    return result;
}
} // namespace entropy::egress
