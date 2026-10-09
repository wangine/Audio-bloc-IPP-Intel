#include "HeadroomEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace entropy::headroom
{
namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
constexpr std::uint32_t mask16 = 0xffffu;
static_assert(std::atomic<double>::is_always_lock_free);
}

void HeadroomEngine::Ramp::begin(double value, std::uint32_t frames) noexcept
{
    if (value == target && remaining == 0) return;
    start = current;
    target = value;
    total = (std::max)(1u, frames);
    remaining = current == target ? 0u : total;
}

void HeadroomEngine::Ramp::force(double value) noexcept
{
    current = start = target = value;
    total = 1;
    remaining = 0;
}

double HeadroomEngine::Ramp::next() noexcept
{
    if (!remaining) return current;
    const auto elapsed = total - remaining + 1;
    const auto phase = static_cast<double>(elapsed) / total;
    const auto weight = 0.5 - 0.5 * std::cos(pi * phase);
    current = start + (target - start) * weight;
    if (--remaining == 0) current = target;
    return current;
}

double HeadroomEngine::Ramp::progress() const noexcept
{
    return remaining ? static_cast<double>(total - remaining) / total : 1.0;
}

bool HeadroomEngine::prepare(std::uint32_t rate, std::uint32_t frames) noexcept
{
    if (rate < 8000 || rate > 768000 || !frames || frames > 65536) return false;
    rate_ = rate;
    maxFrames_ = frames;
    prepared_ = true;
    reset();
    return true;
}

void HeadroomEngine::reset() noexcept
{
    for (auto& ramp : ramps_) ramp = {};
    audio_ = {};
    audio_.revision = (std::numeric_limits<std::uint32_t>::max)();
    audio_.mode = (std::numeric_limits<std::uint32_t>::max)();
    audio_.policyRevision = (std::numeric_limits<std::uint32_t>::max)();
    for (auto& value : currentTelemetry_) value.store(1.0, std::memory_order_relaxed);
    for (auto& value : progressTelemetry_) value.store(1.0, std::memory_order_relaxed);
    effectiveMode_.store(ENTROPY_HEADROOM_MODE_ACTIVE, std::memory_order_relaxed);
    modeTransitionActive_.store(0, std::memory_order_relaxed);
    modeTransitionProgress_.store(1.0, std::memory_order_relaxed);
    processed_.store(0, std::memory_order_relaxed);
    nonfinite_.store(0, std::memory_order_relaxed);
    appliedRevision_.store(0, std::memory_order_relaxed);
    measuredFinalPeak_.store(0.0, std::memory_order_relaxed);
    for (auto& value : measuredChannelPeak_) value.store(0.0, std::memory_order_relaxed);
    limitingChannelMask_.store(0, std::memory_order_relaxed);
    autoAnalysisReductionDb_.store(0.0, std::memory_order_relaxed);
    analyzerState_.store(ENTROPY_HEADROOM_ANALYZER_IDLE, std::memory_order_relaxed);
    autoTargets_.fill(1.0);
    for (auto& target : autoTargetTelemetry_) target.store(1.0);
    terminalAnalysis_ = false;
    autoHoldRemaining_.fill(0);
    // A reset invalidates the previous configuration. A subsequent valid
    // setState() is the authority that clears only configuration/ledger faults.
    validState_.store(false, std::memory_order_release);
    faults_.store(ENTROPY_HEADROOM_FAULT_INVALID_CONFIG, std::memory_order_release);
}

