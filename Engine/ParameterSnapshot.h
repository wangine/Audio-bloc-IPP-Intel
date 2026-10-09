#pragma once
#include <entropy/output_matrix_abi.h>
#include <entropy/headroom_abi.h>
#include <array>
#include <atomic>
namespace entropy::control {
struct ParameterSnapshot {
 uint64_t revision=0;
 EntropyOutputMatrixState matrix{};
 EntropyHeadroomState headroom{};
 EntropyHeadroomTimingStateV5 timing{};
 unsigned mode=1;double manualGainDb=-6;
 uint32_t renderLaneMask=0xffffu;
 bool renderMaskChanged=false;
 bool matrixChanged=false,headroomChanged=false,timingChanged=false,policyChanged=false,clearFault=false;
};
class SnapshotQueue {
public:
 bool push(const ParameterSnapshot&) noexcept;
 bool take(ParameterSnapshot&) noexcept;
 bool full() const noexcept{return write_.load()-read_.load(std::memory_order_acquire)>=capacity;}
 bool empty() const noexcept{return write_.load(std::memory_order_acquire)==read_.load(std::memory_order_acquire);}
 static constexpr uint64_t capacity=64;
private:
 std::array<ParameterSnapshot,capacity> slots_{};
 alignas(64) std::atomic<uint64_t> write_{0},read_{0};
};
}
