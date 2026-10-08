#include "FinalGuardianEngine.h"
#include "GuardianPhaseKernel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <emmintrin.h>

namespace entropy::guardian
{
static_assert(std::atomic<double>::is_always_lock_free,
              "Guardian telemetry requires lock-free FP64 atomics");

namespace
{
constexpr double pi = 3.141592653589793238462643383279502884;

double besselI0(double value) noexcept
{
    double sum = 1.0;
    double term = 1.0;
    const double quarter = value * value * 0.25;
    for (int order = 1; order < 64; ++order)
    {
        term *= quarter / static_cast<double>(order * order);
        sum += term;
        if (term < sum * 1.0e-19) break;
    }
    return sum;
}

double sinc(double value) noexcept
{
    if (std::abs(value) < 1.0e-15) return 1.0;
    const double angle = pi * value;
    return std::sin(angle) / angle;
}
}

std::uint32_t FinalGuardianEngine::nextPowerOfTwo(std::uint32_t value) noexcept
{
    if (value <= 1) return 1;
    --value;
    value |= value >> 1; value |= value >> 2; value |= value >> 4;
    value |= value >> 8; value |= value >> 16;
    return value + 1;
}

bool FinalGuardianEngine::buildDetector()
{
    try { coefficients_.assign(static_cast<std::size_t>(kPhases) * kTaps, 0.0); }
    catch (...) { return false; }
    constexpr double beta = 18.0;
    constexpr double halfWidth = kTaps * 0.5;
    const double denominator = besselI0(beta);
    maximumKernelL1_ = 1.0;
    for (int phase = 0; phase < kPhases; ++phase)
    {
        const double fraction = static_cast<double>(phase) / kPhases;
        auto* row = coefficients_.data() + static_cast<std::size_t>(phase) * kTaps;
        double sum = 0.0;
        for (int tap = 0; tap < kTaps; ++tap)
        {
            const double distance = static_cast<double>(kFirstOffset + tap) - fraction;
            const double normalized = distance / halfWidth;
            const double window = besselI0(beta * std::sqrt((std::max)(
                0.0, 1.0 - normalized * normalized))) / denominator;
            row[tap] = sinc(distance) * window;
            sum += row[tap];
        }
        if (sum != 0.0)
        {
            double l1 = 0.0;
            for (int tap = 0; tap < kTaps; ++tap)
            {
                row[tap] /= sum;
                l1 += std::abs(row[tap]);
            }
            maximumKernelL1_ = (std::max)(maximumKernelL1_, l1);
        }
    }
    // Configure-time packing only. All 16 phases and all 256 taps are retained.
    static_assert(kPhases == 16 && kTaps == 256);
    useAvx2_ = detail::hasAvx2();
    if (useAvx2_)
    {
        try { coefficientsPhase4_.resize(static_cast<std::size_t>(kPhases) * kTaps); }
        catch (...) { return false; }
        for (int group = 0; group < 4; ++group)
            for (int tap = 0; tap < kTaps; ++tap)
                for (int lane = 0; lane < 4; ++lane)
                    coefficientsPhase4_[group * kTaps * 4 + tap * 4 + lane] =
                        coefficients_[(group * 4 + lane) * kTaps + tap];
    }
    return true;
}

bool FinalGuardianEngine::configure(const EntropyFinalGuardianConfig& requested)
{
    if (requested.struct_size < sizeof(requested) ||
        requested.sample_rate < 8000 || requested.sample_rate > 768000 ||
        requested.channels == 0 || requested.channels > ENTROPY_FINAL_GUARDIAN_MAX_CHANNELS ||
        requested.maximum_frames == 0 || requested.maximum_frames > 65536 ||
        requested.oversample_factor != ENTROPY_FINAL_GUARDIAN_OVERSAMPLE_FACTOR ||
        !std::isfinite(requested.ceiling_dbtp) || requested.ceiling_dbtp > 0.0 ||
        requested.ceiling_dbtp < -12.0 ||
        !std::isfinite(requested.lookahead_ms) || requested.lookahead_ms <= 0.0 ||
        requested.lookahead_ms > 100.0 ||
        !std::isfinite(requested.emergency_fade_ms) || requested.emergency_fade_ms <= 0.0 ||
        requested.emergency_fade_ms > 50.0)
    {
        faultFlags_.fetch_or(ENTROPY_GUARDIAN_FAULT_INVALID_CONFIG, std::memory_order_relaxed);
        state_.store(ENTROPY_GUARDIAN_FAULT_MUTED, std::memory_order_relaxed);
        return false;
    }

    const auto lookahead = static_cast<std::uint32_t>(std::llround(
        requested.sample_rate * requested.lookahead_ms * 0.001));
    const auto fade = static_cast<std::uint32_t>(std::llround(
        requested.sample_rate * requested.emergency_fade_ms * 0.001));
    if (lookahead < fade + kLastOffset + 2)
    {
        faultFlags_.fetch_or(ENTROPY_GUARDIAN_FAULT_INVALID_CONFIG, std::memory_order_relaxed);
        state_.store(ENTROPY_GUARDIAN_FAULT_MUTED, std::memory_order_relaxed);
        return false;
    }

    config_ = requested;
    config_.struct_size = sizeof(config_);
    lookaheadFrames_ = lookahead;
    fadeFrames_ = fade;
    ceilingLinear_ = std::pow(10.0, config_.ceiling_dbtp / 20.0);
    capacityFrames_ = nextPowerOfTwo(lookaheadFrames_ + config_.maximum_frames +
                                      2 * kTaps + 32);
    try
    {
        ring_.assign(static_cast<std::size_t>(config_.channels) * capacityFrames_ * 2u, 0.0);
    }
    catch (...)
    {
        configured_ = false;
        return false;
    }
    if (!buildDetector()) { configured_ = false; return false; }
    configured_ = true;
    reset(false);
    return true;
}

void FinalGuardianEngine::reset(bool preserveInterventionCount) noexcept
{
    if (!configured_) return;
    const auto count = preserveInterventionCount
        ? interventionCount_.load(std::memory_order_relaxed) : 0;
    std::fill(ring_.begin(), ring_.end(), 0.0);
    writeFrame_ = 0;
    nextDetectFrame_ = 0;
    tripFrame_ = (std::numeric_limits<std::int64_t>::max)();
    state_.store(ENTROPY_GUARDIAN_ARMED, std::memory_order_relaxed);
    faultFlags_.store(ENTROPY_GUARDIAN_FAULT_NONE, std::memory_order_relaxed);
    tripChannel_.store(0, std::memory_order_relaxed);
    maximumObservedPeak_.store(0.0, std::memory_order_relaxed);
    tripPeak_.store(0.0, std::memory_order_relaxed);
    tripInputFrame_.store(0, std::memory_order_relaxed);
    processedInputFrames_.store(0, std::memory_order_relaxed);
    producedOutputFrames_.store(0, std::memory_order_relaxed);
    interventionCount_.store(count, std::memory_order_relaxed);
    nonfiniteCount_.store(0, std::memory_order_relaxed);
}

double FinalGuardianEngine::sample(std::uint32_t channel, std::int64_t frame) const noexcept
{
    if (frame < 0 || frame >= writeFrame_) return 0.0;
    const auto slot = static_cast<std::size_t>(
        static_cast<std::uint64_t>(frame) & (capacityFrames_ - 1));
    return ring_[static_cast<std::size_t>(channel) * capacityFrames_ * 2u + slot];
}

double FinalGuardianEngine::reconstruct(std::uint32_t channel,
                                        std::int64_t frame,
                                        int phase) const noexcept
{
    const auto* kernel = coefficients_.data() + static_cast<std::size_t>(phase) * kTaps;
    const auto first = frame + kFirstOffset;
    if (first < 0)
    {
        double sum = 0.0;
        for (int tap = 0; tap < kTaps; ++tap)
            sum += sample(channel, first + tap) * kernel[tap];
        return sum;
    }
    const auto slot = static_cast<std::size_t>(
        static_cast<std::uint64_t>(first) & (capacityFrames_ - 1));
    const auto* samples = ring_.data() +
        static_cast<std::size_t>(channel) * capacityFrames_ * 2u + slot;
    __m128d a = _mm_setzero_pd(), b = a, c = a, d = a;
    for (int tap = 0; tap < kTaps; tap += 8)
    {
        a = _mm_add_pd(a, _mm_mul_pd(_mm_loadu_pd(samples + tap), _mm_loadu_pd(kernel + tap)));
        b = _mm_add_pd(b, _mm_mul_pd(_mm_loadu_pd(samples + tap + 2), _mm_loadu_pd(kernel + tap + 2)));
        c = _mm_add_pd(c, _mm_mul_pd(_mm_loadu_pd(samples + tap + 4), _mm_loadu_pd(kernel + tap + 4)));
        d = _mm_add_pd(d, _mm_mul_pd(_mm_loadu_pd(samples + tap + 6), _mm_loadu_pd(kernel + tap + 6)));
    }
    const auto pair = _mm_add_pd(_mm_add_pd(a, b), _mm_add_pd(c, d));
    double lanes[2];
    _mm_storeu_pd(lanes, pair);
    return lanes[0] + lanes[1];
}

void FinalGuardianEngine::writeInput(const EntropyAudioBlockF64& block) noexcept
{
    const auto mask = static_cast<std::uint64_t>(capacityFrames_ - 1);
    for (std::uint32_t frame = 0; frame < block.frames; ++frame)
    {
        const auto slot = static_cast<std::size_t>(
            static_cast<std::uint64_t>(writeFrame_ + frame) & mask);
        for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        {
            const auto base = static_cast<std::size_t>(channel) * capacityFrames_ * 2u;
            const double value = block.inputs[channel][frame];
            ring_[base + slot] = value;
            ring_[base + slot + capacityFrames_] = value;
        }
    }
    writeFrame_ += block.frames;
}

void FinalGuardianEngine::latchTrip(std::int64_t frame, std::uint32_t channel,
                                    double peak) noexcept
{
    if (tripFrame_ != (std::numeric_limits<std::int64_t>::max)()) return;
    tripFrame_ = frame;
    tripChannel_.store(channel, std::memory_order_relaxed);
    tripPeak_.store(peak, std::memory_order_relaxed);
    tripInputFrame_.store(static_cast<std::uint64_t>(frame), std::memory_order_relaxed);
    interventionCount_.fetch_add(1, std::memory_order_relaxed);
}

void FinalGuardianEngine::detectAvailable() noexcept
{
    double maximum = maximumObservedPeak_.load(std::memory_order_relaxed);
    const auto detectEnd = writeFrame_ - kLastOffset;
    if (nextDetectFrame_ >= detectEnd) return;

    // Once protection is latched, further true-peak work cannot change the
    // fail-safe result.  Preserve the already captured trip evidence and keep
    // the audio thread bounded while the declared fade reaches safe mute.
    if (tripFrame_ != (std::numeric_limits<std::int64_t>::max)())
    {
        nextDetectFrame_ = detectEnd;
        return;
    }

    // Certified conservative fast path.  For every polyphase FIR output,
    // |y| <= max(|x|) * sum(|h|).  Scan the complete sample support needed by
    // all currently available reconstruction frames.  Only when that upper
    // bound is at or below the ceiling may the expensive 16x/256-tap
    // reconstruction be skipped.  Near the ceiling the original detector runs
    // unchanged, so this is a compute optimization, never a quality shortcut.
    std::array<double, ENTROPY_FINAL_GUARDIAN_MAX_CHANNELS> supportMaximum {};
    std::array<bool, ENTROPY_FINAL_GUARDIAN_MAX_CHANNELS> channelNeedsReconstruction {};
    const auto supportFirst = nextDetectFrame_ + kFirstOffset;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        for (auto frame = supportFirst; frame < writeFrame_; ++frame)
            supportMaximum[channel] = (std::max)(supportMaximum[channel],
                                                 std::abs(sample(channel, frame)));

    bool anyChannelNeedsReconstruction = false;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
    {
        const double certifiedUpperBound = supportMaximum[channel] * maximumKernelL1_;
        channelNeedsReconstruction[channel] = certifiedUpperBound > ceilingLinear_;
        anyChannelNeedsReconstruction = anyChannelNeedsReconstruction ||
                                        channelNeedsReconstruction[channel];
        if (!channelNeedsReconstruction[channel])
            maximum = (std::max)(maximum, certifiedUpperBound);
    }
    if (!anyChannelNeedsReconstruction)
    {
        maximumObservedPeak_.store(maximum, std::memory_order_relaxed);
        nextDetectFrame_ = detectEnd;
        return;
    }

    while (nextDetectFrame_ + kLastOffset < writeFrame_)
    {
        for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        {
            // A loud programme channel must not force the full 16x detector
            // onto silent or low-level destination lanes.  The per-channel
            // L1 bound above proves these lanes cannot cross the ceiling.
            if (!channelNeedsReconstruction[channel])
                continue;
            double phases[kPhases];
            const auto first = nextDetectFrame_ + kFirstOffset;
            if (useAvx2_ && first >= 0)
            {
                const auto slot = static_cast<std::size_t>(
                    static_cast<std::uint64_t>(first) & (capacityFrames_ - 1));
                const auto* samples = ring_.data() +
                    static_cast<std::size_t>(channel) * capacityFrames_ * 2u + slot;
                detail::reconstruct16Avx2(samples, coefficientsPhase4_.data(), phases);
            }
            else
            {
                // Preserve the original scalar startup arithmetic and SSE2 fallback.
                for (int phase = 0; phase < kPhases; ++phase)
                    phases[phase] = reconstruct(channel, nextDetectFrame_, phase);
            }
            for (int phase = 0; phase < kPhases; ++phase)
            {
                const double value = phases[phase];
                const double peak = std::abs(value);
                maximum = (std::max)(maximum, peak);
                if (peak > ceilingLinear_)
                    latchTrip(nextDetectFrame_, channel, peak);
            }
        }
        ++nextDetectFrame_;
    }
    maximumObservedPeak_.store(maximum, std::memory_order_relaxed);
}

double FinalGuardianEngine::safetyGain(std::int64_t outputFrame) noexcept
{
    if (tripFrame_ == (std::numeric_limits<std::int64_t>::max)())
    {
        state_.store(ENTROPY_GUARDIAN_ARMED, std::memory_order_relaxed);
        return 1.0;
    }
    const auto fadeStart = tripFrame_ - static_cast<std::int64_t>(fadeFrames_);
    if (outputFrame < fadeStart) return 1.0;
    if (outputFrame >= tripFrame_)
    {
        state_.store(ENTROPY_GUARDIAN_SAFE_MUTED, std::memory_order_relaxed);
        return 0.0;
    }
    state_.store(ENTROPY_GUARDIAN_FADING_TO_SAFE, std::memory_order_relaxed);
    const double position = static_cast<double>(outputFrame - fadeStart) / fadeFrames_;
    return 0.5 * (1.0 + std::cos(pi * position));
}

void FinalGuardianEngine::silence(const EntropyAudioBlockF64& block) noexcept
{
    for (std::uint32_t channel = 0; channel < block.output_channels; ++channel)
        std::fill_n(block.outputs[channel], block.frames, 0.0);
}

EntropyResult FinalGuardianEngine::process(const EntropyAudioBlockF64& block,
                                           const EntropyProcessContext* context) noexcept
{
    if (!configured_) return ENTROPY_ERROR_NOT_PREPARED;
    if (block.struct_size < sizeof(block) || block.frames == 0 ||
        block.frames > config_.maximum_frames ||
        block.input_channels != config_.channels || block.output_channels != config_.channels ||
        block.inputs == nullptr || block.outputs == nullptr)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        if (block.inputs[channel] == nullptr || block.outputs[channel] == nullptr)
            return ENTROPY_ERROR_INVALID_ARGUMENT;

    if (faultFlags_.load(std::memory_order_relaxed) != ENTROPY_GUARDIAN_FAULT_NONE)
    {
        silence(block);
        return ENTROPY_ERROR_PROCESSING;
    }
    if (context != nullptr && (context->flags & ENTROPY_PROCESS_DISCONTINUITY) != 0 &&
        tripFrame_ == (std::numeric_limits<std::int64_t>::max)())
        reset(true);

    bool finite = true;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        for (std::uint32_t frame = 0; frame < block.frames; ++frame)
            if (!std::isfinite(block.inputs[channel][frame]))
            {
                nonfiniteCount_.fetch_add(1, std::memory_order_relaxed);
                finite = false;
            }
    if (!finite)
    {
        faultFlags_.fetch_or(ENTROPY_GUARDIAN_FAULT_NONFINITE_AUDIO, std::memory_order_relaxed);
        state_.store(ENTROPY_GUARDIAN_FAULT_MUTED, std::memory_order_relaxed);
        interventionCount_.fetch_add(1, std::memory_order_relaxed);
        silence(block);
        return ENTROPY_ERROR_PROCESSING;
    }

    const auto firstInputFrame = writeFrame_;
    writeInput(block);
    processedInputFrames_.fetch_add(block.frames, std::memory_order_relaxed);
    detectAvailable();

    for (std::uint32_t frame = 0; frame < block.frames; ++frame)
    {
        const auto outputFrame = firstInputFrame + frame -
            static_cast<std::int64_t>(lookaheadFrames_);
        if (outputFrame < 0)
        {
            for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
                block.outputs[channel][frame] = 0.0;
            continue;
        }
        const double gain = safetyGain(outputFrame);
        for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
            block.outputs[channel][frame] = sample(channel, outputFrame) * gain;
    }
    producedOutputFrames_.fetch_add(block.frames, std::memory_order_relaxed);
    return ENTROPY_OK;
}

EntropyFinalGuardianTelemetry FinalGuardianEngine::telemetry() const noexcept
{
    EntropyFinalGuardianTelemetry value {};
    value.struct_size = sizeof(value);
    value.applied_revision = config_.revision;
    value.state = state_.load(std::memory_order_relaxed);
    value.fault_flags = faultFlags_.load(std::memory_order_relaxed);
    value.sample_rate = config_.sample_rate;
    value.channels = config_.channels;
    value.lookahead_frames = lookaheadFrames_;
    value.emergency_fade_frames = fadeFrames_;
    value.oversample_factor = config_.oversample_factor;
    value.trip_channel = tripChannel_.load(std::memory_order_relaxed);
    value.ceiling_dbtp = config_.ceiling_dbtp;
    value.ceiling_linear = ceilingLinear_;
    value.maximum_observed_true_peak = maximumObservedPeak_.load(std::memory_order_relaxed);
    value.trip_true_peak = tripPeak_.load(std::memory_order_relaxed);
    value.trip_input_frame = tripInputFrame_.load(std::memory_order_relaxed);
    value.processed_input_frames = processedInputFrames_.load(std::memory_order_relaxed);
    value.produced_output_frames = producedOutputFrames_.load(std::memory_order_relaxed);
    value.intervention_count = interventionCount_.load(std::memory_order_relaxed);
    value.nonfinite_sample_count = nonfiniteCount_.load(std::memory_order_relaxed);
    return value;
}
} // namespace entropy::guardian
