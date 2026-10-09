#pragma once

#include "EngineTelemetry.h"

#include <entropy/src_abi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace entropy::host
{
enum class LatencyMeasurementValidity : std::uint32_t
{
    Measured,
    Estimated,
    Unknown
};

enum class LatencyClockDomain : std::uint32_t
{
    SourceCapture,
    Engine192k,
    DacSink,
    UserTime
};

enum class LatencyBranch : std::uint32_t
{
    Shared,
    LeftRight,
    CenterOnly,
    DestinationLane
};

enum class LatencyComponent : std::uint32_t
{
    CaptureFifo,
    EndpointAvailability,
    SourceFifo,
    SrcAvailability,
    CrossoverFir,
    UserDelay,
    Center,
    EgressFifo,
    ClockSyncRemainder,
    DacTransport,
    Guardian,
    IirGroupDelay
};

inline constexpr double unknownLatencyValue() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}

struct LatencyLedgerContext
{
    std::uint64_t programEpoch = 0;
    bool soxrLatencySnapshotValid = false;
    double captureQueueAgeMs = unknownLatencyValue();
    double sourceQueueAgeMs = unknownLatencyValue();
    double egressQueueAgeMs = unknownLatencyValue();
};

struct LatencyLedgerEntry
{
    LatencyComponent component = LatencyComponent::CaptureFifo;
    double queueFrames = 0.0;
    double queueRate = 0.0;
    double queueDurationMs = 0.0;
    double queueAgeMs = unknownLatencyValue();
    double algorithmicDelayFrames = 0.0;
    double algorithmicDelayRate = 0.0;
    double algorithmicDurationMs = 0.0;
    double userDelayMs = 0.0;
    LatencyMeasurementValidity measurementValidity = LatencyMeasurementValidity::Unknown;
    std::uint64_t programEpoch = 0;
    LatencyClockDomain clockDomain = LatencyClockDomain::Engine192k;
    LatencyBranch branch = LatencyBranch::Shared;
    bool additive = true;
};

struct LatencyBranchLedger
{
    double knownLiveMs = 0.0;
    std::uint32_t unknownComponentCount = 0;
    LatencyMeasurementValidity liveEstimateValidity = LatencyMeasurementValidity::Measured;
    double endToEndMeasuredMs = unknownLatencyValue();
};

inline constexpr std::size_t latencyLedgerCapacity = 16;

struct LatencyLedger
{
    std::array<LatencyLedgerEntry, latencyLedgerCapacity> entries{};
    std::size_t entryCount = 0;
    LatencyBranchLedger shared{};
    LatencyBranchLedger leftRight{};
    LatencyBranchLedger centerOnly{};
    std::uint64_t programEpoch = 0;
    bool epochCoherent = false;
    double userDelayMinimumMs = 0.0;
    double userDelayMaximumMs = 0.0;
};

struct LegacyProfileAssessment
{
    double estimatedMs = unknownLatencyValue();
    double thresholdMs = unknownLatencyValue();
    bool withinThreshold = false;
    LatencyMeasurementValidity measurementValidity = LatencyMeasurementValidity::Unknown;
};

