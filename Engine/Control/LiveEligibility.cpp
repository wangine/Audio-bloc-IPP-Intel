#include "LiveEligibility.h"
#include "GraphControlAccess.h"
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
namespace entropy::control {
using A=engine::GraphControlAccess;
namespace {std::atomic<uint64_t> benchmarkCalls{0};}
uint64_t diagnosticBenchmarkCalls() noexcept {return benchmarkCalls.load();}
void recordDiagnosticBenchmarkCall() noexcept {benchmarkCalls.fetch_add(1);}
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
bool liveSceneChangeRequested(const ipc::Value& request){
 const auto action=request.at("action").asString();
 if(action=="program")return true;
 if(action=="parameter")return request.at("id").asString()=="cinemaSource";
 if(action=="parameters")for(const auto& v:request.at("values").asArray())if(v.at("id").asString()=="cinemaSource")return true;
 return false;
}
bool liveCrossoverChangeAllowed(const ipc::Value& current,const ipc::Value& request){
 const auto action=request.at("action").asString();auto candidate=current;
 // A saved scene cannot be checked against the current flat parameter list.
 // Core validates it separately on an independent native graph before commit.
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
}
