#pragma once
#include "Control/ControlPlane.h"
#include "Egress/OfflineSinkWorkers.h"
#include <thread>
namespace entropy::control {
struct SchedulerTelemetry {uint64_t blocks=0,misses=0;double callbackUs=0,maxCallbackUs=0,latenessUs=0,maxLatenessUs=0;unsigned faults=0;};
class AudioScheduler {
public:
 AudioScheduler(engine::AudioGraph&,ControlPlane&,bool offlineSignal,OfflineSinkWorkers* offlineSinks=nullptr);
 ~AudioScheduler(){stop();}
 void start();void stop();void disarm() noexcept;
 SchedulerTelemetry telemetry() const noexcept;
private:
 void run() noexcept;
 engine::AudioGraph& graph_;ControlPlane& controls_;bool signal_;OfflineSinkWorkers* offlineSinks_;
 engine::PlanarBuffer input_,output_;
 std::atomic<bool> stopping_{false};std::thread owner_;
 std::atomic<uint64_t> blocks_{0},misses_{0};
 std::atomic<double> cost_{0},maxCost_{0},late_{0},maxLate_{0};std::atomic<unsigned> faults_{0};
};
}
