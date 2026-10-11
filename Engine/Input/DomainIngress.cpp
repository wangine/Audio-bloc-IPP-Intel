#include "DomainIngress.h"
#include "Engine/HighResolutionWait.h"
#include <windows.h>
#include <avrt.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace entropy::input {
namespace {uint64_t tick() noexcept {LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}}
struct DomainIngress::Impl {
 struct Slot {StageBusBlockHeader block{};uint64_t token=0;std::array<std::array<double,1024>,8> audio{};};
 std::array<Slot,16> queue{};Slot scratch{};StageBusSessionHeader session{};
 unsigned head=0,size=0;Domain domain=Domain::Music;std::thread worker;
 mutable std::atomic_flag guard=ATOMIC_FLAG_INIT;
 std::atomic<bool> stopping{false},reconnect{false},demand{false};
 std::atomic<State> state{State::Disabled};
 DomainTelemetry stats{};uint64_t lastEpoch=0,lastGeneration=0,lastMusicRevision=0,lastAuxGeneration=0;
 void lock() noexcept {while(guard.test_and_set(std::memory_order_acquire))std::this_thread::yield();}
 void unlock() noexcept {guard.clear(std::memory_order_release);}
 void fail(bool gap) noexcept {lock();++stats.faults;if(gap)++stats.gaps;++stats.generation;size=0;state=State::Fault;unlock();}
 void run(std::wstring endpoint) noexcept {
  control::HighResolutionWait wait;DWORD task=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
  if(!wait.valid()||!mmcss||!AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL)){
   if(mmcss)AvRevertMmThreadCharacteristics(mmcss);fail(false);return;
  }
  publish::Subscriber subscriber;bool connected=false;uint64_t nextConnect=0,lastReceive=0,previousReceiveQpc=0;std::string error;
  while(!stopping.load()){
   if(reconnect.exchange(false)){subscriber.close();connected=false;lock();size=0;++stats.generation;state=State::Waiting;unlock();nextConnect=0;}
   // A fault retires this local subscription, not the independent input
   // worker. Retry with normal session/PCM validation and retain counters.
   if(!connected){if(GetTickCount64()<nextConnect){Sleep(1);continue;}
    connected=subscriber.connect(endpoint,error);nextConnect=GetTickCount64()+100;
    if(connected&&!validateSessionForDomain(subscriber.session(),domain)){subscriber.close();connected=false;fail(false);continue;}
    if(!connected)continue;
    lock();session=subscriber.session();stats.session=session;stats.producerPid=session.producer_pid;size=0;++stats.generation;lastEpoch=lastGeneration=lastMusicRevision=lastAuxGeneration=0;state=State::Waiting;unlock();lastReceive=GetTickCount64();
   }
   subscriber.heartbeat(tick());std::array<double*,8> planes{};for(unsigned c=0;c<8;++c)planes[c]=scratch.audio[c].data();
   auto result=subscriber.read(scratch.block,planes.data());
   if(result==publish::ReadResult::Empty){if(GetTickCount64()-lastReceive>200){fail(false);subscriber.close();connected=false;}else wait.forDuration(std::chrono::microseconds(100));continue;}
   if(result==publish::ReadResult::Block){
    if(!validateBlockForDomain(session,scratch.block,domain))result=publish::ReadResult::Invalid;
    for(unsigned c=0;c<session.channels;++c)for(double v:scratch.audio[c])if(!std::isfinite(v))result=publish::ReadResult::Invalid;
   }
   if(result!=publish::ReadResult::Block){fail(result==publish::ReadResult::Gap);subscriber.close();connected=false;continue;}
   lastReceive=GetTickCount64();const auto receivedQpc=tick();lock();
   if(previousReceiveQpc)stats.maximumReceiveGapQpc=std::max(stats.maximumReceiveGapQpc,receivedQpc-previousReceiveQpc);
   if(receivedQpc>=scratch.block.qpc_timestamp)stats.maximumSourceAgeQpc=std::max(stats.maximumSourceAgeQpc,receivedQpc-scratch.block.qpc_timestamp);
   previousReceiveQpc=receivedQpc;
   if(!demand.load()&&size){stats.discarded+=size;head=0;size=0;}
   if(size==queue.size()){unlock();fail(true);subscriber.close();connected=false;continue;}
   auto generation=scratch.block.source_generation[domain==Domain::Music?0:2];
   if(lastEpoch!=scratch.block.stream_epoch||lastGeneration!=generation||(domain==Domain::Music&&(lastMusicRevision!=scratch.block.music_session_revision||lastAuxGeneration!=scratch.block.source_generation[1]))){++stats.generation;lastEpoch=scratch.block.stream_epoch;lastGeneration=generation;lastMusicRevision=scratch.block.music_session_revision;lastAuxGeneration=scratch.block.source_generation[1];}
   scratch.token=stats.generation;queue[(head+size)%queue.size()]=scratch;++size;
   stats.block=scratch.block;stats.lastSequence=scratch.block.block_sequence;stats.lastFirstFrame=scratch.block.first_frame;stats.lastEpoch=scratch.block.stream_epoch;stats.sourceQpc=scratch.block.qpc_timestamp;stats.lastBlockQpc=tick();stats.validityMask=scratch.block.validity_mask;stats.sourceDiscontinuityMask=scratch.block.discontinuity_mask;
   if(scratch.block.discontinuity_mask)++stats.sourceFlaggedBlocks;
   memcpy(stats.sourceSession.data(),session.session_id,16);for(unsigned c=0;c<session.channels;++c){double p=0;for(double v:scratch.audio[c])p=std::max(p,std::abs(v));stats.peaks[c]=p;stats.maximumPeaks[c]=std::max(stats.maximumPeaks[c],p);}
   ++stats.received;state=State::Receiving;unlock();
  }
  subscriber.close();state=State::Disabled;AvRevertMmThreadCharacteristics(mmcss);
 }
};
DomainIngress::DomainIngress():impl_(std::make_unique<Impl>()){}
DomainIngress::~DomainIngress(){stop();}
bool DomainIngress::start(Domain d,std::wstring endpoint){auto& i=*impl_;if(i.worker.joinable()||!localEndpoint(endpoint))return false;i.domain=d;i.stopping=false;i.reconnect=false;i.state=State::Waiting;i.worker=std::thread([&i,endpoint=std::move(endpoint)]{i.run(endpoint);});return true;}
void DomainIngress::stop() noexcept {auto& i=*impl_;i.stopping=true;if(i.worker.joinable())i.worker.join();i.lock();i.size=0;i.state=State::Disabled;i.unlock();}
void DomainIngress::restart() noexcept {impl_->reconnect=true;}
void DomainIngress::required(bool required) noexcept {impl_->demand=required;}
State DomainIngress::state() const noexcept {return impl_->state.load();}
DomainTelemetry DomainIngress::telemetry() const noexcept {auto& i=*impl_;i.lock();auto t=i.stats;t.queued=i.size;t.state=i.state.load();i.unlock();return t;}
bool DomainIngress::acquire() noexcept {return !impl_->guard.test_and_set(std::memory_order_acquire);}
void DomainIngress::release() noexcept {impl_->unlock();}
bool DomainIngress::ready() const noexcept {return impl_->size&&impl_->state==State::Receiving;}
void DomainIngress::readLocked(double* const* output,DomainContext& context) noexcept {auto& i=*impl_;auto& slot=i.queue[i.head];for(unsigned c=0;c<i.session.channels;++c)memcpy(output[c],slot.audio[c].data(),8192);context={i.session,slot.block,slot.token};i.head=(i.head+1)%16;--i.size;++i.stats.consumed;}
bool DomainIngress::tryRead(double* const* output,StageBusBlockHeader& block) noexcept {if(!output||!acquire())return false;bool ok=ready();if(ok){DomainContext c;readLocked(output,c);block=c.block;}release();return ok;}
}
