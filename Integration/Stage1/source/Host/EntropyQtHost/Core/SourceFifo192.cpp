#include "SourceFifo192.h"
#include <cstring>
#include <limits>

#include <algorithm>
#include <limits>

namespace entropy::host {

bool SourceFifo192::prepare(std::uint32_t channels, std::uint32_t capacityFrames)
{
    if (channels == 0 || channels > 16 || capacityFrames < 1024) return false;
    try { storage_.assign(static_cast<std::size_t>(channels) * capacityFrames, 0.0); }
    catch (...) { return false; }
    channels_=channels; capacity_=capacityFrames; abandoned_=0;
    return reset(SourceStreamDescriptor{0,0,channels,0,0,0});
}

bool SourceFifo192::reset(const SourceStreamDescriptor& descriptor) noexcept
{
    resetting_.store(true);
    // A reset never waits without a bound. If a consumer is copying, the
    // producer retries at its next scheduling opportunity and the consumer
    // remains entirely nonblocking.
    constexpr unsigned maximumAttempts=4096;
    unsigned attempt=0;
    while (operations_.load(std::memory_order_acquire)!=0 &&
           attempt++<maximumAttempts) {}
    if (operations_.load(std::memory_order_acquire)!=0) {
        resetting_.store(false,std::memory_order_release);
        return false;
    }
    const auto unread=write_.load(std::memory_order_relaxed)-read_.load(std::memory_order_relaxed);
    abandoned_.fetch_add(unread,std::memory_order_relaxed);
    read_.store(0,std::memory_order_relaxed); write_.store(0,std::memory_order_relaxed);
    descriptor_=descriptor;
    produced_=0; consumed_=0; underruns_=0; overflows_=0;
    lowWater_.store(capacity_,std::memory_order_relaxed); highWater_=0;
    resetting_.store(false,std::memory_order_release);
    return true;
}

bool SourceFifo192::reset(std::uint64_t generation) noexcept
{
    return reset(SourceStreamDescriptor{generation,0,channels_,0,0,0});
}

bool SourceFifo192::push(const double* const* inputs, std::uint32_t frames,
                         const SourceStreamDescriptor& descriptor) noexcept
{
    Operation operation(*this);
    if (!operation.entered) return false;
    if (!inputs || !frames || descriptor != descriptor_) return false;
    for(std::uint32_t c=0;c<channels_;++c) if(!inputs[c]) return false;
    const auto w=write_.load(std::memory_order_relaxed);
    const auto r=read_.load(std::memory_order_acquire);
    const auto fill=w-r;
    if (frames > capacity_-fill) { overflows_.fetch_add(1,std::memory_order_relaxed); return false; }
    // Preserve the baseline's unsigned cursor-wrap behavior even for nonpower capacities.
    if (w > (std::numeric_limits<std::uint64_t>::max)()-frames) {
        for(std::uint32_t c=0;c<channels_;++c)
            for(std::uint32_t n=0;n<frames;++n) storage_[static_cast<std::size_t>(c)*capacity_ + ((w+n)%capacity_)]=inputs[c][n];
    } else {
        const auto position=static_cast<std::uint32_t>(w%capacity_);
        const auto first=(std::min)(frames,capacity_-position), second=frames-first;
        for(std::uint32_t c=0;c<channels_;++c) {
            auto* ring=storage_.data()+static_cast<std::size_t>(c)*capacity_;
            std::memcpy(ring+position,inputs[c],first*sizeof(double));
            std::memcpy(ring,inputs[c]+first,second*sizeof(double));
        }
    }
    write_.store(w+frames,std::memory_order_release);
    produced_.fetch_add(frames,std::memory_order_relaxed);
    const auto newFill=static_cast<std::uint32_t>(fill+frames);
    auto hi=highWater_.load(); while(newFill>hi && !highWater_.compare_exchange_weak(hi,newFill)) {}
    auto lo=lowWater_.load(); while(newFill<lo && !lowWater_.compare_exchange_weak(lo,newFill)) {}
    return true;
}

bool SourceFifo192::push(const double* const* inputs, std::uint32_t frames,
                         std::uint64_t generation) noexcept
{
    SourceStreamDescriptor descriptor;
    {
        Operation operation(*this);
        if (!operation.entered || generation != descriptor_.generation) return false;
        descriptor=descriptor_;
    }
    return push(inputs,frames,descriptor);
}

bool SourceFifo192::pop(double* const* outputs, std::uint32_t frames,
                        SourceStreamDescriptor& descriptor) noexcept
{
    Operation operation(*this);
    if (!operation.entered) return false;
    if (!outputs || !frames) return false;
    for(std::uint32_t c=0;c<channels_;++c) if(!outputs[c]) return false;
    const auto r=read_.load(std::memory_order_relaxed);
    const auto w=write_.load(std::memory_order_acquire);
    const auto fill=w-r;
    if (fill < frames) { underruns_.fetch_add(1,std::memory_order_relaxed); return false; }
    // Preserve the baseline's unsigned cursor-wrap behavior even for nonpower capacities.
    if (r > (std::numeric_limits<std::uint64_t>::max)()-frames) {
        for(std::uint32_t c=0;c<channels_;++c)
            for(std::uint32_t n=0;n<frames;++n) outputs[c][n]=storage_[static_cast<std::size_t>(c)*capacity_ + ((r+n)%capacity_)];
    } else {
        const auto position=static_cast<std::uint32_t>(r%capacity_);
        const auto first=(std::min)(frames,capacity_-position), second=frames-first;
        for(std::uint32_t c=0;c<channels_;++c) {
            auto* ring=storage_.data()+static_cast<std::size_t>(c)*capacity_;
            std::memcpy(outputs[c],ring+position,first*sizeof(double));
            std::memcpy(outputs[c]+first,ring,second*sizeof(double));
        }
    }
    read_.store(r+frames,std::memory_order_release);
    consumed_.fetch_add(frames,std::memory_order_relaxed);
    descriptor=descriptor_;
    const auto newFill=static_cast<std::uint32_t>(fill-frames);
    auto lo=lowWater_.load(); while(newFill<lo && !lowWater_.compare_exchange_weak(lo,newFill)) {}
    return true;
}

bool SourceFifo192::pop(double* const* outputs, std::uint32_t frames,
                        std::uint64_t& generation) noexcept
{
    SourceStreamDescriptor descriptor;
    if (!pop(outputs,frames,descriptor)) return false;
    generation=descriptor.generation;
    return true;
}

SourceFifo192Telemetry SourceFifo192::telemetry() const noexcept
{
    SourceFifo192Telemetry t;
    t.channels=channels_; t.capacity_frames=capacity_;
    Operation operation(*this);
    if (!operation.entered) return t;
    const auto r=read_.load(std::memory_order_acquire), w=write_.load(std::memory_order_acquire);
    // A third-party observer can span a complete ring turnover, unlike the
    // SPSC owners. Its approximate occupancy must remain within the capacity.
    t.fill_frames=static_cast<std::uint32_t>((std::min)(w-r,std::uint64_t(capacity_)));
    t.low_water_frames=lowWater_.load(); t.high_water_frames=highWater_.load();
    t.generation=descriptor_.generation; t.produced_frames=produced_.load(); t.consumed_frames=consumed_.load();
    t.underruns=underruns_.load(); t.overflows=overflows_.load();
    t.abandoned_frames=abandoned_.load();
    return t;
}
} // namespace entropy::host
