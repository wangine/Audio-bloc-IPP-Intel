#include "DomainIngress.h"
#include "IngressConcurrency.h"
#include <chrono>
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
 IngressSpscQueue<Slot,16> queue;Slot scratch{};StageBusSessionHeader session{};
 Domain domain=Domain::Music;std::thread worker;
 LatestIngressTelemetry<DomainTelemetry> observation;
 std::atomic<uint64_t> consumed{0};
 std::atomic<bool> stopping{false},reconnect{false},demand{false};
 std::atomic<State> state{State::Disabled};
 DomainTelemetry stats{};uint64_t lastEpoch=0,lastGeneration=0,lastMusicRevision=0,lastAuxGeneration=0;
 // Producer/lifecycle only. Graph tries once; it never sleeps or spins.
 // This is used for actual session retirement and inactive keep-latest only,
 // never for a normal active enqueue or for reading/publishing telemetry.
 void maintenance() noexcept {
  while(!queue.beginMaintenance())
   std::this_thread::sleep_for(std::chrono::microseconds(100));
 }
 void publishTelemetry() noexcept {
  stats.consumed=consumed.load(std::memory_order_relaxed);
  stats.queued=queue.readableApprox();stats.state=state.load(std::memory_order_acquire);
  observation.publish(stats);
 }
 void fail(bool gap) noexcept {
  state.store(State::Fault,std::memory_order_release);
  maintenance();++stats.faults;if(gap)++stats.gaps;++stats.generation;
  queue.discardExclusive();queue.endMaintenance();publishTelemetry();
 }
 void run(std::wstring endpoint) noexcept {
  control::HighResolutionWait wait;DWORD task=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
  if(!wait.valid()||!mmcss||!AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL)){
   if(mmcss)AvRevertMmThreadCharacteristics(mmcss);fail(false);return;
  }
  publish::Subscriber subscriber;bool connected=false;uint64_t nextConnect=0,lastReceive=0,previousReceiveQpc=0;std::string error;
  while(!stopping.load()){
   if(reconnect.exchange(false)){
    subscriber.close();connected=false;state=State::Waiting;
    maintenance();queue.discardExclusive();++stats.generation;
    queue.endMaintenance();publishTelemetry();nextConnect=0;
   }
   // A fault retires this local subscription, not the independent input
   // worker. Retry with normal session/PCM validation and retain counters.
   if(!connected){if(GetTickCount64()<nextConnect){Sleep(1);continue;}
    connected=subscriber.connect(endpoint,error);nextConnect=GetTickCount64()+100;
    if(connected&&!validateSessionForDomain(subscriber.session(),domain)){subscriber.close();connected=false;fail(false);continue;}
    if(!connected)continue;
    maintenance();queue.discardExclusive();
    session=subscriber.session();stats.session=session;stats.producerPid=session.producer_pid;
    ++stats.generation;lastEpoch=lastGeneration=lastMusicRevision=lastAuxGeneration=0;
    state=State::Waiting;queue.endMaintenance();publishTelemetry();lastReceive=GetTickCount64();
   }
   subscriber.heartbeat(tick());std::array<double*,8> planes{};for(unsigned c=0;c<8;++c)planes[c]=scratch.audio[c].data();
   auto result=subscriber.read(scratch.block,planes.data());
   if(result==publish::ReadResult::Empty){if(GetTickCount64()-lastReceive>200){fail(false);subscriber.close();connected=false;}else wait.forDuration(std::chrono::microseconds(100));continue;}
   // Scratch belongs only to this producer. Compute telemetry while the
   // Graph is still free to dequeue the preceding block.
   std::array<double,8> blockPeaks{};
   if(result==publish::ReadResult::Block){
    if(!validateBlockForDomain(session,scratch.block,domain))result=publish::ReadResult::Invalid;
    for(unsigned c=0;c<session.channels;++c)for(double v:scratch.audio[c]){
     if(!std::isfinite(v))result=publish::ReadResult::Invalid;
     else blockPeaks[c]=std::max(blockPeaks[c],std::abs(v));
    }
   }
   if(result!=publish::ReadResult::Block){fail(result==publish::ReadResult::Gap);subscriber.close();connected=false;continue;}
   lastReceive=GetTickCount64();const auto receivedQpc=tick();
   if(previousReceiveQpc)stats.maximumReceiveGapQpc=std::max(stats.maximumReceiveGapQpc,receivedQpc-previousReceiveQpc);
   if(receivedQpc>=scratch.block.qpc_timestamp)stats.maximumSourceAgeQpc=std::max(stats.maximumSourceAgeQpc,receivedQpc-scratch.block.qpc_timestamp);
   previousReceiveQpc=receivedQpc;
   bool idleMaintenance=false;
   if(!demand.load(std::memory_order_acquire)){
    maintenance();idleMaintenance=true;
    // A selection may have become active while an earlier read was finishing.
    // Recheck after exclusion; never discard a newly demanded queue here.
    if(!demand.load(std::memory_order_acquire))stats.discarded+=queue.discardExclusive();
   }
   auto* available=queue.producerSlot();
   if(!available){
    if(idleMaintenance)queue.endMaintenance();
    fail(true);subscriber.close();connected=false;continue;
   }
   auto generation=scratch.block.source_generation[domain==Domain::Music?0:2];
   if(lastEpoch!=scratch.block.stream_epoch||lastGeneration!=generation||(domain==Domain::Music&&(lastMusicRevision!=scratch.block.music_session_revision||lastAuxGeneration!=scratch.block.source_generation[1]))){++stats.generation;lastEpoch=scratch.block.stream_epoch;lastGeneration=generation;lastMusicRevision=scratch.block.music_session_revision;lastAuxGeneration=scratch.block.source_generation[1];}
   scratch.token=stats.generation;
   auto& destination=*available;
   destination.block=scratch.block;destination.token=scratch.token;
   // Copy only the session's real channels, not all eight scratch planes.
   // Consumer reads the same channel count; inactive planes are never exposed.
   for(unsigned c=0;c<session.channels;++c)
    std::memcpy(destination.audio[c].data(),scratch.audio[c].data(),sizeof(scratch.audio[c]));
   queue.commitProducer();
   if(idleMaintenance)queue.endMaintenance();
   stats.block=scratch.block;stats.lastSequence=scratch.block.block_sequence;stats.lastFirstFrame=scratch.block.first_frame;stats.lastEpoch=scratch.block.stream_epoch;stats.sourceQpc=scratch.block.qpc_timestamp;stats.lastBlockQpc=tick();stats.validityMask=scratch.block.validity_mask;stats.sourceDiscontinuityMask=scratch.block.discontinuity_mask;
   if(scratch.block.discontinuity_mask)++stats.sourceFlaggedBlocks;
   memcpy(stats.sourceSession.data(),session.session_id,16);for(unsigned c=0;c<session.channels;++c){stats.peaks[c]=blockPeaks[c];stats.maximumPeaks[c]=std::max(stats.maximumPeaks[c],blockPeaks[c]);}
   ++stats.received;state=State::Receiving;publishTelemetry();
  }
  subscriber.close();state=State::Disabled;publishTelemetry();AvRevertMmThreadCharacteristics(mmcss);
 }
};
DomainIngress::DomainIngress():impl_(std::make_unique<Impl>()){}
DomainIngress::~DomainIngress(){stop();}
bool DomainIngress::start(Domain d,std::wstring endpoint){auto& i=*impl_;if(i.worker.joinable()||!localEndpoint(endpoint))return false;i.domain=d;i.stopping=false;i.reconnect=false;i.state=State::Waiting;i.worker=std::thread([&i,endpoint=std::move(endpoint)]{i.run(endpoint);});return true;}
void DomainIngress::stop() noexcept {
 auto& i=*impl_;i.stopping=true;if(i.worker.joinable())i.worker.join();
 i.state=State::Disabled;i.maintenance();i.queue.discardExclusive();
 i.queue.endMaintenance();i.publishTelemetry();
}
void DomainIngress::restart() noexcept {impl_->reconnect=true;}
void DomainIngress::required(bool required) noexcept {impl_->demand=required;}
State DomainIngress::state() const noexcept {return impl_->state.load();}
DomainTelemetry DomainIngress::telemetry() const noexcept {
 auto& i=*impl_;auto t=i.observation.read();
 // Live counters/state are observations, not authority to reset or consume PCM.
 t.consumed=i.consumed.load(std::memory_order_relaxed);
 t.queued=i.queue.readableApprox();t.state=i.state.load(std::memory_order_acquire);return t;
}
bool DomainIngress::acquire() noexcept {return impl_->queue.acquireConsumer();}
void DomainIngress::release() noexcept {impl_->queue.releaseConsumer();}
bool DomainIngress::ready() const noexcept {return impl_->state==State::Receiving&&impl_->queue.front()!=nullptr;}
void DomainIngress::readLocked(double* const* output,DomainContext& context) noexcept {
 // The existing private API name denotes a pinned consumer reservation, not
 // the old shared data/telemetry lock. ProgramIngress validates both domains
 // before consuming either, and holds reservations through the complete copy.
 auto& i=*impl_;const auto& slot=*i.queue.front();
 for(unsigned c=0;c<i.session.channels;++c)memcpy(output[c],slot.audio[c].data(),sizeof(slot.audio[c]));
 context={i.session,slot.block,slot.token};i.queue.popConsumer();
 i.consumed.fetch_add(1,std::memory_order_relaxed);
}
bool DomainIngress::tryRead(double* const* output,StageBusBlockHeader& block) noexcept {if(!output||!acquire())return false;bool ok=ready();if(ok){DomainContext c;readLocked(output,c);block=c.block;}release();return ok;}
}
