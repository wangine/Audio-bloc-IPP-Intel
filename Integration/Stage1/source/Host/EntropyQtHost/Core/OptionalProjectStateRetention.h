#pragma once

#include "ProjectPersistence.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>

namespace entropy::host
{
class OptionalProjectStateRetention final
{
public:
    OptionalProjectStateRetention(std::string moduleId, std::string instanceId,
                                  std::uint32_t stateVersion);
    OptionalProjectStateRetention(std::string moduleId, std::string instanceId,
                                  std::initializer_list<std::uint32_t> stateVersions);

    bool retain(const ProjectModuleState& state);
    ProjectModuleState forSave(bool modulePresent,
                               const ProjectModuleState& captured) const;
    const ProjectModuleState& retained() const noexcept { return retained_; }

private:
    bool compatible(const ProjectModuleState& state) const noexcept;

    std::string moduleId_;
    std::string instanceId_;
    std::array<std::uint32_t, 4> stateVersions_{};
    std::size_t stateVersionCount_ = 0;
    ProjectModuleState retained_;
};
} // namespace entropy::host
