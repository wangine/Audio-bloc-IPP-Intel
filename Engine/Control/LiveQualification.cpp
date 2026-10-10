#include "LiveQualification.h"
#include "GraphControlAccess.h"
#include <windows.h>
#include <avrt.h>
#include <algorithm>
#include <cmath>
namespace entropy::control {
using A=engine::GraphControlAccess;
namespace {
struct PreflightMmcss {
 HANDLE task=nullptr;
 PreflightMmcss(){DWORD index=0;task=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);if(!task)throw std::runtime_error("Profile preflight MMCSS registration failed; no device opened");if(!AvSetMmThreadPriority(task,AVRT_PRIORITY_CRITICAL)){AvRevertMmThreadCharacteristics(task);task=nullptr;throw std::runtime_error("Profile preflight MMCSS priority failed; no device opened");}}
 ~PreflightMmcss(){if(task)AvRevertMmThreadCharacteristics(task);}
};
}
ipc::Value liveProfilePreflight(ControlPlane& live,const ipc::Value& change){
 recordDiagnosticBenchmarkCall();
 const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(20);
 auto timeBudget=[&]{if(std::chrono::steady_clock::now()>until)throw std::runtime_error("Profile preflight time budget exceeded; no device opened");};
 if(!change.isNull()&&!liveSceneChangeRequested(change))throw std::invalid_argument("Scene preview requires a MUSIC/CINEMA command");
 if(change.isNull()&&!liveFirAllowed(live.crossover().snapshot()))throw std::invalid_argument("FIR8193 x16 is not authorized for live");
 auto cloneOwner=std::make_unique<engine::AudioGraph>();auto& clone=*cloneOwner;if(!clone.prepare(1024))throw std::runtime_error("Live profile preflight prepare failed");
 clone.enableOriginalMusicAnalysis(true);
 ControlPlane candidate(clone);candidate.crossover().importBytes(live.crossover().exportBytes());candidate.replace(live.desired());candidate.atBlockBoundary();candidate.afterBlock();
 engine::PlanarBuffer input,output;input.prepare(13,1024);output.prepare(16,1024);input.clear(1024);
 for(unsigned n=0;n<1024;++n)for(unsigned c=0;c<11;++c)input.channel(c)[n]=.001*std::sin(6.2831853071795864769*(1000+c*73)*n/192000);
 // Minimum five virtual seconds for histories. A fresh copy must also finish
 // the original mode ramp (native transitions may be ten seconds). No user
 // timing is shortened and no live clock/source/history is advanced here.
 auto warm=[&]{unsigned blocks=0;
  do{timeBudget();candidate.atBlockBoundary();if(clone.process(input.readPointers(),output.writePointers(),1024)!=ENTROPY_OK)throw std::runtime_error("Profile offline DSP failure");candidate.afterBlock();++blocks;
  }while(blocks<940||A::headroom(clone).modeState().transition_active||A::headroom(clone).modeState().requested_mode!=A::headroom(clone).modeState().effective_mode);
  return blocks;
 };
 // Make the source scene truly audio-active before publishing the target.
 // Otherwise the target would replace the initial pending snapshot and the
 // original 9600-sample old/new coefficient crossfade would never be exercised.
 const unsigned sourceWarmupBlocks=change.isNull()?0:warm();
 if(!change.isNull())candidate.execute(change);
 const auto target=candidate.crossover().snapshot();
 if(!liveFirAllowed(target))throw std::invalid_argument("FIR8193 x16 is not authorized for live");
 LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
 std::array<double,20> transitionTimes{};unsigned transitionBlocks=0,transitionMisses=0;double transitionPeak=0;
 if(!change.isNull()){
  PreflightMmcss scheduling;
  for(auto& elapsed:transitionTimes){timeBudget();LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);candidate.atBlockBoundary();const auto result=clone.process(input.readPointers(),output.writePointers(),1024);candidate.afterBlock();QueryPerformanceCounter(&b);
   if(result!=ENTROPY_OK)throw std::runtime_error("Scene transition offline DSP failure");elapsed=double(b.QuadPart-a.QuadPart)*1e6/double(frequency.QuadPart);++transitionBlocks;if(elapsed>1024.0/192000*1e6)++transitionMisses;
   for(unsigned c=0;c<16;++c)for(unsigned n=0;n<1024;++n){const auto value=output.channel(c)[n];if(!std::isfinite(value))throw std::runtime_error("Scene transition offline nonfinite output");transitionPeak=std::max(transitionPeak,std::abs(value));}
  }
  if(transitionMisses)throw std::invalid_argument("Scene transition realtime preflight FAIL: "+std::to_string(transitionMisses)+" deadline misses; live scene unchanged");
 }
 const unsigned warmupBlocks=warm();
 if(!liveHeadroomReady(clone))throw std::invalid_argument("Headroom reports an original module fault or an unsettled mode transition; user gain was not changed");
 std::array<double,1000> times{};double peak=0;unsigned misses=0;
 {
 PreflightMmcss scheduling; // Control-only measurement; no live DSP thread changes.
 for(unsigned i=0;i<times.size();++i){timeBudget();LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);candidate.atBlockBoundary();const auto result=clone.process(input.readPointers(),output.writePointers(),1024);candidate.afterBlock();QueryPerformanceCounter(&b);
  if(result!=ENTROPY_OK)throw std::runtime_error("Profile offline DSP failure");times[i]=double(b.QuadPart-a.QuadPart)*1e6/double(frequency.QuadPart);if(times[i]>1024.0/192000*1e6)++misses;
  for(unsigned c=0;c<16;++c)for(unsigned n=0;n<1024;++n){double value=output.channel(c)[n];if(!std::isfinite(value))throw std::runtime_error("Profile offline nonfinite output");peak=std::max(peak,std::abs(value));}
 }
 }
 std::sort(times.begin(),times.end());
 std::sort(transitionTimes.begin(),transitionTimes.end());
 if(misses)throw std::invalid_argument("Profile realtime preflight FAIL: "+std::to_string(misses)+" deadline misses; no device opened");
 if(peak<1e-12)throw std::invalid_argument("Profile preflight has no active output signal: inspect Crossover/Matrix/Mute; no device opened");
 return ipc::Value::Object{{"program",target.at("program")},{"sourceWarmupBlocks",sourceWarmupBlocks},{"transitionBlocks",transitionBlocks},{"transitionDeadlineMisses",transitionMisses},{"transitionMedianUs",(transitionTimes[9]+transitionTimes[10])/2},{"transitionMaximumUs",transitionTimes.back()},{"transitionMaximumOutputPeak",transitionPeak},{"frames",1024},{"iterations",1000},{"warmupBlocks",warmupBlocks},{"medianUs",(times[499]+times[500])/2},{"p99Us",times[989]},{"maximumUs",times.back()},{"deadlineUs",1024.0/192000*1e6},{"deadlineMisses",misses},{"maximumOutputPeak",peak},{"mmcssRegistered",true},{"mmcssPriorityCritical",true},{"source","independent offline copy; no live histories advanced"}};
}
}
