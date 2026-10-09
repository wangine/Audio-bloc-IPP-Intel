#pragma once
#include <atomic>
#include <cstdint>
namespace entropy::host {
// Single SRC producer; single nonblocking consumer. Each producer publication
// advances the revision. A drain may clear ready only in its observed revision.
// Epoch resets advance it too, so decisions from a retired descriptor cannot win.
class SourceReadiness final {
 std::atomic<std::uint64_t> state_{0};
public:
 std::uint64_t revision() const noexcept {return state_.load(std::memory_order_acquire);}
 bool load(std::memory_order order=std::memory_order_acquire) const noexcept {return (state_.load(order)&1)!=0;}
 void operator=(bool ready) noexcept {auto s=revision();while(!state_.compare_exchange_weak(s,((s+2)&~std::uint64_t(1))|std::uint64_t(ready),std::memory_order_acq_rel)){} }
 void produced(std::uint32_t fill,std::uint32_t preroll) noexcept {
  auto s=revision();while(!state_.compare_exchange_weak(s,((s+2)&~std::uint64_t(1))|std::uint64_t((s&1)||fill>=preroll),std::memory_order_acq_rel)){}
 }
 void drained(std::uint64_t observed,std::uint32_t fill) noexcept {
  if(fill<1024)state_.compare_exchange_strong(observed,observed&~std::uint64_t(1),std::memory_order_acq_rel);
 }
};
}
