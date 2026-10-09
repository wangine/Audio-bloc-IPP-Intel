#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
namespace entropy::stage {
struct DspSignalSnapshot {
 std::array<double,10> inputPeaks{};std::array<double,11> outputPeaks{};
 uint32_t inputMask=0,outputMask=0,validity=0,reserved=0;
 uint64_t sampledQpc=0,publishedQpc=0;
};
inline void measureSignal(DspSignalSnapshot& s,const double* const* input,const double* const* output,uint32_t validity,uint64_t now,bool centerActive=false) noexcept {
 s={};s.validity=validity;s.sampledQpc=s.publishedQpc=now;
 const auto peak=[](const double* p){double value=0;for(unsigned i=0;i<1024;i++)if(std::isfinite(p[i]))value=std::max(value,std::abs(p[i]));return value;};
 for(unsigned c=0;c<10;c++){s.inputPeaks[c]=peak(input[c]);if(s.inputPeaks[c]>1e-12 && (validity&(c<2?3u:4u)))s.inputMask|=1u<<c;}
 for(unsigned c=0;c<11;c++){s.outputPeaks[c]=peak(output[c]);if((c!=2 || centerActive) && s.outputPeaks[c]>1e-12 && (validity&(c<3?3u:4u)))s.outputMask|=1u<<c;}
}
inline std::pair<uint32_t,uint32_t> liveSignalMasks(const DspSignalSnapshot& s,uint64_t now,uint64_t frequency,bool receiving) noexcept {
 if(!receiving || !frequency || !s.publishedQpc || now<s.publishedQpc || now-s.publishedQpc>frequency/2)return {};
 return {s.inputMask,s.outputMask};
}
}
