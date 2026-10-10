#include "Engine/Egress/LiveSinkWorkers.h"
#include "Engine/Egress/PresentationContract.h"
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
using namespace entropy;
namespace {
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct Sim {
 HANDLE step=CreateEventW(nullptr,FALSE,FALSE,nullptr),finished=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 std::atomic<unsigned> starts{0};std::atomic<int> lastPcm{0};std::atomic<bool> waiting{false},closed{false};std::atomic<uint64_t> firstNonzero{0},submitted{0};
 double rate=192000;uint64_t start=0;unsigned openDelay=0,buffer=1024;
 ~Sim(){CloseHandle(step);CloseHandle(finished);}
};
struct Fake final:egress_host::Sink {
 Sim& s;void* cancel=nullptr;EntropyDacSinkConfig c{};uint64_t index=0;bool running=false;
 explicit Fake(Sim& v):s(v){}
 void setCancelEvent(void* e) noexcept override{cancel=e;}
 EntropyResult open(const EntropyDacSinkConfig& v) override{c=v;std::this_thread::sleep_for(std::chrono::milliseconds(s.openDelay));return ENTROPY_OK;}
 EntropyResult start() noexcept override{++s.starts;running=true;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{
  s.waiting=true;HANDLE events[2]={HANDLE(cancel),s.step};const auto result=WaitForMultipleObjects(2,events,FALSE,1000);s.waiting=false;
  if(result!=WAIT_OBJECT_0+1)return ENTROPY_ERROR_INVALID_STATE;
  ++index;r.frames=s.buffer;r.bytes=s.buffer*c.channels*3;r.device_position_frames=index*s.buffer;
  r.device_qpc_position=s.start+uint64_t(double(index*s.buffer)*10000000/s.rate);return ENTROPY_OK;
 }
 egress_host::SinkTiming timing() const noexcept override{return {192000,31000,true,double(index*s.buffer),true};}
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{
  const auto* bytes=static_cast<const uint8_t*>(b.data);
  if(!s.firstNonzero.load())for(unsigned f=0;f<b.frames;++f){bool nonzero=false;for(unsigned c=0;c<3;++c)nonzero|=bytes[f*3+c]!=0;if(nonzero){s.firstNonzero=s.start+uint64_t(double(index*s.buffer+f)*10000000/s.rate);break;}}
  const auto* tail=bytes+(b.frames-1)*3;int value=int(tail[0])|(int(tail[1])<<8)|(int(tail[2])<<16);if(value&0x800000)value|=int(0xff000000u);s.lastPcm=value;
  b.accepted_frames=b.frames;++s.submitted;SetEvent(s.finished);return ENTROPY_OK;
 }
 EntropyDacSinkStatus status() const noexcept override{EntropyDacSinkStatus v{};v.struct_size=sizeof(v);v.state=running?ENTROPY_DAC_SINK_RUNNING:ENTROPY_DAC_SINK_OPEN;v.buffer_frames=s.buffer;v.device_position_frames=index*s.buffer;v.submitted_frames=s.submitted*s.buffer;return v;}
 void stop() noexcept override{running=false;}
 void close() noexcept override{s.closed=true;SetEvent(s.finished);}
};
}
void scenario(unsigned bufferA,unsigned bufferB){
 std::array<Sim,2> sim;sim[0].buffer=bufferA;sim[1].buffer=bufferB;sim[0].rate=192000*(1+100e-6);sim[1].rate=192000*(1-70e-6);sim[0].start=10000000;sim[1].start=10037000;sim[1].openDelay=4;
 control::LiveSinkWorkers workers([&](unsigned slot){return std::make_unique<Fake>(sim[slot]);});
 std::array<control::LiveDomain,2> config;for(unsigned i=0;i<2;++i){config[i].enabled=true;config[i].channels=1;config[i].endpoint=i?"sim-B":"sim-A";config[i].logicalId=i?"DAC_B":"DAC_A";config[i].lanes[0]=i;}
 require(workers.prepare(config)&&workers.start(),"full calibrated group ARM");
 constexpr unsigned tick=512;std::array<double,tick> tone;std::array<const double*,16> pointers;for(auto& p:pointers)p=tone.data();
 double maximumDifference=0;unsigned compared=0,pcmChecked=0;uint64_t graphFrame=0;
 for(unsigned step=0;step<3000;++step){
  for(unsigned f=0;f<tick;++f)tone[f]=.1+double((graphFrame+f)%32768)*1e-5;
  workers.atBlockBoundary();workers.publish(pointers.data(),tick,true,{42,graphFrame,10000000+graphFrame*10000000/192000,tick});graphFrame+=tick;
  for(auto& s:sim)if((step+1)%(s.buffer/tick)==0)SetEvent(s.step);
  for(auto& s:sim)if((step+1)%(s.buffer/tick)==0){const bool ok=WaitForSingleObject(s.finished,2000)==WAIT_OBJECT_0&&!s.closed;if(!ok)std::fprintf(stderr,"step=%u snapshot=%s\n",step,ipc::dump(workers.snapshot()).c_str());require(ok,"simulated cycle failed/timeout");}
  auto snapshot=workers.snapshot();const auto& a=snapshot.asArray()[0];const auto& b=snapshot.asArray()[1];
  if(sim[0].firstNonzero&&sim[1].firstNonzero){const double error=std::abs(a.at("estimatedABDifferenceFrames").asNumber());maximumDifference=(std::max)(maximumDifference,error);++compared;
   for(unsigned i=0;i<2;++i){const auto& d=snapshot.asArray()[i];const double last=d.at("endGraphFrame").asNumber()-(1+d.at("ppm").asNumber()*1e-6);const double phase=std::fmod(last,32768.0);
    if(phase>100&&phase<32668){const int wanted=int(std::nearbyint((.1+phase*1e-5)*8388608));if(std::abs(sim[i].lastPcm.load()-wanted)>2)std::fprintf(stderr,"PCM ORACLE i=%u step=%u last=%.9f actual=%d wanted=%d\n",i,step,last,sim[i].lastPcm.load(),wanted);require(std::abs(sim[i].lastPcm.load()-wanted)<=2,"unannounced PCM drop/duplicate/identity mismatch");++pcmChecked;}}
  }
  if(step==2999){std::printf("buffers=%u/%u frames=%llu calibratedA=%d calibratedB=%d ppmA=%.6f ppmB=%.6f capacity=%u target=%u\n",bufferA,bufferB,static_cast<unsigned long long>(graphFrame),a.at("originalClockCalibrated").asBool(),b.at("originalClockCalibrated").asBool(),a.at("ppm").asNumber(),b.at("ppm").asNumber(),a.at("fifoCapacity").asUnsigned(),a.at("targetFrames").asUnsigned());}
 }
 if(compared<=100)std::fprintf(stderr,"FINAL %s\n",ipc::dump(workers.snapshot()).c_str());require(pcmChecked>100,"PCM oracle not exercised");require(compared>100,"all-zero startup is not PASS");
 const double onset=std::abs(double(int64_t(sim[0].firstNonzero.load())-int64_t(sim[1].firstNonzero.load())))*192000/10000000;
 std::printf("comparisons=%u pcmOracleSamples=%u maximumSameTimeDifferenceFrames=%.9f firstNonzeroOnsetDifferenceFrames=%.9f\n",compared,pcmChecked,maximumDifference,onset);
 require(maximumDifference<=1&&onset<=1,"software chronology exceeds 1 sample");
 // DISARM while both owners are in wait; graph keeps producing and acknowledges
 // retirement. No subscription or graph restart is involved.
 const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(1);while((!sim[0].waiting||!sim[1].waiting)&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
 require(sim[0].waiting&&sim[1].waiting,"owners not in wait");
 workers.closePublicationGate();std::thread retire([&]{workers.stop();});
 for(unsigned i=0;i<10;++i){workers.atBlockBoundary();graphFrame+=tick;std::this_thread::yield();}
 workers.graphStopped();retire.join();require(sim[0].closed&&sim[1].closed&&!workers.armed(),"cancel did not wake/close owners");
}
int main(int argc,char** argv)try{
 if(argc>1&&std::strcmp(argv[1],"--diagnostic-large-buffer")==0){scenario(1024,4096);return 0;}
 // Sideband splits and merges retain every actual identity, including wrap.
 egress_host::SpscAudioFifo fifo;require(fifo.prepare(1,16),"FIFO prepare");double src[16],dst[16];for(unsigned n=0;n<16;++n)src[n]=n+.25;const double* in[]={src};double* out[]={dst};
 require(fifo.push(in,7,{9,1000,500,7})&&fifo.push(in,7,{9,1007,600,7}),"sideband push");egress_host::GraphInterval interval;
 require(fifo.pop(out,11,&interval)&&interval.epoch==9&&interval.firstGraphFrame==1000&&interval.frames==11,"merged identity");require(fifo.pop(out,3,&interval)&&interval.firstGraphFrame==1011,"split identity");
 require(fifo.push(in,16,{9,1014,700,16})&&fifo.pop(out,16,&interval)&&interval.firstGraphFrame==1014,"wrapped identity");
 scenario(1024,1024);scenario(512,1024);
 // Unsupported full-target driver buffer is refused before any Start.
 std::array<Sim,2> unsupported;unsupported[1].buffer=4096;
 control::LiveSinkWorkers blocked([&](unsigned i){return std::make_unique<Fake>(unsupported[i]);});
 std::array<control::LiveDomain,2> config;for(unsigned i=0;i<2;++i){config[i].enabled=true;config[i].channels=1;config[i].logicalId=i?"DAC_B":"DAC_A";config[i].endpoint=i?"unsupported-B":"unsupported-A";config[i].lanes[0]=i;}
 require(blocked.prepare(config)&&blocked.start(),"unsupported candidate creation");blocked.atBlockBoundary();
 const auto timeout=std::chrono::steady_clock::now()+std::chrono::seconds(2);while((!unsupported[0].closed||!unsupported[1].closed)&&std::chrono::steady_clock::now()<timeout)std::this_thread::yield();
 require(unsupported[0].closed&&unsupported[1].closed&&!unsupported[0].submitted&&!unsupported[1].submitted&&!unsupported[0].starts&&!unsupported[1].starts,"unsupported buffer did not fail silently before Start");blocked.graphStopped();blocked.stop();
 std::puts("PASS contract software verificat; skew analogic nemasurat; sideband, different ppm/start, sample onset, cancel; no hardware");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL Stage019 %s\n",e.what());return 1;}
