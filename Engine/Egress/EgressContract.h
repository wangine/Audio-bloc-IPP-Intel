#pragma once
#include "SpscAudioFifo.h"
#include <entropy/dac_sink_abi.h>
namespace entropy::egress_host {
// Shared transport types; independent of Graph/IPP/legacy ClockSync algorithms.
enum class State : unsigned { Disarmed, Prepared, Armed, Preroll, Running, Stopping, Fault };
enum Fault : unsigned { None=0, Overflow=1, Starvation=2, Clock=4, Pcm=8, Device=16, Input=32 };
struct SinkTiming {uint64_t clockFrequency=0,latency100ns=0;bool freshClock=false;double precisePositionFrames=-1;bool latencyValid=true;};
class Sink {
public:
 virtual ~Sink()=default;
 virtual void setCancelEvent(void*) noexcept {}
 virtual SinkTiming timing() const noexcept {return {192000,0,true};}
 virtual EntropyResult open(const EntropyDacSinkConfig&)=0;
 virtual EntropyResult start() noexcept=0;
 virtual EntropyResult wait(EntropyDacSinkBufferRequest&) noexcept=0;
 virtual EntropyResult submit(EntropyDacSinkPcmBlock&) noexcept=0;
 virtual EntropyDacSinkStatus status() const noexcept=0;
 virtual void stop() noexcept=0;
 virtual void close() noexcept=0;
};
}
