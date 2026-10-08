#pragma once
#include "NativeEgress.h"
#include "IPC/Json.h"
#include <thread>
namespace entropy::control {
// Explicit test fixture only. Never constructs or opens a WASAPI sink.
class OfflineSinkWorkers {
public:
 ~OfflineSinkWorkers(){stop();}
 void start();void stop() noexcept;
 void publish(const double* const* outputs,unsigned frames) noexcept;
 ipc::Value snapshot() const;
private:
 void run(unsigned slot) noexcept;
 struct Domain {egress_host::DacPipeline pipeline;std::thread owner;std::atomic<uint64_t> callbacks{0};std::atomic<double> ppm{0},fill{0};std::atomic<unsigned> clockState{0};};
 std::array<Domain,2> domains_;std::atomic<bool> enabled_{false},stopping_{true};
};
}
