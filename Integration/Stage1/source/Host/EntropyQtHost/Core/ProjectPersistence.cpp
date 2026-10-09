#include "ProjectPersistence.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <windows.h>

namespace entropy::host
{
namespace
{
constexpr char base64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<std::uint8_t>& bytes)
{
    std::string result;
    result.reserve(((bytes.size() + 2u) / 3u) * 4u);
    for (std::size_t offset = 0; offset < bytes.size(); offset += 3)
    {
        const auto remaining = bytes.size() - offset;
        const std::uint32_t value =
            (static_cast<std::uint32_t>(bytes[offset]) << 16u) |
            (remaining > 1 ? static_cast<std::uint32_t>(bytes[offset + 1]) << 8u : 0u) |
            (remaining > 2 ? static_cast<std::uint32_t>(bytes[offset + 2]) : 0u);
        result.push_back(base64Alphabet[(value >> 18u) & 63u]);
        result.push_back(base64Alphabet[(value >> 12u) & 63u]);
        result.push_back(remaining > 1 ? base64Alphabet[(value >> 6u) & 63u] : '=');
        result.push_back(remaining > 2 ? base64Alphabet[value & 63u] : '=');
    }
    return result;
}

int base64Value(char value) noexcept
{
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

bool base64Decode(std::string_view text, std::vector<std::uint8_t>& bytes)
{
    if (text.empty() || text.size() % 4u != 0u) return false;
    bytes.clear();
    bytes.reserve((text.size() / 4u) * 3u);
    for (std::size_t offset = 0; offset < text.size(); offset += 4)
    {
        const int a = base64Value(text[offset]);
        const int b = base64Value(text[offset + 1]);
        const int c = text[offset + 2] == '=' ? 0 : base64Value(text[offset + 2]);
        const int d = text[offset + 3] == '=' ? 0 : base64Value(text[offset + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) return false;
        if (text[offset + 2] == '=' && text[offset + 3] != '=') return false;
        if ((text[offset + 2] == '=' || text[offset + 3] == '=') &&
            offset + 4u != text.size()) return false;
        const std::uint32_t value = (static_cast<std::uint32_t>(a) << 18u) |
            (static_cast<std::uint32_t>(b) << 12u) |
            (static_cast<std::uint32_t>(c) << 6u) |
            static_cast<std::uint32_t>(d);
        bytes.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
        if (text[offset + 2] != '=')
            bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
        if (text[offset + 3] != '=')
            bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
    }
    return true;
}

std::uint64_t checksum(const std::vector<std::uint8_t>& bytes) noexcept
{
    std::uint64_t value = 14695981039346656037ull;
    for (const auto byte : bytes)
    {
        value ^= byte;
        value *= 1099511628211ull;
    }
    return value;
}

std::string checksumText(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << checksum(bytes);
    return stream.str();
}

std::string escapeJson(std::string_view text)
{
    std::string result;
    for (const char value : text)
    {
        if (value == '\\' || value == '"') result.push_back('\\');
        result.push_back(value);
    }
    return result;
}

bool jsonString(const std::string& json, std::string_view key, std::string& value)
{
    const auto marker = std::string("\"") + std::string(key) + "\"";
    auto position = json.find(marker);
    if (position == std::string::npos) return false;
    position = json.find(':', position + marker.size());
    if (position == std::string::npos) return false;
    position = json.find('"', position + 1u);
    if (position == std::string::npos) return false;
    ++position;
    value.clear();
    bool escaped = false;
    for (; position < json.size(); ++position)
    {
        const char current = json[position];
        if (escaped) { value.push_back(current); escaped = false; continue; }
        if (current == '\\') { escaped = true; continue; }
        if (current == '"') return true;
        value.push_back(current);
    }
    return false;
}

bool jsonUnsigned(const std::string& json, std::string_view key, std::uint32_t& value)
{
    const auto marker = std::string("\"") + std::string(key) + "\"";
    auto position = json.find(marker);
    if (position == std::string::npos) return false;
    position = json.find(':', position + marker.size());
    if (position == std::string::npos) return false;
    ++position;
    while (position < json.size() && (json[position] == ' ' || json[position] == '\t')) ++position;
    const auto end = json.data() + json.size();
    auto parsed = std::from_chars(json.data() + position, end, value);
    return parsed.ec == std::errc{};
}

bool moduleObject(const std::string& json, std::string_view moduleId,
                  std::string& object)
{
    // Locate the key and decode its value; JSON whitespace is not significant.
    std::size_t identity = 0;
    for (;;)
    {
        identity = json.find("\"module_id\"", identity);
        if (identity == std::string::npos) return false;
        std::string found;
        if (jsonString(json.substr(identity), "module_id", found) && found == moduleId)
            break;
        identity += 11;
    }
    const auto begin = json.rfind('{', identity);
    const auto end = json.find('}', identity);
    if (begin == std::string::npos || end == std::string::npos || end < begin)
        return false;
    object.assign(json, begin, end - begin + 1u);
    return true;
}

bool decodeModule(const std::string& json, std::string_view expectedModuleId,
                  ProjectModuleState& module)
{
    std::string object, encoding, expectedChecksum, encoded;
    if (!moduleObject(json, expectedModuleId, object) ||
        !jsonString(object, "module_id", module.moduleId) ||
        !jsonString(object, "instance_id", module.instanceId) ||
        !jsonString(object, "module_version", module.moduleVersion) ||
        !jsonUnsigned(object, "state_version", module.stateVersion) ||
        !jsonString(object, "encoding", encoding) || encoding != "base64" ||
        !jsonString(object, "state_checksum_fnv1a64", expectedChecksum) ||
        !jsonString(object, "serialized_state", encoded) ||
        !base64Decode(encoded, module.serializedState))
        return false;
    return checksumText(module.serializedState) == expectedChecksum;
}

ProjectIoResult writeAll(const std::filesystem::path& path, std::string_view data)
{
    const auto temporary = std::filesystem::path(path.wstring() + L".tmp");
    const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {false, "Cannot create temporary project file (Win32 " +
            std::to_string(GetLastError()) + ")"};
    std::size_t offset = 0;
    bool ok = true;
    while (offset < data.size())
    {
        const auto amount = static_cast<DWORD>((std::min)(
            data.size() - offset, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written = 0;
        if (!WriteFile(file, data.data() + offset, amount, &written, nullptr) || written != amount)
        {
            ok = false;
            break;
        }
        offset += written;
    }
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    const auto writeError = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    if (!ok)
    {
        DeleteFileW(temporary.c_str());
        return {false, "Cannot flush project file (Win32 " + std::to_string(writeError) + ")"};
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const auto moveError = GetLastError();
        DeleteFileW(temporary.c_str());
        return {false, "Cannot atomically replace project (Win32 " +
            std::to_string(moveError) + ")"};
    }
    return {true, "Project saved atomically"};
}
} // namespace

ProjectIoResult saveProjectAtomic(const std::filesystem::path& path,
                                  const EntropyProjectDocument& project) noexcept
{
    try
    {
        if (path.empty() || project.crossover.serializedState.empty() ||
            project.outputMatrix.serializedState.empty())
            return {false, "Project path or required module state is empty"};
        std::error_code directoryError;
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path(), directoryError);
        if (directoryError) return {false, "Cannot create project directory"};
        const auto crossoverEncoded = base64Encode(project.crossover.serializedState);
        const auto outputMatrixEncoded = base64Encode(project.outputMatrix.serializedState);
        std::ostringstream json;
        json << "{\n"
             << "  \"format\": \"EntropyProject\",\n"
             << "  \"format_version\": " << project.formatVersion << ",\n"
             << "  \"host_engine_rate\": " << project.hostEngineRate << ",\n"
             << "  \"graph_topology\": \"" << escapeJson(project.graphTopology) << "\",\n"
             << "  \"modules\": [\n"
             << "    {\n"
             << "      \"module_id\": \"" << escapeJson(project.crossover.moduleId) << "\",\n"
             << "      \"instance_id\": \"" << escapeJson(project.crossover.instanceId) << "\",\n"
             << "      \"module_version\": \"" << escapeJson(project.crossover.moduleVersion) << "\",\n"
             << "      \"state_version\": " << project.crossover.stateVersion << ",\n"
             << "      \"encoding\": \"base64\",\n"
             << "      \"state_checksum_fnv1a64\": \""
             << checksumText(project.crossover.serializedState) << "\",\n"
             << "      \"serialized_state\": \"" << crossoverEncoded << "\"\n"
             << "    },\n"
             << "    {\n"
             << "      \"module_id\": \"" << escapeJson(project.outputMatrix.moduleId) << "\",\n"
             << "      \"instance_id\": \"" << escapeJson(project.outputMatrix.instanceId) << "\",\n"
             << "      \"module_version\": \"" << escapeJson(project.outputMatrix.moduleVersion) << "\",\n"
             << "      \"state_version\": " << project.outputMatrix.stateVersion << ",\n"
             << "      \"encoding\": \"base64\",\n"
             << "      \"state_checksum_fnv1a64\": \""
             << checksumText(project.outputMatrix.serializedState) << "\",\n"
             << "      \"serialized_state\": \"" << outputMatrixEncoded << "\"\n"
             << "    }\n";
        if (!project.headroom.serializedState.empty())
        {
            const auto headroomEncoded = base64Encode(project.headroom.serializedState);
            json << "    ,{\n"
                 << "      \"module_id\": \"" << escapeJson(project.headroom.moduleId) << "\",\n"
                 << "      \"instance_id\": \"" << escapeJson(project.headroom.instanceId) << "\",\n"
                 << "      \"module_version\": \"" << escapeJson(project.headroom.moduleVersion) << "\",\n"
                 << "      \"state_version\": " << project.headroom.stateVersion << ",\n"
                 << "      \"encoding\": \"base64\",\n"
                 << "      \"state_checksum_fnv1a64\": \""
                 << checksumText(project.headroom.serializedState) << "\",\n"
                 << "      \"serialized_state\": \"" << headroomEncoded << "\"\n"
                 << "    }\n";
        }
        if (!project.center.serializedState.empty())
            json << "    ,{\n"
                 << "      \"module_id\": \"" << escapeJson(project.center.moduleId) << "\",\n"
                 << "      \"instance_id\": \"" << escapeJson(project.center.instanceId) << "\",\n"
                 << "      \"module_version\": \"" << escapeJson(project.center.moduleVersion) << "\",\n"
                 << "      \"state_version\": " << project.center.stateVersion << ",\n"
                 << "      \"encoding\": \"base64\",\n"
                 << "      \"state_checksum_fnv1a64\": \"" << checksumText(project.center.serializedState) << "\",\n"
                 << "      \"serialized_state\": \"" << base64Encode(project.center.serializedState) << "\"\n"
                 << "    }\n";
        if (!project.loudness.serializedState.empty())
            json << "    ,{\n"
                 << "      \"module_id\": \"" << escapeJson(project.loudness.moduleId) << "\",\n"
                 << "      \"instance_id\": \"" << escapeJson(project.loudness.instanceId) << "\",\n"
                 << "      \"module_version\": \"" << escapeJson(project.loudness.moduleVersion) << "\",\n"
                 << "      \"state_version\": " << project.loudness.stateVersion << ",\n"
                 << "      \"encoding\": \"base64\",\n"
                 << "      \"state_checksum_fnv1a64\": \"" << checksumText(project.loudness.serializedState) << "\",\n"
                 << "      \"serialized_state\": \"" << base64Encode(project.loudness.serializedState) << "\"\n"
                 << "    }\n";
        if (!project.parametricEq.serializedState.empty())
            json << "    ,{\n"
                 << "      \"module_id\": \"" << escapeJson(project.parametricEq.moduleId) << "\",\n"
                 << "      \"instance_id\": \"" << escapeJson(project.parametricEq.instanceId) << "\",\n"
                 << "      \"module_version\": \"" << escapeJson(project.parametricEq.moduleVersion) << "\",\n"
                 << "      \"state_version\": " << project.parametricEq.stateVersion << ",\n"
                 << "      \"encoding\": \"base64\",\n"
                 << "      \"state_checksum_fnv1a64\": \"" << checksumText(project.parametricEq.serializedState) << "\",\n"
                 << "      \"serialized_state\": \"" << base64Encode(project.parametricEq.serializedState) << "\"\n"
                 << "    }\n";
        json << "  ],\n"
             << "  \"reserved_sections\": [\"input_matrix\", "
                "\"src\", \"dac_clock_domains\", \"ui\"]\n"
             << "}\n";
        return writeAll(path, json.str());
    }
    catch (const std::exception& error) { return {false, error.what()}; }
    catch (...) { return {false, "Unknown project save failure"}; }
}

ProjectIoResult loadProject(const std::filesystem::path& path,
                            EntropyProjectDocument& project) noexcept
{
    try
    {
        const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return {false, "Cannot open project (Win32 " + std::to_string(GetLastError()) + ")"};
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 64ll * 1024ll * 1024ll)
        {
            CloseHandle(file);
            return {false, "Project size is invalid"};
        }
        std::string json(static_cast<std::size_t>(size.QuadPart), '\0');
        DWORD read = 0;
        const bool readOk = ReadFile(file, json.data(), static_cast<DWORD>(json.size()), &read, nullptr) &&
            read == json.size();
        CloseHandle(file);
        if (!readOk) return {false, "Cannot read complete project"};

        std::string format;
        EntropyProjectDocument loaded;
        if (!jsonString(json, "format", format) || format != "EntropyProject" ||
            !jsonUnsigned(json, "format_version", loaded.formatVersion) ||
            loaded.formatVersion != entropyProjectFormatVersion ||
            !jsonUnsigned(json, "host_engine_rate", loaded.hostEngineRate) ||
            !jsonString(json, "graph_topology", loaded.graphTopology) ||
            !decodeModule(json, "audio.entropy.crossover", loaded.crossover))
            return {false, "Project manifest is invalid or incompatible"};
        // Version-1 projects created before Output Matrix integration remain
        // loadable. A present matrix module, however, must validate completely.
        if (json.find("\"module_id\": \"audio.entropy.output-matrix.fp64-192\"") !=
            std::string::npos &&
            !decodeModule(json, "audio.entropy.output-matrix.fp64-192",
                          loaded.outputMatrix))
            return {false, "Output Matrix state checksum mismatch"};
        // Headroom was added later to format-v1 projects. Its presence is
        // optional for backward compatibility, but a present state must be exact.
        if (json.find("\"module_id\": \"audio.entropy.headroom.manager.fp64\"") !=
            std::string::npos &&
            !decodeModule(json, "audio.entropy.headroom.manager.fp64",
                          loaded.headroom))
            return {false, "Headroom state checksum mismatch"};
        // Absence is permitted only for legacy projects; a present state must validate.
        const bool hasCenterV2 = json.find(
            "\"audio.entropy.center-extractor.fp64-v2\"") != std::string::npos;
        const bool hasCenterV1 = json.find(
            "\"audio.entropy.center-extractor.fp64-golden\"") != std::string::npos;
        if ((hasCenterV2 || hasCenterV1 ||
             json.find("\"center.music\"") != std::string::npos) &&
            !decodeModule(json,
                hasCenterV2 ? "audio.entropy.center-extractor.fp64-v2"
                            : "audio.entropy.center-extractor.fp64-golden",
                loaded.center))
            return {false, "Center state checksum mismatch"};
        if ((json.find("\"audio.entropy.loudness.fp64\"") != std::string::npos ||
             json.find("\"loudness.program\"") != std::string::npos) &&
            !decodeModule(json, "audio.entropy.loudness.fp64", loaded.loudness))
            return {false, "Loudness state checksum mismatch"};
        if ((json.find("\"audio.entropy.parametric-eq.fp64\"") != std::string::npos ||
             json.find("\"parametric-eq.program\"") != std::string::npos) &&
            !decodeModule(json, "audio.entropy.parametric-eq.fp64",
                          loaded.parametricEq))
            return {false, "ParametricEQ state checksum mismatch"};
        project = std::move(loaded);
        return {true, "Project read and validated"};
    }
    catch (const std::exception& error) { return {false, error.what()}; }
    catch (...) { return {false, "Unknown project load failure"}; }
}
} // namespace entropy::host
