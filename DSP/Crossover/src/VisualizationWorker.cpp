#include "VisualizationWorker.h"
#include "PluginProcessor.h"
#include <windows.h>
#include <chrono>
namespace entropy::standalone::visual {
namespace {
uint64_t now(){LARGE_INTEGER q{};QueryPerformanceCounter(&q);return uint64_t(q.QuadPart);}
using C=std::complex<double>;
C section(const Section& s,C z){return (s.b0+s.b1*z+s.b2*z*z)/(1.0+s.a1*z+s.a2*z*z);}
}
Worker::Worker(EntropyCrossoverAudioProcessor& e):engine_(e),source_(e.visualizationSource()){
 LARGE_INTEGER f{};QueryPerformanceFrequency(&f);qpcFrequency_=double(f.QuadPart);
 owner_=std::thread([this]{run();});
}
Worker::~Worker(){stop_.store(true);if(owner_.joinable())owner_.join();}
bool Worker::response(Frame& frame,uint64_t descriptor){
 const auto generation=descriptor>>2;if(!source_.copy(generation,*configuration_))return false;
 const auto& model=*configuration_;frame.sampleRate=model.sampleRate;frame.program=model.program;
 for(unsigned p=0;p<points;++p)frame.responseFrequencyHz[p]=20*std::exp(double(p)/(points-1)*std::log(3000.0));
 for(unsigned ch=0;ch<16;++ch){const auto& c=model.channels[ch];int count=0;
  if(c.fir&&!c.bypass){std::copy(c.kernel.head.begin(),c.kernel.head.end(),taps_.begin());count=128;
   for(int part=0;part<c.kernel.partitions;++part){partition_=c.kernel.tail[part];inverse_.runNative(partition_.data(),true);
    for(int i=0;i<128;++i)taps_[count++]=partition_[i].re;}}
  for(unsigned p=0;p<points;++p){if(stop_.load()||(source_.applied.load()>>2)!=generation)return false;
   const double w=6.2831853071795864769*frame.responseFrequencyHz[p]/model.sampleRate;const C z=std::polar(1.0,-w);C h=1;
   if(!c.bypass){if(c.fir){h=0;C power=1;for(int i=0;i<count;++i){h+=taps_[i]*power;power*=z;}}
    else for(unsigned i=0;i<c.stages;++i){if(c.hpOn)h*=section(c.hp[i],z);if(c.lpOn)h*=section(c.lp[i],z);}
    if(c.apOn)h*=section(c.ap,z);}
   if(c.delay.mode&&c.delay.order){C denominator=0,numerator=0,power=1;for(int i=0;i<=c.delay.order;++i){denominator+=c.delay.a[i]*power;numerator+=c.delay.a[c.delay.order-i]*power;power*=z;}h*=std::polar(1.0,-w*c.delay.integerTap)*numerator/denominator;}
   else{const double whole=std::floor(c.delay.delay),fraction=c.delay.delay-whole;h*=std::polar(1.0,-w*whole)*((1-fraction)+fraction*z);}
   h*=c.enabled?c.gain:0;frame.responseDb[ch][p]=float(std::max(-120.0,20*std::log10(std::max(1e-9,std::abs(h)))));frame.responsePhaseRadians[ch][p]=float(std::arg(h));
  }
 }
 frame.generation=generation;++frame.responseBuilds;return true;
}
void Worker::run()noexcept{
 try{Frame working{};uint64_t lastSamples=0,lastIdentity=0;
  while(!stop_.load()){
   const auto descriptor=source_.applied.load(std::memory_order_acquire);
   if((descriptor>>2)&&working.generation!=(descriptor>>2)){
    Frame candidate=working;
    if(response(candidate,descriptor)&&(source_.applied.load()>>2)==candidate.generation)working=candidate;
   }
   Capture capture{};
   if(engine_.copyLiveSpectrumFrame(samples_.data(),fftSize,capture)
     &&(capture.monitorIdentity!=lastIdentity||capture.generation!=working.capture.generation||capture.samples>=lastSamples+fftSize/8)){
    if(spectrum_->process(samples_.data(),capture,qpcFrequency_)){const auto& analyzed=spectrum_->frame();working.capture=analyzed.capture;working.frequency=analyzed.frequency;working.spectrum=analyzed.spectrum;working.spectrumAvailable=true;lastSamples=capture.samples;lastIdentity=capture.monitorIdentity;}
   }
   {std::lock_guard lock(frameMutex_);frame_=working;}
   std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
 }catch(...){std::lock_guard lock(frameMutex_);frame_.spectrumAvailable=frame_.responseAvailable=false;}
}
Frame Worker::snapshot()const{
 Frame result;{std::lock_guard lock(frameMutex_);result=frame_;}
 const auto applied=source_.applied.load(std::memory_order_acquire);
 result.transition=bool(applied&2u);result.responseAvailable=result.generation&&(result.generation==(applied>>2))&&!result.transition;
 result.spectrumAvailable=result.spectrumAvailable&&captureAvailable(result.capture,source_.monitorIdentity.load(),applied,now(),qpcFrequency_);
 result.stale=!result.spectrumAvailable;return result;
}
}
