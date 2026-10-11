#pragma once
#include "../provider/EndpointIdentity.h"
#include "../provider/AuditObserver.h"
#include "PauseEvidence.h"
#include <string>
namespace entropy::music::audit {
struct RunConfig {uint32_t rate=48000,bits=16,seconds=10,periodFrames=0,pauseMs=0;bool eventDriven=true,completeFp64Tail=false;std::wstring session;};
struct RunReport {uint64_t emittedFrames=0,clockFrames=0,receivedFrames=0;uint32_t bufferFrames=0,observedAllocation=0;bool drained=false;PauseEvidence pause{};bridge::Info driver{};bridge::Trace traceBefore{},traceAfterInitialize{},traceAfterPrefill{},traceAfter{};std::string endpointId;};
struct SharedProgress {volatile LONG64 frames=0;volatile LONG clientPid=0,errors=0;volatile LONG64 outputFrames=0;volatile LONG outputReady=0,outputErrors=0;};
std::wstring pipeName(const std::wstring& session);
std::wstring progressName(const std::wstring& session);
class WasapiClient {public:static RunReport run(const RunConfig&);};
}
