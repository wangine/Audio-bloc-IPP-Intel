#include "WaveRtTimeline.h"
namespace entropy::music {
bool WaveRtTimeline::prepare(const PcmFormat& format,uint64_t generation,uint64_t frequency,uint8_t* dma,uint32_t dmaFrames,uint32_t notifications,PcmRing& pending,PcmRing& ready) noexcept {
    if(prepared_ || reading_ || !dma || !dmaFrames || dmaFrames>RingFrames || notifications>2 ||
        (notifications && (dmaFrames%notifications || dmaFrames/notifications>MaxPacketFrames)) ||
        !timeline_.prepare(format,generation,frequency,pending,ready))return false;
    if(!clock_.reset(format.sampleRate,frequency,generation))return false;
    format_=format;generation_=generation;frequency_=frequency;dma_=dma;dmaFrames_=dmaFrames;notifications_=notifications;
    packetFrames_=notifications?dmaFrames/notifications:0;owner_=notifications?BufferOwner::Packet:BufferOwner::Position;
    ready_=&ready;stopping_=false;prepared_=true;writtenPackets_=positionCaptured_=nominalFrames_=sequence_=0;return true;
}
bool WaveRtTimeline::start(uint64_t qpc) noexcept {return prepared_ && !stopping_ && clock_.start(qpc);}
bool WaveRtTimeline::pause(uint64_t qpc) noexcept {
    if(!prepared_ || stopping_ || !advance(qpc,owner_==BufferOwner::Position))return false;
    return clock_.pause(qpc);
}
bool WaveRtTimeline::advance(uint64_t qpc,bool exposePosition) noexcept {
    if(!prepared_ || stopping_)return false;
    uint64_t frames=0;
    if(!clock_.position(qpc,frames) || frames<nominalFrames_)return false;
    if(owner_==BufferOwner::Position && exposePosition && clock_.running()) {
        // The OS can only reuse data after this function has snapshotted it
        // and its caller returns the new DMA offset. No position register.
        // An unseen complete lap has ambiguous chronology and is refused.
        if(frames-positionCaptured_>=dmaFrames_)return false;
        while(positionCaptured_<frames) {
            const uint32_t offset=static_cast<uint32_t>(positionCaptured_%dmaFrames_);
            uint64_t chunk=frames-positionCaptured_;
            if(chunk>dmaFrames_-offset)chunk=dmaFrames_-offset;
            if(chunk>MaxPacketFrames)chunk=MaxPacketFrames;
            PacketDescriptor d{generation_,sequence_+1,positionCaptured_,qpc,frequency_,0,static_cast<uint32_t>(chunk),RawIdentity,0,0};
            const auto result=timeline_.commit(d,dma_+offset*format_.blockAlign,d.frames*format_.blockAlign);
            if(result!=CommitResult::Accepted && result!=CommitResult::Overrun)return false;
            ++sequence_;positionCaptured_+=chunk;
        }
        if(!timeline_.advance(frames))return false;
    } else if(owner_==BufferOwner::Packet && clock_.running()) {
        if(!timeline_.advance(frames))return false;
    }
    nominalFrames_=frames;return true;
}
CommitResult WaveRtTimeline::capturePacket(uint32_t packetNumber,bool eos,uint32_t eosBytes,uint64_t qpc) noexcept {
    if(!prepared_ || stopping_)return CommitResult::Stopped;
    if(owner_!=BufferOwner::Packet || timeline_.eos())return CommitResult::Invalid;
    const uint32_t expected=static_cast<uint32_t>(writtenPackets_);
    if(packetNumber!=expected) {
        return static_cast<int32_t>(packetNumber-expected)<0?CommitResult::Duplicate:CommitResult::Invalid;
    }
    if(!advance(qpc,false))return CommitResult::Invalid;
    const uint64_t current=nominalFrames_/packetFrames_;
    if(writtenPackets_>=current+notifications_)return CommitResult::Overrun;
    if(writtenPackets_>UINT64_MAX/packetFrames_ || sequence_==UINT64_MAX)return CommitResult::Invalid;
    const uint32_t frames=eos?eosBytes/format_.blockAlign:packetFrames_;
    if((eos && (eosBytes%format_.blockAlign || frames>packetFrames_)) || (!eos && eosBytes))return CommitResult::Invalid;
    const uint32_t offset=static_cast<uint32_t>(writtenPackets_%notifications_)*packetFrames_*format_.blockAlign;
    PacketDescriptor d{generation_,sequence_+1,writtenPackets_*packetFrames_,qpc,frequency_,0,frames,RawIdentity|(eos?EndOfStream:0),0,0};
    const auto result=timeline_.commit(d,frames?dma_+offset:nullptr,frames*format_.blockAlign);
    if(result==CommitResult::Accepted || result==CommitResult::Late || result==CommitResult::Overrun){++writtenPackets_;++sequence_;}
    return result;
}
bool WaveRtTimeline::requestStop(uint64_t qpc) noexcept {
    if(!prepared_)return true;
    if(!stopping_){clock_.pause(qpc);stopping_=true;}
    if(reading_)return false;
    timeline_.stop();prepared_=false;nominalFrames_=0;return true;
}
bool WaveRtTimeline::beginRead(PacketView& view) noexcept {
    if(!prepared_ || stopping_ || reading_ || !ready_->peek(view))return false;
    reading_=true;return true;
}
bool WaveRtTimeline::finishRead(uint64_t sequence,bool consume) noexcept {
    if(!reading_)return false;
    PacketView view{};
    if(!ready_->peek(view) || view.packet.sequence!=sequence)return false;
    if(consume)ready_->consume(sequence);
    reading_=false;
    if(stopping_){timeline_.stop();prepared_=false;nominalFrames_=0;}
    return true;
}
}