EntropyResult HeadroomEngine::setState(const EntropyHeadroomState& state) noexcept
{
    const auto invalid = [this](std::uint32_t fault) {
        faults_.fetch_or(fault, std::memory_order_release);
        validState_.store(false, std::memory_order_release);
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    };
    if (!prepared_) return ENTROPY_ERROR_NOT_PREPARED;
    if (state.struct_size < sizeof(state) || state.group_count == 0 ||
        state.group_count > 16 || (state.active_channel_mask & ~mask16) ||
        !std::isfinite(state.transition_ms) || state.transition_ms < 1.0 ||
        state.transition_ms > 10000.0)
        return invalid(ENTROPY_HEADROOM_FAULT_INVALID_CONFIG);
    for (std::uint32_t channel = 0; channel < 16; ++channel)
        if (state.channel_group[channel] >= state.group_count)
            return invalid(ENTROPY_HEADROOM_FAULT_INVALID_CONFIG);

    std::array<double, 16> targets{}, required{}, margins{};
    for (std::uint32_t group = 0; group < state.group_count; ++group)
    {
        const auto& entry = state.groups[group];
        if (entry.struct_size < sizeof(entry) || entry.bounded != 1 ||
            !std::isfinite(entry.declared_positive_contribution_db) ||
            !std::isfinite(entry.guaranteed_input_headroom_db) ||
            !std::isfinite(entry.preventive_reserve_db) ||
            entry.declared_positive_contribution_db < 0.0 ||
            entry.guaranteed_input_headroom_db < 0.0 ||
            entry.preventive_reserve_db < 0.0 ||
            entry.declared_positive_contribution_db > 300.0 ||
            entry.guaranteed_input_headroom_db > 300.0 ||
            entry.preventive_reserve_db > 300.0)
            return invalid(ENTROPY_HEADROOM_FAULT_INVALID_LEDGER);
        required[group] = entry.declared_positive_contribution_db +
                          entry.preventive_reserve_db;
        const auto reduction = (std::max)(0.0,
            required[group] - entry.guaranteed_input_headroom_db);
        targets[group] = std::pow(10.0, -reduction / 20.0);
        margins[group] = entry.guaranteed_input_headroom_db + reduction -
                         required[group];
    }
    const auto transition = (std::max)(1u, static_cast<std::uint32_t>(
        std::llround(static_cast<double>(rate_) * state.transition_ms / 1000.0)));

    sequence_.fetch_add(1, std::memory_order_acq_rel);
    revision_.store(state.revision, std::memory_order_relaxed);
    groupCount_.store(state.group_count, std::memory_order_relaxed);
    activeMask_.store(state.active_channel_mask, std::memory_order_relaxed);
    transitionSamples_.store(transition, std::memory_order_relaxed);
    for (std::uint32_t channel = 0; channel < 16; ++channel)
        channelGroup_[channel].store(state.channel_group[channel],
                                     std::memory_order_relaxed);
    for (std::uint32_t group = 0; group < 16; ++group)
    {
        target_[group].store(group < state.group_count ? targets[group] : 1.0,
                             std::memory_order_relaxed);
        required_[group].store(group < state.group_count ? required[group] : 0.0,
                               std::memory_order_relaxed);
        margin_[group].store(group < state.group_count ? margins[group] : 0.0,
                             std::memory_order_relaxed);
        const auto& entry = state.groups[group];
        positive_[group].store(group < state.group_count
            ? entry.declared_positive_contribution_db : 0.0, std::memory_order_relaxed);
        inputHeadroom_[group].store(group < state.group_count
            ? entry.guaranteed_input_headroom_db : 0.0, std::memory_order_relaxed);
        reserve_[group].store(group < state.group_count
            ? entry.preventive_reserve_db : 0.0, std::memory_order_relaxed);
        bounded_[group].store(group < state.group_count ? entry.bounded : 0,
                              std::memory_order_relaxed);
    }
    sequence_.fetch_add(1, std::memory_order_release);
    validState_.store(true, std::memory_order_release);
    // A valid configuration supersedes stale startup/configuration ledger
    // sentinels. Preserve genuine audio-domain NONFINITE evidence.
    faults_.fetch_and(ENTROPY_HEADROOM_FAULT_NONFINITE_AUDIO,
                      std::memory_order_release);
    return ENTROPY_OK;
}

EntropyResult HeadroomEngine::setMode(std::uint32_t mode) noexcept
{
    if (!prepared_) return ENTROPY_ERROR_NOT_PREPARED;
    if (mode != ENTROPY_HEADROOM_MODE_AUTO &&
        mode != ENTROPY_HEADROOM_MODE_BIT_PERFECT &&
        mode != ENTROPY_HEADROOM_MODE_MANUAL_SAFE)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    sequence_.fetch_add(1, std::memory_order_acq_rel);
    requestedMode_.store(mode, std::memory_order_relaxed);
    policyRevision_.fetch_add(1, std::memory_order_relaxed);
    sequence_.fetch_add(1, std::memory_order_release);
    return ENTROPY_OK;
}

