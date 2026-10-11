#pragma once
#include "PcmPattern.h"
#include <entropy/stage_bus_v1.h>
#include <cstring>
#include <string>
namespace entropy::music::audit {
// Native 192k only: every signed PCM value is exactly representable in binary64.
// Other rates require an independently specified SRC reference, not equality.
class Fp64Verifier {
    PcmFormat format_;uint64_t expected_,generation_,frames_=0,sequence_=0,mismatches_=0,invalidFrames_=0,interiorInvalidFrames_=0;
    std::string reason_;
    bool fail(const char* reason){if(reason_.empty())reason_=reason;return false;}
public:
    Fp64Verifier(const PcmFormat& f,uint64_t frames,uint64_t generation):format_(f),expected_(frames),generation_(generation){
        if(!validateFormat(f) || f.sampleRate!=192000 || !frames || frames%1024 || !generation)fail("Exact FP64 test requires native192k and complete1024-frame blocks");
    }
    bool accept(const StageBusBlockHeader& h,const double* l,const double* r){
        if(!reason_.empty())return false;
        if(!l || !r || h.frames!=1024 || !h.block_sequence || h.block_sequence>UINT64_MAX/1024 || h.first_frame!=(h.block_sequence-1)*1024 ||
            (sequence_ && h.block_sequence!=sequence_+1) || (h.validity_mask|h.discontinuity_mask)&~1u || h.source_generation[1] || h.source_generation[2])return fail("Invalid/gapped output block or AUX present");
        sequence_=h.block_sequence;
        if(!(h.validity_mask&1)){
            for(unsigned i=0;i<1024;++i)if(l[i]!=0 || r[i]!=0)return fail("Nonzero invalid output block");
            invalidFrames_+=1024;
            if(frames_ && frames_<expected_){interiorInvalidFrames_+=1024;return fail("Inserted invalid/silent clock block inside source sequence");}
            return true;
        }
        if(h.source_generation[0]!=generation_ || h.source_rate[0]!=192000 || (frames_ && h.discontinuity_mask))return fail("Wrong generation/rate or interior discontinuity");
        if(frames_>expected_ || expected_-frames_<1024)return fail("Extra output frames");
        uint8_t bytes[8]{};
        for(unsigned i=0;i<1024;++i){
            PcmPattern::frame(frames_+i,format_,bytes);
            for(unsigned c=0;c<2;++c){
                double value;
                if(format_.containerBits==16){int16_t v;std::memcpy(&v,bytes+c*2,2);value=v/32768.0;}
                else {int32_t v;std::memcpy(&v,bytes+c*4,4);value=(v>>8)/8388608.0;}
                if(std::memcmp(&value,(c?r:l)+i,sizeof(value)))++mismatches_;
            }
        }
        frames_+=1024;if(mismatches_)return fail("FP64 sample differs from exact original PCM value");return true;
    }
    bool finish(bool footer,uint64_t errors){
        if(!footer || errors)return fail("Incomplete output capture or subscriber error");
        if(frames_!=expected_)return fail("Missing output first/tail frames");return reason_.empty();
    }
    uint64_t frames() const noexcept{return frames_;}
    uint64_t mismatchedSamples() const noexcept{return mismatches_;}
    uint64_t invalidFrames() const noexcept{return invalidFrames_;}
    uint64_t interiorInvalidFrames() const noexcept{return interiorInvalidFrames_;}
    const std::string& reason() const noexcept{return reason_;}
};
}
