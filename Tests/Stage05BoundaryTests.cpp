#include "Engine/Control/ControlPlane.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Egress/NativeEgress.h"
#include <thread>
#include <iostream>
#include <cmath>
#include <stdexcept>
int main(int argc,char**){try{
 entropy::engine::AudioGraph graph;if(!graph.prepare(256)||!entropy::egress_host::FirstDacGraph::configureProbeGraph(graph,0))throw std::runtime_error("prepare");
 entropy::control::ControlPlane controls(graph);entropy::engine::PlanarBuffer input,output;input.prepare(13,256);output.prepare(16,256);input.clear(256);
 for(int f=0;f<256;++f)input.channel(0)[f]=input.channel(1)[f]=.001*std::sin(6.2831853071795864769*1000*f/192000.0);
 auto process=[&]{if(graph.process(input.readPointers(),output.writePointers(),256)!=ENTROPY_OK)throw std::runtime_error("process");};
 for(int n=0;n<50;++n){controls.atBlockBoundary();process();controls.afterBlock();}
 controls.atBlockBoundary(); // This block's immutable interlock view is now frozen.
 std::exception_ptr failure;std::thread control([&]{try{controls.execute(entropy::ipc::parse(R"({"action":"parameter","id":"emergencyMute1","value":1})"));}catch(...){failure=std::current_exception();}});control.join();if(failure)std::rethrow_exception(failure);
 if(argc>1)entropy::engine::GraphControlAccess::crossover(graph)->setHostBlockParameterView(nullptr); // Baseline regression oracle, deliberately fails.
 process();controls.afterBlock();double peak=0;for(int f=0;f<256;++f)peak=std::max(peak,std::abs(output.readChannel(0)[f]));if(peak<1e-8)throw std::runtime_error("Command submitted AFTER boundary silenced the already frozen block");
 controls.atBlockBoundary();process();controls.afterBlock();for(int f=0;f<256;++f)if(output.readChannel(0)[f]!=0)throw std::runtime_error("Emergency Mute not effective at next block");
 entropy::engine::AudioGraph independent;if(!independent.prepare(256)||!entropy::egress_host::FirstDacGraph::configureProbeGraph(independent,0))throw std::runtime_error("independent prepare");
 for(int n=0;n<50;++n)independent.process(input.readPointers(),output.writePointers(),256);
 controls.atBlockBoundary();independent.process(input.readPointers(),output.writePointers(),256);controls.afterBlock();double isolatedPeak=0;for(int f=0;f<256;++f)isolatedPeak=std::max(isolatedPeak,std::abs(output.readChannel(0)[f]));if(isolatedPeak<1e-8)throw std::runtime_error("Frozen host view leaked into another processor on same owner thread");
 std::cout<<"Block-boundary interlock PASS; active output peak="<<peak<<" then exact silence\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