EntropyResult HeadroomEngine::setPolicy(std::uint32_t mode,
                                        double fixedAttenuationDb) noexcept
{
    if (!std::isfinite(fixedAttenuationDb) || fixedAttenuationDb < 0.0 ||
        fixedAttenuationDb > 300.0)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto result = setManualGainDb(-fixedAttenuationDb);
    if (result != ENTROPY_OK) return result;
    return setMode(mode);
}

EntropyResult HeadroomEngine::setManualGainDb(double manualGainDb) noexcept
{
    if (!prepared_) return ENTROPY_ERROR_NOT_PREPARED;
    if (!std::isfinite(manualGainDb) || manualGainDb > 0.0 || manualGainDb < -300.0)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto manualTarget = std::pow(10.0, manualGainDb / 20.0);
    sequence_.fetch_add(1, std::memory_order_acq_rel);
    manualGainDb_.store(manualGainDb, std::memory_order_relaxed);
    manualTarget_.store(manualTarget, std::memory_order_relaxed);
    policyRevision_.fetch_add(1, std::memory_order_relaxed);
    sequence_.fetch_add(1, std::memory_order_release);
    return ENTROPY_OK;
}

bool HeadroomEngine::acquire(Snapshot& result) const noexcept
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const auto before = sequence_.load(std::memory_order_acquire);
        if (before & 1u) continue;
        Snapshot next{};
        next.revision = revision_.load(std::memory_order_relaxed);
        next.groupCount = groupCount_.load(std::memory_order_relaxed);
        next.activeMask = activeMask_.load(std::memory_order_relaxed);
        next.transitionSamples = transitionSamples_.load(std::memory_order_relaxed);
        next.mode = requestedMode_.load(std::memory_order_relaxed);
        next.policyRevision = policyRevision_.load(std::memory_order_relaxed);
        next.manualGainDb = manualGainDb_.load(std::memory_order_relaxed);
        next.manualTarget = manualTarget_.load(std::memory_order_relaxed);
        for (std::uint32_t channel = 0; channel < 16; ++channel)
            next.channelGroup[channel] = channelGroup_[channel].load(std::memory_order_relaxed);
        for (std::uint32_t group = 0; group < 16; ++group)
        {
            next.target[group] = target_[group].load(std::memory_order_relaxed);
            next.required[group] = required_[group].load(std::memory_order_relaxed);
            next.margin[group] = margin_[group].load(std::memory_order_relaxed);
        }
        if (before == sequence_.load(std::memory_order_acquire))
        {
            result = next;
            return true;
        }
    }
    return false;
}

EntropyResult HeadroomEngine::setTiming(const EntropyHeadroomTimingStateV5& value) noexcept
{
    if (!prepared_) return ENTROPY_ERROR_NOT_PREPARED;
    if (value.struct_size < sizeof(value) || !std::isfinite(value.attack_ms) ||
        !std::isfinite(value.release_ms) || value.attack_ms < 100.0 ||
        value.attack_ms > 2000.0 || value.release_ms < 1000.0 || value.release_ms > 10000.0)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    timingSequence_.fetch_add(1);
    attackMs_.store(value.attack_ms);
    releaseMs_.store(value.release_ms);
    timingSequence_.fetch_add(1);
    return ENTROPY_OK;
}

EntropyHeadroomTimingStateV5 HeadroomEngine::timing() const noexcept
{
    EntropyHeadroomTimingStateV5 result{sizeof(result),0,500.0,4000.0};
    for (;;) {
        const auto before=timingSequence_.load();
        if (before & 1u) continue;
        result.attack_ms=attackMs_.load(); result.release_ms=releaseMs_.load();
        if (before==timingSequence_.load()) {
            result.revision=static_cast<std::uint32_t>(before/2); return result;
        }
    }
}

EntropyHeadroomState HeadroomEngine::state() const noexcept
{
    EntropyHeadroomState result{};
    result.struct_size = sizeof(result);
    result.revision = revision_.load();
    result.group_count = groupCount_.load();
    result.active_channel_mask = activeMask_.load();
    result.transition_ms = rate_ ? 1000.0 * transitionSamples_.load() / rate_ : 0.0;
    for (std::uint32_t channel = 0; channel < 16; ++channel)
        result.channel_group[channel] = channelGroup_[channel].load();
    for (std::uint32_t group = 0; group < 16; ++group)
    {
        auto& entry = result.groups[group];
        entry.struct_size = sizeof(entry);
        entry.bounded = bounded_[group].load();
        entry.declared_positive_contribution_db = positive_[group].load();
        entry.guaranteed_input_headroom_db = inputHeadroom_[group].load();
        entry.preventive_reserve_db = reserve_[group].load();
    }
    return result;
}

