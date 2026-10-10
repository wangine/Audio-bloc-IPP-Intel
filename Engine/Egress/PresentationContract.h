#pragma once
#include <cstdint>
namespace entropy::egress_host {
// Position is in device frames; IAudioClock QPC is already in 100ns units.
// IAudioClock position refers to playback, so GetStreamLatency is telemetry,
// not added a second time. Only separately measured residual latency belongs
// in the final parameter (zero until analog calibration). Includes initial SILENT
// buffer in nextDeviceFrame. No modification of the per-DAC servo or sinc.
struct PresentationContract {
 static uint64_t bufferTime(uint64_t qpc,double position,uint64_t next,uint64_t latency,double rate=192000) noexcept {
  return qpc+static_cast<uint64_t>(static_cast<long double>(next-position)*10000000/rate)+latency;
 }
 static double graphPosition(uint64_t first,uint64_t common,uint64_t time) noexcept {
  return double(first)+double(int64_t(time)-int64_t(common))*192000/10000000;
 }
};
}
