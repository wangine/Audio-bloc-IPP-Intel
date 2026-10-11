#pragma once
#include "PacketTimeline.h"
namespace entropy::music {
// Device lifetime evidence survives stream allocation/free and format changes.
// Caller serializes metadata only; no PCM bytes or blocking work in the lock.
class DriverLedger {
    Totals lifetime_{},ended_{};uint64_t generation_=0;
    static uint64_t add(uint64_t a,uint64_t b) noexcept{return a>UINT64_MAX-b?UINT64_MAX:a+b;}
public:
    bool finish(uint64_t generation,const Totals& t) noexcept {
        if(!generation || generation<=generation_)return false;
        lifetime_.committedFrames=add(lifetime_.committedFrames,t.committedFrames);lifetime_.publishedFrames=add(lifetime_.publishedFrames,t.publishedFrames);
        lifetime_.lostFrames=add(lifetime_.lostFrames,t.lostFrames);lifetime_.cancelledFrames=add(lifetime_.cancelledFrames,t.cancelledFrames);
        lifetime_.committedPackets=add(lifetime_.committedPackets,t.committedPackets);lifetime_.publishedPackets=add(lifetime_.publishedPackets,t.publishedPackets);
        ended_=t;generation_=generation;return true;
    }
    uint64_t generation() const noexcept{return generation_;}
    Totals ended() const noexcept{return ended_;}
    Totals lifetime() const noexcept{return lifetime_;}
};
}
