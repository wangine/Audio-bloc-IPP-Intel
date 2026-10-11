#pragma once
#include "../model/PacketTimeline.h"
namespace entropy::music {
// Sole SetWritePacket producer. Its state is independent of publication/read.
class PacketAdmission {
    uint64_t next_=0,sequence_=0,generation_=0,frequency_=0;
    uint32_t frames_=0,notifications_=0,align_=0;
    bool eos_=false;
public:
    void reset(uint64_t generation,uint64_t frequency,uint32_t frames,uint32_t notifications,uint32_t align) noexcept {
        next_=sequence_=0;generation_=generation;frequency_=frequency;frames_=frames;notifications_=notifications;align_=align;eos_=false;
    }
    CommitResult acceptWindowsPacket(uint32_t number,bool eos,uint32_t bytes,uint64_t source,uint64_t qpc,PacketDescriptor& d,bool running) noexcept {
        // Observed WASAPI prefill: ReleaseBuffer in initial PAUSE reports the
        // write cursor 1 after filling DMA half 0. RUN then reports packet 1.
        // Bind only that initial two-half, non-EOS callback to packet 0;
        // all subsequent callbacks retain strict packet admission semantics.
        const auto physical=(!running && source==0 && notifications_==2 && !sequence_ && number==1 && !eos)?0:number;
        return accept(physical,eos,bytes,source,qpc,d,running);
    }
    CommitResult accept(uint32_t number,bool eos,uint32_t bytes,uint64_t source,uint64_t qpc,PacketDescriptor& d,bool running=true) noexcept {
        if(!frames_ || !notifications_ || eos_)return CommitResult::Invalid;
        const uint32_t delta=number-static_cast<uint32_t>(next_);
        if(delta>0x7fffffffU)return CommitResult::Duplicate;
        if(next_>UINT64_MAX-delta)return CommitResult::Invalid;
        const auto index=next_+delta,current=source/frames_;
        if(!running && index!=current)return CommitResult::Overrun;
        if(index>=current+notifications_)return CommitResult::Overrun;
        const auto count=eos?bytes/align_:frames_;
        // EosPacketLength is undefined/ignored without the EOS flag (PortCls).
        if((eos && (bytes%align_ || count>frames_)) || index>UINT64_MAX/frames_ || sequence_==UINT64_MAX || index==UINT64_MAX)return CommitResult::Invalid;
        const auto first=index*frames_;if(first>UINT64_MAX-count)return CommitResult::Invalid;
        d={generation_,++sequence_,first,qpc,frequency_,0,count,RawIdentity|(eos?EndOfStream:0),0,0};
        next_=index+1;eos_=eos;
        return count && first+count<=source?CommitResult::Late:CommitResult::Accepted;
    }
};
}