namespace latency_ledger_detail
{
inline double milliseconds(double frames, double rate) noexcept
{
    return frames >= 0.0 && rate > 0.0 && std::isfinite(frames) && std::isfinite(rate)
        ? frames * 1000.0 / rate
        : unknownLatencyValue();
}

inline void include(LatencyBranchLedger& branch, const LatencyLedgerEntry& entry) noexcept
{
    if (!entry.additive) return;
    if (entry.measurementValidity == LatencyMeasurementValidity::Unknown)
    {
        ++branch.unknownComponentCount;
        branch.liveEstimateValidity = LatencyMeasurementValidity::Unknown;
        return;
    }

    const auto queue = std::isfinite(entry.queueDurationMs) ? entry.queueDurationMs : 0.0;
    const auto algorithmic = std::isfinite(entry.algorithmicDurationMs)
        ? entry.algorithmicDurationMs : 0.0;
    const auto user = std::isfinite(entry.userDelayMs) ? entry.userDelayMs : 0.0;
    branch.knownLiveMs += queue + algorithmic + user;
    if (entry.measurementValidity == LatencyMeasurementValidity::Estimated &&
        branch.liveEstimateValidity == LatencyMeasurementValidity::Measured)
        branch.liveEstimateValidity = LatencyMeasurementValidity::Estimated;
}

inline void append(LatencyLedger& ledger, const LatencyLedgerEntry& entry) noexcept
{
    if (ledger.entryCount >= ledger.entries.size())
    {
        LatencyLedgerEntry overflow{};
        overflow.measurementValidity = LatencyMeasurementValidity::Unknown;
        include(ledger.shared, overflow);
        include(ledger.leftRight, overflow);
        include(ledger.centerOnly, overflow);
        return;
    }
    ledger.entries[ledger.entryCount++] = entry;
    switch (entry.branch)
    {
        case LatencyBranch::Shared:
            include(ledger.shared, entry);
            include(ledger.leftRight, entry);
            include(ledger.centerOnly, entry);
            break;
        case LatencyBranch::LeftRight:
            include(ledger.leftRight, entry);
            break;
        case LatencyBranch::CenterOnly:
            include(ledger.centerOnly, entry);
            break;
        case LatencyBranch::DestinationLane:
            include(ledger.leftRight, entry);
            include(ledger.centerOnly, entry);
            break;
    }
}

inline LatencyLedgerEntry queue(
    LatencyComponent component,
    double frames,
    double rate,
    double ageMs,
    LatencyClockDomain domain,
    LatencyBranch branch,
    std::uint64_t epoch) noexcept
{
    LatencyLedgerEntry entry{};
    entry.component = component;
    entry.queueFrames = frames;
    entry.queueRate = rate;
    entry.queueDurationMs = milliseconds(frames, rate);
    entry.queueAgeMs = std::isfinite(ageMs) && ageMs >= 0.0 ? ageMs : unknownLatencyValue();
    entry.measurementValidity = std::isfinite(entry.queueDurationMs)
        ? LatencyMeasurementValidity::Measured
        : LatencyMeasurementValidity::Unknown;
    entry.programEpoch = epoch;
    entry.clockDomain = domain;
    entry.branch = branch;
    return entry;
}

inline LatencyLedgerEntry algorithmic(
    LatencyComponent component,
    double frames,
    double rate,
    LatencyMeasurementValidity validity,
    LatencyClockDomain domain,
    LatencyBranch branch,
    std::uint64_t epoch) noexcept
{
    LatencyLedgerEntry entry{};
    entry.component = component;
    entry.algorithmicDelayFrames = frames;
    entry.algorithmicDelayRate = rate;
    entry.algorithmicDurationMs = validity == LatencyMeasurementValidity::Unknown
        ? unknownLatencyValue() : milliseconds(frames, rate);
    entry.measurementValidity = std::isfinite(entry.algorithmicDurationMs)
        ? validity : LatencyMeasurementValidity::Unknown;
    entry.programEpoch = epoch;
    entry.clockDomain = domain;
    entry.branch = branch;
    return entry;
}

inline bool parameter(
    const EngineTelemetrySnapshot& telemetry,
    const std::string& identifier,
    double& value)
{
    const auto count = (std::min)(telemetry.crossoverParameterCount,
                                  crossoverParameterCapacity);
    for (std::uint32_t index = 0; index < count; ++index)
        if (identifier == telemetry.crossoverParameters[index].identifier)
        {
            value = telemetry.crossoverParameters[index].value;
            return std::isfinite(value);
        }
    return false;
}
}

