#include "PacketTimeline.h"
namespace entropy::music {
bool PacketTimeline::prepare(const PcmFormat& format,uint64_t generation,uint64_t frequency,PcmRing& pending,PcmRing& ready) noexcept {
    if(active_ || !validateFormat(format) || !frequency || !generation || generation<=generation_ ||
        &pending==&ready || pending.align()!=format.blockAlign || ready.align()!=format.blockAlign)return false;
    pending.clear();ready.clear();format_=format;generation_=generation;frequency_=frequency;
    pending_=&pending;ready_=&ready;nextFrame_=lastSequence_=transferred_=lastPublishedEnd_=0;
    producerLoss_=publishLoss_=uncommitted_=0;
    eos_=false;active_=true;generationTotals_={};return true;
}
CommitResult PacketTimeline::commit(const PacketDescriptor& d,const uint8_t* input,uint32_t bytes) noexcept {
    return commitView(PacketView{d,input,nullptr,bytes,0},false,false);
}
CommitResult PacketTimeline::commitView(const PacketView& view,bool retained,bool dropped) noexcept {
    const auto& d=view.packet;
    if(!active_)return CommitResult::Stopped;
    if(!d.sequence || d.generation!=generation_)return CommitResult::Invalid;
    if(d.sequence<=lastSequence_)return CommitResult::Duplicate;
    if(eos_ || d.reserved || d.lostFrames || d.qpcFrequency!=frequency_ || d.mode>2 || d.firstSourceFrame<nextFrame_ ||
        d.firstSourceFrame>UINT64_MAX-d.frames || d.flags&~(RawIdentity|EndOfStream) || !(d.flags&RawIdentity))return CommitResult::Invalid;
    const bool control=!d.frames && (d.flags&EndOfStream) && !view.firstBytes && !view.secondBytes;
    uint32_t expected=0;
    if(!control && (!payloadSize(format_,d.frames,expected) ||
        (!dropped && (view.firstBytes>expected || view.secondBytes!=expected-view.firstBytes ||
          (view.firstBytes && !view.first) || (view.secondBytes && !view.second)))))return CommitResult::Invalid;
    if(lifetimeTotals_.committedFrames>UINT64_MAX-d.frames || lifetimeTotals_.committedPackets==UINT64_MAX)return CommitResult::Invalid;
    const auto gap=d.firstSourceFrame-nextFrame_;uncommitted_+=gap;
    lastSequence_=d.sequence;nextFrame_=d.firstSourceFrame+d.frames;eos_=(d.flags&EndOfStream)!=0;
    generationTotals_.committedFrames+=d.frames;lifetimeTotals_.committedFrames+=d.frames;
    ++generationTotals_.committedPackets;++lifetimeTotals_.committedPackets;
    if(control)return CommitResult::Accepted;
    CommitResult result=CommitResult::Accepted;
    PacketDescriptor queued=d;queued.lostFrames=producerLoss_;if(gap)queued.flags|=Discontinuity;
    // A callback just after the start of the next packet is normal, especially
    // with one notification. Only a completely elapsed packet is already late.
    PacketView stored=view;stored.packet=queued;
    if(dropped)result=CommitResult::Overrun;
    else if(!retained && d.firstSourceFrame+d.frames<=transferred_)result=CommitResult::Late;
    else if(!pending_->push(stored))result=CommitResult::Overrun;
    if(result!=CommitResult::Accepted){generationTotals_.lostFrames+=d.frames;lifetimeTotals_.lostFrames+=d.frames;producerLoss_+=d.frames;}
    return result;
}
bool PacketTimeline::advance(uint64_t sourceFrame) noexcept {
    if(!active_ || sourceFrame<transferred_)return false;
    transferred_=sourceFrame;PacketView view{};
    while(pending_->peek(view)) {
        const auto end=view.packet.firstSourceFrame+view.packet.frames;
        if(end>sourceFrame)break;
        // Loss belongs to the next chronological packet after the gap, never
        // to an older queued packet merely because a future commit overflowed.
        view.packet.lostFrames+=publishLoss_;
        if(view.packet.firstSourceFrame!=lastPublishedEnd_)view.packet.flags|=Discontinuity;
        if(ready_->push(view)) {
            generationTotals_.publishedFrames+=view.packet.frames;lifetimeTotals_.publishedFrames+=view.packet.frames;
            ++generationTotals_.publishedPackets;++lifetimeTotals_.publishedPackets;lastPublishedEnd_=end;
        } else {
            generationTotals_.lostFrames+=view.packet.frames;lifetimeTotals_.lostFrames+=view.packet.frames;
            publishLoss_+=view.packet.frames;
        }
        pending_->consume(view.packet.sequence);
    }
    return true;
}
void PacketTimeline::stop() noexcept {
    if(!active_)return;
    const uint64_t cancelled=static_cast<uint64_t>(pending_->frames())+ready_->frames();
    generationTotals_.cancelledFrames+=cancelled;lifetimeTotals_.cancelledFrames+=cancelled;
    terminatedTotals_=generationTotals_;pending_->clear();ready_->clear();active_=false;
}
bool PacketNumber::extend(uint32_t osNumber,uint64_t& number) noexcept {
    if(!initialized_){initialized_=true;last_=osNumber;extended_=osNumber;number=extended_;return true;}
    const uint32_t delta=osNumber-last_;
    if(!delta || delta>0x7fffffffu || extended_>UINT64_MAX-delta)return false;
    extended_+=delta;last_=osNumber;number=extended_;return true;
}
}
