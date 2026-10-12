// Presentation schema only. No R10 telemetry mappings, native DLL calls or microphone workers.
#include "Core/EngineTelemetry.h"
#include <windows.h>
namespace entropy::host {
void populateStage2UiSchema(EngineTelemetrySnapshot&);
bool readEngineTelemetry(EngineTelemetrySnapshot& s) noexcept {
 static EngineTelemetrySnapshot schema=[](){EngineTelemetrySnapshot v{};populateStage2UiSchema(v);return v;}();s=schema;LARGE_INTEGER now;QueryPerformanceCounter(&now);s.heartbeat=now.QuadPart;s.enginePid=GetCurrentProcessId();return true;
}
bool writeEngineLoudnessRequest(uint32_t,const EntropyLoudnessSettings&,const char*,uint64_t&) noexcept{return false;}
bool writeEngineParametricEqSettings(const EntropyParametricEqSettingsV1&,uint64_t&) noexcept{return false;}
}
