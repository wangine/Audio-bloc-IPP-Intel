#pragma once
#include "PcmPattern.h"
#include "../provider/BridgeClient.h"
#include <string>
namespace entropy::music::audit {
struct Comparison {bool pass=false;uint64_t frames=0,mismatchedBytes=0,lostFrames=0;std::string reason;};
class PcmVerifier {
    PcmFormat format_;
    uint64_t expected_,next_=0,sequence_=0,generation_=0,qpc_=0;
    Comparison result_;
    bool reject(const char* text){if(result_.reason.empty())result_.reason=text;return false;}
public:
    PcmVerifier(const PcmFormat& f,uint64_t frames):format_(f),expected_(frames){}
    bool accept(const EntropyIngressPacketV2&,const uint8_t* bytes);
    Comparison finish(bool footer,uint64_t overflow,uint64_t failedWrites,bool drained);
    uint64_t received() const noexcept{return next_;}
};
}
