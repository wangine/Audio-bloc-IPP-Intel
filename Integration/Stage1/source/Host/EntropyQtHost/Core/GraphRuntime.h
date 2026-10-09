#pragma once

#include "InputSourceCatalog.h"
#include "SourceModuleResolver.h"
#include "ModuleRegistry.h"
#include "ModuleRuntime.h"
#include "LoudnessRuntime.h"
#include "StereoExpansionChain.h"

#include <memory>
#include <array>
#include <string>
#include <vector>

namespace entropy::host
{
enum class RuntimeInstanceState
{
    created,
    extensionValidated,
    preparedStopped,
    closedHardwareBoundary,
    fault
};

enum class PresencePolicy
{
    Required,
    OptionalWire
};

struct GraphMusicBinding
{
    bool enabled = false;
    InputSourceKey key;
    std::uint64_t revision = 0;
    std::uint32_t leftChannel = 0;
    std::uint32_t rightChannel = 1;
};

struct GraphRuntimeBuildOptions
{
    GraphMusicBinding music;
    bool musicSourceOnly = false;
    bool loudnessSlot1 = false;
    bool centerV2 = false;
};

struct RuntimeInstanceInfo
{
    std::string instanceId;
    std::string moduleIdentifier;
    RuntimeInstanceState state = RuntimeInstanceState::fault;
    std::uint32_t inputChannels = 0;
    std::uint32_t outputChannels = 0;
    PresencePolicy presencePolicy = PresencePolicy::Required;
    OptionalStageState optionalState = OptionalStageState::Active;
    std::string detail;
};

class GraphRuntime final : public SourceModuleResolver
{
public:
    ModuleInstance* findInstance(const char* id) const noexcept override { return const_cast<GraphRuntime*>(this)->findInstance(std::string(id)); }
    GraphRuntime();
    ~GraphRuntime();

    bool buildStopped(const ModuleRegistry& registry, std::string& error,
                      const GraphRuntimeBuildOptions& options = {});
    void shutdown() noexcept;

    bool readyStopped() const noexcept override { return readyStopped_; }
    bool outputGateClosed() const noexcept { return true; }
    std::size_t libraryCount() const noexcept { return libraries_.size(); }
    const std::vector<RuntimeInstanceInfo>& instances() const noexcept { return instanceInfo_; }
    ModuleInstance* findInstance(const std::string& instanceId) noexcept;
    const ModuleInstance* findInstance(const std::string& instanceId) const noexcept;
    RuntimeInstanceInfo* findInstanceInfo(const std::string& instanceId) noexcept;
    const RuntimeInstanceInfo* findInstanceInfo(const std::string& instanceId) const noexcept;
    const std::array<RuntimeInstanceInfo, 3>& expansionSlots() const noexcept
    {
        return expansionSlots_;
    }
    LoudnessRuntime& loudness() noexcept { return loudness_; }
    const LoudnessRuntime& loudness() const noexcept { return loudness_; }
    OptionalAudioStageRuntime& centerStage() noexcept { return centerStage_; }
    const OptionalAudioStageRuntime& centerStage() const noexcept { return centerStage_; }
    std::uint32_t centerOutputChannel() const noexcept { return centerOutputChannel_; }
    const std::string& centerModuleIdentifier() const noexcept
    {
        return centerModuleIdentifier_;
    }
    OptionalAudioStageRuntime& headroomStage() noexcept { return headroomStage_; }
    const OptionalAudioStageRuntime& headroomStage() const noexcept { return headroomStage_; }
    OptionalAudioStageRuntime& parametricEqStage() noexcept { return parametricEqStage_; }
    const OptionalAudioStageRuntime& parametricEqStage() const noexcept
    {
        return parametricEqStage_;
    }
    StereoExpansionChain& expansionChain() noexcept { return expansionChain_; }

private:
    std::vector<std::shared_ptr<ModuleLibrary>> libraries_;
    std::vector<std::unique_ptr<ModuleInstance>> instances_;
    std::vector<RuntimeInstanceInfo> instanceInfo_;
    bool readyStopped_ = false;
    LoudnessRuntime loudness_;
    OptionalAudioStageRuntime centerStage_{2, 3, OptionalFallback::SilenceOutputs};
    std::uint32_t centerOutputChannel_ = 2;
    std::string centerModuleIdentifier_ = "audio.entropy.center-extractor.fp64-golden";
    OptionalAudioStageRuntime headroomStage_{16, 16, OptionalFallback::ExactWire};
    OptionalAudioStageRuntime parametricEqStage_{2, 2, OptionalFallback::ExactWire};
    StereoExpansionChain expansionChain_;
    std::array<RuntimeInstanceInfo, 3> expansionSlots_{};
};

const char* runtimeStateName(RuntimeInstanceState state) noexcept;
} // namespace entropy::host

