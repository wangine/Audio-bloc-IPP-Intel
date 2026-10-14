#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace entropy::live {
struct DeviceConfig {std::string endpoint;unsigned channels=0,format=0,requestedFrames=1024;};
struct DeviceObservation {uint64_t submitted=0,position=0,clockFrequency=0,qpc100ns=0,timeouts=0;unsigned quantum=0;int32_t lastResult=0;bool clockValid=false;};
// Owner-thread-only API. Construct, open, submit, close and destroy on one thread.
class WasapiOutput final {
public:
 enum class Wait {Ready,Canceled,Failed};
 WasapiOutput();~WasapiOutput();
 WasapiOutput(const WasapiOutput&)=delete;WasapiOutput& operator=(const WasapiOutput&)=delete;
 void open(const DeviceConfig&); // throws before Start; never format/endpoint fallback
 bool start() noexcept;
 Wait wait(void* cancel,unsigned milliseconds=100) noexcept;
 bool submit(const uint8_t*,unsigned frames,unsigned bytes) noexcept;
 void stop() noexcept;
 DeviceObservation observation() const noexcept;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
