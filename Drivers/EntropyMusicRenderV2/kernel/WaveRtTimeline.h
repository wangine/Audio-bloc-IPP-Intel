#pragma once
#include "../model/Clock.h"
#include "../model/PacketTimeline.h"
namespace entropy::music {
enum class BufferOwner:uint32_t {None=0,Packet=1,Position=2};
// OS-independent DDI binding. Kernel callers serialize operations; read leases
// keep an immutable front packet alive while bytes are copied outside the lock.
class WaveRtTimeline {
    Clock clock_;
    PacketTimeline timeline_;
    PcmFormat format_{};
    PcmRing* ready_=nullptr;
    uint8_t* dma_=nullptr;
    uint32_t dmaFrames_=0, notifications_=0, packetFrames_=0;
    uint64_t generation_=0, frequency_=0, writtenPackets_=0, positionCaptured_=0, nominalFrames_=0, sequence_=0;
    bool stopping_=false, prepared_=false, reading_=false;
    BufferOwner owner_=BufferOwner::None;
public:
    bool prepare(const PcmFormat&,uint64_t generation,uint64_t frequency,uint8_t* dma,uint32_t dmaFrames,uint32_t notifications,PcmRing& pending,PcmRing& ready) noexcept;
    bool start(uint64_t qpc) noexcept;
    bool pause(uint64_t qpc) noexcept;
    bool advance(uint64_t qpc,bool exposePosition) noexcept;
    CommitResult capturePacket(uint32_t packetNumber,bool eos,uint32_t eosBytes,uint64_t qpc) noexcept;
    bool requestStop(uint64_t qpc) noexcept;
    bool beginRead(PacketView&) noexcept;
    bool finishRead(uint64_t sequence,bool consume) noexcept;
    bool readActive() const noexcept {return reading_;}
    bool stopping() const noexcept {return stopping_;}
    bool prepared() const noexcept {return prepared_;}
    bool running() const noexcept {return clock_.running();}
    bool eos() const noexcept {return timeline_.eos();}
    BufferOwner owner() const noexcept {return owner_;}
    uint64_t frames() const noexcept {return nominalFrames_;}
    uint64_t packetCount() const noexcept {
        if(!packetFrames_)return 0;
        if(timeline_.eos() && nominalFrames_>=timeline_.nextFrame())
            return timeline_.nextFrame()/packetFrames_+(timeline_.nextFrame()%packetFrames_?1:0);
        return nominalFrames_/packetFrames_;
    }
    uint64_t committedEnd() const noexcept {return timeline_.nextFrame();}
    uint64_t generation() const noexcept {return generation_;}
    uint32_t packetFrames() const noexcept {return packetFrames_;}
    Totals totals() const noexcept {return timeline_.totals();}
    Totals lifetimeTotals() const noexcept {return timeline_.lifetimeTotals();}
    const PcmFormat& format() const noexcept {return format_;}
};
}
