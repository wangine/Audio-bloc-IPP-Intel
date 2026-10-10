#include "Engine/Control/ControlPlane.h"
#include "Engine/Control/GraphControlAccess.h"
#include "SpectrumView.h"
#include "Shared/Visualization/FrequencyAxis.h"
#include <iostream>
#include <thread>
using namespace entropy;
using J=ipc::Value;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()try{
 using namespace standalone::visual;
 const double marker=visual::frequencyAt(visual::frequencyUnit(119.5,20,5000),20,5000);
 check(std::abs(marker-119.5)<1e-10,"119.5Hz changed on local 5k axis");
 std::cout<<"PASS axis: marker="<<marker<<" Hz; 60k source cropped, not relabeled\n";
 auto analyzer=std::make_unique<SpectrumView>();std::array<float,fftSize> samples{};
 for(unsigned n=0;n<fftSize;++n)samples[n]=float(.125*std::sin(6.2831853071795864769*1500*n/192000));
 Capture capture{};capture.monitorIdentity=33;capture.channel=0;capture.generation=1;capture.frameSequence=1;capture.captureQpc=1000000;capture.samples=fftSize;
 check(analyzer->process(samples.data(),capture,1000000),"new frame rejected");
 const double expected=20*std::log10(.125),measured=analyzer->binDb(128);
 check(std::abs(measured-expected)<.05,"FFT sine is not calibrated dBFS peak");
 std::cout<<"PASS FFT: N=16384, normalized Hann, 1500Hz peak=.125 -> "<<measured<<" dBFS peak; expected="<<expected<<"\n";
 std::array<float,fftSize> silence{};double decay[2]{};
 for(unsigned client=0;client<2;++client){auto a=std::make_unique<SpectrumView>();a->process(samples.data(),capture,1000000);
  auto next=capture;for(unsigned step=1;step<=3;++step){for(unsigned poll=0;poll<(client?4:2);++poll)check(!a->process(silence.data(),next,1000000),"duplicate audio frame caused decay");next.frameSequence++;next.captureQpc=capture.captureQpc+step*100000;next.samples+=19200;a->process(silence.data(),next,1000000);}decay[client]=a->binDb(128);}
 check(std::abs(decay[0]-decay[1])<1e-6&&std::abs(decay[0]-(measured-18))<1e-4,"decay depends on reader rate");
 check(!captureAvailable(capture,66,4,1100000,1000000),"old frame relabeled another channel");
 check(!captureAvailable(capture,65,4,1100000,1000000),"A-B-A reused old identity");
 check(!captureAvailable(capture,33,8,1100000,1000000),"old generation labeled current");
 check(!captureAvailable(capture,33,4,1500001,1000000),"500ms stale gate failed");
 std::cout<<"PASS time/identity: 10Hz="<<decay[0]<<";20Hz="<<decay[1]<<" dBFS after .3s; stale/channel/generation rejected\n";
 auto graph=std::make_unique<engine::AudioGraph>();check(graph->prepare(256),"prepare");control::ControlPlane controls(*graph);
 controls.execute(J::Object{{"action","parameters"},{"values",J::Array{J::Object{{"id","hpOn1"},{"value",0}},J::Object{{"id","lpOn1"},{"value",0}},J::Object{{"id","allPass1"},{"value",0}},J::Object{{"id","musicGainDb1"},{"value",0}}}}});
 controls.execute(J::Object{{"action","monitor"},{"channel",0}});
 native::AudioBuffer<double> input(16,256);native::MidiBuffer midi;auto xo=engine::GraphControlAccess::crossover(*graph);
 for(unsigned block=0;block<160;++block){input.clear();for(unsigned n=0;n<256;++n)input.getWritePointer(0)[n]=double(samples[(block*256+n)%fftSize]);controls.crossover().captureBeforeBlock();xo->processBlock(input,midi);controls.crossover().captureAfterBlock();}
 J view;for(unsigned wait=0;wait<25;++wait){view=controls.crossover().snapshot().at("view");if(view.at("spectrumAvailable").asBool()&&view.at("responseAvailable").asBool())break;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 check(view.at("version").asUnsigned()==2&&view.at("responseFrequencyHz").asArray().size()==256,"versioned explicit axis");
 check(view.at("spectrumAvailable").asBool(),"coherent worker spectrum unavailable");
 double peak=-120;for(const auto& v:view.at("spectrum").asArray())peak=std::max(peak,v.asNumber());check(std::abs(peak-expected)<.05,"worker calibration");
 check(view.at("responseAvailable").asBool(),"applied response unavailable");auto builds=view.at("responseBuilds").asU64();
 controls.execute(J::Object{{"action","monitor"},{"channel",0}});std::this_thread::sleep_for(std::chrono::milliseconds(20));auto unchanged=controls.crossover().snapshot().at("view");check(unchanged.at("responseBuilds").asU64()==builds,"control reads rebuilt response");check(unchanged.at("spectrumAvailable").asBool(),"idempotent monitor reset window");
 auto oneBlock=[&]{input.clear();controls.crossover().captureBeforeBlock();xo->processBlock(input,midi);controls.crossover().captureAfterBlock();};
 controls.execute(J::Object{{"action","program"},{"program",1}});for(unsigned n=0;n<44;++n)oneBlock();
 check(!controls.crossover().snapshot().at("view").at("spectrumAvailable").asBool(),"old MUSIC frame retained as CINEMA");
 controls.execute(J::Object{{"action","program"},{"program",0}});oneBlock();
 check(!controls.crossover().snapshot().at("view").at("spectrumAvailable").asBool(),"MUSIC-CINEMA-MUSIC reused old cached window");
 std::cout<<"PASS MUSIC-CINEMA-MUSIC: new capture epoch, unavailable until full new window\n";
 controls.execute(J::Object{{"action","monitor"},{"channel",1}});check(!controls.crossover().snapshot().at("view").at("spectrumAvailable").asBool(),"channel change retained previous window");
 std::cout<<"PASS integration: worker frame="<<view.at("frameSequence").asU64()<<"; response builds="<<builds<<"; control getter is read-only\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
