#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
namespace entropy::egress {
// B2-FF-006 TEMP_FORENSIC. One producer, one collector, fixed storage.
// Full queue drops NEW diagnostic data, never blocks or overwrites a reader.
// Lifecycle reset is only legal after producer AND collector have stopped.
template<class T,std::size_t N> class ForensicQueue final {
 static_assert(N && (N&(N-1))==0);
 static_assert(std::is_trivially_copyable_v<T>);
 static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
public:
 bool push(const T& value) noexcept {
  const auto w=write_.load(std::memory_order_relaxed);
  const auto r=read_.load(std::memory_order_acquire);
  if(w-r>=N) {dropped_.fetch_add(1,std::memory_order_relaxed);return false;}
  slots_[w&(N-1)]=value;
  write_.store(w+1,std::memory_order_release);return true;
 }
 bool pop(T& value) noexcept {
  const auto r=read_.load(std::memory_order_relaxed);
  if(r==write_.load(std::memory_order_acquire))return false;
  value=slots_[r&(N-1)];read_.store(r+1,std::memory_order_release);return true;
 }
 std::uint64_t dropped()const noexcept{return dropped_.load(std::memory_order_relaxed);}
 std::uint64_t written()const noexcept{return write_.load(std::memory_order_relaxed);}
 void resetQuiescent() noexcept{write_.store(0);read_.store(0);dropped_.store(0);}
private:
 alignas(64) std::atomic<std::uint64_t> write_{0};
 alignas(64) std::atomic<std::uint64_t> read_{0};
 alignas(64) std::atomic<std::uint64_t> dropped_{0};
 std::array<T,N> slots_{};
};
}
