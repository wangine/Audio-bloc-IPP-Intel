#include "StereoMetrics.h"
#include <algorithm>
#include <cmath>
namespace entropy::music::audit {
void StereoMetrics::add(double l,double r) noexcept{
    ++n_;const double dl=l-lMean_,dr=r-rMean_;lMean_+=dl/static_cast<double>(n_);rMean_+=dr/static_cast<double>(n_);
    lVariance_+=dl*(l-lMean_);rVariance_+=dr*(r-rMean_);covariance_+=dl*(r-rMean_);
    l2_+=l*l;r2_+=r*r;lr_+=l*r;const auto m=(l+r)*.5,s=(l-r)*.5;m2_+=m*m;s2_+=s*s;lp_=std::max(lp_,std::abs(l));rp_=std::max(rp_,std::abs(r));
}
bool StereoMetrics::accumulate(const EntropyIngressPacketV2& p,const uint8_t* bytes){
    PcmFormat f;std::memcpy(&f,&p.format,64);if(!bytes || !validateFormat(f) || p.payloadBytes!=p.frames*f.blockAlign || p.frames>MaxPacketFrames)return false;
    for(uint32_t i=0;i<p.frames;++i){double values[2]{};for(uint32_t c=0;c<2;++c){const auto start=bytes+i*f.blockAlign+c*f.containerBits/8;uint32_t word=0;for(uint32_t b=0;b<f.containerBits/8;++b)word|=static_cast<uint32_t>(start[b])<<(8*b);const auto sign=f.containerBits==16?0x8000ULL:0x80000000ULL;const auto full=f.containerBits==16?0x10000ULL:0x100000000ULL;const auto signedWord=static_cast<int64_t>(word)-(word>=sign?static_cast<int64_t>(full):0);values[c]=static_cast<double>(signedWord)/static_cast<double>(sign);}add(values[0],values[1]);}return true;
}
StereoResult StereoMetrics::result() const{
    StereoResult r;r.frames=n_;r.leftPeak=lp_;r.rightPeak=rp_;if(!n_)return r;const auto n=static_cast<double>(n_);r.leftRms=std::sqrt(l2_/n);r.rightRms=std::sqrt(r2_/n);r.midRms=std::sqrt(m2_/n);r.sideRms=std::sqrt(s2_/n);
    if(lVariance_>0 && rVariance_>0)r.pearson=std::clamp(covariance_/std::sqrt(lVariance_*rVariance_),-1.,1.);
    if(l2_>0 && r2_>0)r.uncentered=std::clamp(lr_/std::sqrt(l2_*r2_),-1.,1.);return r;
}
}
