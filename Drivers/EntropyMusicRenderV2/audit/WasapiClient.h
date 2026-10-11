#pragma once
#include "../provider/EndpointIdentity.h"
#include "../provider/AuditObserver.h"
#include <string>
namespace entropy::music::audit {
struct RunConfig {uint32_t rate=48000,bits=16,seconds=10,periodFrames=0;bool eventDriven=true;std::wstring session;};
struct RunReport {uint64_t emittedFrames=0,clockFrames=0,receivedFrames=0;uint32_t bufferFrames=0,observedAllocation=0;bool drained=false;bridge::Info driver{};std::string endpointId;};
struct SharedProgress {volatile LONG64 frames=0;volatile LONG clientPid=0,errors=0;};
std::wstring pipeName(const std::wstring& session);
std::wstring progressName(const std::wstring& session);
class WasapiClient {public:static RunReport run(const RunConfig&);};
}
