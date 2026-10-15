#include "Clock.h"
namespace entropy::music {
namespace {
constexpr uint64_t Scale=1000000000ULL;
// Exact floor((a*b+carry)/d) and remainder. Bounded 64-step arithmetic, no FP,
// allocation, platform 128-bit divide or overflowing intermediate product.
bool multiplyAddDivide(uint64_t a,uint64_t b,uint64_t d,uint64_t carry,
                       uint64_t& answer,uint64_t& residue) noexcept {
    if(!d || carry>=d)return false;
    const uint64_t whole=a/d,fraction=a%d;
    if(b && whole>UINT64_MAX/b)return false;
    const uint64_t base=whole*b;
    uint64_t quotient=0,remainder=0;
    for(int bit=63;bit>=0;--bit){
        if(quotient>UINT64_MAX/2)return false;
        quotient*=2;
        if(remainder>=d-remainder){remainder-=d-remainder;if(quotient==UINT64_MAX)return false;++quotient;}
        else remainder*=2;
        if((b>>bit)&1ULL){
            if(remainder>=d-fraction){remainder-=d-fraction;if(quotient==UINT64_MAX)return false;++quotient;}
            else remainder+=fraction;
        }
    }
    if(remainder>=d-carry){remainder-=d-carry;if(quotient==UINT64_MAX)return false;++quotient;}
    else remainder+=carry;
    if(quotient>UINT64_MAX-base)return false;
    answer=base+quotient;residue=remainder;return true;
}
bool integrate(uint64_t ticks,uint32_t rate,uint64_t frequency,int32_t ppb,
               uint64_t& frames,uint64_t& remainder) noexcept {
    const uint64_t scale=ppb>=0 ? Scale+static_cast<uint32_t>(ppb)
                              : Scale-static_cast<uint32_t>(-ppb);
    const uint64_t multiplier=uint64_t(rate)*scale,denominator=frequency*Scale;
    uint64_t delta=0,nextRemainder=0;
    if(!multiplyAddDivide(ticks,multiplier,denominator,remainder,delta,nextRemainder)
       || delta>UINT64_MAX-frames)return false;
    frames+=delta;remainder=nextRemainder;return true;
}
}
// Retained original autonomous arithmetic and its full uint64 frequency range.
bool multiplyDivide(uint64_t ticks,uint32_t rate,uint64_t frequency,uint64_t& frames) noexcept {
    if(!frequency || !rate)return false;
    const auto whole=ticks/frequency;
    if(whole>UINT64_MAX/rate)return false;
    const auto base=whole*rate,fraction=ticks%frequency;
    uint64_t quotient=0,remainder=0;
    for(int bit=31;bit>=0;--bit){
        quotient*=2;
        if(remainder>=frequency-remainder){remainder-=frequency-remainder;++quotient;}else remainder*=2;
        if(rate&(1u<<bit)){
            if(remainder>=frequency-fraction){remainder-=frequency-fraction;++quotient;}else remainder+=fraction;
        }
    }
    if(quotient>UINT64_MAX-base)return false;
    frames=base+quotient;return true;
}
bool Clock::reset(uint32_t rate,uint64_t frequency,uint64_t generation) noexcept {
    if(!frequency || !generation ||
       !(rate==44100 || rate==48000 || rate==88200 || rate==96000 || rate==176400 || rate==192000))return false;
    rate_=rate;frequency_=frequency;generation_=generation;
    frames_=remainder_=anchor_=expires_=accumulatedTicks_=0;correctionPpb_=0;running_=false;legacy_=true;return true;
}
bool Clock::start(uint64_t qpc) noexcept {
    if(!generation_)return false;
    if(running_)return qpc>=anchor_;
    anchor_=qpc;running_=true;return true;
}
bool Clock::elapsed(uint64_t qpc,uint64_t& ticks) const noexcept {
    ticks=accumulatedTicks_;
    if(running_){if(qpc<anchor_ || qpc-anchor_>UINT64_MAX-ticks)return false;ticks+=qpc-anchor_;}
    return generation_!=0;
}
bool Clock::evaluate(uint64_t qpc,uint64_t& frames,uint64_t& remainder) const noexcept {
    if(!generation_ || frequency_>UINT64_MAX/Scale)return false;
    uint64_t f=frames_,r=remainder_;
    if(legacy_){uint64_t ticks=0;if(!elapsed(qpc,ticks))return false;f=r=0;
        if(!integrate(ticks,rate_,frequency_,0,f,r))return false;
    }else if(running_){
        if(qpc<anchor_)return false;
        uint64_t split=anchor_;
        if(expires_>anchor_){split=qpc<expires_?qpc:expires_;
            if(!integrate(split-anchor_,rate_,frequency_,correctionPpb_,f,r))return false;}
        if(!integrate(qpc-split,rate_,frequency_,0,f,r))return false;
    }
    frames=f;remainder=r;return true;
}
bool Clock::pause(uint64_t qpc) noexcept {
    if(!generation_)return false;
    if(!running_)return true;
    if(legacy_){uint64_t ticks=0,p=0;if(!elapsed(qpc,ticks)||!multiplyDivide(ticks,rate_,frequency_,p))return false;accumulatedTicks_=ticks;}
    else{uint64_t f=0,r=0;if(!evaluate(qpc,f,r))return false;frames_=f;remainder_=r;}
    anchor_=qpc;running_=false;correctionPpb_=0;expires_=0;return true;
}
bool Clock::position(uint64_t qpc,uint64_t& frames) const noexcept {
    if(legacy_){uint64_t ticks=0;return elapsed(qpc,ticks)&&multiplyDivide(ticks,rate_,frequency_,frames);}
    uint64_t remainder=0;return evaluate(qpc,frames,remainder);
}
bool Clock::discipline(uint64_t qpc,int32_t ppb,uint64_t validUntilQpc) noexcept {
    if(!running_ || ppb<-MaxCorrectionPpb || ppb>MaxCorrectionPpb ||
       (validUntilQpc && validUntilQpc<=qpc) || (ppb && !validUntilQpc))return false;
    uint64_t f=0,r=0;if(!evaluate(qpc,f,r))return false;
    frames_=f;remainder_=r;anchor_=qpc;correctionPpb_=ppb;expires_=validUntilQpc;legacy_=false;return true;
}
}
