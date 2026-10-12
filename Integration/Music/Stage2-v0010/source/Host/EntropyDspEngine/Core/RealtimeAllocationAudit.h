#pragma once
#include <cstdint>
namespace entropy::host::allocation_audit {
// Audit-only executable. Thread-local scope spans each actual EngineMain cycle.
// Host C++ operators and instrumented Windows imports of loaded images are counted.
// Direct/dynamically resolved calls, syscalls, new images and unlisted APIs remain outside coverage.
struct Snapshot {
 std::uint64_t cppNew=0,cppDelete=0,heapAlloc=0,heapFree=0,wait=0,lock=0,io=0;
};
bool installHooks() noexcept; // stopped-only: instrument imports of currently loaded images
void reset() noexcept; // test-only, no concurrent producers/reporters
Snapshot snapshot() noexcept;
bool selfTest();
void begin() noexcept;
void audioPhase() noexcept;
void end() noexcept;
class CycleScope final {
public:
 CycleScope() noexcept {begin();}
 ~CycleScope() {end();}
 CycleScope(const CycleScope&)=delete;
 CycleScope& operator=(const CycleScope&)=delete;
};
void report() noexcept; // observer only; stderr is redirected by audit launcher
}
