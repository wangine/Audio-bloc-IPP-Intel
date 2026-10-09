#pragma once

#include <entropy/module_abi.h>

#include <filesystem>
#include <memory>
#include <string>

namespace entropy::host
{
class ModuleLibrary final
{
public:
    ~ModuleLibrary();
    ModuleLibrary(const ModuleLibrary&) = delete;
    ModuleLibrary& operator=(const ModuleLibrary&) = delete;

    static std::shared_ptr<ModuleLibrary> load(const std::filesystem::path& path,
                                               std::string& error);

    const EntropyModuleApiV1& api() const noexcept { return api_; }
    const EntropyModuleDescriptor& descriptor() const noexcept { return descriptor_; }
    const std::filesystem::path& path() const noexcept { return path_; }

private:
    ModuleLibrary() = default;

    void* nativeLibrary_ = nullptr;
    std::filesystem::path path_;
    EntropyModuleApiV1 api_{};
    EntropyModuleDescriptor descriptor_{};
};

class ModuleInstance final
{
public:
    explicit ModuleInstance(std::shared_ptr<ModuleLibrary> library);
    ~ModuleInstance();
    ModuleInstance(const ModuleInstance&) = delete;
    ModuleInstance& operator=(const ModuleInstance&) = delete;

    bool create(std::string& error);
    bool prepare(std::uint32_t sampleRate, std::uint32_t maximumFrames,
                 std::uint32_t inputChannels, std::uint32_t outputChannels,
                 std::string& error);
    bool reset(std::string& error);
    bool queryExtension(const char* identifier, std::uint32_t version,
                        void* extensionApi, std::uint32_t extensionSize,
                        std::string& error) const;

    const ModuleLibrary& library() const noexcept { return *library_; }
    const EntropyModuleApiV1& api() const noexcept { return library_->api(); }
    EntropyModuleHandle handle() const noexcept { return handle_; }
    bool created() const noexcept { return handle_ != nullptr; }

private:
    std::shared_ptr<ModuleLibrary> library_;
    EntropyModuleHandle handle_ = nullptr;
};
} // namespace entropy::host
