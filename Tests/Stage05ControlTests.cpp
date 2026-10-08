#include "Engine/Control/ControlPlane.h"
#include <iostream>
#include <stdexcept>
using entropy::ipc::Value;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
 entropy::engine::AudioGraph graph;check(graph.prepare(256),"prepare");
 entropy::control::ControlPlane controls(graph);
 auto meta=controls.metadata();check(meta.at("parameters").asArray().size()>400,"all parameters exposed");
 auto before=controls.snapshot();
 try{controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"cinemaSource","value":2})"));throw std::runtime_error("invalid accepted");}catch(const std::invalid_argument&){}
 check(controls.snapshot().at("program").asUnsigned()==0,"failed mutation changed program");
 controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"cinemaSource","value":1})"));
 check(controls.snapshot().at("crossoverApplied").asU64()==0,"applied before block");
 entropy::engine::PlanarBuffer input,output;input.prepare(13,256);output.prepare(16,256);
 controls.atBlockBoundary();check(graph.process(input.readPointers(),output.writePointers(),256)==ENTROPY_OK,"process");controls.afterBlock();
 check(controls.snapshot().at("crossoverApplied").asU64()>0,"crossover snapshot not applied");
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
 controls.atBlockBoundary();controls.afterBlock();controls.execute(entropy::ipc::parse(R"({"action":"headroom-reserve","group":1,"value":3})"));controls.atBlockBoundary();
 controls.afterBlock();auto groupState=controls.snapshot().at("headroom").at("groups").asArray();check(groupState[0].at("reserve").asNumber()==0&&groupState[1].at("reserve").asNumber()==3,"reserve changed wrong group");
 controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"cinemaSource","value":0})"));
 controls.execute(entropy::ipc::parse(R"({"action":"gesture","id":"cinemaSource","begin":false})"));
 check(!controls.settled(),"gesture bypassed pending audible save barrier");
 controls.atBlockBoundary();check(graph.process(input.readPointers(),output.writePointers(),256)==ENTROPY_OK,"gesture barrier process");controls.afterBlock();check(controls.settled(),"save barrier did not settle");
 for(int i=0;i<64;++i)controls.execute(entropy::ipc::parse(R"({"action":"headroom-attack","value":500})"));
 try{controls.execute(entropy::ipc::parse(R"({"action":"headroom-attack","value":500})"));throw std::runtime_error("full queue accepted");}catch(const std::invalid_argument&){}
 for(int i=0;i<64;++i){controls.atBlockBoundary();controls.afterBlock();}check(controls.settled(),"queue did not drain");
 auto saved=controls.crossover().exportBytes();controls.crossover().importBytes(saved);check(controls.snapshot().at("blockSize").asUnsigned()==256,"profile import lost prepare maximum frames");
 std::cout<<"Stage05 controls PASS: validation, deferred block snapshots, native Crossover status, Matrix, Headroom\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
