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
bool DspStageRuntime::resetHistory() noexcept{
 if(center_.loaded() && !center_.reset())return false;diag_.center=center_.snapshot();
 if(eq_.loaded() && !eq_.reset())return false;diag_.eq=eq_.snapshot();
 if(aligner_.loaded()){if(!aligner_.reset())return false;++diag_.alignment.generation;diag_.alignment.boundaryFrame=diag_.blocksOut?diag_.lastHeader.first_frame+1024:0;refreshAlignment();}
 return true;
}
bool DspStageRuntime::attach(const StageBusSessionHeader&,std::string& error){
 error="Incompatible V1 demand session; Stage2 requires passive Stage1 StagePublish V2";return false;
}
bool DspStageRuntime::attachPassive(const std::wstring& endpoint,std::string& error){
 stop();faulted_=false;
 if(!input_.connect(endpoint,error))return false;
 const auto s=input_.session();
 if(s.layout_id!=MUSIC_STEREO2_V1 || validateSession(s,s.mapping_bytes)!=StageBusValidationResult::Ok){input_.close();error="Incompatible passive Source contract; expected MUSIC_STEREO2_V1";return false;}
 if(!resetHistory()){input_.close();error="Native DSP history reset failed";return false;}
 inputSession_=s;outputSession_=makeSession(MUSIC_LRC3_V1,s.session_id,s.qpc_frequency);outputSession_.producer_pid=GetCurrentProcessId();
 for(unsigned c=0;c<2;c++){inputPointers_[c]=inputPlanes_[c].data();inputConst_[c]=inputPointers_[c];}
 for(unsigned c=0;c<3;c++){outputPointers_[c]=outputPlanes_[c].data();outputConst_[c]=outputPointers_[c];}
 haveInput_=false;attached_=true;downstreamReady_=true;
 if(publisher_)publisher_->setSession(outputSession_);error.clear();return true;
}
void DspStageRuntime::stop() noexcept{
 if(publisher_)publisher_->setSession({});
 input_.close();attached_=false;ready_=false;haveInput_=false;downstreamReady_=false;
}
DspStepResult DspStageRuntime::fail(bool invalid) noexcept{
 if(!faulted_){diag_.faults++;if(invalid)diag_.invalid++;}faulted_=true;stop();return DspStepResult::Fault;
}
void DspStageRuntime::heartbeat(uint64_t now) noexcept{if(attached_)input_.heartbeat(now);}
bool DspStageRuntime::peersAlive(uint64_t now) noexcept{heartbeat(now);return attached_ && !faulted_;}
DspStepResult DspStageRuntime::step(uint64_t) noexcept{
 if(!attached_ || faulted_)return DspStepResult::Fault;
 StageBusBlockHeader b{};const auto read=input_.read(b,inputPointers_.data());
 if(read==entropy::publish::ReadResult::Empty)return DspStepResult::Idle;
 if(read!=entropy::publish::ReadResult::Block){if(read==entropy::publish::ReadResult::Gap)++diag_.missing;return fail(read==entropy::publish::ReadResult::Invalid);}
 if(validateBlock(inputSession_,b,inputSession_.slot_stride)!=StageBusValidationResult::Ok)return fail(true);
 // Transport gap/epoch changes retire the subscription. A fresh subscription
 // resets native histories; source-generation/music-binding transitions reset
 // locally too. Sticky provider discontinuity flags alone are not reset commands.
 if(haveInput_ && (std::memcmp(lastHeader_.source_generation,b.source_generation,sizeof(b.source_generation)) || lastHeader_.music_session_revision!=b.music_session_revision)){
  if(!resetHistory())return fail(true);
 }
 received_=tick();diag_.queueQpc=received_>=b.qpc_timestamp?received_-b.qpc_timestamp:0;diag_.maxQueueQpc=std::max(diag_.maxQueueQpc,diag_.queueQpc);recordLatency(2,diag_.queueQpc);
 const auto start=tick();
 // Original executor is an epoch-1 V1 validator plus exact channel routing.
 // Validate the real V2 header above, execute its unchanged routing with a local
 // epoch view, then restore the complete original producer header.
 auto executionHeader=b;executionHeader.stream_epoch=1;
 auto done=executor_.process(inputSession_,outputSession_,executionHeader,inputConst_.data(),pendingHeader_,outputPointers_.data());
 pendingHeader_=b;
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
 ++diag_.blocksIn;diag_.framesIn+=1024;ready_=true;
 // Preserve source metadata verbatim. Clearing the Stage2 session on a local
 // loss makes every output subscriber explicitly reconnect before new history.
 if(publisher_){publisher_->publish(pendingHeader_,outputConst_.data());diag_.publishDrops=publisher_->dropped();}
 lastHeader_=pendingHeader_;diag_.lastHeader=lastHeader_;++diag_.blocksOut;diag_.framesOut+=1024;
 measureSignal(diag_.signal,inputConst_.data(),outputConst_.data(),lastHeader_.validity_mask,tick(),center_.loaded());
 if(recorder_){recorder_->tryCapture(lastHeader_,outputConst_.data());diag_.captureDrops=recorder_->dropped();}
 diag_.traversalQpc=tick()-received_;diag_.maxTraversalQpc=std::max(diag_.maxTraversalQpc,diag_.traversalQpc);recordLatency(1,diag_.traversalQpc);
 ready_=false;haveInput_=true;return DspStepResult::Published;
}
}
