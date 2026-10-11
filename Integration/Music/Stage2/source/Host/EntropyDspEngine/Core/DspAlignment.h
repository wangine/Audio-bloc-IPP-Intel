#pragma once
#include <array>
#include <span>
#include <cstdint>
namespace entropy::stage {
struct PathLatency {uint32_t channels;bool active,valid;uint64_t samples;};
struct AlignmentProposal {std::array<uint64_t,3> detected{},delay{};uint64_t target=0;};
bool calculateAlignment(std::span<const PathLatency>,AlignmentProposal&) noexcept;
struct alignas(8) AlignmentSnapshot {
 uint64_t loaded=0,valid=0,mode=0,target=0,appliedTarget=0,generation=0,boundaryFrame=0,commandSerial=0,rejected=0;
 std::array<uint64_t,3> detected{},proposed{},current{};
};
enum class AlignmentApplyResult {Applied,Busy,Stale,NativeFault};
}
