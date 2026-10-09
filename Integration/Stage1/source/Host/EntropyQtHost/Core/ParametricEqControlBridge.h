#pragma once

#include "EngineTelemetry.h"
#include "GraphRuntime.h"
#include "ProjectPersistence.h"

#include <array>

namespace entropy::host
{
class ParametricEqControlBridge final
{
public:
    bool initialize(GraphRuntime& runtime);
    void observe(EngineControlSnapshot& control,
                 EngineTelemetrySnapshot& telemetry, bool allowSubmissions=true);
    bool available() const noexcept { return module_ != nullptr; }
    bool captureProjectState(ProjectModuleState& state) const;
    bool validateProjectState(const ProjectModuleState& state) const;
    bool restoreProjectState(const ProjectModuleState& state,
                             std::uint64_t& sequence);
    bool projectStateApplied(std::uint64_t sequence) const;

private:
    ModuleInstance* module_ = nullptr;
    GraphRuntime* runtime_ = nullptr;
    EntropyParametricEqApiV1 api_{};
    std::array<EntropyParametricEqParameterV1, 121> parameters_{};
    std::uint32_t parameterCount_ = 0;
    std::uint32_t commandStatus_ = 0;
    std::uint64_t requestSequence_ = 0;
    std::uint64_t pendingAppliedSequence_ = 0;
    EntropyResult commandResult_ = ENTROPY_OK;
    ProjectModuleState freshProjectState_{
        "audio.entropy.parametric-eq.fp64", "parametric-eq.program", "",
        ENTROPY_PARAMETRIC_EQ_STATE_VERSION, {}};
    ProjectModuleState retainedAbsentProjectState_{};
};
} // namespace entropy::host
