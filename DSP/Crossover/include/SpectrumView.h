#pragma once
#include "IppSpectrum.h"
#include "VisualizationData.h"
namespace entropy::standalone::visual {
// Sole-worker analyzer. Reading the last frame never changes smoothing or FFT.
class SpectrumView {
 native::IppSpectrum fft_{14};
 std::array<double,fftSize> window_{},input_{};
 std::array<double,fftSize/2+1> magnitudes_{};
 std::array<float,fftSize/2+1> smooth_{};
 Frame frame_{};bool initialized_=false;
public:
 SpectrumView(){double sum=0;for(unsigned i=0;i<fftSize;++i){window_[i]=.5-.5*std::cos(6.2831853071795864769*i/(fftSize-1));sum+=window_[i];}for(auto& w:window_)w*=fftSize/sum;smooth_.fill(-120);}
 bool process(const float* samples,const Capture& capture,double qpcFrequency){
  if(initialized_&&capture.frameSequence==frame_.capture.frameSequence)return false;
  const bool changed=!initialized_||capture.monitorIdentity!=frame_.capture.monitorIdentity||capture.generation!=frame_.capture.generation;
  if(changed)smooth_.fill(-120);
  const double elapsed=changed?0:(capture.captureQpc>frame_.capture.captureQpc?double(capture.captureQpc-frame_.capture.captureQpc)/qpcFrequency:0);
  for(unsigned i=0;i<fftSize;++i)input_[i]=double(samples[i])*window_[i];
  fft_.magnitudes(input_.data(),magnitudes_.data());
  for(unsigned i=0;i<=fftSize/2;++i){
   const double scale=(i==0||i==fftSize/2)?1.0/fftSize:2.0/fftSize;
   const float db=float(std::max(-120.0,20*std::log10(std::max(1e-9,magnitudes_[i]*scale))));
   smooth_[i]=std::max(db,smooth_[i]-float(60*elapsed));
  }
  for(unsigned p=0;p<points;++p)frame_.frequency[p]=float(std::exp(std::log(20.0)+double(p)/(points-1)*std::log(60000.0/20)));
  const double binHz=capture.sampleRate/fftSize;
  for(unsigned p=0;p<points;++p){const double center=frame_.frequency[p];
   const double lower=p?std::sqrt(double(frame_.frequency[p-1])*center):20;
   const double upper=p+1<points?std::sqrt(center*double(frame_.frequency[p+1])):60000;
   int first=std::clamp(int(std::ceil(lower/binHz)),0,int(fftSize/2)),last=std::clamp(int(std::floor(upper/binHz)),0,int(fftSize/2));
   if(first>last)first=last=std::clamp(int(std::lround(center/binHz)),0,int(fftSize/2));
   float peak=-120;for(int bin=first;bin<=last;++bin)peak=std::max(peak,smooth_[bin]);frame_.spectrum[p]=peak;
  }
  frame_.capture=capture;frame_.spectrumAvailable=true;initialized_=true;return true;
 }
 const Frame& frame()const noexcept{return frame_;}
 float binDb(unsigned bin)const noexcept{return smooth_[bin];}
};
}
