#include "ModuleRegistry.h"

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <system_error>
#include <windows.h>

namespace entropy::host
{
namespace
{
std::string windowsError(const char* operation)
{
    return std::string(operation) + " failed (Win32 " + std::to_string(GetLastError()) + ")";
}

bool hasRequiredApi(const EntropyModuleApiV1& api) noexcept
{
    return api.abi_version == ENTROPY_MODULE_ABI_VERSION &&
           api.get_descriptor != nullptr && api.create != nullptr &&
           api.destroy != nullptr && api.prepare != nullptr &&
           (api.process_f64 != nullptr || api.query_extension != nullptr);
}

bool terminated(const char* text, std::size_t capacity) noexcept
{
    return std::memchr(text, '\0', capacity) != nullptr;
}
} // namespace

void ModuleRegistry::scan(const std::filesystem::path& root)
{
    root_ = root;
    records_.clear();

    std::error_code error;
    if (!std::filesystem::is_directory(root_, error))
    {
        ModuleRecord record;
        record.path = root_;
        record.error = "Module root is not a directory";
        records_.push_back(std::move(record));
        return;
    }

    std::vector<std::filesystem::path> candidates;
    for (std::filesystem::directory_iterator it(
             root_, std::filesystem::directory_options::skip_permission_denied, error), end;
         it != end; it.increment(error))
    {
        if (error)
        {
            error.clear();
            continue;
        }
        if (!it->is_regular_file(error)) continue;
        auto extension = it->path().extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
        if (extension == L".dll") candidates.push_back(it->path());
    }
    std::sort(candidates.begin(), candidates.end());
    records_.reserve(candidates.size());
    for (const auto& path : candidates) records_.push_back(inspect(path));
}

ModuleRecord ModuleRegistry::inspect(const std::filesystem::path& path)
{
    ModuleRecord record;
    record.path = path;

    const auto library = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (library == nullptr)
    {
        record.error = windowsError("LoadLibraryExW");
        return record;
    }

    const auto getApi = reinterpret_cast<EntropyModuleGetApiFn>(
        GetProcAddress(library, ENTROPY_MODULE_GET_API_SYMBOL));
    if (getApi == nullptr)
    {
        record.error = "Missing export: " ENTROPY_MODULE_GET_API_SYMBOL;
        FreeLibrary(library);
        return record;
    }

    EntropyModuleApiV1 api{};
    api.struct_size = sizeof(api);
    const auto apiResult = getApi(ENTROPY_MODULE_ABI_VERSION, &api);
    if (apiResult != ENTROPY_OK || !hasRequiredApi(api))
    {
        record.error = "Rejected ABI v1 or incomplete function table";
        FreeLibrary(library);
        return record;
    }

    record.descriptor = {};
    record.descriptor.struct_size = sizeof(record.descriptor);
    if (api.get_descriptor(&record.descriptor) != ENTROPY_OK)
    {
        record.error = "Module descriptor unavailable";
        FreeLibrary(library);
        return record;
    }
    if (record.descriptor.abi_version != ENTROPY_MODULE_ABI_VERSION ||
        record.descriptor.module_kind == ENTROPY_MODULE_UNKNOWN ||
        !terminated(record.descriptor.identifier, sizeof(record.descriptor.identifier)) ||
        !terminated(record.descriptor.name, sizeof(record.descriptor.name)) ||
        record.descriptor.identifier[0] == '\0')
    {
        record.error = "Invalid module descriptor";
        FreeLibrary(library);
        return record;
    }

    record.state = ModuleLoadState::loaded;
    FreeLibrary(library);
    return record;
}

const ModuleRecord* ModuleRegistry::findLoaded(const std::string& identifier) const noexcept
{
    for (const auto& record : records_)
    {
        if (record.state == ModuleLoadState::loaded && identifier == record.descriptor.identifier)
            return &record;
    }
    return nullptr;
}

const ModuleRecord* ModuleRegistry::findByFileName(
    const std::filesystem::path& fileName) const noexcept
{
    for (const auto& record : records_)
        if (_wcsicmp(record.path.filename().c_str(), fileName.c_str()) == 0)
            return &record;
    return nullptr;
}

std::size_t ModuleRegistry::loadedCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(records_.begin(), records_.end(),
        [](const ModuleRecord& record) { return record.state == ModuleLoadState::loaded; }));
}

std::size_t ModuleRegistry::rejectedCount() const noexcept
{
    return records_.size() - loadedCount();
}

const char* moduleKindName(std::uint32_t kind) noexcept
{
    switch (kind)
    {
        case ENTROPY_MODULE_TEST: return "Test";
        case ENTROPY_MODULE_SRC: return "SRC";
        case ENTROPY_MODULE_CROSSOVER: return "Crossover";
        case ENTROPY_MODULE_CENTER_EXTRACTOR: return "Center";
        case ENTROPY_MODULE_LOUDNESS: return "Loudness";
        case ENTROPY_MODULE_PARAMETRIC_EQ: return "Parametric EQ";
        case ENTROPY_MODULE_INPUT_MATRIX: return "Input Matrix";
        case ENTROPY_MODULE_CLOCK_SYNC: return "Clock Sync";
        case ENTROPY_MODULE_DAC_SINK: return "DAC Sink";
        case ENTROPY_MODULE_ENDPOINT: return "Endpoint";
        case ENTROPY_MODULE_PCM_CONVERTER: return "PCM Converter";
        case ENTROPY_MODULE_OUTPUT_MATRIX: return "Output Matrix";
        case ENTROPY_MODULE_HEADROOM_MANAGER: return "Headroom";
        case ENTROPY_MODULE_FINAL_GUARDIAN: return "Guardian";
        case ENTROPY_MODULE_DEVICE_MANAGER: return "Device Manager";
        default: return "Unknown";
    }
}
} // namespace entropy::host

