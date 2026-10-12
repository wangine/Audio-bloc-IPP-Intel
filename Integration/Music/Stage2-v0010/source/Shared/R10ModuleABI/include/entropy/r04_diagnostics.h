#pragma once
#include <entropy/r04_diagnostics_abi.h>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <type_traits>
#include <cstdint>
namespace entropy::r04 {
inline constexpr unsigned maxChannels=16;
struct ChannelMetric {
 double first=0,last=0,delta=0,second_difference=0,peak=0;
 std::uint32_t nonfinite=0,history_valid=0;
};
struct Metrics {
 std::uint32_t frames=0,channels=0;
 std::array<ChannelMetric,maxChannels> channel{};
};
class Continuity {
 std::array<double,maxChannels> last_{}, previous_{};
 std::array<bool,maxChannels> valid_{},previousValid_{};
public:
 void reset() noexcept {valid_.fill(false);previousValid_.fill(false);}
 Metrics measure(const double* const* input,unsigned channels,unsigned frames) noexcept {
  Metrics m{};m.channels=(std::min)(channels,maxChannels);m.frames=frames;
  if(!input||!frames){reset();return m;}
  for(unsigned c=0;c<m.channels;++c){auto& v=m.channel[c];if(!input[c]){valid_[c]=previousValid_[c]=false;continue;}
   const auto* p=input[c];v.first=p[0];v.last=p[frames-1];
   const bool firstFinite=std::isfinite(v.first);
   v.history_valid=valid_[c]&&previousValid_[c]&&firstFinite;
   if(v.history_valid){v.delta=v.first-last_[c];v.second_difference=v.delta-(last_[c]-previous_[c]);}
   for(unsigned i=0;i<frames;++i){if(std::isfinite(p[i]))v.peak=(std::max)(v.peak,std::abs(p[i]));else ++v.nonfinite;}
   previous_[c]=frames>1?p[frames-2]:last_[c];
   previousValid_[c]=frames>1?std::isfinite(previous_[c]):valid_[c];
   last_[c]=v.last;valid_[c]=std::isfinite(v.last);
  }return m;
 }
};
class ClockQuality {
 std::uint64_t lastPosition_=0,lastQpc_=0;bool have_=false;
public:
 EntropyR04ClockQuality value{};
 void observe(std::int32_t hr,std::uint64_t position,std::uint64_t qpc) noexcept {
  value.called=1;value.hresult=hr;value.raw_position=position;value.raw_qpc_100ns=qpc;
  if(hr==0)++value.ok;else if(hr==1)++value.s_false;else if(hr<0)++value.fail;
  value.fresh=hr==0 && qpc!=0 && (!have_||(position>lastPosition_&&qpc>lastQpc_));
  value.stale=!value.fresh;
  if(hr>=0){lastPosition_=position;lastQpc_=qpc;have_=true;}
 }
};
template<class T,std::size_t N> class Queue {
 static_assert(N&&(N&(N-1))==0 && std::is_trivially_copyable_v<T>);
 static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
 alignas(64) std::atomic<std::uint64_t> write_{0};
 alignas(64) std::atomic<std::uint64_t> read_{0};
 alignas(64) std::atomic<std::uint64_t> dropped_{0};
 std::array<T,N> slots_{};
public:
 bool push(const T& v) noexcept {const auto w=write_.load(std::memory_order_relaxed);
  if(w-read_.load(std::memory_order_acquire)>=N){dropped_.fetch_add(1,std::memory_order_relaxed);return false;}
  slots_[w&(N-1)]=v;write_.store(w+1,std::memory_order_release);return true;}
 bool pop(T& v) noexcept {const auto r=read_.load(std::memory_order_relaxed);if(r==write_.load(std::memory_order_acquire))return false;
  v=slots_[r&(N-1)];read_.store(r+1,std::memory_order_release);return true;}
 std::uint64_t dropped()const noexcept{return dropped_.load(std::memory_order_relaxed);}
 std::uint64_t written()const noexcept{return write_.load(std::memory_order_relaxed);}
};
}
