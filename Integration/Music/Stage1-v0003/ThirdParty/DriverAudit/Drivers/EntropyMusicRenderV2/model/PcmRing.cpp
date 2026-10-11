#include "PcmRing.h"
#include <string.h>
namespace entropy::music {
bool PcmRing::init(uint8_t* data,uint32_t frames,uint32_t align,RingSlot* slots,uint32_t descriptors) noexcept {
    if(!data || !slots || !frames || frames>RingFrames || !descriptors || descriptors>RingDescriptors || (align!=4 && align!=8))return false;
    data_=data;slots_=slots;frameCapacity_=frames;descriptorCapacity_=descriptors;align_=align;clear();return true;
}
void PcmRing::clear() noexcept {head_=0;count_=0;writeFrame_=0;usedFrames_=0;}
bool PcmRing::push(const PacketDescriptor& d,const uint8_t* input,uint32_t bytes) noexcept {
    return push(PacketView{d,input,nullptr,bytes,0});
}
bool PcmRing::push(const PacketView& view) noexcept {
    const auto& d=view.packet;
    if(!data_ || !d.sequence || !d.generation || !d.frames || d.frames>MaxPacketFrames || d.frames>frameCapacity_-usedFrames_ || count_==descriptorCapacity_ ||
        view.firstBytes>d.frames*align_ || view.secondBytes!=d.frames*align_-view.firstBytes ||
        (view.firstBytes && !view.first) || (view.secondBytes && !view.second))return false;
    if(count_ && slots_[(head_+count_-1)%descriptorCapacity_].packet.sequence>=d.sequence)return false;
    uint32_t offset=writeFrame_*align_;
    const uint32_t capacity=frameCapacity_*align_;
    const uint8_t* sources[]={view.first,view.second};
    const uint32_t sizes[]={view.firstBytes,view.secondBytes};
    for(unsigned i=0;i<2;++i) {
        const uint32_t first=sizes[i]<capacity-offset?sizes[i]:capacity-offset;
        if(first)memcpy(data_+offset,sources[i],first);
        if(sizes[i]>first)memcpy(data_,sources[i]+first,sizes[i]-first);
        offset=(offset+sizes[i])%capacity;
    }
    slots_[(head_+count_)%descriptorCapacity_]={d,writeFrame_,0};
    writeFrame_=(writeFrame_+d.frames)%frameCapacity_;usedFrames_+=d.frames;++count_;return true;
}
bool PcmRing::peek(PacketView& view) const noexcept {
    if(!count_)return false;
    const auto& slot=slots_[head_];
    const auto firstFrames=slot.packet.frames<frameCapacity_-slot.startFrame?slot.packet.frames:frameCapacity_-slot.startFrame;
    view={slot.packet,data_+slot.startFrame*align_,data_,firstFrames*align_,(slot.packet.frames-firstFrames)*align_};return true;
}
bool PcmRing::copyFront(void* output,uint32_t capacity,uint32_t& copied) const noexcept {
    copied=0;PacketView view{};
    if(!output || !peek(view) || capacity<view.firstBytes+view.secondBytes)return false;
    memcpy(output,view.first,view.firstBytes);
    if(view.secondBytes)memcpy(static_cast<uint8_t*>(output)+view.firstBytes,view.second,view.secondBytes);
    copied=view.firstBytes+view.secondBytes;return true;
}
bool PcmRing::consume(uint64_t sequence) noexcept {
    if(!count_ || slots_[head_].packet.sequence!=sequence)return false;
    usedFrames_-=slots_[head_].packet.frames;head_=(head_+1)%descriptorCapacity_;--count_;return true;
}
}
