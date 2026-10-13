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
class LiveSinkWorkers final {
public:
 using Factory=std::function<std::unique_ptr<egress_host::Sink>(unsigned)>;
 explicit LiveSinkWorkers(Factory={},bool calibrateClock=true);
 ~LiveSinkWorkers();
 bool prepare(const std::array<LiveDomain,2>&); // lifecycle worker, inactive candidate only
 bool start();
 void stop() noexcept;
 void closePublicationGate() noexcept; // admission: gate + cancel only
 void atBlockBoundary() noexcept; // Graph owner: O(1) pointer handoff/ack, even without input
 void graphStopped() noexcept; // after Graph join only
 void publish(const double* const*,unsigned,bool) noexcept;
 void publish(const double* const*,unsigned,bool,egress_host::GraphInterval) noexcept;
 void failAll(unsigned) noexcept;
 void waitForInput() noexcept; // Graph owner: source hold, no fabricated PCM
 bool sourceWaitFault() const noexcept;
 unsigned sourceWaitFaults() const noexcept;
 bool armed() const noexcept;
 bool recoveryAtTarget() const noexcept;
 bool faulted() const noexcept {return faulted_.load(std::memory_order_acquire);}
 bool engaged() const noexcept {return enabled_.load(std::memory_order_acquire);}
 ipc::Value snapshot() const;
 static std::array<LiveDomain,2> decode(const ipc::Value&);
 static uint32_t renderLaneMask(const std::array<LiveDomain,2>&) noexcept;
private:
 struct Domain {
  LiveDomain config;egress_host::DacPipeline pipeline;std::thread owner;
  std::atomic<bool> open{false},running{false},done{true},mmcss{false},clockValid{false},calibrated{false},latencyValid{false},estimateValid{false};
  std::atomic<unsigned> buffer{0},sinkState{ENTROPY_DAC_SINK_CLOSED},sinkFaults{0},clockState{0},resetReason{0};
  std::atomic<int> driverResult{0};
  // Passive owner-published observations; never used as playback gates.
  std::atomic<uint64_t> submitTimingSamples{0},estimatedLateSubmissions{0};
  std::atomic<double> waitReturnIntervalUs{0},maxWaitReturnIntervalUs{0},serviceToSubmitUs{0},maxServiceToSubmitUs{0},submitSlackUs{0},minSubmitSlackUs{0};
  std::atomic<uint64_t> submitted{0},position{0},callbacks{0},cycles{0},misses{0},nonzeroPcmBlocks{0},resync{0},epoch{0},capture100ns{0},clockFrequency{0},latency100ns{0},presentation100ns{0},clockReady{0},startupSkipped{0},mappingVersion{0};
  std::atomic<double> ppm{0},clockFill{0},waitUs{0},callbackUs{0},maxCallbackUs{0},firstFrame{0},endFrame{0},rateHz{192000},primeWanted{0},primeBase{0},primeNominal{0};
 };
 struct Session {
  std::array<Domain,2> domains;
  std::atomic<bool> stop{false},fault{false},sourceHold{false},fatal{false};
  std::atomic<uint64_t> baseFrame{0},baseEpoch{0},baseCapture{0},lastGraphEnd{0},presentation{0},audiblePresentation{0};
  std::atomic<unsigned> publishedBlocks{0},replans{0},startupDecision{0},primedMask{0};
  std::atomic<uint64_t> referenceFrame{0}; // written by coordinator before release of presentation
 };
 void run(Session&,unsigned) noexcept;
 void fail(Session&,unsigned,bool source=false) noexcept;
 bool detached() const noexcept;
 Factory factory_;bool calibrateClock_=true;std::unique_ptr<Session> session_;
 std::array<void*,2> cancel_{}; // fixed lifetime; no close while owner waits
 std::atomic<Session*> requested_{nullptr};Session* attached_=nullptr; // Graph only
 std::atomic<uint64_t> request_{0},ack_{0};std::atomic<bool> graphRunning_{false};
 std::atomic<bool> enabled_{false},faulted_{false};bool prepared_=false;
 uint64_t legacyFrame_=0;
};
}