EntropyHeadroomModeState HeadroomEngine::modeState() const noexcept
{
    EntropyHeadroomModeState result{};
    result.struct_size = sizeof(result);
    result.requested_mode = requestedMode_.load();
    result.effective_mode = effectiveMode_.load();
    result.transition_active = modeTransitionActive_.load();
    result.transition_progress = modeTransitionProgress_.load();
    return result;
}

EntropyHeadroomPolicyStateV2 HeadroomEngine::policyState() const noexcept
{
    EntropyHeadroomPolicyStateV2 result{};
    result.struct_size = sizeof(result);
    result.requested_mode = requestedMode_.load();
    result.effective_mode = effectiveMode_.load();
    result.transition_active = modeTransitionActive_.load();
    result.transition_progress = modeTransitionProgress_.load();
    result.fixed_attenuation_db = -manualGainDb_.load();
    result.fixed_target_gain = manualTarget_.load();
    return result;
}

EntropyHeadroomControlStateV3 HeadroomEngine::controlState() const noexcept
{
    EntropyHeadroomControlStateV3 result{};
    result.struct_size = sizeof(result);
    result.requested_mode = requestedMode_.load();
    result.effective_mode = effectiveMode_.load();
    result.transition_active = modeTransitionActive_.load();
    result.transition_progress = modeTransitionProgress_.load();
    result.manual_gain_db = manualGainDb_.load();
    result.manual_target_gain = manualTarget_.load();
    return result;
}

EntropyResult HeadroomEngine::clearFault() noexcept
{
    if (!prepared_) return ENTROPY_ERROR_NOT_PREPARED;
    if (!validState_.load(std::memory_order_acquire)) return ENTROPY_ERROR_INVALID_STATE;
    faults_.store(0, std::memory_order_release);
    return ENTROPY_OK;
}

EntropyHeadroomTelemetry HeadroomEngine::telemetry() const noexcept
{
    EntropyHeadroomTelemetry result{};
    result.struct_size = sizeof(result);
    result.applied_revision = appliedRevision_.load();
    result.group_count = groupCount_.load();
    result.fault_flags = faults_.load();
    result.host_engine_rate = rate_;
    result.transition_samples = transitionSamples_.load();
    result.nonfinite_sample_count = nonfinite_.load();
    result.processed_frames = processed_.load();
    const auto mode = requestedMode_.load();
    for (std::uint32_t group = 0; group < 16; ++group)
    {
        auto& entry = result.groups[group];
        entry.current_gain = currentTelemetry_[group].load();
        entry.target_gain = mode == ENTROPY_HEADROOM_MODE_BIT_PERFECT
            ? 1.0 : mode == ENTROPY_HEADROOM_MODE_MANUAL_SAFE
            ? manualTarget_.load() : autoTargetTelemetry_[group].load();
        entry.gain_reduction_db = entry.current_gain > 0.0
            ? -20.0 * std::log10(entry.current_gain) : 300.0;
        entry.ledger_required_headroom_db = required_[group].load();
        entry.reserve_margin_db = margin_[group].load();
        entry.transition_progress = progressTelemetry_[group].load();
        entry.status = result.fault_flags ? ENTROPY_HEADROOM_GROUP_FAULT
            : entry.transition_progress < 1.0 ? ENTROPY_HEADROOM_GROUP_TRANSITION
            : entry.current_gain == 1.0 ? ENTROPY_HEADROOM_GROUP_UNITY
                                        : ENTROPY_HEADROOM_GROUP_REDUCED;
    }
    // Retained field names preserve the V1 telemetry layout. Since V4 these
    // values explicitly describe the late, pre-Guardian final FP64 analyzer.
    result.measured_input_peak = measuredFinalPeak_.load();
    result.measured_input_true_peak = 0.0;
    result.auto_analysis_reduction_db = autoAnalysisReductionDb_.load();
    result.analyzer_state = analyzerState_.load();
    result.limiting_channel_mask = limitingChannelMask_.load();
    for (std::uint32_t channel = 0; channel < 16; ++channel)
        result.channel_input_peak[channel] = measuredChannelPeak_[channel].load();
    result.true_peak_status = 0; // NOT_IMPLEMENTED: sample peak only in this candidate.
    return result;
}

