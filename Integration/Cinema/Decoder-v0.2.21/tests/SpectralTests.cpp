#include "CinemaOutput.h"
#include <cmath>
#include <iostream>
#include <vector>
#include <chrono>
#include <stdexcept>
constexpr double pi=3.14159265358979323846;
struct Capture{std::vector<double> pcm;static void block(void* v,const double* const* p){auto& s=*static_cast<Capture*>(v);s.pcm.insert(s.pcm.end(),p[0],p[0]+1024);}};
double amplitude(const std::vector<double>& v,double frequency){double real=0,imag=0;for(unsigned i=0;i<192000;i++){double phase=2*pi*frequency*i/192000.;real+=v[i+8192]*std::cos(phase);imag+=v[i+8192]*std::sin(phase);}return 2*std::hypot(real,imag)/192000.;}
int main(){try{for(double f:{1000.,10000.,18000.,20000.,22000.}){CinemaBlockProcessor processor;if(!processor.prepare(8))return 1;Capture s;s.pcm.reserve(220000);const auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<54000;i++){std::array<float,8> frame{};frame[0]=float(.25*std::sin(2*pi*f*i/48000.));if(!processor.push(frame,Capture::block,&s))return 1;}const auto duration=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(s.pcm.size()<200192)return 1;const double a=amplitude(s.pcm,f);double image=0;for(double hz:{48000-f,48000+f,96000-f})image=std::max(image,amplitude(s.pcm,hz));const double gain=20*std::log10(a/.25),db=20*std::log10(image/a);std::cout<<"frequency="<<f<<" gain_dB="<<gain<<" maximum_image_dBc="<<db<<" cpu_ms_for_1.125s="<<duration<<" SRC_latency_ms="<<processor.latencySeconds()*1000<<'\n';if(f<=20000&&(std::abs(gain)>.1||db> -80))throw std::runtime_error("spectral threshold");}return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
