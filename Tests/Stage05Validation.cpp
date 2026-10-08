#include "Engine/Control/ControlPlane.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Egress/NativeEgress.h"
#include "Tests/Stage02/RealtimeAllocationAudit.h"
#include "Tests/Stage04Scheduling.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace entropy;
namespace audit=host::allocation_audit;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(int argc,char** argv) try {
 std::filesystem::path root=argc>1?argv[1]:"stage05-validation";std::filesystem::create_directories(root);
 check(audit::selfTest(),"audit positive control");audit::reset();std::ofstream csv(root/"control-benchmark.csv");csv.precision(17);csv<<"program,frames,median_wall_us,p99_wall_us,max_wall_us,median_thread_cycles,max_context_switches,wall_deadline_misses,max_abs_error,rms_error,active_peak\n";
 double worst=0,activePeak=0;uint64_t totalSamples=0;
 for(unsigned program=0;program<2;++program)for(unsigned n:{128u,256u,512u,1024u}){
  engine::AudioGraph graph,reference;check(graph.prepare(n)&&reference.prepare(n)&&egress_host::FirstDacGraph::configureProbeGraph(graph,program)&&egress_host::FirstDacGraph::configureProbeGraph(reference,program),"graph setup");
  control::ControlPlane controls(graph);engine::PlanarBuffer input,output,expected;input.prepare(13,n);output.prepare(16,n);expected.prepare(16,n);input.clear(n);
  for(unsigned i=0;i<n;++i)for(unsigned ch=0;ch<11;++ch)input.channel(ch)[i]=.001*std::sin(6.2831853071795864769*1000*i/192000);
  std::vector<double> wall;std::vector<uint64_t> cycles;wall.reserve(1000);cycles.reserve(1000);stage04_test::SchedulingProbe probe(true);check(probe.ready(),"MMCSS scheduling probe");LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);double sum=0,peak=0,error=0;uint64_t switches=0,misses=0,count=0;
  for(unsigned iteration=0;iteration<1100;++iteration){
   if(iteration==100){controls.execute(ipc::parse(R"({"action":"matrix-route","source":0,"destination":1,"enabled":true,"sequence":"2"})"));auto state=engine::GraphControlAccess::matrix(reference).state();state.generation=2;state.source_destination_mask[0]|=2;check(engine::GraphControlAccess::matrix(reference).setState(state)==ENTROPY_OK,"reference matrix");}
   check(reference.process(input.readPointers(),expected.writePointers(),n)==ENTROPY_OK,"reference process");
   auto before=probe.capture();LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
   {audit::CycleScope scope;audit::audioPhase();controls.atBlockBoundary();check(graph.process(input.readPointers(),output.writePointers(),n)==ENTROPY_OK,"controlled process");controls.afterBlock();}
   QueryPerformanceCounter(&end);auto after=probe.capture();if(iteration>=100){double us=double(end.QuadPart-begin.QuadPart)*1e6/frequency.QuadPart;wall.push_back(us);cycles.push_back(after.data.CycleTime-before.data.CycleTime);switches=std::max(switches,uint64_t(after.data.ContextSwitchCount-before.data.ContextSwitchCount));misses+=us>n*1e6/192000;}
   for(unsigned ch=0;ch<16;++ch)for(unsigned i=0;i<n;++i){double difference=output.readChannel(ch)[i]-expected.readChannel(ch)[i];error=std::max(error,std::abs(difference));sum+=difference*difference;peak=std::max(peak,std::abs(output.readChannel(ch)[i]));++count;}
  }
  check(error==0&&peak>.0011,"controlled graph identity/active signal");worst=std::max(worst,error);activePeak=std::max(activePeak,peak);totalSamples+=count;std::sort(wall.begin(),wall.end());std::sort(cycles.begin(),cycles.end());csv<<program<<','<<n<<','<<wall[500]<<','<<wall[989]<<','<<wall.back()<<','<<cycles[500]<<','<<switches<<','<<misses<<','<<error<<','<<std::sqrt(sum/count)<<','<<peak<<'\n';
 }
 // Original Stage3 analyzer integration versus the original module on exactly the Matrix buffer.
 engine::AudioGraph analyzed;check(analyzed.prepare(256)&&egress_host::FirstDacGraph::configureProbeGraph(analyzed,0),"analyzer prepare");analyzed.enableOriginalMusicAnalysis(true);engine::PlanarBuffer in,out;in.prepare(13,256);out.prepare(16,256);in.clear(256);for(unsigned i=0;i<256;++i)in.channel(0)[i]=in.channel(1)[i]=.001;
 {audit::CycleScope scope;audit::audioPhase();check(analyzed.process(in.readPointers(),out.writePointers(),256)==ENTROPY_OK,"original analyzer callback");}
 auto h=engine::GraphControlAccess::headroom(analyzed).telemetry();check(h.measured_input_peak>1e-8,"original Headroom analyzer not fed");
 auto counts=audit::snapshot();audit::report();check(!counts.cppNew&&!counts.cppDelete&&!counts.heapAlloc&&!counts.heapFree&&!counts.wait&&!counts.lock&&!counts.io,"callback realtime audit");
 std::cout<<"Stage05 controlled FP64 graph PASS: samples="<<totalSamples<<" max_abs="<<worst<<" active_peak="<<activePeak<<"; callback allocation/lock/wait/IO audit PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"Stage05 validation FAIL: "<<e.what()<<'\n';return 1;}
