#pragma once
#include "Format.h"
namespace entropy::music {
constexpr uint32_t Discontinuity=1, RawIdentity=4, EndOfStream=0x100;
struct PacketDescriptor {
    uint64_t generation, sequence, firstSourceFrame, qpcTimestamp, qpcFrequency, lostFrames;
    uint32_t frames, flags, mode, reserved;
};
static_assert(sizeof(PacketDescriptor)==64);
struct RingSlot {PacketDescriptor packet; uint32_t startFrame, reserved;};
struct PacketView {
    PacketDescriptor packet;
    const uint8_t* first;
    const uint8_t* second;
    uint32_t firstBytes, secondBytes;
};
// Externally supplied storage, serialized by the owner. Peek remains valid until
// consume/clear; producers never overwrite it. Teardown must first end reads.
class PcmRing {
    uint8_t* data_=nullptr;
    RingSlot* slots_=nullptr;
    uint32_t frameCapacity_=0, descriptorCapacity_=0, align_=0;
    uint32_t head_=0, count_=0, writeFrame_=0, usedFrames_=0;
public:
    bool init(uint8_t*,uint32_t frames,uint32_t align,RingSlot*,uint32_t descriptors) noexcept;
    bool push(const PacketDescriptor&,const uint8_t*,uint32_t bytes) noexcept;
    bool push(const PacketView&) noexcept;
    bool peek(PacketView&) const noexcept;
    bool copyFront(void*,uint32_t capacity,uint32_t& copied) const noexcept;
    bool consume(uint64_t sequence) noexcept;
    void clear() noexcept;
    uint32_t frames() const noexcept {return usedFrames_;}
    uint32_t count() const noexcept {return count_;}
    uint32_t align() const noexcept {return align_;}
};
}