void HeadroomEngine::synchronizeAudioState() noexcept
{
    // Bounded read on the audio thread; a concurrent control write is retried
    // next block. Existing ramps keep their stored sample duration.
    for (int attempt=0;attempt<4;++attempt) {
        const auto before=timingSequence_.load();
        if(before & 1u) continue;
        const double attack=attackMs_.load(), release=releaseMs_.load();
        if(before==timingSequence_.load()) { audioAttackMs_=attack; audioReleaseMs_=release; break; }
    }
    Snapshot next{};
    if (acquire(next) &&
        (next.revision != audio_.revision || next.mode != audio_.mode ||
         next.policyRevision != audio_.policyRevision))
    {
        audio_ = next;
        for (std::uint32_t group = 0; group < audio_.groupCount; ++group)
        {
            autoTargets_[group] = terminalAnalysis_ ? 1.0 : audio_.target[group];
            autoTargetTelemetry_[group].store(autoTargets_[group]);
            autoHoldRemaining_[group] = 0;
            const auto desired = audio_.mode == ENTROPY_HEADROOM_MODE_BIT_PERFECT
                ? 1.0 : audio_.mode == ENTROPY_HEADROOM_MODE_MANUAL_SAFE
                ? audio_.manualTarget : autoTargets_[group];
            ramps_[group].begin(desired,
                terminalAnalysis_ && audio_.mode == ENTROPY_HEADROOM_MODE_AUTO
                    ? static_cast<std::uint32_t>(std::llround(rate_*audioAttackMs_/1000.0))
                    : audio_.transitionSamples);
        }
        appliedRevision_.store(audio_.revision, std::memory_order_release);
    }
}

EntropyResult HeadroomEngine::process(const EntropyAudioBlockF64& block) noexcept
{
    if (!prepared_) return ENTROPY_ERROR_NOT_PREPARED;
    if (block.struct_size < sizeof(block) || block.input_channels != 16 ||
        block.output_channels != 16 || block.frames > maxFrames_ ||
        !block.inputs || !block.outputs)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t channel = 0; channel < 16; ++channel)
        if (!block.inputs[channel] || !block.outputs[channel])
            return ENTROPY_ERROR_INVALID_ARGUMENT;
    synchronizeAudioState();

    std::uint64_t bad = 0;
    for (std::uint32_t channel = 0; channel < 16; ++channel)
        if (audio_.activeMask & (1u << channel))
            for (std::uint32_t frame = 0; frame < block.frames; ++frame)
            {
                const auto sample = block.inputs[channel][frame];
                bad += !std::isfinite(sample);
            }
    if (bad)
    {
        nonfinite_.fetch_add(bad);
        faults_.fetch_or(ENTROPY_HEADROOM_FAULT_NONFINITE_AUDIO);
    }
    if (faults_.load(std::memory_order_acquire))
    {
        for (std::uint32_t channel = 0; channel < 16; ++channel)
            std::fill_n(block.outputs[channel], block.frames, 0.0);
        processed_.fetch_add(block.frames);
        return ENTROPY_OK;
    }

    if (audio_.mode != ENTROPY_HEADROOM_MODE_AUTO)
    {
        autoAnalysisReductionDb_.store(0.0, std::memory_order_relaxed);
        analyzerState_.store(ENTROPY_HEADROOM_ANALYZER_IDLE, std::memory_order_relaxed);
    }

    std::array<double, 16> gains{};
    for (std::uint32_t frame = 0; frame < block.frames; ++frame)
    {
        for (std::uint32_t group = 0; group < audio_.groupCount; ++group)
            gains[group] = ramps_[group].next();
        for (std::uint32_t channel = 0; channel < 16; ++channel)
        {
            if (!(audio_.activeMask & (1u << channel)))
            {
                block.outputs[channel][frame] = block.inputs[channel][frame];
                continue;
            }
            const auto gain = gains[audio_.channelGroup[channel]];
            block.outputs[channel][frame] = gain == 1.0
                ? block.inputs[channel][frame]
                : block.inputs[channel][frame] * gain;
        }
    }

    bool transitioning = false;
    double progress = 1.0;
    bool requestedTargetReached = true;
    for (std::uint32_t group = 0; group < audio_.groupCount; ++group)
    {
        currentTelemetry_[group].store(ramps_[group].current);
        const auto groupProgress = ramps_[group].progress();
        progressTelemetry_[group].store(groupProgress);
        transitioning = transitioning || groupProgress < 1.0;
        progress = (std::min)(progress, groupProgress);
        const auto requestedTarget = audio_.mode == ENTROPY_HEADROOM_MODE_BIT_PERFECT
            ? 1.0 : audio_.mode == ENTROPY_HEADROOM_MODE_MANUAL_SAFE
            ? audio_.manualTarget : autoTargets_[group];
        requestedTargetReached = requestedTargetReached &&
            ramps_[group].current == requestedTarget && groupProgress == 1.0;
    }
    modeTransitionActive_.store(transitioning ? 1u : 0u, std::memory_order_release);
    modeTransitionProgress_.store(progress, std::memory_order_release);
    effectiveMode_.store(requestedTargetReached ? audio_.mode
                                                : ENTROPY_HEADROOM_MODE_ACTIVE,
                         std::memory_order_release);
    processed_.fetch_add(block.frames);
    return ENTROPY_OK;
}

