#pragma once
#include "EngineTelemetry.h"
#include "LatencyLedger.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace entropy::host {
inline double latencyMs(double frames, double rate) noexcept {
    return rate > 0 && frames >= 0 ? frames * 1000.0 / rate
        : std::numeric_limits<double>::quiet_NaN();
}
struct LatencyBudget {
    double sourceFifoMs=0, captureFifoMs=0, endpointQueueMs=0, srcMs=0;
    double egressFifoMs=0, clockSrcMs=0, dacBufferMs=0, guardianMs=0;
    double crossoverFirMs=0, sceneDelayMinMs=0, sceneDelayMaxMs=0, centerOnlyMs=0;
    double minimumMs=0, maximumMs=0;
    bool available=false;
};
inline bool latencyParameter(const EngineTelemetrySnapshot& t, const std::string& key, double& value) {
    for (unsigned i=0; i<(std::min)(t.crossoverParameterCount, crossoverParameterCapacity); ++i)
        if (key == t.crossoverParameters[i].identifier) { value=t.crossoverParameters[i].value; return true; }
    return false;
}
inline LatencyBudget latencyBudget(const EngineTelemetrySnapshot& t, unsigned source, unsigned dac) {
    LatencyBudget b;
    if(source>=3 || dac>=engineMaximumDacDomains) return b;
    const auto& s=t.inputs[source]; const auto& d=t.outputDomains[dac];
    const auto& q=t.sourceBuffers[source]; const auto& c=t.clockBuffers[dac];
    if(!s.bridgeConnected || !s.sourceRate || !d.configured || !d.sampleRate ||
       !t.crossoverTelemetryAvailable || !c.latencyAvailable) return b;
    b.sourceFifoMs=latencyMs(s.sourceQueueDepthFrames,192000);
    b.captureFifoMs=latencyMs(q.captureFillFrames,s.sourceRate);
    b.endpointQueueMs=latencyMs(s.endpointAvailableFrames,s.sourceRate);
    b.srcMs=latencyMs(s.srcLatencyFrames,192000);
    b.egressFifoMs=latencyMs(d.fifoFillFrames,d.sampleRate);
    // ClockSync's reported latency already contains its target. Only its
    // nominal SRC remainder is additive to measured FIFO occupancy.
    b.clockSrcMs=latencyMs((std::max)(0.0,double(c.latencySinkFrames)-c.targetFrames),d.sampleRate);
    b.dacBufferMs=latencyMs(d.bufferFrames,d.sampleRate);
    b.guardianMs=latencyMs(d.guardianLookaheadFrames,d.sampleRate);
    b.crossoverFirMs=latencyMs(t.crossoverLatencySamples,t.crossoverSampleRate);
    const std::string prefix=t.crossoverCinemaProgram ? "cinema" : "music";
    double global=0;
    if(!latencyParameter(t,prefix+"GlobalDelayMs",global)) return b;
    b.sceneDelayMinMs=1600; b.sceneDelayMaxMs=0;
    for(unsigned ch=1;ch<=16;++ch) {
        double relative=0;
        if(!latencyParameter(t,prefix+"DelayMs"+std::to_string(ch),relative)) return b;
        const double delay=std::clamp(global+relative,0.0,1600.0);
        b.sceneDelayMinMs=(std::min)(b.sceneDelayMinMs,delay);
        b.sceneDelayMaxMs=(std::max)(b.sceneDelayMaxMs,delay);
    }
    b.centerOnlyMs=t.crossoverCinemaProgram ? 0 : latencyMs(t.centerLatencySamples,192000);
    const double shared=b.sourceFifoMs+b.captureFifoMs+b.endpointQueueMs+b.srcMs+
        b.egressFifoMs+b.clockSrcMs+b.dacBufferMs+b.guardianMs+b.crossoverFirMs;
    b.minimumMs=shared+b.sceneDelayMinMs;
    b.maximumMs=shared+b.sceneDelayMaxMs;
    b.available=std::isfinite(b.minimumMs) && std::isfinite(b.maximumMs);
    return b;
}
inline const char* latencyValidityName(LatencyMeasurementValidity validity) noexcept {
    switch(validity) {
        case LatencyMeasurementValidity::Measured: return "MEASURED";
        case LatencyMeasurementValidity::Estimated: return "ESTIMATED";
        case LatencyMeasurementValidity::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}
inline const char* latencyClockDomainName(LatencyClockDomain domain) noexcept {
    switch(domain) {
        case LatencyClockDomain::SourceCapture: return "SOURCE_CAPTURE";
        case LatencyClockDomain::Engine192k: return "ENGINE_192K";
        case LatencyClockDomain::DacSink: return "DAC_SINK";
        case LatencyClockDomain::UserTime: return "USER_TIME";
    }
    return "UNKNOWN";
}
inline const char* latencyBranchName(LatencyBranch branch) noexcept {
    switch(branch) {
        case LatencyBranch::Shared: return "SHARED";
        case LatencyBranch::LeftRight: return "LEFT_RIGHT";
        case LatencyBranch::CenterOnly: return "CENTER_ONLY";
        case LatencyBranch::DestinationLane: return "DESTINATION_LANE";
    }
    return "UNKNOWN";
}
inline const char* latencyComponentName(LatencyComponent component) noexcept {
    switch(component) {
        case LatencyComponent::CaptureFifo: return "CAPTURE_FIFO";
        case LatencyComponent::EndpointAvailability: return "ENDPOINT_AVAILABILITY";
        case LatencyComponent::SourceFifo: return "SOURCE_FIFO";
        case LatencyComponent::SrcAvailability: return "SRC_AVAILABILITY";
        case LatencyComponent::CrossoverFir: return "CROSSOVER_FIR";
        case LatencyComponent::UserDelay: return "USER_DELAY";
        case LatencyComponent::Center: return "CENTER";
        case LatencyComponent::EgressFifo: return "EGRESS_FIFO";
        case LatencyComponent::ClockSyncRemainder: return "CLOCK_SYNC_REMAINDER";
        case LatencyComponent::DacTransport: return "DAC_TRANSPORT";
        case LatencyComponent::Guardian: return "GUARDIAN";
        case LatencyComponent::IirGroupDelay: return "IIR_GROUP_DELAY";
    }
    return "UNKNOWN";
}
inline void appendLatencyLedgerText(std::ostringstream& o,
                                    const EngineTelemetrySnapshot& t,
                                    unsigned source, unsigned dac) {
    const auto& input=t.inputs[source];
    LatencyLedgerContext context{};
    context.programEpoch=input.generation;
    context.soxrLatencySnapshotValid=
        input.srcEngine==ENTROPY_SRC_SOXR_VHQ && input.srcControlAvailable!=0;
    const auto ledger=buildLatencyLedger(t,source,dac,context);
    o<<"LATENCY LEDGER "<<input.role<<"->DAC"<<dac+1
     <<" epoch="<<ledger.programEpoch<<" coherent="<<ledger.epochCoherent<<'\n';
    for(std::size_t index=0;index<ledger.entryCount;++index) {
        const auto& entry=ledger.entries[index];
        o<<"component="<<latencyComponentName(entry.component)
         <<" queue_frames="<<entry.queueFrames<<" queue_rate="<<entry.queueRate
         <<" algorithmic_frames="<<entry.algorithmicDelayFrames
         <<" algorithmic_rate="<<entry.algorithmicDelayRate
         <<" user_delay_ms="<<entry.userDelayMs
         <<" validity="<<latencyValidityName(entry.measurementValidity)
         <<" clock_domain="<<latencyClockDomainName(entry.clockDomain)
         <<" branch="<<latencyBranchName(entry.branch)
         <<" epoch="<<entry.programEpoch<<" additive="<<entry.additive<<'\n';
    }
    o<<"known_subtotal_ms="<<ledger.leftRight.knownLiveMs
     <<" unknown_components="<<ledger.leftRight.unknownComponentCount
     <<" validity="<<latencyValidityName(ledger.leftRight.liveEstimateValidity)
     <<" end_to_end=UNKNOWN"
     <<" scene_delay_ms="<<ledger.userDelayMinimumMs<<".."
     <<ledger.userDelayMaximumMs<<'\n';
    o<<"center_known_subtotal_ms="<<ledger.centerOnly.knownLiveMs
     <<" center_unknown_components="<<ledger.centerOnly.unknownComponentCount
     <<" center_validity="<<latencyValidityName(ledger.centerOnly.liveEstimateValidity)
     <<" center_end_to_end=UNKNOWN\n";
}
inline std::string latencyDiagnosticsText(const EngineTelemetrySnapshot& t) {
    std::ostringstream o; o<<std::fixed<<std::setprecision(3);
    o<<"LATENCY BUDGET (telemetry estimate, not physical loopback)\n"
     <<"engine_pid="<<t.enginePid<<" heartbeat="<<t.heartbeat<<" program="<<(t.crossoverCinemaProgram?"CINEMA":"MUSIC")
     <<" armed="<<t.outputArmed<<" crossover_fir_frames="<<t.crossoverLatencySamples<<" center_C_only_frames="<<t.centerLatencySamples
     <<" recovery_reanchors="<<t.engineRecoveryReanchors<<'\n';
    for(unsigned i=0;i<3;++i) { const auto& s=t.inputs[i]; const auto& q=t.sourceBuffers[i];
        o<<s.role<<" rate="<<s.sourceRate<<" connected="<<s.bridgeConnected<<" peak="<<s.endpointPeak
         <<" fifo="<<s.sourceQueueDepthFrames<<" capacity="<<s.sourceFifoCapacityFrames<<" low="<<q.lowWaterFrames<<" high="<<s.sourceFifoHighWaterFrames
         <<" preroll="<<q.prerollFrames<<" target="<<q.targetFrames<<" produced="<<s.sourceFifoProducedFrames<<" consumed="<<s.sourceFifoConsumedFrames
         <<" underruns="<<s.sourceFifoUnderruns<<" overflows="<<s.sourceFifoOverflows<<" src_latency_frames="<<s.srcLatencyFrames
         <<" endpoint_available="<<s.endpointAvailableFrames<<" capture_fill="<<q.captureFillFrames<<" capture_capacity="<<q.captureCapacityFrames
         <<" capture_low="<<q.captureLowWaterFrames<<" capture_high="<<q.captureHighWaterFrames<<" capture_empty_polls="<<q.captureUnderruns
         <<" capture_overflows="<<q.captureOverflows<<" src_resets="<<q.srcResets<<" faults="<<q.faults<<" generation="<<s.generation<<'\n';
    }
    for(unsigned i=0;i<3;++i) { const auto& d=t.sourceDelivery[i];
        o<<"DELIVERY "<<t.inputs[i].role<<" delivered_blocks="<<d.deliveredBlocks
         <<" active_missing_blocks="<<d.activeMissingBlocks<<" preroll_blocks="<<d.prerollBlocks
         <<" idle_unavailable_blocks="<<d.idleBlocks<<" muted_blocks="<<d.mutedBlocks
         <<" excluded_blocks="<<d.excludedBlocks<<" generation_changes="<<d.generationChanges
         <<" generation="<<d.generation<<" missing_run="<<d.missingRun
         <<" max_missing_run="<<d.maxMissingRun<<" last_missing_fill="<<d.lastMissingFill<<'\n';
    }
    o<<"Delivery counts observe source availability before DSP, not measured output gaps. Idle uses endpoint state; each block is 1024/192000 seconds.\n";
    for(unsigned i=0;i<engineMaximumDacDomains;++i) { const auto& d=t.outputDomains[i]; const auto& c=t.clockBuffers[i];
        o<<"DAC"<<i+1<<" configured="<<d.configured<<" rate="<<d.sampleRate<<" fifo="<<d.fifoFillFrames<<" target="<<c.targetFrames<<" capacity="<<c.capacityFrames
         <<" buffer="<<d.bufferFrames<<" clock_in="<<d.clockInputFrames<<" clock_nominal="<<d.clockNominalFrames<<" clock_out="<<d.clockOutputFrames
         <<" starved="<<d.clockStarvedCallbacks<<" resync="<<d.clockResyncCount<<" clock_state="<<d.clockState<<" clock_fault="<<c.faultFlags
         <<" xrun="<<d.xrunCount<<" fault="<<d.faultFlags<<" guardian_state="<<d.guardianState<<" guardian_lookahead="<<d.guardianLookaheadFrames
         <<" guardian_interventions="<<d.guardianInterventionCount<<" guardian_fault="<<d.guardianFaultFlags<<'\n';
        for(unsigned s=0;s<3;++s) {
            if((s==2)!=(t.crossoverCinemaProgram!=0)) continue;
            appendLatencyLedgerText(o,t,s,i);
        }
    }
    o<<"Scene range covers all configured crossover outputs (not a per-speaker routing measurement). Center is C-only, never added to L/R.\n"
     <<"Unmeasured: source application/driver queues, device converter delay and block phase. Known Loudness latency=0.\n"
     <<"Capture empty polls are producer polling, not DSP FIFO starvation. Values describe queued time; no audio processing is changed.\n";
    return o.str();
}
}
