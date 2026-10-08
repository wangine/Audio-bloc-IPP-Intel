#include "EgressClockEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <emmintrin.h>
#include <limits>

namespace entropy::egress
{
static_assert(std::atomic<double>::is_always_lock_free,
              "Realtime telemetry requires lock-free 64-bit atomics");
static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "SPSC counters require lock-free 64-bit atomics");
namespace
{
bool powerOfTwo(std::uint32_t value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

bool validRate(std::uint32_t value) noexcept
{
    return value >= 8000 && value <= 768000;
}
}

bool EgressClockEngine::configure(const EntropyEgressClockConfig& requested)
{
    if (requested.struct_size < sizeof(requested) ||
        !validRate(requested.engine_rate) || !validRate(requested.sink_rate) ||
        requested.channels == 0 || requested.channels > ENTROPY_EGRESS_CLOCK_MAX_CHANNELS ||
        requested.maximum_input_frames == 0 || requested.maximum_input_frames > 65536 ||
        requested.maximum_render_frames == 0 || requested.maximum_render_frames > 65536 ||
        !powerOfTwo(requested.fifo_capacity_frames) ||
        requested.fifo_capacity_frames < 4096 ||
        requested.target_fill_frames < 512 ||
        requested.target_fill_frames + requested.maximum_render_frames + kGuardFrames >=
            requested.fifo_capacity_frames ||
        !std::isfinite(requested.maximum_correction_ppm) ||
        requested.maximum_correction_ppm < 1.0 || requested.maximum_correction_ppm > 10000.0 ||
        !std::isfinite(requested.correction_smoothing_ms) ||
        requested.correction_smoothing_ms < 0.0 || requested.correction_smoothing_ms > 10000.0)
    {
#if ENTROPY_EGRESS_FIRST_FAULT
        // B2-FF-001: detector record, before downstream propagation.
        firstFault_.capture(ENTROPY_EGRESS_CLOCK_FAULT_INVALID_CONFIG,
            ENTROPY_FF_ORIGIN_CONFIGURE, ENTROPY_FF_SITE_CONFIG,
            ENTROPY_ERROR_INVALID_ARGUMENT, requested);
#endif
        faultFlags_.fetch_or(ENTROPY_EGRESS_CLOCK_FAULT_INVALID_CONFIG, std::memory_order_relaxed);
        state_ = ENTROPY_EGRESS_CLOCK_FAULT;
        return false;
    }

    EntropySrcConfig srcConfig {};
    srcConfig.struct_size = sizeof(srcConfig);
    // Explicit nominal stage: wire at 192 -> 192, R8Brain FP64 at 192 -> 96.
    // The following fractional-delay stage tracks only independent DAC ppm.
    // This statically linked SRC implementation does not load another DLL.
    srcConfig.engine = ENTROPY_SRC_R8BRAIN_REFERENCE;
    srcConfig.source_rate = requested.engine_rate;
    srcConfig.destination_rate = requested.sink_rate;
    srcConfig.channels = requested.channels;
    srcConfig.maximum_input_frames = requested.maximum_input_frames;
    if (!nominalSrc_.configureNominalEgress(srcConfig))
    {
#if ENTROPY_EGRESS_FIRST_FAULT
        // B2-FF-001: detector record, before downstream propagation.
        firstFault_.capture(ENTROPY_EGRESS_CLOCK_FAULT_INVALID_CONFIG,
            ENTROPY_FF_ORIGIN_CONFIGURE, ENTROPY_FF_SITE_NOMINAL_CONFIG,
            ENTROPY_ERROR_INVALID_ARGUMENT, requested);
#endif
        faultFlags_.fetch_or(ENTROPY_EGRESS_CLOCK_FAULT_INVALID_CONFIG, std::memory_order_relaxed);
        state_ = ENTROPY_EGRESS_CLOCK_FAULT;
        return false;
    }

    config_ = requested;
    config_.struct_size = sizeof(config_);
    if (!clockEstimator_.prepare(config_.sink_rate) ||
        !ratioController_.prepare(config_))
    {
#if ENTROPY_EGRESS_FIRST_FAULT
        firstFault_.capture(ENTROPY_EGRESS_CLOCK_FAULT_INVALID_CONFIG,
            ENTROPY_FF_ORIGIN_CONFIGURE, ENTROPY_FF_SITE_CONTROL_PREPARE,
            ENTROPY_ERROR_INVALID_ARGUMENT, requested);
#endif
        faultFlags_.fetch_or(ENTROPY_EGRESS_CLOCK_FAULT_INVALID_CONFIG,
                             std::memory_order_relaxed);
        state_ = ENTROPY_EGRESS_CLOCK_FAULT;
        return false;
    }
    // Beta2: all immutable fractional-delay coefficients are constructed
    // during configure, never lazily from the first render callback.
    asrc::PolyphaseSinc::warmUp();
    const auto outputCapacity = nominalSrc_.config().required_output_capacity;
    try
    {
        // Mirrored per-channel storage makes every 96-tap fractional-delay
        // window contiguous across the power-of-two FIFO wrap point.
        ring_.assign(static_cast<std::size_t>(config_.channels) *
                         config_.fifo_capacity_frames * 2u, 0.0);
        nominalStorage_.assign(static_cast<std::size_t>(config_.channels) *
                                   outputCapacity, 0.0);
    }
    catch (...)
    {
        configured_ = false;
        return false;
    }
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        nominalOutputs_[channel] = nominalStorage_.data() +
            static_cast<std::size_t>(channel) * outputCapacity;
    configured_ = true;
    faultFlags_ = ENTROPY_EGRESS_CLOCK_FAULT_NONE;
    resetStream();
    return true;
}

void EgressClockEngine::resetStream() noexcept
{
    if (!configured_) return;
#if ENTROPY_EGRESS_FIRST_FAULT
    // B2-FF-001: same existing stopped lifecycle. Caller archives before reset.
    firstFault_.resetQuiescent();
#endif
    nominalSrc_.reset();
    std::fill(ring_.begin(), ring_.end(), 0.0);
    writeFrame_.store(kGuardFrames, std::memory_order_relaxed);
    publishedReadFrame_.store(kGuardFrames, std::memory_order_relaxed);
    discontinuityBoundaryFrame_.store(kGuardFrames, std::memory_order_relaxed);
    producerEpoch_.store(0, std::memory_order_relaxed);
    consumerEpoch_ = 0;
    consumerBoundaryFrame_ = kGuardFrames;
    readPosition_ = static_cast<double>(kGuardFrames);
    clockEstimator_.reset();
    ratioController_.reset();
    correctionPpm_ = 0.0;
    requestedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
    publishedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
    active_ = false;
    state_ = ENTROPY_EGRESS_CLOCK_PREROLL;
    inputFrames_ = nominalFrames_ = outputFrames_ = renderCallbacks_ = 0;
    starvedCallbacks_ = resyncCount_ = nonfiniteSamples_ = 0;
    lastSinkPosition_ = lastHostTime100ns_ = 0;
    lastFill_ = 0.0;
}

void EgressClockEngine::reset() noexcept
{
    faultFlags_ = ENTROPY_EGRESS_CLOCK_FAULT_NONE;
    resetStream();
}

bool EgressClockEngine::finite(const double* const* inputs, std::uint32_t frames) noexcept
{
    bool result = true;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        for (std::uint32_t frame = 0; frame < frames; ++frame)
            if (!std::isfinite(inputs[channel][frame]))
            {
                nonfiniteSamples_.fetch_add(1, std::memory_order_relaxed);
                result = false;
            }
    return result;
}

void EgressClockEngine::writeNominal(const double* const* source,
                                     std::uint32_t frames) noexcept
{
    const auto mask = static_cast<std::uint64_t>(config_.fifo_capacity_frames - 1);
    auto writeFrame = writeFrame_.load(std::memory_order_relaxed);
    for (std::uint32_t frame = 0; frame < frames; ++frame)
    {
        const auto slot = static_cast<std::size_t>(
            static_cast<std::uint64_t>(writeFrame) & mask);
        for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        {
            const auto base = static_cast<std::size_t>(channel) *
                              config_.fifo_capacity_frames * 2u;
            ring_[base + slot] = source[channel][frame];
            ring_[base + slot + config_.fifo_capacity_frames] = source[channel][frame];
        }
        ++writeFrame;
    }
    writeFrame_.store(writeFrame, std::memory_order_release);
}

EntropyResult EgressClockEngine::push(EntropyEgressClockPushBlockF64& block) noexcept
{
    block.consumed_frames = 0;
    block.nominal_frames_written = 0;
    if (!configured_) return ENTROPY_ERROR_NOT_PREPARED;
    if (block.struct_size < sizeof(block) || block.channels != config_.channels ||
        block.frames == 0 || block.frames > config_.maximum_input_frames ||
        block.inputs == nullptr)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        if (block.inputs[channel] == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;

    const bool discontinuity =
        (block.flags & ENTROPY_EGRESS_CLOCK_DISCONTINUITY) != 0;
    if (!finite(block.inputs, block.frames))
    {
#if ENTROPY_EGRESS_FIRST_FAULT
        firstFault_.capture(ENTROPY_EGRESS_CLOCK_FAULT_NONFINITE_AUDIO,
            ENTROPY_FF_ORIGIN_PUSH, ENTROPY_FF_SITE_NONFINITE,
            ENTROPY_ERROR_PROCESSING, config_, false, 0, 0, 0, 0,
            block.frames, block.flags);
#endif
        faultFlags_.fetch_or(ENTROPY_EGRESS_CLOCK_FAULT_NONFINITE_AUDIO,
                             std::memory_order_release);
        return ENTROPY_ERROR_PROCESSING;
    }

    // Capacity is checked before advancing the nominal SRC. A rejected
    // producer call must not consume input, mutate SRC history, or overwrite
    // samples still required by the render-side interpolation window.
    // B2-FF-001: capture the actual capacity-check operands, never a later reread.
    const auto writeFrameUsed = writeFrame_.load(std::memory_order_relaxed);
    const auto readFrameUsed = publishedReadFrame_.load(std::memory_order_acquire);
    const auto retained = static_cast<std::uint64_t>(writeFrameUsed - readFrameUsed);
    const auto maximumProduced = config_.engine_rate == config_.sink_rate
        ? static_cast<std::uint64_t>(block.frames)
        : static_cast<std::uint64_t>((std::min)(
            nominalSrc_.config().required_output_capacity,
            src::MultiChannelSrcEngine::requiredOutputCapacity(
                config_.engine_rate, config_.sink_rate, block.frames)));
    if (retained + maximumProduced + kGuardFrames >= config_.fifo_capacity_frames)
    {
#if ENTROPY_EGRESS_FIRST_FAULT
        firstFault_.capture(ENTROPY_EGRESS_CLOCK_FAULT_FIFO_OVERRUN,
            ENTROPY_FF_ORIGIN_PUSH, ENTROPY_FF_SITE_CAPACITY,
            ENTROPY_ERROR_PROCESSING, config_, true, writeFrameUsed, readFrameUsed,
            retained, maximumProduced, block.frames, block.flags, kGuardFrames,
            producerEpoch_.load(std::memory_order_relaxed));
#endif
        faultFlags_.fetch_or(ENTROPY_EGRESS_CLOCK_FAULT_FIFO_OVERRUN,
                             std::memory_order_release);
        return ENTROPY_ERROR_PROCESSING;
    }

    if (discontinuity)
    {
        // Same-rate wire is stateless; nominal R8Brain clears its preallocated
        // filter history without construction or allocation on this owner.
        if (config_.engine_rate != config_.sink_rate)
            nominalSrc_.reset();

        const auto boundary = writeFrame_.load(std::memory_order_relaxed);
        discontinuityBoundaryFrame_.store(boundary, std::memory_order_relaxed);
        producerEpoch_.fetch_add(1, std::memory_order_release);
    }

    EntropySrcBlockF64 converted {};
    converted.struct_size = sizeof(converted);
    converted.channels = config_.channels;
    converted.input_frames = block.frames;
    converted.output_capacity = nominalSrc_.config().required_output_capacity;
    converted.inputs = block.inputs;
    converted.outputs = nominalOutputs_.data();
    converted.flags = (block.flags & ENTROPY_EGRESS_CLOCK_END_OF_STREAM) != 0
        ? ENTROPY_PROCESS_END_OF_STREAM : ENTROPY_PROCESS_NONE;
    const auto result = nominalSrc_.process(converted);
    if (result != ENTROPY_OK) return result;

    writeNominal(const_cast<const double* const*>(nominalOutputs_.data()),
                 converted.produced_frames);
    inputFrames_.fetch_add(converted.consumed_frames, std::memory_order_relaxed);
    nominalFrames_.fetch_add(converted.produced_frames, std::memory_order_relaxed);
    block.consumed_frames = converted.consumed_frames;
    block.nominal_frames_written = converted.produced_frames;
    return ENTROPY_OK;
}

double EgressClockEngine::interpolate(std::uint32_t channel,
                                      std::int64_t lower,
                                      const asrc::PolyphaseSinc::Kernel& kernel) const noexcept
{
    // Cardinal phase copies the stored FP64 sample directly, including -0.0.
    // Fractional phases retain the existing drift ASRC path.
    if (kernel.blend == 0.0 && kernel.first[-asrc::PolyphaseSinc::kFirstOffset] == 1.0)
    {
        const auto cardinalSlot = static_cast<std::size_t>(
            static_cast<std::uint64_t>(lower) & (config_.fifo_capacity_frames - 1));
        return ring_[static_cast<std::size_t>(channel) * config_.fifo_capacity_frames * 2u + cardinalSlot];
    }
    const auto first = lower + asrc::PolyphaseSinc::kFirstOffset;
    const auto slot = static_cast<std::size_t>(
        static_cast<std::uint64_t>(first) &
        static_cast<std::uint64_t>(config_.fifo_capacity_frames - 1));
    const auto* samples = ring_.data() +
        static_cast<std::size_t>(channel) * config_.fifo_capacity_frames * 2u + slot;
    const auto blend = _mm_set1_pd(kernel.blend);
    __m128d a = _mm_setzero_pd(), b = a, c = a, d = a;
    for (int tap = 0; tap < asrc::PolyphaseSinc::kTaps; tap += 8)
    {
        const auto coefficients = [&](int offset) {
            const auto firstRow = _mm_loadu_pd(kernel.first + tap + offset);
            const auto secondRow = _mm_loadu_pd(kernel.second + tap + offset);
            return _mm_add_pd(firstRow,
                _mm_mul_pd(_mm_sub_pd(secondRow, firstRow), blend));
        };
        a = _mm_add_pd(a, _mm_mul_pd(_mm_loadu_pd(samples + tap), coefficients(0)));
        b = _mm_add_pd(b, _mm_mul_pd(_mm_loadu_pd(samples + tap + 2), coefficients(2)));
        c = _mm_add_pd(c, _mm_mul_pd(_mm_loadu_pd(samples + tap + 4), coefficients(4)));
        d = _mm_add_pd(d, _mm_mul_pd(_mm_loadu_pd(samples + tap + 6), coefficients(6)));
    }
    const auto pair = _mm_add_pd(_mm_add_pd(a, b), _mm_add_pd(c, d));
    double lanes[2];
    _mm_storeu_pd(lanes, pair);
    return lanes[0] + lanes[1];
}

void EgressClockEngine::silence(double* const* outputs,
                                std::uint32_t frames) noexcept
{
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        std::fill_n(outputs[channel], frames, 0.0);
}

EntropyResult EgressClockEngine::starve(EntropyEgressClockRenderBlockF64& block,
                                        EntropyEgressClockState state) noexcept
{
    silence(block.outputs, block.frames);
    block.produced_frames = block.frames;
    block.state = state;
    state_ = state;
    if (state == ENTROPY_EGRESS_CLOCK_STARVED)
        starvedCallbacks_.fetch_add(1, std::memory_order_relaxed);
#if ENTROPY_EGRESS_FIRST_FAULT
    // B2-FF-002: diagnostic reason counters retain the exact silence cause.
    firstFault_.silence(state);
#endif
    return ENTROPY_OK;
}

EntropyResult EgressClockEngine::render(EntropyEgressClockRenderBlockF64& block) noexcept
{
    block.produced_frames = 0;
    if (!configured_) return ENTROPY_ERROR_NOT_PREPARED;
    if (block.struct_size < sizeof(block) || block.channels != config_.channels ||
        block.frames == 0 || block.frames > config_.maximum_render_frames ||
        block.outputs == nullptr)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        if (block.outputs[channel] == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;

    renderCallbacks_.fetch_add(1, std::memory_order_relaxed);
#if ENTROPY_A06_R04_DIAG
    diagnostic_={};diagnostic_.callback_sequence=renderCallbacks_.load(std::memory_order_relaxed);
    diagnostic_.read_position_begin=readPosition_;
    diagnostic_.fractional_phase_begin=readPosition_-std::floor(readPosition_);
    diagnostic_.target_fill=config_.target_fill_frames;diagnostic_.fifo_capacity=config_.fifo_capacity_frames;
    diagnostic_.fill_after=static_cast<double>(writeFrame_.load(std::memory_order_acquire))-readPosition_;
    diagnostic_.fill_error=diagnostic_.fill_after-diagnostic_.target_fill;
#endif
    lastSinkPosition_ = block.sink_position_frames;
    lastHostTime100ns_ = block.host_time_100ns;
    if (faultFlags_.load(std::memory_order_relaxed) != ENTROPY_EGRESS_CLOCK_FAULT_NONE)
        return starve(block, ENTROPY_EGRESS_CLOCK_FAULT);
    if ((block.flags & ENTROPY_EGRESS_CLOCK_DISCONTINUITY) != 0)
    {
#if ENTROPY_A06_R04_DIAG
        diagnostic_.engine_reset_reason=1; // render discontinuity
#endif
        active_ = false;
        clockEstimator_.reset();
        ratioController_.reset();
        correctionPpm_ = 0.0;
        requestedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
        publishedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
        resyncCount_.fetch_add(1, std::memory_order_relaxed);
    }

    const auto writeFrame = writeFrame_.load(std::memory_order_acquire);
    const auto producerEpoch = producerEpoch_.load(std::memory_order_acquire);
    if (producerEpoch != consumerEpoch_)
    {
#if ENTROPY_A06_R04_DIAG
        diagnostic_.engine_reset_reason=2; // producer generation boundary
#endif
        const auto previousEpoch = consumerEpoch_;
        consumerEpoch_ = producerEpoch;
        consumerBoundaryFrame_ =
            discontinuityBoundaryFrame_.load(std::memory_order_relaxed);

        // Consumer owns all phase/servo/render state. Old unread frames are
        // intentionally abandoned at the published stream boundary.
        readPosition_ = static_cast<double>(consumerBoundaryFrame_);
        publishedReadFrame_.store(consumerBoundaryFrame_, std::memory_order_release);
        active_ = false;
        clockEstimator_.reset();
        ratioController_.reset();
        correctionPpm_ = 0.0;
        requestedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
        publishedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
        resyncCount_.fetch_add(producerEpoch - previousEpoch,
                               std::memory_order_relaxed);
    }

    double fill = static_cast<double>(writeFrame) - readPosition_;
    const auto target = static_cast<double>(config_.target_fill_frames);
#if ENTROPY_A06_R04_DIAG
    diagnostic_.fill_error=fill-target;diagnostic_.fill_after=fill;
#endif
    if (!active_)
    {
        if (fill < target + asrc::PolyphaseSinc::kLastOffset + block.frames)
            return starve(block, ENTROPY_EGRESS_CLOCK_PREROLL);
        readPosition_ = static_cast<double>(writeFrame) - target;
        active_ = true;
        fill = target;
    }

    const auto oldest = writeFrame - static_cast<std::int64_t>(config_.fifo_capacity_frames);
    if (readPosition_ + asrc::PolyphaseSinc::kFirstOffset < oldest ||
        fill > static_cast<double>(config_.fifo_capacity_frames - kGuardFrames))
    {
#if ENTROPY_A06_R04_DIAG
        diagnostic_.engine_reset_reason=3; // FIFO phase recenter
#endif
        readPosition_ = static_cast<double>(writeFrame) - target;
        clockEstimator_.reset();
        ratioController_.reset();
        correctionPpm_ = 0.0;
        requestedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
        publishedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
        resyncCount_.fetch_add(1, std::memory_order_relaxed);
        fill = target;
        state_ = ENTROPY_EGRESS_CLOCK_RESYNC;
    }

    ClockObservation observation {};
    observation.sinkFrame = block.sink_position_frames;
    observation.host100ns = block.host_time_100ns;
    observation.nominalPublished =
        nominalFrames_.load(std::memory_order_relaxed);
    observation.renderFrames = block.frames;
    observation.fillBefore = fill;
    observation.continuous = active_ &&
        block.sink_position_frames != 0 &&
        block.host_time_100ns != 0;

    const auto estimate = clockEstimator_.observe(observation);
#if ENTROPY_A06_R04_DIAG
    diagnostic_.control_called=1;
    diagnostic_.read_position_begin=readPosition_;
    diagnostic_.fractional_phase_begin=readPosition_-std::floor(readPosition_);
    diagnostic_.fill_error=fill-target;
#endif
    const auto decision = ratioController_.update(observation, estimate);
#if ENTROPY_A06_R04_DIAG
    diagnostic_.control=forensicControlOnRenderThread();
    diagnostic_.delta=ratioController_.diagnosticDelta();
#endif
    correctionPpm_ = decision.appliedPpm;
    requestedCorrectionPpm_.store(decision.requestedPpm,
                                  std::memory_order_relaxed);
    publishedCorrectionPpm_.store(decision.appliedPpm,
                                  std::memory_order_relaxed);

    const double stepBegin = decision.stepBegin;
    const double stepEnd = decision.stepEnd;
    const double stepDelta = stepEnd - stepBegin;
    const double finalPosition = block.frames > 1
        ? readPosition_ +
            static_cast<double>(block.frames - 1) * stepBegin +
            stepDelta * static_cast<double>(block.frames - 2) * 0.5
        : readPosition_;

    if (static_cast<std::int64_t>(std::floor(finalPosition)) +
            asrc::PolyphaseSinc::kLastOffset >= writeFrame)
    {
#if ENTROPY_A06_R04_DIAG
        diagnostic_.engine_reset_reason=4; // ASRC exhausted available input
#endif
        active_ = false;
        clockEstimator_.reset();
        ratioController_.reset();
        correctionPpm_ = 0.0;
        requestedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
        publishedCorrectionPpm_.store(0.0, std::memory_order_relaxed);
        return starve(block, ENTROPY_EGRESS_CLOCK_STARVED);
    }

    for (std::uint32_t frame = 0; frame < block.frames; ++frame)
    {
        const auto lower = static_cast<std::int64_t>(std::floor(readPosition_));
        const auto kernel = asrc::PolyphaseSinc::kernelFor(readPosition_ - lower);
        for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
            block.outputs[channel][frame] = interpolate(channel, lower, kernel);

        const double alpha = block.frames > 1
            ? static_cast<double>(frame) /
              static_cast<double>(block.frames - 1)
            : 1.0;
        readPosition_ += stepBegin + stepDelta * alpha;
    }
    outputFrames_.fetch_add(block.frames, std::memory_order_relaxed);
    lastFill_.store(static_cast<double>(writeFrame) - readPosition_, std::memory_order_relaxed);
#if ENTROPY_A06_R04_DIAG
    diagnostic_.fill_after=static_cast<double>(writeFrame)-readPosition_;
#endif
    publishedReadFrame_.store(static_cast<std::int64_t>(std::floor(readPosition_)),
                              std::memory_order_release);
    state_ = ENTROPY_EGRESS_CLOCK_LOCKED;
    block.produced_frames = block.frames;
    block.state = state_;
    return ENTROPY_OK;
}

EntropyResult EgressClockEngine::clearFault() noexcept
{
    if (!configured_) return ENTROPY_ERROR_NOT_PREPARED;
    faultFlags_ = ENTROPY_EGRESS_CLOCK_FAULT_NONE;
    resetStream();
    return ENTROPY_OK;
}

EntropyEgressClockTelemetry EgressClockEngine::telemetry() const noexcept
{
    EntropyEgressClockTelemetry value {};
    value.struct_size = sizeof(value);
    value.applied_revision = config_.revision;
    value.state = state_;
    value.fault_flags = faultFlags_;
    value.engine_rate = config_.engine_rate;
    value.sink_rate = config_.sink_rate;
    value.channels = config_.channels;
    value.fifo_capacity_frames = config_.fifo_capacity_frames;
    value.fill_frames = lastFill_;
    value.target_fill_frames = config_.target_fill_frames;
    value.correction_ppm = publishedCorrectionPpm_.load(std::memory_order_relaxed);
    value.requested_correction_ppm = requestedCorrectionPpm_.load(std::memory_order_relaxed);
    value.input_frames = inputFrames_;
    value.nominal_frames = nominalFrames_;
    value.output_frames = outputFrames_;
    value.render_callbacks = renderCallbacks_;
    value.starved_callbacks = starvedCallbacks_;
    value.resync_count = resyncCount_;
    value.nonfinite_sample_count = nonfiniteSamples_;
    value.sink_position_frames = lastSinkPosition_;
    value.host_time_100ns = lastHostTime100ns_;
    return value;
}

std::uint32_t EgressClockEngine::latencySinkFrames() const noexcept
{
    if (!configured_) return 0;
    return config_.target_fill_frames + nominalSrc_.latencyOutputFrames();
}
} // namespace entropy::egress
