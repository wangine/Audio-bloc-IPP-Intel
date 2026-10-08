#pragma once
#include "Core/EngineTelemetry.h"
#include <array>
#include <atomic>
#include <cstring>
namespace entropy::stage {
// Presentation uses the confirmed native parameter. Last-block output onset
// remains in the owner snapshot and never feeds back from this UI projection.
inline void presentSelectedCrossoverProgram(entropy::host::EngineTelemetrySnapshot& s) noexcept {
 if(!s.crossoverControlAvailable)return;
 const auto count=std::min<uint32_t>(s.crossoverParameterCount,entropy::host::crossoverParameterCapacity);
 for(uint32_t n=0;n<count;n++)if(std::strncmp(s.crossoverParameters[n].identifier,"cinemaSource",sizeof(s.crossoverParameters[n].identifier))==0){s.crossoverCinemaProgram=s.crossoverParameters[n].value>=0.5;return;}
}
class RenderUiBridge {
public:
 static constexpr size_t capacity=64;
 RenderUiBridge();
 ~RenderUiBridge();
 bool enqueue(entropy::host::EngineControlSnapshot&,uint64_t&) noexcept; // UI single producer.
 void publish(const entropy::host::EngineTelemetrySnapshot&) noexcept;
 bool read(entropy::host::EngineTelemetrySnapshot&) const noexcept;
 static RenderUiBridge* current() noexcept;
private:
 std::atomic<uint64_t> revision_{0};
 std::array<std::atomic<uint64_t>,(sizeof(entropy::host::EngineTelemetrySnapshot)+7)/8> telemetry_{};
};
}