inline LatencyLedger buildLatencyLedger(
    const EngineTelemetrySnapshot& telemetry,
    std::uint32_t source,
    std::uint32_t dac,
    const LatencyLedgerContext& context) noexcept
{
    using namespace latency_ledger_detail;
    LatencyLedger ledger{};
    ledger.programEpoch = context.programEpoch;
    if (source >= 3 || dac >= engineMaximumDacDomains) return ledger;

    const auto& input = telemetry.inputs[source];
    const auto& sourceBuffer = telemetry.sourceBuffers[source];
    const auto& output = telemetry.outputDomains[dac];
    const auto& clock = telemetry.clockBuffers[dac];
    ledger.epochCoherent = context.programEpoch != 0 &&
                           input.generation == context.programEpoch;
    if (!ledger.epochCoherent || !input.bridgeConnected || input.sourceRate == 0 ||
        !output.configured || output.sampleRate == 0)
    {
        ledger.leftRight.liveEstimateValidity = LatencyMeasurementValidity::Unknown;
        ledger.centerOnly.liveEstimateValidity = LatencyMeasurementValidity::Unknown;
        ledger.shared.liveEstimateValidity = LatencyMeasurementValidity::Unknown;
        return ledger;
    }

    append(ledger, queue(LatencyComponent::CaptureFifo,
        sourceBuffer.captureFillFrames, input.sourceRate, context.captureQueueAgeMs,
        LatencyClockDomain::SourceCapture, LatencyBranch::Shared, context.programEpoch));
    append(ledger, queue(LatencyComponent::EndpointAvailability,
        input.endpointAvailableFrames, input.sourceRate, unknownLatencyValue(),
        LatencyClockDomain::SourceCapture, LatencyBranch::Shared, context.programEpoch));
    append(ledger, queue(LatencyComponent::SourceFifo,
        input.sourceQueueDepthFrames, 192000.0, context.sourceQueueAgeMs,
        LatencyClockDomain::Engine192k, LatencyBranch::Shared, context.programEpoch));

    auto srcValidity = LatencyMeasurementValidity::Unknown;
    if (input.sourceRate == 192000)
        srcValidity = LatencyMeasurementValidity::Measured;
    else if (input.srcEngine == ENTROPY_SRC_SOXR_VHQ && input.srcControlAvailable &&
             context.soxrLatencySnapshotValid)
        srcValidity = LatencyMeasurementValidity::Measured;
    append(ledger, algorithmic(LatencyComponent::SrcAvailability,
        input.srcLatencyFrames, 192000.0, srcValidity,
        LatencyClockDomain::Engine192k, LatencyBranch::Shared, context.programEpoch));

    const auto crossoverValidity = telemetry.crossoverTelemetryAvailable &&
        telemetry.crossoverSampleRate > 0.0
        ? LatencyMeasurementValidity::Estimated
        : LatencyMeasurementValidity::Unknown;
    append(ledger, algorithmic(LatencyComponent::CrossoverFir,
        telemetry.crossoverLatencySamples, telemetry.crossoverSampleRate,
        crossoverValidity, LatencyClockDomain::Engine192k,
        LatencyBranch::Shared, context.programEpoch));

    const std::string prefix = telemetry.crossoverCinemaProgram ? "cinema" : "music";
    double global = 0.0;
    bool delayKnown = parameter(telemetry, prefix + "GlobalDelayMs", global);
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (std::uint32_t lane = 1; lane <= 16; ++lane)
    {
        double relative = 0.0;
        if (!parameter(telemetry, prefix + "DelayMs" + std::to_string(lane), relative))
        {
            delayKnown = false;
            continue;
        }
        const auto delay = std::clamp(global + relative, 0.0, 1600.0);
        minimum = (std::min)(minimum, delay);
        maximum = (std::max)(maximum, delay);
    }
    LatencyLedgerEntry delay{};
    delay.component = LatencyComponent::UserDelay;
    delay.userDelayMs = delayKnown && std::isfinite(maximum)
        ? maximum : unknownLatencyValue();
    delay.measurementValidity = delayKnown
        ? LatencyMeasurementValidity::Estimated
        : LatencyMeasurementValidity::Unknown;
    delay.programEpoch = context.programEpoch;
    delay.clockDomain = LatencyClockDomain::UserTime;
    delay.branch = LatencyBranch::DestinationLane;
    ledger.userDelayMinimumMs = delayKnown ? minimum : unknownLatencyValue();
    ledger.userDelayMaximumMs = delayKnown ? maximum : unknownLatencyValue();
    append(ledger, delay);

    append(ledger, algorithmic(LatencyComponent::Center,
        telemetry.centerLatencySamples, 192000.0,
        telemetry.crossoverCinemaProgram ? LatencyMeasurementValidity::Measured
                                         : LatencyMeasurementValidity::Estimated,
        LatencyClockDomain::Engine192k, LatencyBranch::CenterOnly,
        context.programEpoch));

    append(ledger, queue(LatencyComponent::EgressFifo,
        output.fifoFillFrames, output.sampleRate, context.egressQueueAgeMs,
        LatencyClockDomain::DacSink, LatencyBranch::Shared, context.programEpoch));

    const auto clockRemainder = clock.latencyAvailable
        ? (std::max)(0.0, static_cast<double>(clock.latencySinkFrames) - clock.targetFrames)
        : 0.0;
    append(ledger, algorithmic(LatencyComponent::ClockSyncRemainder,
        clockRemainder, output.sampleRate,
        clock.latencyAvailable ? LatencyMeasurementValidity::Estimated
                               : LatencyMeasurementValidity::Unknown,
        LatencyClockDomain::DacSink, LatencyBranch::Shared, context.programEpoch));
    append(ledger, algorithmic(LatencyComponent::DacTransport,
        output.bufferFrames, output.sampleRate, LatencyMeasurementValidity::Measured,
        LatencyClockDomain::DacSink, LatencyBranch::Shared, context.programEpoch));
    append(ledger, algorithmic(LatencyComponent::Guardian,
        output.guardianLookaheadFrames, output.sampleRate,
        LatencyMeasurementValidity::Measured, LatencyClockDomain::DacSink,
        LatencyBranch::Shared, context.programEpoch));

    auto iir = algorithmic(LatencyComponent::IirGroupDelay, 0.0, 0.0,
        LatencyMeasurementValidity::Unknown, LatencyClockDomain::Engine192k,
        LatencyBranch::Shared, context.programEpoch);
    iir.additive = false;
    append(ledger, iir);
    return ledger;
}

inline LegacyProfileAssessment evaluateLegacyProfile(
    std::uint32_t sourceTargetFrames,
    std::uint32_t egressTargetFrames,
    std::uint32_t rate,
    double crossoverMs,
    double guardianMs,
    double thresholdMs) noexcept
{
    LegacyProfileAssessment result{};
    result.thresholdMs = thresholdMs;
    const auto sourceMs = latency_ledger_detail::milliseconds(sourceTargetFrames, rate);
    const auto egressMs = latency_ledger_detail::milliseconds(egressTargetFrames, rate);
    if (!std::isfinite(sourceMs) || !std::isfinite(egressMs) ||
        !std::isfinite(crossoverMs) || crossoverMs < 0.0 ||
        !std::isfinite(guardianMs) || guardianMs < 0.0 ||
        !std::isfinite(thresholdMs) || thresholdMs < 0.0)
        return result;
    result.estimatedMs = sourceMs + egressMs + crossoverMs + guardianMs;
    result.withinThreshold = result.estimatedMs <= thresholdMs;
    result.measurementValidity = LatencyMeasurementValidity::Estimated;
    return result;
}
}
