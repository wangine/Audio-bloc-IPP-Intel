#pragma once
#include "EngineTelemetry.h"
#include "GraphRuntime.h"
namespace entropy::host {
class LoudnessControlBridge final {
    ModuleInstance* module_=nullptr;
    GraphRuntime* runtime_=nullptr;
    EntropyLoudnessApiV1 api_{};
    EntropyLoudnessDisplayApiV1 displayApi_{};
    bool displayAvailable_=false;
    std::array<EntropyLoudnessParameter,64> parameters_{};
    std::array<EntropyLoudnessMicrophone,32> microphones_{};
    unsigned parameterCount_=0,microphoneCount_=0,status_=0;
    uint64_t request_=0,pending_=0;
    EntropyResult result_=ENTROPY_OK;
    void refreshMicrophones();
public:
    bool initialize(GraphRuntime&);
    void observe(EngineControlSnapshot&,EngineTelemetrySnapshot&, bool allowSubmissions=true);
};
}
