#pragma once
#include "Types.h"
namespace entropy::music {
// QPC ticks -> source frames. Caller serializes mutations and pairs every query
// timestamp with the same immutable Clock snapshot. No PCM or floating point.
class Clock {
    uint64_t frequency_=0,generation_=0,frames_=0,remainder_=0,anchor_=0,expires_=0,accumulatedTicks_=0;
    uint32_t rate_=0;
    int32_t correctionPpb_=0;
    bool running_=false,legacy_=true;
    bool elapsed(uint64_t qpc,uint64_t& ticks) const noexcept;
    bool evaluate(uint64_t qpc,uint64_t& frames,uint64_t& remainder) const noexcept;
public:
    static constexpr int32_t MaxCorrectionPpb=1000000; // +/-1000 ppm
    bool reset(uint32_t rate,uint64_t frequency,uint64_t generation) noexcept;
    bool start(uint64_t qpc) noexcept;
    bool pause(uint64_t qpc) noexcept;
    bool position(uint64_t qpc,uint64_t& frames) const noexcept;
    // Rebase continuous phase, retaining the exact fractional remainder. Expiry
    // changes only the future slope to nominal. Fine control supports QPC
    // frequencies <= UINT64_MAX/1e9; autonomous operation retains the old range.
    bool discipline(uint64_t qpc,int32_t correctionPpb,uint64_t validUntilQpc) noexcept;
    int32_t correctionPpb(uint64_t qpc) const noexcept {
        return running_ && expires_ && qpc<expires_ ? correctionPpb_ : 0;
    }
    uint64_t generation() const noexcept {return generation_;}
    uint64_t frequency() const noexcept {return frequency_;}
    uint32_t nominalRate() const noexcept {return rate_;}
    bool running() const noexcept {return running_;}
};
bool multiplyDivide(uint64_t ticks,uint32_t rate,uint64_t frequency,uint64_t& frames) noexcept;
}
