#include "Stage2Ingress.h"
#include "Engine/HighResolutionWait.h"
#include <StageLinkClient.h>
#include <windows.h>
#include <avrt.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <iostream>
namespace entropy::input {
namespace {
uint64_t tick() noexcept { LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart; }
void silence(double* const* output) noexcept {
    if(output)for(unsigned c=0;c<13;++c)if(output[c])std::fill_n(output[c],Stage2Ingress::frames,0.0);
}
}
bool Stage2Ingress::start(std::wstring endpoint,bool claimOwner) {
    if(worker_.joinable())return false;
    if(endpoint.empty())endpoint=entropy::link::endpointName()+L".Downstream.V1";
    if(endpoint.empty())return false;
    generation_.fetch_add(1,std::memory_order_acq_rel);
    read_.store(written_.load(std::memory_order_acquire),std::memory_order_release);
    stopping_=false;state_=State::Waiting;
    worker_=std::thread(&Stage2Ingress::run,this,std::move(endpoint),claimOwner);return true;
}
void Stage2Ingress::stop() noexcept {
    stopping_=true;generation_.fetch_add(1,std::memory_order_acq_rel);
    if(worker_.joinable())worker_.join();
    // An iteration already running when stop began may have stamped the
    // first invalidation generation. Invalidate again after its last write.
    generation_.fetch_add(1,std::memory_order_acq_rel);
    state_=State::Disabled;producerPid_=0;validityMask_=0;
}
bool Stage2Ingress::tryRead(double* const* output) noexcept {
    if(!output)return false;
    for(unsigned c=0;c<13;++c)if(!output[c])return false;
    const auto generation=generation_.load(std::memory_order_acquire);
    auto read=read_.load(std::memory_order_relaxed);
    const auto written=written_.load(std::memory_order_acquire);
    while(read<written){
        const auto& slot=queue_[read%capacity];
        if(slot.generation!=generation){read_.store(++read,std::memory_order_release);continue;}
        for(unsigned c=0;c<channels;++c)std::memcpy(output[c],slot.planes[c].data(),frames*sizeof(double));
        for(unsigned c=channels;c<13;++c)std::fill_n(output[c],frames,0.0);
        read_.store(read+1,std::memory_order_release);
        if(generation_.load(std::memory_order_acquire)!=generation||stopping_.load(std::memory_order_acquire)){silence(output);return false;}
        lastBlockQpc_.store(tick(),std::memory_order_release);consumed_.fetch_add(1,std::memory_order_relaxed);return true;
    }
    silence(output);empty_.fetch_add(1,std::memory_order_relaxed);return false;
}
Telemetry Stage2Ingress::telemetry() const noexcept {
    const auto read=read_.load(std::memory_order_acquire),written=written_.load(std::memory_order_acquire);
    return {state_.load(),received_.load(),consumed_.load(),empty_.load(),faults_.load(),generation_.load(),unsigned(std::min<uint64_t>(written>=read?written-read:0,capacity)),producerPid_.load(),validityMask_.load(),lastBlockQpc_.load(),releaseFailures_.load()};
}
const char* Stage2Ingress::stateText(State state) noexcept {
    switch(state){case State::Receiving:return "STAGE2 FP64 / 192 kHz";case State::Fault:return "STAGE2 INPUT FAULT";case State::OwnershipBusy:return "STAGE2 OWNED BY ANOTHER RENDER";case State::Disabled:return "STAGE2 INPUT DISABLED";default:return "WAITING FOR STAGE2";}
}
void Stage2Ingress::run(std::wstring endpoint,bool claimOwner) noexcept {
    entropy::control::HighResolutionWait wait;
    if(!wait.valid()){state_=State::Fault;faults_.fetch_add(1);return;}
    struct RealtimeTask {
        DWORD index=0;HANDLE handle=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);
        bool registered=handle&&AvSetMmThreadPriority(handle,AVRT_PRIORITY_CRITICAL);
        ~RealtimeTask(){if(handle)AvRevertMmThreadCharacteristics(handle);}
    } realtime;
    if(!realtime.registered){state_=State::Fault;faults_.fetch_add(1);return;}
    using namespace entropy::link;using namespace entropy::stage;
    HANDLE owner=nullptr;
    while(claimOwner&&!stopping_.load(std::memory_order_acquire)){
        owner=CreateMutexW(nullptr,FALSE,L"Local\\Entropy.Stage3.Render.Owner.V1");
        if(owner&&GetLastError()!=ERROR_ALREADY_EXISTS)break;
        if(owner)CloseHandle(owner);owner=nullptr;state_=State::OwnershipBusy;
        for(unsigned n=0;n<50&&!stopping_;++n)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    StageBusWin32 bus;StageLinkClient client(std::move(endpoint));
    StageLinkResponse bound{};bool attached=false,pending=false;std::array<uint8_t,16> failed{};
    StageBusFrameRequest request{};uint64_t sequence=1,revision=1,started=0;
    auto next=std::chrono::steady_clock::now();std::string error;
    auto invalidate=[&]{generation_.fetch_add(1,std::memory_order_acq_rel);producerPid_=0;validityMask_=0;};
    auto release=[&]{
        if(!zero(bound.lease,16)){
            auto command=makeRequest(StageLinkAction::Release);std::memcpy(command.lease,bound.lease,16);std::memcpy(command.expectedSession,bound.session.session_id,16);
            StageLinkResponse reply{};
            if(!client.transact(command,reply,2000,error)||reply.status!=StageLinkStatus::Ok){
                releaseFailures_.fetch_add(1,std::memory_order_relaxed);
                // Transport worker only, never the DSP callback. Do not report
                // a refused/unconfirmed release as a successful handoff.
                std::cerr<<"FP64 ingress: lease release unconfirmed; status="<<unsigned(reply.status)<<"; "<<error<<'\n';
            }
        }
        bound={};bus.close();attached=pending=false;
    };
    try {
        while(!stopping_.load(std::memory_order_acquire)){
            if(!attached){
                StageLinkResponse query{};
                if(!client.transact(makeRequest(StageLinkAction::Query),query,500,error)||query.status!=StageLinkStatus::Ok){
                    state_=State::Waiting;for(unsigned n=0;n<100&&!stopping_;++n)std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;
                }
                if(query.session.layout_id!=LRC_CINEMA11_V1||validateSession(query.session,query.session.mapping_bytes)!=StageBusValidationResult::Ok||query.session.producer_pid!=query.sourcePid||!std::memcmp(query.session.session_id,failed.data(),16)){
                    state_=State::Fault;std::this_thread::sleep_for(std::chrono::milliseconds(100));continue;
                }
                auto command=makeRequest(StageLinkAction::Attach);std::memcpy(command.expectedSession,query.session.session_id,16);
                if(!client.transact(command,bound,5000,error)||bound.status!=StageLinkStatus::Ok){bound={};state_=State::Waiting;std::this_thread::sleep_for(std::chrono::milliseconds(100));continue;}
                if(!std::memcmp(query.session.session_id,bound.session.session_id,16)||bound.sourcePid!=query.sourcePid||bound.session.producer_pid!=bound.sourcePid||bound.session.layout_id!=LRC_CINEMA11_V1||!bound.controlRevision||zero(bound.lease,16)||!bus.open(bound.session,error)){
                    faults_.fetch_add(1);state_=State::Fault;invalidate();release();continue;
                }
                StageLinkResponse confirmed{};
                if(!client.transact(makeRequest(StageLinkAction::Query),confirmed,500,error)||confirmed.status!=StageLinkStatus::Ok||confirmed.sourcePid!=bound.sourcePid||std::memcmp(confirmed.session.session_id,bound.session.session_id,16)){
                    faults_.fetch_add(1);invalidate();release();continue;
                }
                generation_.fetch_add(1,std::memory_order_acq_rel);producerPid_=bound.sourcePid;sequence=1;revision=bound.controlRevision;attached=true;pending=false;next=std::chrono::steady_clock::now();
            }
            const auto now=tick();bus.heartbeat(now);
            // The publisher can update its heartbeat after our QPC snapshot.
            // The immutable SDK rejects that future snapshot; retry with fresh
            // time before faulting, retaining its process/one-second checks.
            bool fault=!bus.peerAlive(now);
            if(fault)fault=!bus.peerAlive(tick());
            StageBusSourceControl control{};const auto controlIo=bus.tryReceiveSourceControl(control);
            if(controlIo==StageBusIoResult::Fault)fault=true;
            else if(controlIo==StageBusIoResult::Ok){if(control.source_revision<revision)fault=true;else revision=control.source_revision;}
            if(!fault&&pending){
                auto written=written_.load(std::memory_order_relaxed);auto& slot=queue_[written%capacity];
                std::array<double*,channels> pointers{};for(unsigned c=0;c<channels;++c)pointers[c]=slot.planes[c].data();
                StageBusBlockHeader block{};const auto io=bus.tryConsume(block,pointers.data());
                if(io==StageBusIoResult::Fault)fault=true;
                else if(io==StageBusIoResult::Ok){
                    // The transport request completed even if this receiver rejects
                    // its samples. Do not leave a fictitious request pending.
                    pending=false;
                    if(block.stream_epoch!=request.stream_epoch||block.block_sequence!=request.block_sequence||block.first_frame!=request.first_frame||block.control_revision!=request.control_revision)fault=true;
                    else {
                        for(const auto& plane:slot.planes)for(double value:plane)if(!std::isfinite(value))fault=true;
                        if(!fault){slot.header=block;slot.generation=generation_.load(std::memory_order_acquire);written_.store(written+1,std::memory_order_release);received_.fetch_add(1);validityMask_=block.validity_mask;state_=State::Receiving;pending=false;++sequence;next+=std::chrono::nanoseconds(5333333);}
                    }
                }else if(now<started||now-started>bound.session.qpc_frequency)fault=true;
            }
            if(fault){
                // A receiver-local validation/timeout failure must not poison the
                // publisher's shared bus. Silence locally and release the lease.
                // The immutable SDK still rejects malformed transport contracts.
                std::memcpy(failed.data(),bound.session.session_id,16);state_=State::Fault;faults_.fetch_add(1);invalidate();release();continue;
            }
            // At most one queued block. Capacity is bounded headroom, never a fill target.
            if(!pending&&written_.load(std::memory_order_relaxed)==read_.load(std::memory_order_acquire)&&std::chrono::steady_clock::now()>=next){
                request={};std::memcpy(request.session_id,bound.session.session_id,16);request.stream_epoch=1;request.block_sequence=sequence;request.first_frame=(sequence-1)*frames;request.frames=frames;request.control_revision=revision;
                // Original Stage3 demand requests the complete upstream layout independent of local scene/DAC state.
                request.program.required_inputs=3;request.program.music_program_active=request.program.analyze_music_final=1;
                const auto io=bus.trySendRequest(request);
                if(io==StageBusIoResult::Ok){pending=true;started=now;}
                else if(io==StageBusIoResult::Fault){state_=State::Fault;faults_.fetch_add(1);invalidate();release();}
            }
            wait.forDuration(std::chrono::microseconds(100));
        }
        // Drain an outstanding transport request before releasing ownership, without publishing stale audio.
        const auto deadline=GetTickCount64()+1000;
        while(attached&&pending&&GetTickCount64()<deadline){
            bus.heartbeat(tick());auto& slot=queue_[written_.load()%capacity];std::array<double*,channels> pointers{};for(unsigned c=0;c<channels;++c)pointers[c]=slot.planes[c].data();StageBusBlockHeader block{};
            auto io=bus.tryConsume(block,pointers.data());if(io!=StageBusIoResult::Empty){pending=false;break;}std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        release();
    }catch(...){
        state_=State::Fault;faults_.fetch_add(1);invalidate();
        try{release();}catch(...){bus.close();}
    }
    if(owner)CloseHandle(owner);
}
}
