#include "Stage2Ingress.h"
#include "Engine/HighResolutionWait.h"
#include <windows.h>
#include <avrt.h>
#include <algorithm>
#include <cmath>
namespace entropy::input {
namespace {
uint64_t tick() noexcept { LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart; }
void silence(double* const* output) noexcept {
    if(output)for(unsigned c=0;c<13;++c)if(output[c])std::fill_n(output[c],Stage2Ingress::frames,0.0);
}
}
bool Stage2Ingress::start(std::wstring endpoint,bool claimOwner) {
    if(worker_.joinable())return false;
    if(endpoint.empty())endpoint=publish::endpointName();
    if(endpoint.empty())return false;
    stopping_=false;state_=State::Waiting;
    worker_=std::thread(&Stage2Ingress::run,this,std::move(endpoint),claimOwner);return true;
}
void Stage2Ingress::stop() noexcept {
    stopping_=true;if(worker_.joinable())worker_.join();
    generation_.fetch_add(1,std::memory_order_acq_rel);
    state_=State::Disabled;producerPid_=0;validityMask_=0;
}
void Stage2Ingress::restartSubscription() noexcept {
    // Quiescent graph restart: discard only this consumer's subscription.
    reconnect_.store(true,std::memory_order_release);
}
bool Stage2Ingress::tryRead(double* const* output) noexcept {
    if(!output)return false;for(unsigned c=0;c<13;++c)if(!output[c])return false;
    if(reconnect_.load(std::memory_order_acquire)||access_.test_and_set(std::memory_order_acquire)){
        silence(output);empty_.fetch_add(1,std::memory_order_relaxed);return false;
    }
    struct Unlock {std::atomic_flag& flag;~Unlock(){flag.clear(std::memory_order_release);}} unlock{access_};
    if(!connected_||state_.load()==State::Fault){silence(output);empty_.fetch_add(1);return false;}
    StageBusBlockHeader header{};auto result=subscriber_.read(header,output);
    if(result==publish::ReadResult::Block){
        for(unsigned c=0;c<channels;++c)for(unsigned f=0;f<frames;++f)
            if(!std::isfinite(output[c][f]))result=publish::ReadResult::Invalid;
        if(result==publish::ReadResult::Block){
            for(unsigned c=channels;c<13;++c)std::fill_n(output[c],frames,0.0);
            sequence_=header.block_sequence;firstFrame_=header.first_frame;epoch_=header.stream_epoch;
            lastBlock_=header;sourceDiscontinuityMask_=header.discontinuity_mask;
            if(header.discontinuity_mask)sourceFlaggedBlocks_.fetch_add(1);
            sourceQpc_=header.qpc_timestamp;validityMask_=header.validity_mask;lastBlockQpc_=tick();
            received_.fetch_add(1);state_=State::Receiving;return true;
        }
    }
    if(result!=publish::ReadResult::Empty){
        if(result==publish::ReadResult::Gap)gaps_.fetch_add(1);
        faults_.fetch_add(1);generation_.fetch_add(1);validityMask_=0;producerPid_=0;state_=State::Fault;
    }else empty_.fetch_add(1);
    silence(output);return false;
}
Telemetry Stage2Ingress::telemetry() const noexcept {
    Telemetry t;t.state=state_.load();t.received=t.consumed=received_.load();t.empty=empty_.load();
    t.faults=faults_.load();t.generation=generation_.load();t.producerPid=producerPid_.load();
    t.validityMask=validityMask_.load();t.lastBlockQpc=lastBlockQpc_.load();t.gaps=gaps_.load();
    t.sourceDiscontinuityMask=sourceDiscontinuityMask_.load();
    t.sourceFlaggedBlocks=sourceFlaggedBlocks_.load();
    t.lastSequence=sequence_.load();t.lastFirstFrame=firstFrame_.load();t.lastEpoch=epoch_.load();t.sourceQpc=sourceQpc_.load();
    for(unsigned n=0;n<16;++n)t.sourceSession[n]=sourceSession_[n].load();return t;
}
const char* Stage2Ingress::stateText(State s) noexcept {
    switch(s){case State::Receiving:return "FP64 SOURCE / 192 kHz / PASSIVE V2";
        case State::Fault:return "FP64 INPUT FAULT / LOCAL RESTART REQUIRED";
        case State::OwnershipBusy:return "INPUT OWNED BY ANOTHER CORE";
        case State::Disabled:return "FP64 INPUT DISABLED";default:return "WAITING FOR FP64 PUBLISHER V2";}
}
void Stage2Ingress::run(std::wstring endpoint,bool claimOwner) noexcept {
    control::HighResolutionWait wait;DWORD index=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);
    if(!wait.valid()||!mmcss||!AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL)){
        if(mmcss)AvRevertMmThreadCharacteristics(mmcss);state_=State::Fault;faults_.fetch_add(1);return;
    }
    HANDLE owner=nullptr;
    while(claimOwner&&!stopping_){
        owner=CreateMutexW(nullptr,FALSE,L"Local\\Entropy.Stage3.Render.Owner.V1");
        if(owner&&GetLastError()!=ERROR_ALREADY_EXISTS)break;
        if(owner)CloseHandle(owner);owner=nullptr;state_=State::OwnershipBusy;
        wait.forDuration(std::chrono::milliseconds(100));
    }
    auto nextConnect=std::chrono::steady_clock::now();std::string error;
    while(!stopping_){
        if(!access_.test_and_set(std::memory_order_acquire)){
            if(reconnect_.exchange(false,std::memory_order_acq_rel)){
                subscriber_.close();connected_=false;generation_.fetch_add(1);
                state_=State::Waiting;validityMask_=0;nextConnect=std::chrono::steady_clock::now();
            }
            if(state_==State::Fault){subscriber_.close();connected_=false;}
            else if(!connected_&&std::chrono::steady_clock::now()>=nextConnect){
                connected_=subscriber_.connect(endpoint,error);
                nextConnect=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
                if(connected_){const auto& session=subscriber_.session();producerPid_=session.producer_pid;
                    for(unsigned n=0;n<16;++n)sourceSession_[n]=session.session_id[n];
                    generation_.fetch_add(1);state_=State::Waiting;}
            }
            if(connected_)subscriber_.heartbeat(tick());access_.clear(std::memory_order_release);
        }
        wait.forDuration(std::chrono::microseconds(100));
    }
    while(access_.test_and_set(std::memory_order_acquire))std::this_thread::yield();
    subscriber_.close();connected_=false;access_.clear(std::memory_order_release);
    if(owner)CloseHandle(owner);AvRevertMmThreadCharacteristics(mmcss);
}
}
