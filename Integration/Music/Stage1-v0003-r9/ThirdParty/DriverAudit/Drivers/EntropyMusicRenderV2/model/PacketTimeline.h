#pragma once
#include "PcmRing.h"
namespace entropy::music {
enum class CommitResult {Accepted,Duplicate,Late,Overrun,Invalid,Stopped};
struct Totals {
    uint64_t committedFrames=0, publishedFrames=0, lostFrames=0, cancelledFrames=0;
    uint64_t committedPackets=0, publishedPackets=0;
};
class PacketTimeline {
    PcmFormat format_{};
    PcmRing* pending_=nullptr;
    PcmRing* ready_=nullptr;
    uint64_t generation_=0, frequency_=0, nextFrame_=0, lastSequence_=0, transferred_=0, lastPublishedEnd_=0;
    uint64_t producerLoss_=0,publishLoss_=0,uncommitted_=0;
    bool active_=false, eos_=false;
    Totals generationTotals_{}, terminatedTotals_{}, lifetimeTotals_{};
    CommitResult commitView(const PacketView&,bool retained,bool dropped) noexcept;
public:
    bool prepare(const PcmFormat&,uint64_t generation,uint64_t frequency,PcmRing& pending,PcmRing& ready) noexcept;
    CommitResult commit(const PacketDescriptor&,const uint8_t*,uint32_t bytes) noexcept;
    CommitResult commitSnapshot(const PacketView& view,bool dropped) noexcept{return commitView(view,true,dropped);}
    bool advance(uint64_t sourceFrame) noexcept;
    void stop() noexcept;
    uint64_t transferred() const noexcept {return transferred_;}
    uint64_t nextFrame() const noexcept {return nextFrame_;}
    uint64_t uncommittedFrames() const noexcept{return uncommitted_;}
    bool eos() const noexcept {return eos_;}
    Totals totals() const noexcept {return generationTotals_;}
    Totals terminatedTotals() const noexcept {return terminatedTotals_;}
    Totals lifetimeTotals() const noexcept {return lifetimeTotals_;}
};
class PacketNumber {
    bool initialized_=false;
    uint32_t last_=0;
    uint64_t extended_=0;
public:
    bool extend(uint32_t osNumber,uint64_t& number) noexcept;
    void reset() noexcept {initialized_=false;last_=0;extended_=0;}
};
}
