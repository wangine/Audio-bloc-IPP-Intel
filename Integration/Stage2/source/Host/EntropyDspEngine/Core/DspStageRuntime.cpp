#include "DspStageRuntime.h"
#include <algorithm>
#include <cstring>
#include <windows.h>
namespace entropy::stage {
namespace {uint64_t tick() noexcept {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}}
// Fixed 1-us bins (0..2047 us) plus overflow. Diagnostics expose upper bounds;
// overflow quantiles conservatively use the measured maximum. No RT allocation.
void DspStageRuntime::recordLatency(unsigned channel,uint64_t duration) noexcept{
 auto us=(duration*1000000ULL+inputSession_.qpc_frequency-1)/inputSession_.qpc_frequency;
 latencyBins_[channel][std::min<uint64_t>(us,2048)]++;latencyCounts_[channel]++;
}
void DspStageRuntime::summarizeLatency(DspDiagnostics& destination) const noexcept{
 const uint64_t maxima[]={diag_.maxExecutorQpc,diag_.maxTraversalQpc,diag_.maxQueueQpc};const unsigned percent[]={50,95,99};
 for(unsigned c=0;c<3;c++)for(unsigned p=0;p<3;p++){
  auto target=(latencyCounts_[c]*percent[p]+99)/100;if(!target)continue;uint64_t count=0;
  for(size_t bin=0;bin<latencyBins_[c].size();bin++){count+=latencyBins_[c][bin];if(count>=target){destination.latencyPercentilesUs[c][p]=bin<2048?bin:(maxima[c]*1000000ULL+inputSession_.qpc_frequency-1)/inputSession_.qpc_frequency;break;}}
 }
}
bool DspStageRuntime::prepareAligner(const std::filesystem::path& root,std::string& error){
 if(pending()){error="Prepare aligner before processing";return false;}if(!aligner_.prepare(root,error))return false;
 diag_.alignment={};diag_.alignment.loaded=1;diag_.alignment.generation=1;refreshAlignment();return true;
}
void DspStageRuntime::refreshAlignment() noexcept {
 auto& a=diag_.alignment;if(!aligner_.loaded())return;const auto c=center_.snapshot();
 // Accumulate native pre-aligner paths only; compensation is never an input.
 const auto e=eq_.snapshot();const PathLatency modules[]={{3,e.loaded&&e.settings.mode!=0,bool(e.latencyValid),e.latencySamples},{4,c.loaded&&c.settings.values[15]!=0,bool(c.latencyValid),c.telemetry.latency_samples}};
 AlignmentProposal proposal;a.valid=calculateAlignment(modules,proposal);
 if(a.valid){a.detected=proposal.detected;a.proposed=proposal.delay;a.target=proposal.target;}
}
AlignmentApplyResult DspStageRuntime::applyAlignment(bool align,uint64_t expectedRevision,uint64_t expectedC) noexcept {
 if(pending())return AlignmentApplyResult::Busy;refreshAlignment();auto& a=diag_.alignment;
 if(!aligner_.loaded()||(align&&(!a.valid||center_.snapshot().revision!=expectedRevision||a.detected[2]!=expectedC))){a.rejected=1;return AlignmentApplyResult::Stale;}
 const std::array<uint64_t,3> delay=align?a.proposed:std::array<uint64_t,3>{};const auto target=align?a.target:0;
 if(!aligner_.configure(align,delay,target)){a.rejected=1;return AlignmentApplyResult::NativeFault;}
 a.mode=align;a.current=delay;a.appliedTarget=target;++a.generation;a.boundaryFrame=diag_.blocksOut?diag_.lastHeader.first_frame+1024:0;a.rejected=0;return AlignmentApplyResult::Applied;
}
bool DspStageRuntime::prepareEq(const std::filesystem::path& root,const EqSettings& settings,std::string& error,uint64_t revision){
 if(pending()){error="Prepare EQ before audio requests";return false;}
 if(!validEqSettings(settings)||!eq_.prepare(root,error)||!eq_.apply(settings,revision)){if(error.empty())error="EQ initial settings rejected";eq_.close();return false;}
 diag_.eq=eq_.snapshot();refreshAlignment();return true;
}
bool DspStageRuntime::applyEq(const EqSettings& settings,uint64_t revision) noexcept {
 if(pending()||!eq_.apply(settings,revision))return false;diag_.eq=eq_.snapshot();refreshAlignment();return true;
}
bool DspStageRuntime::prepareCenter(const std::filesystem::path& root,const CenterSettings& settings,std::string& error){
 if(pending()){error="Prepare Center before audio requests";return false;}
 if(!validCenterSettings(settings)){error="Invalid Center settings";return false;}
 if(!center_.prepare(root,error))return false;
 if(!center_.apply(settings,1)){error="Center configuration rejected";center_.close();return false;}
 diag_.center=center_.snapshot();refreshAlignment();return true;
}
bool DspStageRuntime::applyCenter(const CenterSettings& settings,uint64_t revision) noexcept {
 if(pending() || !center_.apply(settings,revision))return false;
 diag_.center=center_.snapshot();refreshAlignment();return true;
}
bool DspStageRuntime::attach(const StageBusSessionHeader& s,std::string& error){
 const auto previousAlignment=diag_.alignment;stop();diag_={};diag_.alignment=previousAlignment;latencyBins_={};latencyCounts_={};faulted_=false;sourceRevision_=0;downstreamSeen_=downstreamReady_=false;downstreamClaimedAt_=0;
 if(s.layout_id!=MIX_CINEMA10_V1 || validateSession(s,s.mapping_bytes)!=StageBusValidationResult::Ok){error="Invalid Source contract/layout";return false;}
 if(center_.loaded() && !center_.reset()){error="Center reset failed";return false;}
 diag_.center=center_.snapshot();
 if(eq_.loaded()&&!eq_.reset()){error="EQ reset failed";return false;}diag_.eq=eq_.snapshot();
 if(aligner_.loaded()){if(!aligner_.reset()){error="LRC reset failed";return false;}++diag_.alignment.generation;diag_.alignment.boundaryFrame=0;refreshAlignment();}
 inputSession_=s;outputSession_=makeSession(LRC_CINEMA11_V1,s.session_id,s.qpc_frequency);
 if(!input_.open(s,error))return false;
 if(!output_.create(outputSession_,error)){input_.fault();input_.close();return false;}
 outputSession_.producer_pid=GetCurrentProcessId();
 for(unsigned c=0;c<10;c++){inputPointers_[c]=inputPlanes_[c].data();inputConst_[c]=inputPointers_[c];}
 for(unsigned c=0;c<11;c++){outputPointers_[c]=outputPlanes_[c].data();outputConst_[c]=outputPointers_[c];}
 demand_.emplace(outputSession_);attached_=true;if(publisher_)publisher_->setSession(outputSession_);return true;
}
void DspStageRuntime::stop() noexcept{
 if(publisher_)publisher_->setSession({});
 if(attached_){input_.fault();output_.fault();}
 input_.close();output_.close();demand_.reset();attached_=false;
 requested_=sent_=ready_=ackPending_=duplicatePending_=controlPending_=false;
}
DspStepResult DspStageRuntime::fail(bool invalid) noexcept{
 if(!faulted_){diag_.faults++;if(invalid)diag_.invalid++;}faulted_=true;input_.fault();output_.fault();return DspStepResult::Fault;
}
void DspStageRuntime::heartbeat(uint64_t now) noexcept{if(attached_){input_.heartbeat(now);output_.heartbeat(now);}}
bool DspStageRuntime::peersAlive(uint64_t) noexcept{
 if(!attached_ || faulted_)return false;
 auto* raw=static_cast<StageBusSessionHeader*>(output_.address());
 auto pid=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&raw->consumer_pid),0,0);
 if(pid && !downstreamSeen_){downstreamSeen_=true;downstreamClaimedAt_=tick();}
 // peerAlive(0) checks process/fault only. Sample shared heartbeat before QPC:
 // a peer can refresh between the caller's timestamp and the shared read.
 // Offsets are frozen StageBusSDK v1 cache-line offsets, not new wire fields.
 const auto heartbeatFresh=[&](StageBusWin32& bus,size_t offset){
  auto* value=reinterpret_cast<volatile LONG64*>(static_cast<unsigned char*>(bus.address())+offset);
  auto last=InterlockedCompareExchange64(value,0,0);auto current=tick();
  return last>0 && static_cast<uint64_t>(last)<=current && current-static_cast<uint64_t>(last)<=inputSession_.qpc_frequency;
 };
 bool alive=input_.peerAlive(0) && heartbeatFresh(input_,384) &&
     (!downstreamSeen_ || output_.peerAlive(0));
 if(alive && downstreamSeen_){
  auto* hb=reinterpret_cast<volatile LONG64*>(static_cast<unsigned char*>(output_.address())+448);
  const auto last=InterlockedCompareExchange64(hb,0,0);
  if(last==0)alive=tick()-downstreamClaimedAt_<=inputSession_.qpc_frequency;
  else {alive=heartbeatFresh(output_,448);downstreamReady_=alive;}
 }
 if(!alive)fail();return alive;
}
DspStepResult DspStageRuntime::step(uint64_t now) noexcept{
 if(!attached_ || faulted_)return DspStepResult::Fault;
 if((requested_ || ackPending_) && (now<started_ || now-started_>inputSession_.qpc_frequency)){diag_.deadlines++;diag_.missing+=!ready_;return fail();}
 bool progress=false;
 StageBusSourceControl update{};auto control=input_.tryReceiveSourceControl(update);
 if(control==StageBusIoResult::Fault)return fail(true);
 if(control==StageBusIoResult::Ok){if(update.source_revision<sourceRevision_)return fail(true);sourceRevision_=update.source_revision;pendingControl_=update;controlPending_=true;progress=true;}
 if(controlPending_){auto result=output_.trySendSourceControl(pendingControl_);if(result==StageBusIoResult::Fault)return fail(true);if(result==StageBusIoResult::Ok){controlPending_=false;progress=true;}}
 if(ackPending_){auto result=output_.trySendResponse(lastHeader_);if(result==StageBusIoResult::Fault)return fail(true);if(result==StageBusIoResult::Full){diag_.full++;return DspStepResult::Idle;}ackPending_=false;progress=true;}
 StageBusFrameRequest incoming{};auto io=output_.tryReceiveRequest(incoming);
 if(io==StageBusIoResult::Fault)return fail(true);
 if(io==StageBusIoResult::Ok){
  auto accepted=demand_->accept(incoming);
  if(accepted==DemandResult::Fault)return fail(true);
  if(accepted==DemandResult::Duplicate){diag_.duplicates++;if(requested_)duplicatePending_=true;else {ackPending_=true;started_=now;auto a=output_.trySendResponse(lastHeader_);if(a==StageBusIoResult::Fault)return fail(true);if(a==StageBusIoResult::Ok)ackPending_=false;} }
  else {request_=incoming;requested_=true;sent_=ready_=false;started_=now;}
  progress=true;
 }
 if(requested_ && !sent_){
  // Source delivery is independent of Render's scene and hardware gate.
  // Preserve transport identity, but request the complete upstream layout.
  auto upstream=request_;upstream.program={};upstream.program.required_inputs=3;
  upstream.program.music_program_active=1;upstream.program.analyze_music_final=1;
  upstream.program.manual_mute_mask=upstream.program.effective_mute_mask=pendingControl_.manual_mute_mask;
  auto sent=input_.trySendRequest(upstream);if(sent==StageBusIoResult::Fault)return fail(true);if(sent==StageBusIoResult::Ok){sent_=true;progress=true;}else diag_.full++;
 }
 if(requested_ && sent_ && !ready_){
  StageBusBlockHeader b{};auto result=input_.tryConsume(b,inputPointers_.data());
  if(result==StageBusIoResult::Fault)return fail(true);
  if(result==StageBusIoResult::Ok){
   if(b.block_sequence!=request_.block_sequence || b.first_frame!=request_.first_frame || b.stream_epoch!=request_.stream_epoch || b.control_revision!=request_.control_revision)return fail(true);
   received_=tick();diag_.queueQpc=received_>=b.qpc_timestamp?received_-b.qpc_timestamp:0;diag_.maxQueueQpc=std::max(diag_.maxQueueQpc,diag_.queueQpc);recordLatency(2,diag_.queueQpc);
   const auto start=tick();auto done=executor_.process(inputSession_,outputSession_,b,inputConst_.data(),pendingHeader_,outputPointers_.data());
   if(done==DspExecuteResult::Ok && eq_.loaded()){
    // Slot 2 affects MUSIC LR only. Center still receives the independent raw tap.
    if(!eq_.process(inputConst_[0],inputConst_[1],outputPointers_[0],outputPointers_[1]))return fail(true);
    diag_.eq=eq_.snapshot();
   }
   if(done==DspExecuteResult::Ok && center_.loaded()){
    if(!center_.process(inputConst_[0],inputConst_[1],outputPointers_[2]))return fail(true);
    diag_.center=center_.snapshot();
   }
   if(done==DspExecuteResult::Ok && aligner_.loaded()){
    refreshAlignment();const double* in[]={outputPointers_[0],outputPointers_[1],outputPointers_[2]};double* out[]={alignedPlanes_[0].data(),alignedPlanes_[1].data(),alignedPlanes_[2].data()};
    if(!aligner_.process(in,out))return fail(true);for(unsigned c=0;c<3;c++)std::memcpy(outputPointers_[c],out[c],1024*sizeof(double));
   }
   diag_.executorQpc=tick()-start;diag_.maxExecutorQpc=std::max(diag_.maxExecutorQpc,diag_.executorQpc);recordLatency(0,diag_.executorQpc);
   if(done!=DspExecuteResult::Ok)return fail(true);
   diag_.blocksIn++;diag_.framesIn+=1024;ready_=true;progress=true;
  }
 }
 if(ready_){
  auto result=output_.tryPublish(pendingHeader_,outputConst_.data());
  if(result==StageBusIoResult::Fault)return fail(true);
  if(result==StageBusIoResult::Full){diag_.full++;return progress?DspStepResult::Progress:DspStepResult::Idle;}
  // Copy the already completed FP64 output only after its V1 commit. A V2 full
  // or disconnected subscription has no authority over input demand or faults.
  if(publisher_){publisher_->publish(pendingHeader_,outputConst_.data());diag_.publishDrops=publisher_->dropped();}
  lastHeader_=pendingHeader_;diag_.lastHeader=lastHeader_;diag_.blocksOut++;diag_.framesOut+=1024;
  measureSignal(diag_.signal,inputConst_.data(),outputConst_.data(),lastHeader_.validity_mask,tick(),center_.loaded());
  if(recorder_){recorder_->tryCapture(lastHeader_,outputConst_.data());diag_.captureDrops=recorder_->dropped();}
  diag_.traversalQpc=tick()-received_;diag_.maxTraversalQpc=std::max(diag_.maxTraversalQpc,diag_.traversalQpc);recordLatency(1,diag_.traversalQpc);
  if(!demand_->complete(request_.block_sequence))return fail(true);
  ready_=requested_=sent_=false;
  if(duplicatePending_){duplicatePending_=false;ackPending_=true;started_=now;}
  return DspStepResult::Published;
 }
 return progress?DspStepResult::Progress:DspStepResult::Idle;
}
}
