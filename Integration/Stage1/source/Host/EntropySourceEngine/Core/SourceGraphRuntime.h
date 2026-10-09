#pragma once
#include "InputSourceCatalog.h"
#include "SourceModuleResolver.h"
#include <vector>
namespace entropy::host
{
class SourceGraphRuntime final : public SourceModuleResolver
{
  public:
    bool prepare(const std::filesystem::path &, std::string &error);
    bool prepareMusic(const std::filesystem::path &, const InputSourceKey &, uint64_t revision,
                      uint32_t left, uint32_t right, std::string &error);
    ModuleInstance *findInstance(const char *) const noexcept override;
    bool readyStopped() const noexcept override { return ready_; }
    size_t instanceCount() const noexcept { return instances_.size(); }

  private:
    bool add(const std::filesystem::path &, const char *file, const char *identifier,
             const char *instance, uint32_t channels, uint32_t sourceRate, std::string &error);
    std::vector<std::unique_ptr<ModuleInstance>> instances_;
    std::vector<std::string> names_;
    bool ready_ = false;
};
} // namespace entropy::host
