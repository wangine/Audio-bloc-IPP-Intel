#pragma once
#include "Types.h"
namespace entropy::music {
// All times are QPC ticks. The caller serializes lifecycle changes and queries.
class Clock {
    uint64_t frequency_=0, generation_=0, accumulated_=0, started_=0;
    uint32_t rate_=0;
    bool running_=false;
public:
    bool reset(uint32_t rate,uint64_t frequency,uint64_t generation) noexcept;
    bool start(uint64_t qpc) noexcept;
    bool pause(uint64_t qpc) noexcept;
    bool position(uint64_t qpc,uint64_t& frames) const noexcept;
    uint64_t generation() const noexcept {return generation_;}
    bool running() const noexcept {return running_;}
};
bool multiplyDivide(uint64_t ticks,uint32_t rate,uint64_t frequency,uint64_t& frames) noexcept;
}
