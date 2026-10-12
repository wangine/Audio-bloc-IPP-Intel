#include "Clock.h"
namespace entropy::music {
bool multiplyDivide(uint64_t ticks,uint32_t rate,uint64_t frequency,uint64_t& frames) noexcept {
    if(!frequency || !rate) return false;
    const auto whole=ticks/frequency;
    if(whole>UINT64_MAX/rate) return false;
    const auto base=whole*rate, fraction=ticks%frequency;
    // Compute floor(fraction*rate/frequency), retaining a remainder without
    // ever overflowing 64 bits. Fixed 32 iterations, no FP or runtime division
    // of a fabricated 128-bit value; works unchanged in kernel and user mode.
    uint64_t quotient=0,remainder=0;
    for(int bit=31;bit>=0;--bit) {
        quotient*=2;
        if(remainder>=frequency-remainder) {remainder-=frequency-remainder;++quotient;}
        else remainder*=2;
        if(rate&(1u<<bit)) {
            if(remainder>=frequency-fraction) {remainder-=frequency-fraction;++quotient;}
            else remainder+=fraction;
        }
    }
    if(quotient>UINT64_MAX-base) return false;
    frames=base+quotient;return true;
}
bool Clock::reset(uint32_t rate,uint64_t frequency,uint64_t generation) noexcept {
    if(!frequency || !generation || !(rate==44100 || rate==48000 || rate==88200 || rate==96000 || rate==176400 || rate==192000))return false;
    rate_=rate;frequency_=frequency;generation_=generation;accumulated_=0;started_=0;running_=false;return true;
}
bool Clock::start(uint64_t qpc) noexcept {
    if(!generation_)return false;
    if(running_)return qpc>=started_;
    started_=qpc;running_=true;return true;
}
bool Clock::pause(uint64_t qpc) noexcept {
    if(!generation_)return false;
    if(!running_)return true;
    if(qpc<started_ || qpc-started_>UINT64_MAX-accumulated_)return false;
    uint64_t frames=0;
    if(!position(qpc,frames))return false;
    accumulated_+=qpc-started_;running_=false;return true;
}
bool Clock::position(uint64_t qpc,uint64_t& frames) const noexcept {
    uint64_t ticks=accumulated_;
    if(running_) {
        if(qpc<started_ || qpc-started_>UINT64_MAX-ticks)return false;
        ticks+=qpc-started_;
    }
    return multiplyDivide(ticks,rate_,frequency_,frames);
}
}
