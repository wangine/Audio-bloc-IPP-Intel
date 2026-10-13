#pragma once
#include <entropy/stage_bus_v1.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <thread>
namespace entropy::stage {
class DspRecorder {
public:
 ~DspRecorder(){stop();}
 bool start(const std::filesystem::path&,uint64_t maximumBlocks,unsigned testDelayMs=0);
 void stop() noexcept; // quiesce producer first
 bool tryCapture(const StageBusBlockHeader&,const double* const*) noexcept;
 uint64_t dropped() const noexcept{return dropped_;}
 uint64_t written() const noexcept{return written_;}
 bool failed() const noexcept{return failed_;}
private:
 friend struct RecorderRaceProbe;
 void (*emptyQueueProbe_)(void*)=nullptr;
 void* probeContext_=nullptr;
 struct Slot{StageBusBlockHeader header{};std::array<std::array<double,1024>,3> data{};};
 std::array<Slot,4> queue_{};
 std::atomic<uint64_t> write_{0},read_{0},attempts_{0},dropped_{0},written_{0};
 std::atomic<bool> active_{false},stopping_{false},failed_{false};
 uint64_t limit_=0;
 std::thread writer_;
};
}
