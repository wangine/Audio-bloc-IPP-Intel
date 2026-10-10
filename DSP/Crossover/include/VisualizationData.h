#pragma once
#include "RealtimeFir.h"
#include "FractionalDelay.h"
#include <atomic>
#include <mutex>
namespace entropy::standalone::visual {
inline constexpr unsigned points=256,fftSize=16384;
struct Section {double b0=1,b1=0,b2=0,a1=0,a2=0;};
struct Channel {
 std::array<Section,4> hp{},lp{};Section ap{};FirKernel kernel{};FractionalDelayDesign delay{};
 double gain=1;unsigned stages=1;bool enabled=false,hpOn=false,lpOn=false,apOn=false,fir=false,bypass=false;
};
struct Configuration {uint64_t generation=0;double sampleRate=192000;unsigned program=0;std::array<Channel,16> channels{};};
struct Capture {
 uint64_t monitorIdentity=0,generation=0,frameSequence=0,captureQpc=0,samples=0;
 double sampleRate=192000;int channel=-1;unsigned program=0;bool transition=false;
};
inline bool captureAvailable(const Capture& c,uint64_t monitor,uint64_t applied,uint64_t now,double qpcFrequency)noexcept{
 return c.channel>=0&&c.monitorIdentity==monitor&&c.channel==int(monitor&31u)-1
  &&c.generation==(applied>>2)&&c.program==(applied&1u)&&c.captureQpc&&now>=c.captureQpc
  &&double(now-c.captureQpc)<=.5*qpcFrequency;
}
// Only preparation and visualization owners take this mutex. Audio publishes
// one scalar descriptor and never accesses the model storage or mutex.
class Source {
 mutable std::mutex mutex_;std::unique_ptr<Configuration[]> models_=std::make_unique<Configuration[]>(3);
public:
 std::atomic<uint64_t> applied{0},monitorIdentity{0};
 template<class Fill>void prepare(unsigned slot,Fill&& fill){std::lock_guard lock(mutex_);fill(models_[slot]);}
 bool copy(uint64_t generation,Configuration& out)const{std::lock_guard lock(mutex_);for(unsigned i=0;i<3;++i)if(models_[i].generation==generation){out=models_[i];return generation!=0;}return false;}
 void publish(uint64_t generation,unsigned program,bool transition)noexcept{applied.store((generation<<2)|(program&1u)|(transition?2u:0u),std::memory_order_release);}
};
struct Frame {
 uint64_t generation=0,responseBuilds=0;double sampleRate=192000;unsigned program=0;
 bool responseAvailable=false,spectrumAvailable=false,transition=false,stale=true;
 Capture capture{};
 std::array<double,points> responseFrequencyHz{};
 std::array<std::array<float,points>,16> responseDb{},responsePhaseRadians{};
 std::array<float,points> frequency{},spectrum{};
};
}
