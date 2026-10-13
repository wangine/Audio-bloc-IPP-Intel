#include "DspLocalDemand.h"
#include <cstring>
namespace entropy::stage {
bool DspLocalDemand::attach(const StageBusSessionHeader& s,const StageBusProgramSnapshot& p,uint64_t revision,std::string& error){
 close();if(s.layout_id!=LRC_CINEMA11_V1 || p.hardware_armed){error="Invalid local monitor contract or armed programme";return false;}
 if(!bus_.open(s,error))return false;session_=s;program_=p;revision_=revision;sequence_=1;due_=started_=blocks_=dueRemainder_=scheduleRebases_=0;last_={};
 for(unsigned c=0;c<11;c++)pointers_[c]=planes_[c].data();attached_=true;return true;
}
void DspLocalDemand::close() noexcept {bus_.close();attached_=pending_=quiescing_=false;}
DspStepResult DspLocalDemand::step(uint64_t now) noexcept {
 if(!attached_)return DspStepResult::Fault;bus_.heartbeat(now);
 StageBusSourceControl control{};auto io=bus_.tryReceiveSourceControl(control);if(io==StageBusIoResult::Fault)return DspStepResult::Fault;
 if(io==StageBusIoResult::Ok){if(control.source_revision<revision_)return DspStepResult::Fault;revision_=control.source_revision;program_.manual_mute_mask=control.manual_mute_mask;program_.effective_mute_mask=control.manual_mute_mask;}
 if(pending_){StageBusBlockHeader b{};io=bus_.tryConsume(b,pointers_.data());if(io==StageBusIoResult::Fault)return DspStepResult::Fault;
  if(io==StageBusIoResult::Ok){if(b.block_sequence!=request_.block_sequence || b.first_frame!=request_.first_frame || b.stream_epoch!=1 || b.control_revision!=request_.control_revision)return DspStepResult::Fault;last_=b;pending_=false;++blocks_;++sequence_;return DspStepResult::Published;}
  if(now<started_ || now-started_>session_.qpc_frequency)return DspStepResult::Fault;return DspStepResult::Idle;
 }
 if(quiescing_ || now<due_)return DspStepResult::Idle;
 request_={};std::memcpy(request_.session_id,session_.session_id,16);request_.stream_epoch=1;request_.block_sequence=sequence_;request_.first_frame=(sequence_-1)*1024;request_.frames=1024;request_.control_revision=revision_;request_.program=program_;
 io=bus_.trySendRequest(request_);if(io==StageBusIoResult::Fault)return DspStepResult::Fault;if(io==StageBusIoResult::Full)return DspStepResult::Idle;
 pending_=true;started_=now;
 // Preserve nominal cadence through small wake jitter; never replay a delayed backlog.
 const auto interval=session_.qpc_frequency*1024/192000;
 if(!due_)due_=now;else if(now>due_ && now-due_>interval){due_=now;dueRemainder_=0;++scheduleRebases_;}
 due_+=interval;dueRemainder_+=(session_.qpc_frequency*1024)%192000;
 due_+=dueRemainder_/192000;dueRemainder_%=192000;return DspStepResult::Progress;
}
}
