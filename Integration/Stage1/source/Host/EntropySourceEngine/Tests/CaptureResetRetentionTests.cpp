#include "SourceDomainStream.h"
#include "CinemaSourceDomainStream.h"
#include "AuditHooks.h"
#include "SourceCycleTimer.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <thread>
using namespace entropy::host;
static const void* target=nullptr;static std::atomic<bool> held{false},released{false};static std::atomic<unsigned> refused{0},providerRead{0},firstPacket{0};
static void hold(const void*p){if(p!=target||held.exchange(true))return;auto limit=GetTickCount64()+5000;while(!released&&GetTickCount64()<limit)std::this_thread::yield();}
static void refusal(const void*p){if(p==target){++refused;released=true;}}
static void capture(unsigned kind,unsigned,const double*const*,unsigned n){if(kind==0){unsigned zero=0;firstPacket.compare_exchange_strong(zero,n);providerRead+=n;}}
template<class F>bool until(F f){auto end=GetTickCount64()+5000;while(!f()){if(GetTickCount64()>end){released=true;return false;}Sleep(1);}return true;}
#define REQUIRE(x) do{if(!(x)){released=true;std::cerr<<"INVALID line "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(0)
template<class Stream>int run(unsigned role,unsigned engine,const char*prefix){std::string error;const char*names[]{"EntropyMusicTransportEndpoint.dll","EntropyAuxEndpoint.dll","EntropyCinemaInput.dll"};auto el=ModuleLibrary::load(std::filesystem::path(AUDIT_LOSS_ROOT)/names[role],error),sl=ModuleLibrary::load(std::filesystem::path(AUDIT_LOSS_ROOT)/"EntropySampleRateConverter.dll",error);REQUIRE(el&&sl);ModuleInstance ep(el),src(sl);unsigned ch=role==2?8:2;REQUIRE(ep.create(error)&&src.create(error)&&ep.prepare(192000,1024,0,ch,error)&&src.prepare(192000,1024,ch,ch,error));SourceDomainConfig cfg;cfg.channels=ch;cfg.srcEngine=engine;cfg.endpointReadFrames=1024;cfg.prerollFrames=8192;cfg.outputTargetFrames=16384;cfg.fifoCapacityFrames=524288;cfg.captureFifoCapacityFrames=131072;Stream stream;REQUIRE(stream.prepare(ep,src,cfg));target=&stream.auditCaptureFifo();auditFifoOperationHook=hold;auditResetRefusedHook=refusal;auditBoundaryHook=capture;REQUIRE(stream.start());REQUIRE(until([&]{return refused.load()>=1&&stream.ready();}));unsigned lost=firstPacket;REQUIRE(lost>0);
#ifdef AUDIT_EXPECT_RETAINED
 lost=0;
#endif
std::array<std::array<double,1024>,8> samples{};std::array<double*,8> p{};for(unsigned c=0;c<ch;++c)p[c]=samples[c].data();SourceStreamDescriptor descriptor;unsigned frames=0;double first=0;const unsigned total=192000/2/256*256;SourceCycleTimer timer;auto epoch=std::chrono::steady_clock::now();while(frames+1024<=total-lost){timer.waitUntil(epoch+std::chrono::nanoseconds(uint64_t(frames)*1000000000/192000));REQUIRE(until([&]{return stream.pop1024(p.data(),descriptor);}));if(frames==0)first=samples[0][0];
for(unsigned n=0;n<1024;++n)for(unsigned channel=0;channel<ch;++channel){
 const double expected=double(frames+n+1)/1048576*(role==2?double(channel+1):(channel==0?1.0:-1.0));
 REQUIRE(samples[channel][n]==expected);
}
REQUIRE(descriptor.source_rate==192000&&descriptor.channels==ch);
frames+=1024;}
REQUIRE(until([&]{return providerRead==total&&stream.telemetry().src_input_frames==total-lost;}));auto t=stream.telemetry();stream.stop();REQUIRE(first==double(lost+1)/1048576&&t.endpoint_frames_read==total-lost&&t.faults==0&&t.capture_fifo.abandoned_frames==0);std::ofstream j(std::string(prefix)+".json");j<<"{\"reproduced\":true,\"role\":"<<role<<",\"channels\":"<<ch<<",\"provider_read_frames\":"<<providerRead<<",\"accepted_frames\":"<<t.endpoint_frames_read<<",\"reset_refusals\":"<<refused<<",\"lost_frames\":"<<lost<<",\"first_delivered_sample_index\":"<<lost<<",\"faults\":"<<t.faults<<",\"fifo_abandoned\":"<<t.capture_fifo.abandoned_frames<<"}";std::cout<<"CAPTURE_RESULT role="<<role<<" ch="<<ch<<" provider="<<providerRead<<" accepted="<<t.endpoint_frames_read<<" silent_loss="<<lost<<" faults="<<t.faults<<'\n';return 0;}
int main(int argc,char**argv){if(argc!=4)return 2;unsigned role=std::stoul(argv[1]),engine=std::stoul(argv[2]);SetEnvironmentVariableA("AUDIT_RATE","192000");SetEnvironmentVariableA("AUDIT_TARGET",argv[1]);if(role==2)return run<CinemaSourceDomainStream>(role,engine,argv[3]);return run<SourceDomainStream>(role,0,argv[3]);}
