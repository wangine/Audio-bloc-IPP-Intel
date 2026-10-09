#include "ControlPlane.h"
#include "GraphControlAccess.h"
#include <stdexcept>
namespace entropy::control {
using J=ipc::Value;using A=engine::GraphControlAccess;
static void valid(const ParameterSnapshot& s){
 entropy::output_matrix::OutputMatrixEngine matrix;entropy::headroom::HeadroomEngine h;
 if(!matrix.prepare(192000,256)||!h.prepare(192000,256)||matrix.setState(s.matrix)!=ENTROPY_OK||h.setState(s.headroom)!=ENTROPY_OK||h.setTiming(s.timing)!=ENTROPY_OK||h.setManualGainDb(s.manualGainDb)!=ENTROPY_OK||h.setMode(s.mode)!=ENTROPY_OK)throw std::invalid_argument("Invalid native Matrix/Headroom snapshot");
}
ControlPlane::ControlPlane(engine::AudioGraph& graph,uint64_t session):graph_(graph),crossover_(graph),session_(session){
 desired_.matrix=A::matrix(graph).state();desired_.headroom=A::headroom(graph).state();desired_.timing=A::headroom(graph).timing();desired_.mode=A::headroom(graph).modeState().requested_mode;desired_.manualGainDb=-6;
}
J ControlPlane::metadata() const {return crossover_.metadata();}
uint64_t ControlPlane::execute(const J& request){
 if(queue_.full())throw std::invalid_argument("Control queue full");
 auto action=request.at("action").asString();auto candidate=desired_;candidate.matrixChanged=candidate.headroomChanged=candidate.timingChanged=candidate.policyChanged=candidate.clearFault=false;
 if(sequence_==UINT64_MAX||(request.contains("sequence")&&request.at("sequence").asU64()==UINT64_MAX))throw std::invalid_argument("Sequence exhausted");
 auto nextSequence=request.contains("sequence")?std::max(sequence_+1,request.at("sequence").asU64()):sequence_+1;
 if(action=="matrix-route"){
  auto source=request.at("source").asUnsigned(15),destination=request.at("destination").asUnsigned(15);auto bit=1u<<destination;
  if(request.at("enabled").asBool())candidate.matrix.source_destination_mask[source]|=bit;else candidate.matrix.source_destination_mask[source]&=~bit;
  candidate.matrixChanged=true;
 }else if(action=="matrix-clear"){for(auto& r:candidate.matrix.source_destination_mask)r=0;candidate.matrixChanged=true;}
 else if(action=="headroom-policy"){candidate.mode=request.at("mode").asUnsigned(2);candidate.manualGainDb=request.at("manualGainDb").asNumber();candidate.policyChanged=true;}
 else if(action=="headroom-transition"){candidate.headroom.transition_ms=request.at("value").asNumber();candidate.headroomChanged=true;}
 else if(action=="headroom-attack"||action=="headroom-release"){(action=="headroom-attack"?candidate.timing.attack_ms:candidate.timing.release_ms)=request.at("value").asNumber();candidate.timingChanged=true;}
 else if(action=="headroom-groups"){candidate.headroom.group_count=request.at("value").asUnsigned(16);for(auto& g:candidate.headroom.channel_group)if(g>=candidate.headroom.group_count)g=0;for(auto& g:candidate.headroom.groups){g.struct_size=sizeof(g);g.bounded=1;}candidate.headroomChanged=true;}
 else if(action=="headroom-channel-group"){candidate.headroom.channel_group[request.at("channel").asUnsigned(15)]=request.at("group").asUnsigned(15);candidate.headroomChanged=true;}
 else if(action=="headroom-reserve"){auto group=request.at("group").asUnsigned(15);if(group>=candidate.headroom.group_count)throw std::invalid_argument("Headroom group unavailable");candidate.headroom.groups[group].preventive_reserve_db=request.at("value").asNumber();candidate.headroomChanged=true;}
 else if(action=="headroom-clear-fault")candidate.clearFault=true;
 else {crossover_.execute(request,session_,nextSequence);sequence_=nextSequence;accepted_.store(sequence_,std::memory_order_release);return sequence_;}
 valid(candidate);candidate.revision=nextSequence;if(candidate.matrixChanged)candidate.matrix.generation=unsigned(candidate.revision);if(candidate.headroomChanged)candidate.headroom.revision=unsigned(candidate.revision);if(candidate.timingChanged)candidate.timing.revision=unsigned(candidate.revision);
 if(!queue_.push(candidate))throw std::invalid_argument("Control queue full");
 lastQueued_=nextSequence;desired_=candidate;sequence_=nextSequence;accepted_.store(sequence_,std::memory_order_release);return sequence_;
}
void ControlPlane::replace(const ParameterSnapshot& state){valid(state);auto s=state;s.revision=sequence_+1;s.matrix.generation=s.headroom.revision=s.timing.revision=unsigned(s.revision);s.matrixChanged=s.headroomChanged=s.timingChanged=s.policyChanged=true;if(!queue_.push(s))throw std::invalid_argument("Control queue full");desired_=s;lastQueued_=s.revision;++sequence_;accepted_.store(sequence_,std::memory_order_release);}
void ControlPlane::atBlockBoundary() noexcept {
 crossover_.captureBeforeBlock();
 // Bounded work: at most one accepted state per block. No heap, lock or I/O.
 ParameterSnapshot s;if(!queue_.take(s))return;unsigned errors=0;
 if(s.matrixChanged)errors|=A::matrix(graph_).setState(s.matrix)!=ENTROPY_OK;
 if(s.headroomChanged)errors|=A::headroom(graph_).setState(s.headroom)!=ENTROPY_OK;
 if(s.timingChanged)errors|=A::headroom(graph_).setTiming(s.timing)!=ENTROPY_OK;
 if(s.policyChanged){errors|=A::headroom(graph_).setManualGainDb(s.manualGainDb)!=ENTROPY_OK;errors|=A::headroom(graph_).setMode(s.mode)!=ENTROPY_OK;}
 if(s.clearFault)errors|=A::headroom(graph_).clearFault()!=ENTROPY_OK;
 applyFault_.store(errors,std::memory_order_release);if(!errors){if(s.matrixChanged)matrixApplied_.store(s.revision,std::memory_order_release);if(s.headroomChanged||s.timingChanged||s.policyChanged||s.clearFault)headroomApplied_.store(s.revision,std::memory_order_release);applied_.store(s.revision,std::memory_order_release);}
}
void ControlPlane::drainAtQuiescentBoundary(){
 // A joined graph is between blocks. Apply accepted host snapshots without
 // processing invented samples or acknowledging an inaudible Crossover change.
 while(!queue_.empty()){atBlockBoundary();afterBlock();}
 if(applyFault_.load(std::memory_order_acquire))throw std::runtime_error("Native state adoption failed");
}
ParameterSnapshot ControlPlane::appliedState() const {ParameterSnapshot s;s.matrix=A::matrix(graph_).state();s.headroom=A::headroom(graph_).state();s.timing=A::headroom(graph_).timing();s.mode=A::headroom(graph_).modeState().requested_mode;s.manualGainDb=A::headroom(graph_).controlState().manual_gain_db;return s;}
J ControlPlane::snapshot() const {
 auto result=crossover_.snapshot();auto m=A::matrix(graph_).state();auto mt=A::matrix(graph_).telemetry();auto h=A::headroom(graph_).state();auto t=A::headroom(graph_).telemetry();auto timing=A::headroom(graph_).timing();auto mode=A::headroom(graph_).modeState();auto policy=A::headroom(graph_).controlState();
 J::Array routes,groups,channelGroups,peaks,headPeaks;for(auto p:t.channel_input_peak)headPeaks.emplace_back(p);for(auto r:m.source_destination_mask)routes.emplace_back(r);for(auto g:h.channel_group)channelGroups.emplace_back(g);for(auto p:A::matrix(graph_).telemetry().destination_peak)peaks.emplace_back(p);
 for(unsigned i=0;i<h.group_count;++i){const auto& g=t.groups[i];groups.emplace_back(J::Object{{"gain",g.current_gain},{"target",g.target_gain},{"reductionDb",g.gain_reduction_db},{"required",g.ledger_required_headroom_db},{"margin",g.reserve_margin_db},{"progress",g.transition_progress},{"status",g.status},{"reserve",h.groups[i].preventive_reserve_db}});}
 result["accepted"]=J::u64(accepted_.load());result["applied"]=J::u64(applied_.load());result["applyFault"]=applyFault_.load();result["blocks"]=J::u64(graph_.telemetry().completedBlocks);
 result["matrix"]=J::Object{{"appliedSequence",J::u64(matrixApplied_.load())},{"activeSourceMask",m.active_source_mask},{"enabledDestinationMask",m.enabled_destination_mask},{"summingMask",mt.summing_destination_mask},{"orphanMask",mt.orphaned_source_mask},{"routeCount",mt.route_count},{"generation",m.generation},{"appliedGeneration",A::matrix(graph_).telemetry().applied_generation},{"routes",std::move(routes)},{"peaks",std::move(peaks)}};
 result["headroom"]=J::Object{{"appliedSequence",J::u64(headroomApplied_.load())},{"revision",h.revision},{"measuredPeak",t.measured_input_peak},{"truePeak",t.measured_input_true_peak},{"truePeakStatus",t.true_peak_status},{"analysisReductionDb",t.auto_analysis_reduction_db},{"analyzerState",t.analyzer_state},{"peaks",std::move(headPeaks)},{"modeTransitionActive",mode.transition_active},{"modeTransitionProgress",mode.transition_progress},{"faults",t.fault_flags},{"mode",mode.requested_mode},{"effectiveMode",mode.effective_mode},{"manualGainDb",policy.manual_gain_db},{"transition",h.transition_ms},{"attack",timing.attack_ms},{"release",timing.release_ms},{"channelGroups",std::move(channelGroups)},{"groups",std::move(groups)}};
 return result;
}
}
