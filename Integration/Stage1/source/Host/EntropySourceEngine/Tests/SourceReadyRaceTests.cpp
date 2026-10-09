#include "SourceDomainStream.h"
#include "RealtimeBufferPolicy.h"
#include <windows.h>
#include <iostream>
#include <array>
#include <chrono>
using namespace entropy::host;
static std::atomic<bool> observed{false},releaseConsumer{false},armed{false};
void sourceReadyTestBarrier(){if(!armed.exchange(false))return;observed=true;while(!releaseConsumer.load())Sleep(1);}
int main(){std::string error;auto el=ModuleLibrary::load(TEST_LATENCY_ENDPOINT,error),sl=ModuleLibrary::load(std::filesystem::path(TEST_MODULE_ROOT)/"EntropySampleRateConverter.dll",error);if(!el||!sl)return 2;ModuleInstance ep(el),src(sl);if(!ep.create(error)||!src.create(error)||!ep.prepare(192000,1024,2,2,error)||!src.prepare(192000,1024,2,2,error))return 2;
SourceDomainConfig cfg;cfg.channels=2;cfg.prerollFrames=8192;cfg.outputTargetFrames=16384;cfg.fifoCapacityFrames=524288;cfg.captureFifoCapacityFrames=131072;SourceDomainStream s;if(!s.prepare(ep,src,cfg)||!s.start())return 2;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!s.ready()&&std::chrono::steady_clock::now()<end)Sleep(1);if(!s.ready()){s.stop();return 2;}
armed=true;std::thread consumer([&]{std::array<std::array<double,1024>,2>b;std::array<double*,2> p{b[0].data(),b[1].data()};SourceStreamDescriptor d;while(!observed&&std::chrono::steady_clock::now()<end)s.pop1024(p.data(),d);});
while(!observed&&std::chrono::steady_clock::now()<end)Sleep(1);while(s.telemetry().fifo.fill_frames<cfg.outputTargetFrames&&std::chrono::steady_clock::now()<end)Sleep(1);const bool refilled=observed&&s.telemetry().fifo.fill_frames>=cfg.outputTargetFrames;releaseConsumer=true;consumer.join();std::array<std::array<double,1024>,2>b;std::array<double*,2>p{b[0].data(),b[1].data()};SourceStreamDescriptor d;bool progress=refilled&&s.pop1024(p.data(),d);s.stop();if(!progress){std::cerr<<"FAIL stale drain observation overrode SRC refill at target; next pop cannot progress\n";return 1;}std::cout<<"PASS paused consumer stale drain cannot invalidate refill at target\n";
}
