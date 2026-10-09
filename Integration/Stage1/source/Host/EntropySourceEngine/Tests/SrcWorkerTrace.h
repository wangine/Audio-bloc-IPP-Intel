#pragma once
#include "SourceDomainStream.h"
#include <entropy/src_abi.h>
#include <windows.h>
#include "SourceCycleTimer.h"
#include "ProAudioThreadScope.h"
#include <chrono>
#include <optional>
#include <fstream>
#include <vector>

namespace src_trace {
inline uint64_t now() noexcept { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
struct Record {
    uint64_t begin=0,end=0,processBegin=0,processEnd=0,sleepBegin=0,sleepEnd=0;
    uint64_t totalIn=0,totalOut=0;
    uint32_t captureBefore=0,convertedBefore=0,captureAfter=0,convertedAfter=0;
    uint32_t consumed=0,produced=0,sleepBranch=0;
    bool readyBefore=false,readyAfter=false;
};
inline std::vector<Record> records;
inline size_t count=0;
inline uint64_t dropped=0;
inline Record* current=nullptr;
inline bool highResolution=false;
inline bool requestProAudio=false,workerProAudioActive=false;
inline int workerPriority=0;
inline std::atomic<bool> captureProAudioActive{false};
inline const char* captureFailureReason="not attempted";
inline const char* workerFailureReason="not attempted";
struct WorkerScope {
    std::optional<entropy::host::ProAudioThreadScope> realtime;
    WorkerScope() noexcept {
        if(requestProAudio)realtime.emplace();
        workerProAudioActive=realtime&&realtime->active();
        workerPriority=GetThreadPriority(GetCurrentThread());
    }
};
struct Timing {
    uint64_t count=0,sum=0,maximum=0;
    void add(uint64_t ticks) noexcept { ++count; sum+=ticks; maximum=(std::max)(maximum,ticks); }
    void write(std::ostream& out) const { out<<"{\"count\":"<<count<<",\"sum_ticks\":"<<sum<<",\"max_ticks\":"<<maximum<<"}"; }
};
inline Timing processTiming,spacingTiming,sleepTiming[4];
inline uint64_t firstTick=0,lastBegin=0,lastEnd=0;
// Allocated and touched by the test thread before either source worker starts.
inline void prepare(size_t capacity) { records.resize(capacity); count=0; dropped=0; highResolution=GetEnvironmentVariableA("ENTROPY_SRC_TRACE_HIGHRES",nullptr,0)!=0; requestProAudio=GetEnvironmentVariableA("ENTROPY_SRC_TRACE_MMCSS",nullptr,0)!=0; processTiming={}; spacingTiming={}; for(auto& t:sleepTiming)t={}; firstTick=lastBegin=lastEnd=0; }
struct Iteration {
    entropy::host::SourceDomainStream& stream;
    Record local{};
    explicit Iteration(entropy::host::SourceDomainStream& s) noexcept : stream(s) {
        local.begin=now(); if(!firstTick)firstTick=local.begin;
        if(lastBegin)spacingTiming.add(local.begin-lastBegin); lastBegin=local.begin;
        const auto t=stream.telemetry();
        local.captureBefore=t.capture_fifo.fill_frames; local.convertedBefore=t.fifo.fill_frames;
        local.readyBefore=stream.ready(); current=&local;
    }
    ~Iteration() {
        const auto t=stream.telemetry(); local.captureAfter=t.capture_fifo.fill_frames;
        local.convertedAfter=t.fifo.fill_frames; local.totalIn=t.src_input_frames;
        local.totalOut=t.src_output_frames; local.readyAfter=stream.ready(); local.end=now();
        if(local.processBegin)processTiming.add(local.processEnd-local.processBegin);
        if(local.sleepBranch)sleepTiming[local.sleepBranch].add(local.sleepEnd-local.sleepBegin);
        lastEnd=local.end; records[count%records.size()]=local; ++count;
        current=nullptr;
    }
};
template<class Call> auto sleepCall(unsigned branch,Call call) noexcept {
    current->sleepBranch=branch; current->sleepBegin=now();
    if constexpr(std::is_void_v<decltype(call())>) {call(); current->sleepEnd=now();}
    else {auto result=call(); current->sleepEnd=now(); return result;}
}
inline void sleep(unsigned branch) noexcept {
    static thread_local entropy::host::SourceCycleTimer timer;
    current->sleepBranch=branch; current->sleepBegin=now();
    if(highResolution)
        timer.waitUntil(std::chrono::steady_clock::now()+std::chrono::milliseconds(1));
    else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    current->sleepEnd=now();
}
template<class Call> auto process(Call call, EntropySrcBlockF64& block) noexcept {
    current->processBegin=now(); const auto result=call(); current->processEnd=now();
    current->consumed=block.consumed_frames; current->produced=block.produced_frames; return result;
}
inline void write(const std::string& prefix,uint64_t frequency) {
    std::ofstream out(prefix+"-worker.csv");
    out<<"begin_qpc,end_qpc,process_begin_qpc,process_end_qpc,sleep_begin_qpc,sleep_end_qpc,sleep_branch,capture_before,converted_before,capture_after,converted_after,consumed,produced,total_in,total_out,ready_before,ready_after\n";
    const size_t retained=(std::min)(count,records.size());
    for(size_t i=count-retained;i<count;++i) { const auto& r=records[i%records.size()];
        out<<r.begin<<','<<r.end<<','<<r.processBegin<<','<<r.processEnd<<','<<r.sleepBegin<<','<<r.sleepEnd<<','<<r.sleepBranch<<','<<r.captureBefore<<','<<r.convertedBefore<<','<<r.captureAfter<<','<<r.convertedAfter<<','<<r.consumed<<','<<r.produced<<','<<r.totalIn<<','<<r.totalOut<<','<<r.readyBefore<<','<<r.readyAfter<<'\n'; }
    std::ofstream meta(prefix+"-trace.json"); meta<<"{\"qpc_frequency\":"<<frequency<<",\"records\":"<<retained<<",\"total_iterations\":"<<count<<",\"capacity\":"<<records.size()<<",\"overwritten_ring_records\":"<<count-retained<<",\"dropped\":0,\"first_qpc\":"<<firstTick<<",\"last_qpc\":"<<lastEnd<<",\"process_timing\":";
    processTiming.write(meta); meta<<",\"worker_mmcss_active\":"<<(workerProAudioActive?"true":"false")<<",\"worker_priority\":"<<workerPriority<<",\"iteration_spacing_timing\":"; spacingTiming.write(meta);
    meta<<",\"capture_mmcss_active\":"<<(captureProAudioActive.load()?"true":"false")<<",\"capture_mmcss_failure\":\""<<captureFailureReason<<"\",\"src_mmcss_failure\":\""<<workerFailureReason<<"\"";
    meta<<",\"sleep_timing\":{"; for(unsigned i=1;i<=3;++i){if(i>1)meta<<',';meta<<'\"'<<i<<"\":";sleepTiming[i].write(meta);} meta<<"},\"sleep_branches\":{\"1\":\"output_target\",\"2\":\"capture_pop_empty\",\"3\":\"configure_failure\"}}";
}
}
