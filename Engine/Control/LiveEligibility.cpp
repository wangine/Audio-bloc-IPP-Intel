#include "LiveEligibility.h"
#include "GraphControlAccess.h"
#include <windows.h>
#include <avrt.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
namespace entropy::control {
using A=engine::GraphControlAccess;
namespace {
struct PreflightMmcss {
 HANDLE task=nullptr;
 PreflightMmcss(){DWORD index=0;task=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);if(!task)throw std::runtime_error("Profile preflight MMCSS registration failed; no device opened");if(!AvSetMmThreadPriority(task,AVRT_PRIORITY_CRITICAL)){AvRevertMmThreadCharacteristics(task);task=nullptr;throw std::runtime_error("Profile preflight MMCSS priority failed; no device opened");}}
 ~PreflightMmcss(){if(task)AvRevertMmThreadCharacteristics(task);}
};
std::string requestedConfiguration(ControlPlane& live){
 // Serialize only requested settings, never applied sequences, live peaks,
 // gain ramps or histories. Compare the full representation, without hashes
 // or ABI padding; the same fields feed the independent offline copy.
 const auto s=live.desired();ipc::Value::Array routes,channelGroups,groups;
 for(auto r:s.matrix.source_destination_mask)routes.emplace_back(r);
 for(auto g:s.headroom.channel_group)channelGroups.emplace_back(g);
 for(const auto& g:s.headroom.groups)groups.emplace_back(ipc::Value::Object{
  {"bounded",g.bounded},{"declaredPositiveDb",g.declared_positive_contribution_db},
  {"guaranteedInputDb",g.guaranteed_input_headroom_db},{"reserve",g.preventive_reserve_db}});
 return ipc::dump(ipc::Value::Object{
  {"parameters",live.crossover().snapshot().at("parameters")},
  {"matrix",ipc::Value::Object{{"activeSourceMask",s.matrix.active_source_mask},{"enabledDestinationMask",s.matrix.enabled_destination_mask},{"routes",std::move(routes)}}},
  {"headroom",ipc::Value::Object{{"groupCount",s.headroom.group_count},{"activeChannelMask",s.headroom.active_channel_mask},
   {"channelGroups",std::move(channelGroups)},{"groups",std::move(groups)},{"transitionMs",s.headroom.transition_ms},
   {"attackMs",s.timing.attack_ms},{"releaseMs",s.timing.release_ms},{"mode",s.mode},{"manualGainDb",s.manualGainDb},{"renderLaneMask",s.renderLaneMask}}}});
}
}
void LiveProfilePreflightCache::invalidate() noexcept {
 report_=ipc::Value{};configuration_.clear();crossoverState_.clear();
}
ipc::Value LiveProfilePreflightCache::run(ControlPlane& live){
 const auto configuration=requestedConfiguration(live);
 // Saved MUSIC/CINEMA scene settings are part of the exact imported profile.
 // Their original serialization contains settings, not live telemetry.
 auto crossoverState=live.crossover().exportBytes();
 if(!report_.isNull()&&configuration_==configuration&&crossoverState_==crossoverState){
  auto reused=report_;reused["cacheHit"]=true;return reused;
 }
 // Clear an older PASS before attempting a different profile, including when
 // the strict validator throws. No failure can create a reusable report.
 invalidate();++validationRun_;
 auto validated=liveProfilePreflight(live);
 validated["cacheHit"]=false;validated["validationRun"]=ipc::Value::u64(validationRun_);
 configuration_=configuration;crossoverState_=std::move(crossoverState);report_=validated;
 return validated;
}
bool liveHeadroomReady(engine::AudioGraph& graph,bool requireSettled) noexcept {
 const auto& h=A::headroom(graph);const auto ledger=h.state();const auto mode=h.modeState();const auto t=h.telemetry();
 // Headroom owns its mathematics and validation. Its user-selected AUTO,
 // MANUAL SAFE and BIT PERFECT modes are not restricted by the host ledger.
 // Keep original faults and finite state checks; impose no attenuation policy.
 if(t.fault_flags||!ledger.group_count||ledger.group_count>16)return false;
 // Native ACTIVE aliases AUTO, so telemetry cannot distinguish an AUTO
 // entry ramp from normal ATTACK/RELEASE. Both are valid native AUTO behavior
 // at ARM; retain original fault/finite checks and non-AUTO settling checks.
 if(requireSettled&&(mode.requested_mode!=mode.effective_mode||
    (mode.transition_active&&mode.requested_mode!=ENTROPY_HEADROOM_MODE_AUTO)))return false;
 for(unsigned i=0;i<ledger.group_count;++i)if(!std::isfinite(t.groups[i].current_gain)||t.groups[i].current_gain<0||t.groups[i].current_gain>1.0)return false;
 return true;
}
bool liveFirAllowed(const ipc::Value& v){
 std::map<std::string,double> p;for(const auto& j:v.at("parameters").asArray())p[j.at("id").asString()]=j.at("value").asNumber();unsigned large=0;
 for(unsigned i=1;i<=16;++i){auto s=std::to_string(i);if(p["processMode"+s]==1&&p["firLength"+s]==2)++large;}
 return large<16;
}
bool liveCrossoverChangeAllowed(const ipc::Value& current,const ipc::Value& request){
 const auto action=request.at("action").asString();auto candidate=current;
 // Switching saved scenes needs a new offline profile preflight. No scene is
 // substituted or altered; DISARM -> select -> ARM remains the explicit path.
 if(action=="program")return false;
 if(action=="parameter"&&request.at("id").asString()=="cinemaSource")return false;
 auto update=[&](const ipc::Value& entry){if(entry.at("id").asString()=="cinemaSource")return false;for(auto& p:candidate["parameters"].asArray())if(p.at("id").asString()==entry.at("id").asString())p["value"]=entry.at("value");return true;};
 if(action=="parameter"){if(!update(request))return false;}
 else if(action=="parameters"){for(auto& e:request.at("values").asArray())if(!update(e))return false;}
 else if(action=="left-to-right"){for(auto& p:current.at("parameters").asArray())if(p.at("id").asString().rfind("firLength",0)==0&&p.at("value").asNumber()==2)return false;}
 return liveFirAllowed(candidate);
}
bool liveGateMatches(const std::filesystem::path& path,const std::string& hash) {
 if(hash.size()!=64)return false;std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f||f.tellg()>4096)return false;
 auto n=f.tellg();std::string text(size_t(n),'\0');f.seekg(0);if(!f.read(text.data(),n))return false;
 try{auto j=ipc::parse(text);return j.at("format").asString()=="ENTROPY_NATIVE_LIVE_OFFLINE_PASS_V1"&&j.at("audioCoreSHA256").asString()==hash&&j.at("ctestPass").asBool()&&j.at("numericRows").asUnsigned()==96&&j.at("clockRows").asUnsigned()==8&&j.at("benchmarkRows").asUnsigned()==80&&j.at("deadlineMisses").asUnsigned()==0&&j.at("realtimeAuditPass").asBool();}catch(...){return false;}
}
ipc::Value liveProfilePreflight(ControlPlane& live){
 const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(20);
 auto timeBudget=[&]{if(std::chrono::steady_clock::now()>until)throw std::runtime_error("Profile preflight time budget exceeded; no device opened");};
 if(!liveFirAllowed(live.crossover().snapshot()))throw std::invalid_argument("FIR8193 x16 is not authorized for live");
 auto cloneOwner=std::make_unique<engine::AudioGraph>();auto& clone=*cloneOwner;if(!clone.prepare(1024))throw std::runtime_error("Live profile preflight prepare failed");
 clone.enableOriginalMusicAnalysis(true);
 ControlPlane candidate(clone);candidate.crossover().importBytes(live.crossover().exportBytes());candidate.replace(live.desired());candidate.atBlockBoundary();candidate.afterBlock();
 engine::PlanarBuffer input,output;input.prepare(13,1024);output.prepare(16,1024);input.clear(1024);
 for(unsigned n=0;n<1024;++n)for(unsigned c=0;c<11;++c)input.channel(c)[n]=.001*std::sin(6.2831853071795864769*(1000+c*73)*n/192000);
 // Minimum five virtual seconds for histories. A fresh copy must also finish
 // the original mode ramp (native transitions may be ten seconds). No user
 // timing is shortened and no live clock/source/history is advanced here.
 unsigned warmupBlocks=0;
 do{timeBudget();candidate.atBlockBoundary();if(clone.process(input.readPointers(),output.writePointers(),1024)!=ENTROPY_OK)throw std::runtime_error("Profile offline DSP failure");candidate.afterBlock();++warmupBlocks;
 }while(warmupBlocks<940||A::headroom(clone).modeState().transition_active||A::headroom(clone).modeState().requested_mode!=A::headroom(clone).modeState().effective_mode);
 if(!liveHeadroomReady(clone))throw std::invalid_argument("Headroom reports an original module fault or an unsettled mode transition; user gain was not changed");
 LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);std::array<double,1000> times{};double peak=0;unsigned misses=0;
 {
 PreflightMmcss scheduling; // Control-only measurement; no live DSP thread changes.
 for(unsigned i=0;i<times.size();++i){timeBudget();LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);candidate.atBlockBoundary();const auto result=clone.process(input.readPointers(),output.writePointers(),1024);candidate.afterBlock();QueryPerformanceCounter(&b);
  if(result!=ENTROPY_OK)throw std::runtime_error("Profile offline DSP failure");times[i]=double(b.QuadPart-a.QuadPart)*1e6/double(frequency.QuadPart);if(times[i]>1024.0/192000*1e6)++misses;
  for(unsigned c=0;c<16;++c)for(unsigned n=0;n<1024;++n){double value=output.channel(c)[n];if(!std::isfinite(value))throw std::runtime_error("Profile offline nonfinite output");peak=std::max(peak,std::abs(value));}
 }
 }
 std::sort(times.begin(),times.end());
 if(misses)throw std::invalid_argument("Profile realtime preflight FAIL: "+std::to_string(misses)+" deadline misses; no device opened");
 if(peak<1e-12)throw std::invalid_argument("Profile preflight has no active output signal: inspect Crossover/Matrix/Mute; no device opened");
 return ipc::Value::Object{{"frames",1024},{"iterations",1000},{"warmupBlocks",warmupBlocks},{"medianUs",(times[499]+times[500])/2},{"p99Us",times[989]},{"maximumUs",times.back()},{"deadlineUs",1024.0/192000*1e6},{"deadlineMisses",misses},{"maximumOutputPeak",peak},{"mmcssRegistered",true},{"mmcssPriorityCritical",true},{"source","independent offline copy; no live histories advanced"}};
}
}
