#pragma once
#include "BridgeClient.h"
#include <atomic>
#include <vector>
#include <cstring>
namespace entropy::music::provider {
struct AuditRecord {EntropyIngressPacketV2 packet{};std::vector<uint8_t> bytes;};
// One serialized Provider read producer and one audit worker consumer. Storage
// is allocated once at open. Failed observation never alters returned PCM.
class AuditQueue {
    std::vector<AuditRecord> slots_;
    uint32_t bytes_;
    std::atomic<uint64_t> write_{0},read_{0},overflow_{0};
public:
    AuditQueue(uint32_t count,uint32_t bytes):slots_(count),bytes_(bytes){for(auto& s:slots_)s.bytes.resize(bytes);}
    bool push(const EntropyIngressPacketV2& p,const void* bytes) noexcept {
        const auto w=write_.load(std::memory_order_relaxed);
        if(!bytes || p.payloadBytes>bytes_ || slots_.empty() || w-read_.load(std::memory_order_acquire)>=slots_.size()){overflow_.fetch_add(1);return false;}
        auto& slot=slots_[w%slots_.size()];slot.packet=p;std::memcpy(slot.bytes.data(),bytes,p.payloadBytes);write_.store(w+1,std::memory_order_release);return true;
    }
    bool pop(AuditRecord& out){const auto r=read_.load(std::memory_order_relaxed);if(r==write_.load(std::memory_order_acquire))return false;const auto& s=slots_[r%slots_.size()];out.packet=s.packet;out.bytes.assign(s.bytes.begin(),s.bytes.begin()+s.packet.payloadBytes);read_.store(r+1,std::memory_order_release);return true;}
    uint64_t overflow() const noexcept{return overflow_.load();}
};
}
