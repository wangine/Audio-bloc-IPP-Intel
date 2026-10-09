#pragma once
#include "NativeEgress.h"
#include "IPC/Json.h"
#include <functional>
#include <memory>
#include <thread>
namespace entropy::control {
struct LiveDomain {
 bool enabled=false;unsigned channels=0,format=0;std::string endpoint,logicalId;
 std::array<unsigned,16> lanes{};
};
// Native Core only. Injected sinks permit the exact lifecycle to run offline.
class LiveSinkWorkers final {
public:
 using Factory=std::function<std::unique_ptr<egress_host::Sink>(unsigned)>;
 explicit LiveSinkWorkers(Factory={});
 ~LiveSinkWorkers(){stop();}
 bool prepare(const std::array<LiveDomain,2>&); // joined Graph + joined sink owners
 bool start(); // explicit ARM; asynchronous preroll is reported honestly
 void stop() noexcept; // control owner; Graph joined before reconfiguration
 void publish(const double* const* outputs,unsigned frames,bool policyReady) noexcept;
 void failAll(unsigned reason) noexcept;
 bool armed() const noexcept;
 bool faulted() const noexcept {return faulted_.load(std::memory_order_acquire);}
 bool engaged() const noexcept {return enabled_.load(std::memory_order_acquire);}
 ipc::Value snapshot() const;
 static std::array<LiveDomain,2> decode(const ipc::Value& configuration);
private:
 struct Domain {
  LiveDomain config;egress_host::DacPipeline pipeline;std::thread owner;
  std::atomic<bool> open{false},running{false},mmcss{false};
  std::atomic<unsigned> buffer{0},sinkState{ENTROPY_DAC_SINK_CLOSED},sinkFaults{0},clockState{0};
  std::atomic<int> driverResult{0};
  std::atomic<uint64_t> submitted{0},position{0},callbacks{0},cycles{0},misses{0},nonzeroPcmBlocks{0};
  std::atomic<double> ppm{0},clockFill{0},waitUs{0},callbackUs{0},maxCallbackUs{0};
 };
 void run(unsigned) noexcept;
 Factory factory_;std::array<Domain,2> domains_;
 std::atomic<bool> enabled_{false},stopping_{true},faulted_{false};bool prepared_=false;
};
}
