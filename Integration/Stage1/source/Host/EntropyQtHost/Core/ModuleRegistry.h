#pragma once

#include <entropy/module_abi.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace entropy::host
{
enum class ModuleLoadState
{
    loaded,
    rejected
};

struct ModuleRecord
{
    std::filesystem::path path;
    ModuleLoadState state = ModuleLoadState::rejected;
    EntropyModuleDescriptor descriptor{};
    std::string error;
};

class ModuleRegistry
{
public:
    void scan(const std::filesystem::path& root);

    const std::filesystem::path& root() const noexcept { return root_; }
    const std::vector<ModuleRecord>& records() const noexcept { return records_; }
    const ModuleRecord* findLoaded(const std::string& identifier) const noexcept;
    const ModuleRecord* findByFileName(const std::filesystem::path& fileName) const noexcept;
    std::size_t loadedCount() const noexcept;
    std::size_t rejectedCount() const noexcept;

private:
    static ModuleRecord inspect(const std::filesystem::path& path);

    std::filesystem::path root_;
    std::vector<ModuleRecord> records_;
};

const char* moduleKindName(std::uint32_t kind) noexcept;
} // namespace entropy::host
