#include "ParameterSnapshot.h"
namespace entropy::control {
bool SnapshotQueue::push(const ParameterSnapshot& s) noexcept {auto w=write_.load(std::memory_order_relaxed);if(w-read_.load(std::memory_order_acquire)>=capacity)return false;slots_[w%capacity]=s;write_.store(w+1,std::memory_order_release);return true;}
bool SnapshotQueue::take(ParameterSnapshot& s) noexcept {auto r=read_.load(std::memory_order_relaxed);if(r==write_.load(std::memory_order_acquire))return false;s=slots_[r%capacity];read_.store(r+1,std::memory_order_release);return true;}
}
