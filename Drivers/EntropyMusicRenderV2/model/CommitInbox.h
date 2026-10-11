#pragma once
#include "PcmRing.h"
#include <string.h>
#ifndef ENTROPY_KERNEL
#include <atomic>
#endif
namespace entropy::music {
class AtomicCount {
#ifdef ENTROPY_KERNEL
    mutable volatile LONG64 value_=0;
#else
    std::atomic<uint64_t> value_{0};
#endif
public:
    uint64_t get() const noexcept {
#ifdef ENTROPY_KERNEL
        return static_cast<uint64_t>(InterlockedCompareExchange64(&value_,0,0));
#else
        return value_.load(std::memory_order_acquire);
#endif
    }
    void set(uint64_t v) noexcept {
#ifdef ENTROPY_KERNEL
        InterlockedExchange64(&value_,static_cast<LONG64>(v));
#else
        value_.store(v,std::memory_order_release);
#endif
    }
};
struct InboxSlot {PacketDescriptor packet{};uint64_t dataStart=0;uint32_t dropped=0,reserved=0;};
struct InboxView {PacketView pcm{};bool dropped=false;};
enum class InboxResult {Stored,RecordedLoss,Full,Invalid};
// Separate sole OS commit producer and sole serialized publication consumer.
// No publication mutex, allocation or retry on push; payload storage is bounded
// by accepted DMA size. If payload is full, retain a chronological loss marker.
class CommitInbox {
    uint8_t* data_=nullptr;InboxSlot* slots_=nullptr;
    uint32_t frames_=0,align_=0,records_=0;
    uint64_t writeData_=0,writeRecord_=0,writeSource_=0,writeLost_=0,readSource_=0,readLost_=0;
    AtomicCount published_,consumed_,consumedData_,source_,lost_,usedSource_,usedLost_;
public:
    bool init(uint8_t* data,uint32_t frames,uint32_t align,InboxSlot* slots,uint32_t records) noexcept {
        if(!data || !slots || !frames || frames>RingFrames || !records || records>RingDescriptors || (align!=4 && align!=8))return false;
        data_=data;frames_=frames;align_=align;slots_=slots;records_=records;writeData_=writeRecord_=writeSource_=writeLost_=readSource_=readLost_=0;
        published_.set(0);consumed_.set(0);consumedData_.set(0);source_.set(0);lost_.set(0);usedSource_.set(0);usedLost_.set(0);return true;
    }
    InboxResult push(const PacketDescriptor& d,const uint8_t* bytes,bool forceLoss=false) noexcept {
        if(!data_ || !d.generation || !d.sequence || d.frames>MaxPacketFrames || (!d.frames && !(d.flags&EndOfStream)) || (d.frames && !bytes && !forceLoss) || writeSource_>UINT64_MAX-d.frames)return InboxResult::Invalid;
        if(writeRecord_-consumed_.get()>=records_ || writeRecord_==UINT64_MAX)return InboxResult::Full;
        const auto used=writeData_-consumedData_.get();const bool drop=forceLoss || used>frames_ || d.frames>frames_-used;
        auto& slot=slots_[writeRecord_%records_];slot={d,writeData_,drop?1u:0u,0};
        if(!drop && d.frames){const uint32_t offset=static_cast<uint32_t>(writeData_%frames_),first=d.frames<frames_-offset?d.frames:frames_-offset;
            memcpy(data_+offset*align_,bytes,first*align_);if(d.frames>first)memcpy(data_,bytes+first*align_,(d.frames-first)*align_);writeData_+=d.frames;}
        writeSource_+=d.frames;source_.set(writeSource_);if(drop){writeLost_+=d.frames;lost_.set(writeLost_);}
        published_.set(++writeRecord_);return drop?InboxResult::RecordedLoss:InboxResult::Stored;
    }
    bool peek(InboxView& view) const noexcept {
        const auto r=consumed_.get();if(r==published_.get())return false;const auto& slot=slots_[r%records_];view.dropped=slot.dropped!=0;
        if(view.dropped){view.pcm={slot.packet,nullptr,nullptr,0,0};return true;}
        const auto offset=static_cast<uint32_t>(slot.dataStart%frames_),first=slot.packet.frames<frames_-offset?slot.packet.frames:frames_-offset;
        view.pcm={slot.packet,data_+offset*align_,data_,first*align_,(slot.packet.frames-first)*align_};return true;
    }
    bool consume(uint64_t sequence) noexcept {
        const auto r=consumed_.get();if(r==published_.get())return false;const auto& s=slots_[r%records_];if(s.packet.sequence!=sequence)return false;
        readSource_+=s.packet.frames;usedSource_.set(readSource_);if(s.dropped){readLost_+=s.packet.frames;usedLost_.set(readLost_);}else consumedData_.set(s.dataStart+s.packet.frames);
        consumed_.set(r+1);return true;
    }
    uint64_t queuedRecords() const noexcept{const auto r=consumed_.get();return published_.get()-r;}
    uint64_t queuedFrames() const noexcept{const auto r=usedSource_.get();return source_.get()-r;}
    uint64_t queuedLostFrames() const noexcept{const auto r=usedLost_.get();return lost_.get()-r;}
};
}
