#include "Engine/Control/ControlPlane.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include "UI/Legacy/R10/DelayEditModel.h"
using entropy::ipc::Value;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static void nonnegativeDelayContract(){
 entropy::engine::AudioGraph graph;check(graph.prepare(256),"delay fixture prepare");entropy::control::ControlPlane controls(graph);
 const auto metadata=controls.metadata();unsigned delays=0;for(const auto& p:metadata.at("parameters").asArray()){
  const auto& id=p.at("id").asString();if(!id.starts_with("delayMs")&&!id.starts_with("musicDelayMs")&&!id.starts_with("cinemaDelayMs"))continue;
  ++delays;check(p.at("min").asNumber()==0&&p.at("max").asNumber()==800,"Delay metadata still exposes negative milliseconds");
  bool rejected=false;try{controls.execute(Value::Object{{"action","parameter"},{"id",id},{"value",-.001}});}catch(const std::invalid_argument&){rejected=true;}
  check(rejected,"IPC accepted negative channel delay");check(!graph.setOfflineParameter(id.c_str(),-.001),"Offline Core accepted negative channel delay");
 }check(delays==48,"Missing legacy/MUSIC/CINEMA delay controls");
 for(const char* xml:{"<PARAMETERS><PARAM id=\"musicDelayMs1\" value=\"-0.001\"/></PARAMETERS>","<PARAMETERS><CINEMA_CROSSOVER_SCENE cinemaDelayMs16=\"-1\"/></PARAMETERS>","<PARAMETERS><MANUAL_SAVE_ALL musicDelayMs1=\"-1\"/></PARAMETERS>","<PARAMETERS><MANUAL_SNAPSHOTS><CHANNEL_1 delayMs1=\"-1\"/></MANUAL_SNAPSHOTS></PARAMETERS>"}){
  auto tree=entropy::native::XmlNode::parse(xml);entropy::native::MemoryBlock data;entropy::native::copyXmlToBinary(*tree,data);std::vector<unsigned char> bytes(static_cast<const unsigned char*>(data.getData()),static_cast<const unsigned char*>(data.getData())+data.getSize());
  bool rejected=false;try{controls.crossover().importBytes(bytes);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Profile restored a negative delay from scalar/scene/manual snapshot");
 }
 entropy::host::ui::DelayEditModel edit;unsigned writes=0;
 auto sink=[&](const std::string&,int kind,double value){check(value>=0&&value<=800,"UI emitted negative delay");if(kind==1)++writes;return true;};
 edit.begin("musicDelayMs1",.001,sink);edit.move(-500,.001,sink);check(edit.value()==0,"Fine Adjust crossed zero");edit.set(-10,sink);check(edit.value()==0,"Coarse delay crossed zero");edit.set(900,sink);check(edit.value()==800,"Delay exceeded maximum");edit.finish(sink);check(writes>=3,"Delay fixture never sent real edits");
 controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"musicDelayMs1","value":0})"));controls.atBlockBoundary();controls.afterBlock();
 controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"musicDelayMs1","value":800})"));controls.atBlockBoundary();controls.afterBlock();
}
static void inactiveCinemaLedger(){
 entropy::engine::AudioGraph graph;check(graph.prepare(256),"descriptor fixture prepare");
 entropy::control::ControlPlane controls(graph);
 controls.execute(entropy::ipc::parse(R"({"action":"parameters","values":[{"id":"cinemaSource","value":1},{"id":"cinemaRole1","value":1},{"id":"cinemaRole16","value":0}]})"));
 controls.execute(entropy::ipc::parse(R"({"action":"matrix-clear"})"));
 controls.execute(entropy::ipc::parse(R"({"action":"matrix-route","source":0,"destination":0,"enabled":true})"));
 controls.execute(entropy::ipc::parse(R"({"action":"matrix-route","source":15,"destination":0,"enabled":true})"));
 entropy::engine::PlanarBuffer input,output;input.prepare(13,256);output.prepare(16,256);
 for(unsigned channel=0;channel<13;++channel)std::fill_n(input.channel(channel),256,.01);
 for(unsigned block=0;block<128;++block){controls.atBlockBoundary();check(graph.process(input.readPointers(),output.writePointers(),256)==ENTROPY_OK,"descriptor fixture process");controls.afterBlock();}
 auto state=controls.snapshot();
 check(!state.at("outputs").asArray()[15].at("active").asBool(),"inactive fixture lane enabled");
 check(!(state.at("matrix").at("activeSourceMask").asUnsigned()&0x8000),"inactive Crossover source included in Matrix ledger");
 check(state.at("matrix").at("routes").asArray()[15].asUnsigned()==1,"dormant route erased");
 check(state.at("headroom").at("groups").asArray()[0].at("declaredPositiveDb").asNumber()==0,"inactive CINEMA route inflated Headroom by 6 dB");
 // Model the control-owned APVTS changing while its next configuration is
 // still being prepared. Descriptors must follow the frozen published view.
 check(graph.setOfflineParameter("cinemaRole1",0),"raw topology fixture mutation");
 controls.atBlockBoundary();controls.afterBlock();
 check(controls.snapshot().at("matrix").at("activeSourceMask").asUnsigned()&1,"unpublished APVTS topology truncated a frozen active lane");
}
int main(){try{
 nonnegativeDelayContract();
 inactiveCinemaLedger();
 entropy::engine::AudioGraph graph;check(graph.prepare(256),"prepare");
 entropy::control::ControlPlane controls(graph);
 auto meta=controls.metadata();check(meta.at("parameters").asArray().size()>400,"all parameters exposed");
 auto before=controls.snapshot();
 const auto& initialHeadroom=before.at("headroom");
 check(initialHeadroom.contains("hostRate")&&initialHeadroom.at("hostRate").asUnsigned()==192000,"Headroom native host-rate telemetry missing");
 check(initialHeadroom.contains("appliedRevision")&&initialHeadroom.at("appliedRevision").asUnsigned()==0,"Headroom applied revision fabricated before processing");
 try{controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"cinemaSource","value":2})"));throw std::runtime_error("invalid accepted");}catch(const std::invalid_argument&){}
 check(controls.snapshot().at("program").asUnsigned()==0,"failed mutation changed program");
 controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"cinemaSource","value":1})"));
 check(controls.snapshot().at("crossoverApplied").asU64()==0,"applied before block");
 entropy::engine::PlanarBuffer input,output;input.prepare(13,256);output.prepare(16,256);
 for(unsigned channel=0;channel<13;++channel)std::fill_n(input.channel(channel),256,.01);
 controls.atBlockBoundary();check(graph.process(input.readPointers(),output.writePointers(),256)==ENTROPY_OK,"process");controls.afterBlock();
 check(controls.snapshot().at("crossoverApplied").asU64()>0,"crossover snapshot not applied");
 const auto cinemaTelemetry=controls.snapshot();double matrixPeak=0;
 for(const auto& peak:cinemaTelemetry.at("matrix").at("peaks").asArray())matrixPeak=std::max(matrixPeak,peak.asNumber());
 check(matrixPeak>0&&cinemaTelemetry.at("headroom").at("measuredPeak").asNumber()==matrixPeak,"CINEMA Headroom input peak stale while analyzer disabled");
 controls.execute(entropy::ipc::parse(R"({"action":"matrix-route","source":0,"destination":1,"enabled":true})"));
 check(controls.snapshot().at("matrix").at("routes").asArray()[0].asUnsigned()==1,"matrix published early");
 controls.atBlockBoundary();check(graph.process(input.readPointers(),output.writePointers(),256)==ENTROPY_OK,"matrix block");controls.afterBlock();
 check(controls.snapshot().at("matrix").at("routes").asArray()[0].asUnsigned()==3,"matrix route missing");
 auto matrixDiagnostic=controls.snapshot().at("matrix");check(matrixDiagnostic.contains("routeCount"),"native Matrix diagnostics missing");check(matrixDiagnostic.at("routeCount").asUnsigned()==17&&matrixDiagnostic.at("summingMask").asUnsigned()==2,"native Matrix route/summing diagnostic mismatch");
 controls.execute(entropy::ipc::parse(R"({"action":"headroom-policy","mode":2,"manualGainDb":-12})"));
 controls.atBlockBoundary();check(graph.process(input.readPointers(),output.writePointers(),256)==ENTROPY_OK,"headroom block");controls.afterBlock();
 check(controls.snapshot().at("headroom").at("manualGainDb").asNumber()==-12,"headroom policy missing");
 controls.execute(entropy::ipc::parse(R"({"action":"apply-side-delay","cinema":true,"right":false,"value":2})"));
 controls.execute(entropy::ipc::parse(R"({"action":"left-to-right","cinema":true})"));
 controls.execute(entropy::ipc::parse(R"({"action":"headroom-groups","value":2})"));
 controls.atBlockBoundary();controls.afterBlock();controls.execute(entropy::ipc::parse(R"({"action":"headroom-reserve","group":0,"value":3})"));controls.atBlockBoundary();
 controls.afterBlock();auto groupState=controls.snapshot().at("headroom").at("groups").asArray();check(groupState.size()==1&&groupState[0].at("reserve").asNumber()==3,"V07 global16 ledger normalization missing");
 controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"cinemaSource","value":0})"));
 controls.execute(entropy::ipc::parse(R"({"action":"gesture","id":"cinemaSource","begin":false})"));
 check(!controls.settled(),"gesture bypassed pending audible save barrier");
 controls.atBlockBoundary();check(graph.process(input.readPointers(),output.writePointers(),256)==ENTROPY_OK,"gesture barrier process");controls.afterBlock();check(controls.settled(),"save barrier did not settle");
 auto musicLedger=controls.snapshot().at("headroom").at("groups").asArray()[0];
 check(std::abs(musicLedger.at("declaredPositiveDb").asNumber()-20*std::log10(4.0))<1e-12,"MUSIC plus Matrix sum reserve not derived");
 check(musicLedger.contains("guaranteedInputDb")&&musicLedger.contains("bounded"),"Headroom original ledger fields missing");
 const auto ledgerRevision=controls.snapshot().at("headroom").at("revision").asUnsigned();
 controls.execute(entropy::ipc::parse(R"({"action":"headroom-reserve","group":0,"value":4})"));controls.atBlockBoundary();controls.afterBlock();
 check(controls.snapshot().at("headroom").at("revision").asUnsigned()==ledgerRevision+1,"single reserve command restarted native ledger twice");
 controls.execute(entropy::ipc::parse(R"({"action":"headroom-reserve","group":0,"value":4})"));controls.atBlockBoundary();controls.afterBlock();
 check(controls.snapshot().at("headroom").at("revision").asUnsigned()==ledgerRevision+1,"unchanged reserve command reset native AUTO state");
 for(int i=0;i<64;++i)controls.execute(entropy::ipc::parse(R"({"action":"headroom-attack","value":500})"));
 try{controls.execute(entropy::ipc::parse(R"({"action":"headroom-attack","value":500})"));throw std::runtime_error("full queue accepted");}catch(const std::invalid_argument&){}
 for(int i=0;i<64;++i){controls.atBlockBoundary();controls.afterBlock();}check(controls.settled(),"queue did not drain");
 auto saved=controls.crossover().exportBytes();controls.crossover().importBytes(saved);check(controls.snapshot().at("blockSize").asUnsigned()==256,"profile import lost prepare maximum frames");
 std::cout<<"Stage05 controls PASS: validation, deferred block snapshots, native Crossover status, Matrix, Headroom\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
