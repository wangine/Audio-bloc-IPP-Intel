#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace entropy::egress_host {
struct GraphInterval {uint64_t epoch=0,firstGraphFrame=0,capture100ns=0;unsigned frames=0;};
// Exactly one producer and one consumer. Prepare/reset require both stopped.
class SpscAudioFifo final {
public:
    bool prepare(unsigned channels,unsigned capacity) {
        if(!channels || channels>16 || !capacity || (capacity&(capacity-1)) || capacity>(1u<<20)) return false;
        storage_.assign(static_cast<size_t>(channels)*capacity,0.0);
        identity_.resize(capacity);channels_=channels; capacity_=capacity; resetQuiescent(); return true;
    }
    void resetQuiescent() noexcept {write_=0;read_=0;overflow_=0;underflow_=0;}
    bool push(const double* const* src,unsigned frames,GraphInterval interval={}) noexcept {
        if(!interval.frames){interval.frames=frames;interval.firstGraphFrame=write_.load(std::memory_order_relaxed);}
        if(interval.frames!=frames)return false;
        if(!src || !frames || frames>capacity_) return false;
        for(unsigned c=0;c<channels_;++c) {
            if(!src[c]) return false;
            for(unsigned f=0;f<frames;++f) if(!std::isfinite(src[c][f])) return false;
        }
        const auto w=write_.load(std::memory_order_relaxed),r=read_.load(std::memory_order_acquire);
        if(frames>capacity_-(w-r)){overflow_.fetch_add(1,std::memory_order_relaxed);return false;}
        for(unsigned c=0;c<channels_;++c) for(unsigned f=0;f<frames;++f)
            storage_[static_cast<size_t>(c)*capacity_+((w+f)&(capacity_-1))]=src[c][f];
        for(unsigned f=0;f<frames;++f) identity_[(w+f)&(capacity_-1)]={interval.epoch,interval.firstGraphFrame+f,interval.capture100ns,1};
        write_.store(w+frames,std::memory_order_release); return true;
    }
    bool pop(double* const* dst,unsigned frames,GraphInterval* interval=nullptr,GraphInterval* metadata=nullptr) noexcept {
        if(!dst || !frames || frames>capacity_) return false;
        for(unsigned c=0;c<channels_;++c) if(!dst[c]) return false;
        const auto r=read_.load(std::memory_order_relaxed),w=write_.load(std::memory_order_acquire);
        if(frames>w-r){underflow_.fetch_add(1,std::memory_order_relaxed);return false;}
        const auto begin=identity_[r&(capacity_-1)];
        for(unsigned f=0;f<frames;++f){const auto& v=identity_[(r+f)&(capacity_-1)];
            if(v.epoch!=begin.epoch||v.firstGraphFrame!=begin.firstGraphFrame+f)return false;}
        if(interval){*interval=begin;interval->frames=frames;}
        if(metadata)for(unsigned f=0;f<frames;++f)metadata[f]=identity_[(r+f)&(capacity_-1)];
        for(unsigned c=0;c<channels_;++c) for(unsigned f=0;f<frames;++f)
            dst[c][f]=storage_[static_cast<size_t>(c)*capacity_+((r+f)&(capacity_-1))];
        read_.store(r+frames,std::memory_order_release); return true;
    }
    // Consumer owner: read stays fixed during this acquire snapshot.
    unsigned readable() const noexcept {auto r=read_.load(std::memory_order_relaxed);const auto w=write_.load(std::memory_order_acquire);return w>=r?static_cast<unsigned>((std::min)(w-r,uint64_t(capacity_))):0;}
    // Producer owner: write stays fixed during this acquire snapshot.
    unsigned writable() const noexcept {auto w=write_.load(std::memory_order_relaxed);return capacity_-static_cast<unsigned>(w-read_.load(std::memory_order_acquire));}
    uint64_t overflows() const noexcept {return overflow_.load();}
    uint64_t underflows() const noexcept {return underflow_.load();}
private:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    std::vector<GraphInterval> identity_;
    std::vector<double> storage_; unsigned channels_=0,capacity_=0;
    alignas(64) std::atomic<uint64_t> write_{0};
    alignas(64) std::atomic<uint64_t> read_{0};
    std::atomic<uint64_t> overflow_{0},underflow_{0};
};
}
