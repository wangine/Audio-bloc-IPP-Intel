#pragma once
#include "FirstFaultSlot.h"
#include <entropy/egress_clock_diagnostics_abi.h>
#include <entropy/egress_clock_abi.h>
#include <atomic>
#include <chrono>
#include <cstring>

#ifndef ENTROPY_FIRST_FAULT_BUILD_PROFILE
#define ENTROPY_FIRST_FAULT_BUILD_PROFILE "UNKNOWN"
#endif
#ifndef ENTROPY_FIRST_FAULT_SOURCE_ID
#define ENTROPY_FIRST_FAULT_SOURCE_ID "UNKNOWN"
#endif

namespace entropy::egress {
// B2-FF-001 KEEP_MINIMAL candidate: compiled only when ENTROPY_EGRESS_FIRST_FAULT=1.
// No call from successful push/render. No worker, ring buffer or test hooks.
// Caller must archive before resetQuiescent; no live Host integration in stage A.
class EgressFirstFaultCapture final {
public:
    void resetQuiescent() noexcept {
        slot_.resetQuiescent(); ++lifecycleId_;
        preroll_.store(0, std::memory_order_relaxed);
        underrun_.store(0, std::memory_order_relaxed);
        faultSilence_.store(0, std::memory_order_relaxed);
    }
    void capture(std::uint32_t code, std::uint32_t origin, std::uint32_t site,
                 EntropyResult result, const EntropyEgressClockConfig& config,
                 bool operandsValid=false, std::int64_t write=0, std::int64_t read=0,
                 std::uint64_t retained=0, std::uint64_t maximumProduced=0,
                 std::uint32_t incoming=0, std::uint32_t flags=0,
                 std::uint32_t guard=0, std::uint64_t epoch=0) noexcept {
        if (slot_.state() != FirstFaultSlot<EntropyEgressFirstFaultRecord>::Empty) return;
        EntropyEgressFirstFaultRecord r{};
        r.struct_size=sizeof(r); r.record_version=ENTROPY_EGRESS_DIAGNOSTICS_RECORD_VERSION;
        r.fault_namespace=ENTROPY_FF_NAMESPACE_EGRESS_CLOCK;
        r.fault_code=code; r.origin=origin; r.site=site; r.api_result=result;
        r.instance_id=instanceId_; r.lifecycle_id=lifecycleId_; r.first_event_id=1;
        const auto ticks=std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        r.monotonic_ns=ticks>0?static_cast<std::uint64_t>(ticks):0;
        // An invalid/short caller struct must never be read past struct_size.
        if (config.struct_size>=sizeof(config)) {
            r.valid_fields|=ENTROPY_FF_VALID_CONFIG; r.config_revision=config.revision;
            r.engine_rate=config.engine_rate; r.sink_rate=config.sink_rate;
            r.channels=config.channels; r.capacity_frames=config.fifo_capacity_frames;
            r.target_frames=config.target_fill_frames;
        }
        if (operandsValid) {
            r.valid_fields|=ENTROPY_FF_VALID_CAPACITY_OPERANDS;
            r.write_frame_used=write; r.published_read_frame_used=read;
            r.retained_used=retained; r.maximum_produced_used=maximumProduced;
            r.guard_frames=guard; r.producer_epoch=epoch;
        }
        r.incoming_frames=incoming; r.block_flags=flags;
        slot_.tryCapture(r);
    }
    void silence(std::uint32_t state) noexcept {
        if (state==ENTROPY_EGRESS_CLOCK_PREROLL) preroll_.fetch_add(1,std::memory_order_relaxed);
        else if (state==ENTROPY_EGRESS_CLOCK_STARVED) underrun_.fetch_add(1,std::memory_order_relaxed);
        else if (state==ENTROPY_EGRESS_CLOCK_FAULT) faultSilence_.fetch_add(1,std::memory_order_relaxed);
    }
    EntropyEgressDiagnosticSnapshot snapshot() const noexcept {
        EntropyEgressDiagnosticSnapshot s{};
        s.struct_size=sizeof(s); s.extension_version=ENTROPY_EGRESS_DIAGNOSTICS_EXTENSION_VERSION;
        s.compact_enabled=1;
        // State and payload are published together from the reader's perspective.
        // A failed read may race READY; report WRITING, never READY with no payload.
        const auto state=slot_.state();
        if (state==FirstFaultSlot<EntropyEgressFirstFaultRecord>::Ready && slot_.read(s.first_fault))
            s.publication_state=ENTROPY_FF_READY;
        else s.publication_state=state==FirstFaultSlot<EntropyEgressFirstFaultRecord>::Empty?ENTROPY_FF_EMPTY:ENTROPY_FF_WRITING;
        s.preroll_silence_callbacks=preroll_.load(std::memory_order_relaxed);
        s.true_underrun_callbacks=underrun_.load(std::memory_order_relaxed);
        s.fault_silence_callbacks=faultSilence_.load(std::memory_order_relaxed);
        copy(s.build_profile,ENTROPY_FIRST_FAULT_BUILD_PROFILE);
        copy(s.build_source_id,ENTROPY_FIRST_FAULT_SOURCE_ID);
        return s;
    }
private:
    template<std::size_t N> static void copy(char (&dst)[N],const char* src) noexcept {
        std::size_t i=0; for(;i+1<N && src[i];++i)dst[i]=src[i]; dst[i]=0;
    }
    inline static std::atomic<std::uint64_t> nextInstance_{1};
    const std::uint64_t instanceId_=nextInstance_.fetch_add(1,std::memory_order_relaxed);
    std::uint64_t lifecycleId_=0; // configure/reset only, with every API caller quiescent
    FirstFaultSlot<EntropyEgressFirstFaultRecord> slot_;
    std::atomic<std::uint64_t> preroll_{0}, underrun_{0}, faultSilence_{0};
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
};
} // namespace entropy::egress
