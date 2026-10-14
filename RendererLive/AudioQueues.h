#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace entropy::live {
// One Graph writer, one preparation reader. Reset/prepare only while quiescent.
class AudioQueue final {
public:
 bool prepare(unsigned channels,unsigned capacity) {
  if(!channels||channels>16||capacity<1024||(capacity&(capacity-1))||capacity>(1u<<20))return false;
  channels_=channels;capacity_=capacity;data_.assign(size_t(channels)*capacity,0.0);
  write_=0;read_=0;base_=0;epoch_=0;haveOrigin_=false;nextFrame_=0;return true;
 }
 bool push(const double* const* src,unsigned frames,uint64_t first,uint64_t epoch) noexcept {
  if(!src||!frames||frames>capacity_||!epoch||first>UINT64_MAX-frames)return false;
  if(haveOrigin_&&(epoch!=epoch_.load(std::memory_order_relaxed)||first!=nextFrame_))return false;
  const auto w=write_.load(std::memory_order_relaxed),r=read_.load(std::memory_order_acquire);
  if(w<r||w-r>capacity_||frames>capacity_-(w-r)||w>UINT64_MAX-frames)return false;
  for(unsigned c=0;c<channels_;++c){if(!src[c])return false;for(unsigned f=0;f<frames;++f)if(!std::isfinite(src[c][f]))return false;}
  for(unsigned c=0;c<channels_;++c)for(unsigned f=0;f<frames;++f)
   std::memcpy(&data_[size_t(c)*capacity_+size_t((w+f)&(capacity_-1))],src[c]+f,sizeof(double));
  if(!haveOrigin_){base_.store(first,std::memory_order_relaxed);epoch_.store(epoch,std::memory_order_relaxed);haveOrigin_=true;}
  nextFrame_=first+frames;write_.store(w+frames,std::memory_order_release);return true;
 }
 bool popInterleaved(double* dst,unsigned frames) noexcept {
  if(!dst||frames>capacity_)return false;
  if(!frames)return true;
  const auto r=read_.load(std::memory_order_relaxed),w=write_.load(std::memory_order_acquire);
  if(w<r||frames>w-r)return false;
  for(unsigned f=0;f<frames;++f)for(unsigned c=0;c<channels_;++c)
   std::memcpy(dst+size_t(f)*channels_+c,&data_[size_t(c)*capacity_+size_t((r+f)&(capacity_-1))],sizeof(double));
  read_.store(r+frames,std::memory_order_release);return true;
 }
 unsigned readable() const noexcept {
  const auto r=read_.load(std::memory_order_acquire),w=write_.load(std::memory_order_acquire);
  return w>=r?unsigned((std::min)(w-r,uint64_t(capacity_))):0;
 }
 uint64_t published() const noexcept {return write_.load(std::memory_order_acquire);}
 uint64_t taken() const noexcept {return read_.load(std::memory_order_acquire);}
 uint64_t baseFrame() const noexcept {return base_.load(std::memory_order_acquire);}
 uint64_t epoch() const noexcept {return epoch_.load(std::memory_order_acquire);}
 unsigned channels() const noexcept {return channels_;}
private:
 static_assert(std::atomic<uint64_t>::is_always_lock_free);
 std::vector<double> data_;unsigned channels_=0,capacity_=0;
 alignas(64) std::atomic<uint64_t> write_{0};
 alignas(64) std::atomic<uint64_t> read_{0};
 std::atomic<uint64_t> base_{0},epoch_{0};uint64_t nextFrame_=0;bool haveOrigin_=false;
};
struct ReadyPacket {std::vector<uint8_t> bytes;unsigned frames=0;double sourceBegin=0,sourceEnd=0;};
// A packet stays pinned until successful device submission and explicit commit.
class ReadyQueue final {
public:
 bool prepare(unsigned channels,unsigned containerBits,unsigned maxFrames,unsigned slots=4) {
  if(!channels||channels>16||(containerBits!=24&&containerBits!=32)||!maxFrames||maxFrames>4096||slots<3||slots>16)return false;
  packets_.resize(slots);for(auto& p:packets_){p.bytes.assign(size_t(channels)*(containerBits/8)*maxFrames,0);p.frames=0;p.sourceBegin=p.sourceEnd=0;}
  slots_=slots;write_=0;read_=0;writing_=nullptr;reading_=nullptr;return true;
 }
 ReadyPacket* beginWrite() noexcept {
  if(writing_)return writing_;
  const auto w=write_.load(std::memory_order_relaxed),r=read_.load(std::memory_order_acquire);
  if(w-r>=slots_)return nullptr;
  writing_=&packets_[size_t(w%slots_)];return writing_;
 }
 bool commitWrite() noexcept {
  if(!writing_)return false;
  writing_=nullptr;write_.fetch_add(1,std::memory_order_release);return true;
 }
 const ReadyPacket* beginRead() noexcept {
  if(reading_)return reading_;
  const auto r=read_.load(std::memory_order_relaxed),w=write_.load(std::memory_order_acquire);
  if(w==r)return nullptr;
  reading_=&packets_[size_t(r%slots_)];return reading_;
 }
 bool commitRead() noexcept {
  if(!reading_)return false;
  reading_=nullptr;read_.fetch_add(1,std::memory_order_release);return true;
 }
 unsigned size() const noexcept {
  const auto r=read_.load(std::memory_order_acquire),w=write_.load(std::memory_order_acquire);
  return w>=r?unsigned((std::min)(w-r,uint64_t(slots_))):0;
 }
private:
 std::vector<ReadyPacket> packets_;unsigned slots_=0;
 alignas(64) std::atomic<uint64_t> write_{0};
 alignas(64) std::atomic<uint64_t> read_{0};
 ReadyPacket* writing_=nullptr;const ReadyPacket* reading_=nullptr;
};
}
