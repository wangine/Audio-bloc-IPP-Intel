#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace entropy::host
{
inline constexpr std::uint32_t entropyProjectFormatVersion = 1;

struct ProjectModuleState
{
    std::string moduleId;
    std::string instanceId;
    std::string moduleVersion;
    std::uint32_t stateVersion = 0;
    std::vector<std::uint8_t> serializedState;
};

struct EntropyProjectDocument
{
    std::uint32_t formatVersion = entropyProjectFormatVersion;
    std::uint32_t hostEngineRate = 192000;
    std::string graphTopology = "entropy.modular.default.v1";
    ProjectModuleState crossover;
    ProjectModuleState outputMatrix;
    ProjectModuleState headroom;
    ProjectModuleState center;
    ProjectModuleState loudness;
    ProjectModuleState parametricEq;
};

struct ProjectIoResult
{
    bool ok = false;
    std::string detail;
};

ProjectIoResult saveProjectAtomic(const std::filesystem::path& path,
                                  const EntropyProjectDocument& project) noexcept;
ProjectIoResult loadProject(const std::filesystem::path& path,
                            EntropyProjectDocument& project) noexcept;
} // namespace entropy::host