EntropyResult HeadroomEngine::analyzeFinal(
    const EntropyHeadroomAnalysisBlockF64& block) noexcept
{
    if (!prepared_) return ENTROPY_ERROR_NOT_PREPARED;
    if (block.struct_size < sizeof(block) || block.channels == 0 ||
        block.channels > 16 || block.frames > maxFrames_ || block.inputs == nullptr)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t channel = 0; channel < block.channels; ++channel)
        if (block.inputs[channel] == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;

    // V4 terminal analysis sees the actual post-matrix, pre-gain FP64 signal.
    // The theoretical upstream sum is already present in these samples.
    // Legacy hosts that never supply analysis retain their ledger-only policy.
    if (!terminalAnalysis_) {
        terminalAnalysis_ = true;
        audio_.revision = (std::numeric_limits<std::uint32_t>::max)();
    }
    synchronizeAudioState();

    std::array<double, 16> channelPeaks{};
    double peak = 0.0;
    std::uint64_t bad = 0;
    for (std::uint32_t channel = 0; channel < block.channels; ++channel)
    {
        if (!(audio_.activeMask & (1u << channel))) continue;
        double channelPeak = 0.0;
        for (std::uint32_t frame = 0; frame < block.frames; ++frame)
        {
            const auto sample = block.inputs[channel][frame];
            if (!std::isfinite(sample)) ++bad;
            else channelPeak = (std::max)(channelPeak, std::abs(sample));
        }
        channelPeaks[channel] = channelPeak;
        peak = (std::max)(peak, channelPeak);
    }

    std::uint32_t limitingMask = 0;
    if (peak > 0.0)
        for (std::uint32_t channel = 0; channel < block.channels; ++channel)
            if (channelPeaks[channel] == peak)
                limitingMask |= 1u << channel;

    measuredFinalPeak_.store(peak, std::memory_order_relaxed);
    limitingChannelMask_.store(limitingMask, std::memory_order_relaxed);
    for (std::uint32_t channel = 0; channel < 16; ++channel)
        measuredChannelPeak_[channel].store(channelPeaks[channel], std::memory_order_relaxed);

    if (bad)
    {
        nonfinite_.fetch_add(bad, std::memory_order_relaxed);
        faults_.fetch_or(ENTROPY_HEADROOM_FAULT_NONFINITE_AUDIO,
                         std::memory_order_release);
        analyzerState_.store(ENTROPY_HEADROOM_ANALYZER_IDLE,
                             std::memory_order_release);
        return ENTROPY_OK;
    }

    if (requestedMode_.load(std::memory_order_acquire) != ENTROPY_HEADROOM_MODE_AUTO)
    {
        autoAnalysisReductionDb_.store(0.0, std::memory_order_relaxed);
        analyzerState_.store(ENTROPY_HEADROOM_ANALYZER_IDLE, std::memory_order_release);
        return ENTROPY_OK;
    }

    const double peakDb = peak > 0.0 ? 20.0 * std::log10(peak) : -300.0;
    double maximumReduction = 0.0;
    std::uint32_t state = ENTROPY_HEADROOM_ANALYZER_TRACKING;
    bool capacityExceeded = false;
    const auto attackSamples = (std::max)(1u, static_cast<std::uint32_t>(
        std::llround(rate_ * audioAttackMs_ / 1000.0)));
    const auto releaseSamples = (std::max)(1u, static_cast<std::uint32_t>(
        std::llround(rate_ * audioReleaseMs_ / 1000.0)));
    constexpr double epsilon = 1.0e-15;

    for (std::uint32_t group = 0; group < audio_.groupCount; ++group)
    {
        const double reserve = reserve_[group].load(std::memory_order_relaxed);
        const double excessDb = (std::max)(0.0, peakDb + reserve);
        const double observedTarget = std::pow(10.0, -excessDb / 20.0);

        // MANUAL SAFE is the maximum attenuation AUTO is authorized to apply.
        // A separate hard safety target expresses the gain required merely to
        // keep this already-inspected block at or below 0 dBFS.
        const double manualFloor = audio_.manualTarget;
        const double safetyTarget = peak > 1.0 ? 1.0 / peak : 1.0;
        const double desired = (std::max)(manualFloor, observedTarget);
        const bool insufficientCapacity = manualFloor > safetyTarget + epsilon;

        maximumReduction = (std::max)(maximumReduction,
            -20.0 * std::log10((std::max)(desired, 1.0e-15)));

        if (insufficientCapacity)
        {
            // Respect the user-defined AUTO limit. Do not clip, limit or apply
            // more attenuation than MANUAL SAFE permits. Apply the maximum
            // authorized scalar immediately and report insufficient capacity.
            capacityExceeded = true;
            autoTargets_[group] = manualFloor;
            autoTargetTelemetry_[group].store(manualFloor, std::memory_order_relaxed);
            autoHoldRemaining_[group] = rate_;
            if (ramps_[group].current > manualFloor + epsilon)
                ramps_[group].force(manualFloor);
            else if (ramps_[group].current + epsilon < manualFloor &&
                     ramps_[group].target != manualFloor)
                ramps_[group].begin(manualFloor, releaseSamples);
            continue;
        }

        // analyzeFinal() observes this exact block before process_f64(). If the
        // current gain would allow >0 dBFS, move immediately only as far as
        // required for sample safety. This is a scalar gain decision, never a
        // sample clamp/limiter. Any extra preventive reserve still follows the
        // configured AUTO attack time.
        if (ramps_[group].current > safetyTarget + epsilon)
            ramps_[group].force(safetyTarget);

        if (desired + epsilon < autoTargets_[group])
        {
            autoTargets_[group] = desired;
            autoTargetTelemetry_[group].store(desired, std::memory_order_relaxed);
        }

        if (desired + epsilon < ramps_[group].current)
        {
            autoHoldRemaining_[group] = rate_;
            // Do not restart an already-running preventive attack on every
            // analyzed block. The ramp duration is fixed when the target first
            // changes; later timing edits apply only to the next ramp.
            if (std::abs(ramps_[group].target - desired) > epsilon)
                ramps_[group].begin(desired, attackSamples);
            state = ENTROPY_HEADROOM_ANALYZER_HOLD;
        }
        else if (desired + epsilon < 1.0)
        {
            // The limiting/preventive condition is still present. Keep the
            // established target and refresh hold without restarting a ramp.
            autoHoldRemaining_[group] = rate_;
            state = ENTROPY_HEADROOM_ANALYZER_HOLD;
        }
        else if (autoHoldRemaining_[group] > block.frames)
        {
            autoHoldRemaining_[group] -= block.frames;
            state = ENTROPY_HEADROOM_ANALYZER_HOLD;
        }
        else
        {
            autoHoldRemaining_[group] = 0;
            if (autoTargets_[group] != 1.0)
            {
                autoTargets_[group] = 1.0;
                autoTargetTelemetry_[group].store(1.0, std::memory_order_relaxed);
                ramps_[group].begin(1.0, releaseSamples);
                state = ENTROPY_HEADROOM_ANALYZER_RECOVERY;
            }
        }
    }

    if (capacityExceeded)
        state = ENTROPY_HEADROOM_ANALYZER_CAPACITY_EXCEEDED;
    autoAnalysisReductionDb_.store(maximumReduction, std::memory_order_relaxed);
    analyzerState_.store(state, std::memory_order_release);
    return ENTROPY_OK;
}

} // namespace entropy::headroom
