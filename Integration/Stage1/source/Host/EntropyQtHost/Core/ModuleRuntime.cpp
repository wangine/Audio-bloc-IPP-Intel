#include "ModuleRuntime.h"

#include <cstring>
#include <windows.h>

namespace entropy::host
{
namespace
{
void ENTROPY_ABI_CALL hostLog(void*, std::int32_t, const char*) {}

std::string resultError(const char* operation, EntropyResult result)
{
    return std::string(operation) + " failed (EntropyResult " + std::to_string(result) + ")";
}

bool completeApi(const EntropyModuleApiV1& api) noexcept
{
    return api.abi_version == ENTROPY_MODULE_ABI_VERSION &&
           api.get_descriptor != nullptr && api.create != nullptr &&
           api.destroy != nullptr && api.prepare != nullptr && api.reset != nullptr &&
           (api.process_f64 != nullptr || api.query_extension != nullptr);
}
} // namespace

ModuleLibrary::~ModuleLibrary()
{
    if (nativeLibrary_ != nullptr) FreeLibrary(static_cast<HMODULE>(nativeLibrary_));
}

std::shared_ptr<ModuleLibrary> ModuleLibrary::load(const std::filesystem::path& path,
                                                   std::string& error)
{
    auto library = std::shared_ptr<ModuleLibrary>(new ModuleLibrary);
    const auto native = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (native == nullptr)
    {
        error = "LoadLibraryExW failed (Win32 " + std::to_string(GetLastError()) + ")";
        return {};
    }
    library->nativeLibrary_ = native;
    library->path_ = path;

    const auto getApi = reinterpret_cast<EntropyModuleGetApiFn>(
        GetProcAddress(native, ENTROPY_MODULE_GET_API_SYMBOL));
    if (getApi == nullptr)
    {
        error = "Missing export: " ENTROPY_MODULE_GET_API_SYMBOL;
        return {};
    }
    library->api_.struct_size = sizeof(library->api_);
    const auto result = getApi(ENTROPY_MODULE_ABI_VERSION, &library->api_);
    if (result != ENTROPY_OK || !completeApi(library->api_))
    {
        error = "Rejected ABI v1 or incomplete function table";
        return {};
    }
    library->descriptor_.struct_size = sizeof(library->descriptor_);
    if (library->api_.get_descriptor(&library->descriptor_) != ENTROPY_OK ||
        library->descriptor_.abi_version != ENTROPY_MODULE_ABI_VERSION ||
        std::memchr(library->descriptor_.identifier, '\0', sizeof(library->descriptor_.identifier)) == nullptr)
    {
        error = "Invalid module descriptor";
        return {};
    }
    return library;
}

ModuleInstance::ModuleInstance(std::shared_ptr<ModuleLibrary> library)
    : library_(std::move(library)) {}

ModuleInstance::~ModuleInstance()
{
    if (handle_ != nullptr) library_->api().destroy(handle_);
}

bool ModuleInstance::create(std::string& error)
{
    if (handle_ != nullptr)
    {
        error = "Instance already created";
        return false;
    }
    EntropyHostServices services{};
    services.struct_size = sizeof(services);
    services.abi_version = ENTROPY_MODULE_ABI_VERSION;
    services.log_non_realtime = hostLog;
    const auto result = library_->api().create(&services, &handle_);
    if (result != ENTROPY_OK || handle_ == nullptr)
    {
        handle_ = nullptr;
        error = resultError("create", result);
        return false;
    }
    return true;
}

bool ModuleInstance::prepare(std::uint32_t sampleRate, std::uint32_t maximumFrames,
                             std::uint32_t inputChannels, std::uint32_t outputChannels,
                             std::string& error)
{
    EntropyPrepareInfo info{};
    info.struct_size = sizeof(info);
    info.sample_rate = sampleRate;
    info.maximum_frames = maximumFrames;
    info.input_channels = inputChannels;
    info.output_channels = outputChannels;
    const auto result = library_->api().prepare(handle_, &info);
    if (result != ENTROPY_OK)
    {
        error = resultError("prepare", result);
        return false;
    }
    return true;
}

bool ModuleInstance::reset(std::string& error)
{
    const auto result = library_->api().reset(handle_);
    if (result != ENTROPY_OK)
    {
        error = resultError("reset", result);
        return false;
    }
    return true;
}

bool ModuleInstance::queryExtension(const char* identifier, std::uint32_t version,
                                    void* extensionApi, std::uint32_t extensionSize,
                                    std::string& error) const
{
    if (library_->api().query_extension == nullptr)
    {
        error = "query_extension unavailable";
        return false;
    }
    const auto result = library_->api().query_extension(
        identifier, version, extensionApi, extensionSize);
    if (result != ENTROPY_OK)
    {
        error = resultError("query_extension", result);
        return false;
    }
    return true;
}
} // namespace entropy::host
