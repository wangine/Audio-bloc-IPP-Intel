#include "CinemaSourceDomainStream.h"
#include "SourceCycleTimer.h"
#include <windows.h>
#include <iostream>
#include <cmath>
using namespace entropy::host;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(0)
int main(){
 std::string error;
 auto el=ModuleLibrary::load(TEST_LATENCY_ENDPOINT,error),sl=ModuleLibrary::load(std::filesystem::path(TEST_MODULE_ROOT)/"EntropySampleRateConverter.dll",error);CHECK(el&&sl);
 for(unsigned rate:{48000u,96000u,192000u}){
  SetEnvironmentVariableA("ENTROPY_LATENCY_RATE",std::to_string(rate).c_str());
  ModuleInstance ep(el),src(sl);CHECK(ep.create(error)&&src.create(error)&&ep.prepare(192000,1024,8,8,error)&&src.prepare(192000,1024,8,8,error));
  CinemaSourceDomainStream stream;
  SourceDomainConfig cfg;cfg.channels=8;cfg.endpointReadFrames=1024;cfg.prerollFrames=8192;cfg.outputTargetFrames=16384;cfg.fifoCapacityFrames=524288;cfg.captureFifoCapacityFrames=131072;
  for(unsigned engine:{2u,0u,2u}){
   cfg.srcEngine=engine;CHECK(stream.prepare(ep,src,cfg)&&stream.start());
   for(unsigned n=0;n<5000&&!stream.ready();++n)Sleep(1);CHECK(stream.ready());
   auto before=stream.telemetry();CHECK(stream.requestSrcEngine(engine));CHECK(!stream.requestSrcEngine(1));CHECK(!stream.requestSrcEngine(engine==2?0:2));CHECK(stream.telemetry().resets==before.resets);
   std::array<std::array<double,1024>,8> block{};std::array<double*,8> pointers{};for(unsigned c=0;c<8;++c)pointers[c]=block[c].data();
   SourceCycleTimer timer;auto epoch=std::chrono::steady_clock::now();
   for(unsigned seq=0;seq<188;++seq){timer.waitUntil(epoch+std::chrono::nanoseconds(uint64_t(seq)*1024*1000000000/192000));SourceStreamDescriptor d;CHECK(stream.pop1024(pointers.data(),d));CHECK(d.channels==8&&d.source_rate==rate&&d.valid_bits==64);for(unsigned c=1;c<8;++c)for(unsigned i=0;i<1024;++i)CHECK(std::abs(block[c][i]-block[0][i]*(c+1))<1e-12);}
   auto t=stream.telemetry();stream.stop();CHECK(t.src_engine==engine&&t.src_output_frames&&t.channels==8&&!t.faults&&!t.fifo.underruns&&!t.capture_fifo.overflows);
   if(engine==2)CHECK(t.src_latency_frames<=512);if(rate==192000&&engine==2)CHECK(t.src_latency_frames==0);
  }
 }
 CHECK(CinemaSourceDomainStream::supportsReferenceRate(48000));CHECK(!CinemaSourceDomainStream::supportsReferenceRate(32000));
 std::cout<<"PASS active CINEMA WDL -> r8brain -> WDL at 48/96/192k, 8ch order, no-op acknowledgement, no missing blocks\n";
}
