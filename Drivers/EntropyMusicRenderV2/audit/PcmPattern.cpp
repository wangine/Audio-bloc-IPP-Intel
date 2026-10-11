#include "PcmPattern.h"
namespace entropy::music::audit {
void PcmPattern::frame(uint64_t index,const PcmFormat& f,uint8_t* bytes) noexcept {
    for(uint32_t channel=0;channel<2;++channel){
        uint64_t mixed=index+(channel?0x9e3779b97f4a7c15ULL:0x517cc1b727220a95ULL);
        mixed=(mixed^(mixed>>30))*0xbf58476d1ce4e5b9ULL;mixed=(mixed^(mixed>>27))*0x94d049bb133111ebULL;mixed^=mixed>>31;
        const auto word=f.containerBits==16?static_cast<uint32_t>((mixed&0x7fff)-0x4000):static_cast<uint32_t>(((mixed&0x7fffff)-0x400000)<<8);
        for(uint32_t b=0;b<f.containerBits/8;++b)bytes[channel*f.containerBits/8+b]=static_cast<uint8_t>(word>>(8*b));
    }
}
}
