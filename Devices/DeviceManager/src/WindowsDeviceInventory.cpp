#include "WindowsDeviceInventory.h"
#include "DeviceIdentityResolver.h"

#include <windows.h>
#include <audioclient.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <propkeydef.h>
#include <devpkey.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propsys.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cwchar>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace entropy::device
{
namespace
{
const PROPERTYKEY kKsAudioControllerInterfacePath = {
    {0x13e004d6, 0xb066, 0x43bd,
     {0x91, 0x3b, 0xa4, 0x15, 0xcd, 0x13, 0xda, 0x87}},
    3};

template <typename Type>
void release(Type*& value) noexcept
{
    if (value != nullptr) { value->Release(); value = nullptr; }
}

class ComScope final
{
public:
    ComScope() noexcept
    {
        const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        usable_ = SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;
        owns_ = result == S_OK || result == S_FALSE;
    }
    ~ComScope() { if (owns_) CoUninitialize(); }
    bool usable() const noexcept { return usable_; }
private:
    bool usable_ = false;
    bool owns_ = false;
};

std::string utf8(const wchar_t* text)
{
    if (text == nullptr || text[0] == 0) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0,
                                           nullptr, nullptr);
    if (count <= 1) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), count,
                        nullptr, nullptr);
    result.resize(static_cast<std::size_t>(count - 1));
    return result;
}

std::wstring wide(const std::string& text)
{
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1,
                                           nullptr, 0);
    if (count <= 1) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), count);
    result.resize(static_cast<std::size_t>(count - 1));
    return result;
}

template <std::size_t Size>
void copyText(char (&destination)[Size], const std::string& source) noexcept
{
    std::fill(std::begin(destination), std::end(destination), '\0');
    const auto count = (std::min)(Size - 1, source.size());
    std::memcpy(destination, source.data(), count);
}

std::string guidText(const GUID& guid)
{
    wchar_t buffer[64]{};
    return StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer))) > 0
        ? utf8(buffer) : std::string{};
}

std::string property(IPropertyStore* store, REFPROPERTYKEY key)
{
    if (store == nullptr) return {};
    PROPVARIANT value{};
    PropVariantInit(&value);
    if (FAILED(store->GetValue(key, &value))) return {};
    std::string result;
    if (value.vt == VT_LPWSTR) result = utf8(value.pwszVal);
    else if (value.vt == VT_BSTR) result = utf8(value.bstrVal);
    else if (value.vt == VT_CLSID && value.puuid != nullptr)
        result = guidText(*value.puuid);
    else if (value.vt == (VT_VECTOR | VT_LPWSTR))
    {
        for (ULONG index = 0; index < value.calpwstr.cElems; ++index)
        {
            const auto item = utf8(value.calpwstr.pElems[index]);
            if (item.empty()) continue;
            if (!result.empty()) result += ';';
            result += item;
        }
    }
    PropVariantClear(&value);
    return result;
}

std::string devNodeProperty(DEVINST node, const DEVPROPKEY& key)
{
    DEVPROPTYPE type = DEVPROP_TYPE_EMPTY;
    ULONG bytes = 0;
    auto status = CM_Get_DevNode_PropertyW(node, &key, &type, nullptr, &bytes, 0);
    if (status != CR_BUFFER_SMALL || bytes == 0) return {};
    std::vector<unsigned char> buffer(bytes);
    status = CM_Get_DevNode_PropertyW(node, &key, &type, buffer.data(),
                                      &bytes, 0);
    if (status != CR_SUCCESS) return {};

    const auto baseType = type & DEVPROP_MASK_TYPE;
    if (baseType == DEVPROP_TYPE_STRING)
        return utf8(reinterpret_cast<const wchar_t*>(buffer.data()));
    if (baseType == DEVPROP_TYPE_GUID && bytes >= sizeof(GUID))
        return guidText(*reinterpret_cast<const GUID*>(buffer.data()));
    if (baseType == DEVPROP_TYPE_STRING_LIST)
    {
        std::string result;
        const auto* item = reinterpret_cast<const wchar_t*>(buffer.data());
        const auto* end = reinterpret_cast<const wchar_t*>(buffer.data() + bytes);
        while (item < end && *item != 0)
        {
            const auto value = utf8(item);
            if (!value.empty())
            {
                if (!result.empty()) result += ';';
                result += value;
            }
            item += std::wcslen(item) + 1;
        }
        return result;
    }
    return {};
}

std::string devNodeId(DEVINST node)
{
    ULONG characters = 0;
    if (CM_Get_Device_ID_Size(&characters, node, 0) != CR_SUCCESS) return {};
    std::vector<wchar_t> buffer(static_cast<std::size_t>(characters) + 1, L'\0');
    if (CM_Get_Device_IDW(node, buffer.data(),
                         static_cast<ULONG>(buffer.size()), 0) != CR_SUCCESS)
        return {};
    return utf8(buffer.data());
}

std::string upper(std::string value);
std::string vidPid(const std::string& source);
std::string hardwareSerial(const std::string& interfacePath);

struct PnpIdentity
{
    std::string interface_path;
    std::string container_id;
    std::string physical_instance_id;
    std::string hardware_ids;
    std::string driver_identity;
};

PnpIdentity pnpIdentity(const std::string& endpointId)
{
    PnpIdentity result;
    auto endpointInstance = wide("SWD\\MMDEVAPI\\" + endpointId);
    if (endpointInstance.empty()) return result;
    DEVINST node = 0;
    if (CM_Locate_DevNodeW(&node, endpointInstance.data(),
                           CM_LOCATE_DEVNODE_PHANTOM) != CR_SUCCESS)
        return result;

    std::string firstUsbInstance;
    std::string firstUsbHardware;
    std::string selectedVidPid;
    for (unsigned level = 0; level < 12; ++level)
    {
        const auto id = devNodeId(node);
        if (id.empty()) break;
        if (level == 0)
            result.container_id = devNodeProperty(node, DEVPKEY_Device_ContainerId);
        else if (result.interface_path.empty())
            result.interface_path = id;

        const auto ids = devNodeProperty(node, DEVPKEY_Device_HardwareIds);
        const auto currentVidPid = vidPid(id + ";" + ids);
        const auto normalized = upper(id);
        if (normalized.rfind("USB\\VID_", 0) == 0)
        {
            if (firstUsbInstance.empty())
            {
                firstUsbInstance = id;
                firstUsbHardware = ids;
                selectedVidPid = currentVidPid;
            }
            if (!hardwareSerial(id).empty() &&
                (selectedVidPid.empty() || currentVidPid == selectedVidPid))
            {
                firstUsbInstance = id;
                firstUsbHardware = ids;
                selectedVidPid = currentVidPid;
            }
        }

        const auto provider = devNodeProperty(node, DEVPKEY_Device_DriverProvider);
        const auto description = devNodeProperty(node, DEVPKEY_Device_DriverDesc);
        const auto version = devNodeProperty(node, DEVPKEY_Device_DriverVersion);
        if (level > 0 && result.driver_identity.empty() &&
            (!provider.empty() || !description.empty() || !version.empty()))
        {
            result.driver_identity = provider;
            if (!description.empty())
                result.driver_identity +=
                    (result.driver_identity.empty() ? "" : " | ") + description;
            if (!version.empty())
                result.driver_identity +=
                    (result.driver_identity.empty() ? "" : " | ") + version;
        }

        DEVINST parent = 0;
        if (CM_Get_Parent(&parent, node, 0) != CR_SUCCESS) break;
        node = parent;
    }
    result.physical_instance_id = firstUsbInstance;
    result.hardware_ids = firstUsbHardware;
    return result;
}

std::string upper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
    return value;
}

std::string vidPid(const std::string& source)
{
    const auto normalized = upper(source);
    const auto vid = normalized.find("VID_");
    const auto pid = normalized.find("PID_");
    if (vid == std::string::npos || pid == std::string::npos ||
        vid + 8 > normalized.size() || pid + 8 > normalized.size())
        return {};
    return normalized.substr(vid, 8) + "&" + normalized.substr(pid, 8);
}

std::string hardwareSerial(const std::string& interfacePath)
{
    const auto normalized = upper(interfacePath);
    const auto vid = normalized.find("VID_");
    if (vid == std::string::npos) return {};
    auto separator = normalized.find('#', vid);
    if (separator == std::string::npos) separator = normalized.find('\\', vid);
    if (separator == std::string::npos || separator + 1 >= normalized.size()) return {};
    const auto end = normalized.find_first_of("#\\", separator + 1);
    const auto token = normalized.substr(separator + 1,
        end == std::string::npos ? std::string::npos : end - separator - 1);
    if (token.empty() || token.find('&') != std::string::npos ||
        token.find("VID_") != std::string::npos)
        return {};
    return token;
}

void addEvidence(EntropyDeviceInfo& device, std::uint32_t kind,
                 std::uint32_t stability, std::uint32_t flags,
                 const std::string& value)
{
    if (value.empty() || device.evidence_count >= ENTROPY_DEVICE_MAX_EVIDENCE) return;
    auto& item = device.evidence[device.evidence_count++];
    item = {};
    item.struct_size = sizeof(item);
    item.kind = kind;
    item.stability = stability;
    item.flags = flags;
    copyText(item.value_utf8, value);
}

DWORD channelMask(std::uint32_t channels) noexcept
{
    switch (channels)
    {
        case 1: return SPEAKER_FRONT_CENTER;
        case 2: return KSAUDIO_SPEAKER_STEREO;
        case 4: return KSAUDIO_SPEAKER_QUAD;
        case 6: return KSAUDIO_SPEAKER_5POINT1_SURROUND;
        case 8: return KSAUDIO_SPEAKER_7POINT1_SURROUND;
        default: return 0;
    }
}

void probeFormats(IMMDevice& device, EntropyDeviceInfo& info)
{
    IAudioClient* client = nullptr;
    if (FAILED(device.Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(&client))))
        return;
    constexpr std::array<std::uint32_t, 2> rates{96000, 192000};
    constexpr std::array<std::uint32_t, 6> widths{1, 2, 4, 6, 8, 16};
    for (const auto rate : rates)
    {
        for (const auto channels : widths)
        {
            for (const auto containerBits : {24u, 32u})
            {
                WAVEFORMATEXTENSIBLE format{};
                format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
                format.Format.nChannels = static_cast<WORD>(channels);
                format.Format.nSamplesPerSec = rate;
                format.Format.wBitsPerSample = static_cast<WORD>(containerBits);
                format.Format.nBlockAlign = static_cast<WORD>(channels * containerBits / 8);
                format.Format.nAvgBytesPerSec = rate * format.Format.nBlockAlign;
                format.Format.cbSize = sizeof(format) - sizeof(WAVEFORMATEX);
                format.Samples.wValidBitsPerSample = 24;
                format.dwChannelMask = channelMask(channels);
                format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                WAVEFORMATEX* closest = nullptr;
                const auto supported = client->IsFormatSupported(
                    AUDCLNT_SHAREMODE_EXCLUSIVE, &format.Format, &closest);
                if (closest != nullptr) CoTaskMemFree(closest);
                if (supported != S_OK || info.format_count >= ENTROPY_DEVICE_MAX_FORMATS)
                    continue;
                auto& exact = info.formats[info.format_count++];
                exact = {};
                exact.struct_size = sizeof(exact);
                exact.sample_rate = rate;
                exact.channels = channels;
                exact.container_bits = containerBits;
                exact.valid_bits = 24;
                exact.encoding = ENTROPY_DEVICE_PCM_INTEGER;
                info.maximum_channels = (std::max)(info.maximum_channels, channels);
            }
        }
    }
    client->Release();
}

std::string formatSummary(const EntropyDeviceInfo& info)
{
    std::string result;
    for (std::uint32_t index = 0; index < info.format_count; ++index)
    {
        const auto& value = info.formats[index];
        char item[64]{};
        std::snprintf(item, sizeof(item), "%u/%u/%u-in-%u",
                      value.sample_rate, value.channels,
                      value.valid_bits, value.container_bits);
        if (!result.empty()) result += ';';
        result += item;
    }
    return result;
}
}

EntropyResult WindowsDeviceInventory::refresh()
{
    ComScope com;
    if (!com.usable()) return ENTROPY_ERROR_PROCESSING;

    IMMDeviceEnumerator* enumerator = nullptr;
    auto result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(result)) return ENTROPY_ERROR_PROCESSING;

    std::string defaultId;
    IMMDevice* defaultDevice = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia,
                                                       &defaultDevice)))
    {
        LPWSTR id = nullptr;
        if (SUCCEEDED(defaultDevice->GetId(&id)))
        {
            defaultId = utf8(id);
            CoTaskMemFree(id);
        }
        defaultDevice->Release();
    }

    IMMDeviceCollection* collection = nullptr;
    result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATEMASK_ALL,
                                             &collection);
    if (FAILED(result)) { enumerator->Release(); return ENTROPY_ERROR_PROCESSING; }
    UINT count = 0;
    collection->GetCount(&count);
    std::vector<EntropyDeviceInfo> found;
    found.reserve(count);

    for (UINT index = 0; index < count; ++index)
    {
        IMMDevice* endpoint = nullptr;
        if (FAILED(collection->Item(index, &endpoint))) continue;
        LPWSTR endpointIdWide = nullptr;
        std::string endpointId;
        if (SUCCEEDED(endpoint->GetId(&endpointIdWide)))
        {
            endpointId = utf8(endpointIdWide);
            CoTaskMemFree(endpointIdWide);
        }
        if (endpointId.empty()) { endpoint->Release(); continue; }

        EntropyDeviceInfo info{};
        info.struct_size = sizeof(info);
        DWORD state = 0;
        endpoint->GetState(&state);
        info.active = (state & DEVICE_STATE_ACTIVE) != 0 ? 1u : 0u;
        info.is_default = endpointId == defaultId ? 1u : 0u;
        copyText(info.endpoint_id_utf8, endpointId);

        IPropertyStore* store = nullptr;
        std::string friendly;
        std::string container;
        std::string instance;
        std::string hardwareIds;
        std::string controllerPath;
        std::string driver;
        if (SUCCEEDED(endpoint->OpenPropertyStore(STGM_READ, &store)))
        {
            friendly = property(store, PKEY_Device_FriendlyName);
            container = property(store, PKEY_Device_ContainerId);
            instance = property(store, PKEY_Device_InstanceId);
            hardwareIds = property(store, PKEY_Device_HardwareIds);
            controllerPath = property(store, kKsAudioControllerInterfacePath);
            const auto provider = property(store, PKEY_Device_DriverProvider);
            const auto description = property(store, PKEY_Device_DriverDesc);
            const auto version = property(store, PKEY_Device_DriverVersion);
            driver = provider;
            if (!description.empty()) driver += (driver.empty() ? "" : " | ") + description;
            if (!version.empty()) driver += (driver.empty() ? "" : " | ") + version;
            store->Release();
        }
        const auto pnp = pnpIdentity(endpointId);
        if (!pnp.container_id.empty()) container = pnp.container_id;
        if (!pnp.physical_instance_id.empty()) instance = pnp.physical_instance_id;
        if (!pnp.hardware_ids.empty()) hardwareIds = pnp.hardware_ids;
        if (!pnp.interface_path.empty()) controllerPath = pnp.interface_path;
        if (!pnp.driver_identity.empty()) driver = pnp.driver_identity;
        copyText(info.display_name_utf8, friendly);

        const auto identitySource = controllerPath + ";" + instance + ";" + hardwareIds;
        const auto deviceVidPid = vidPid(identitySource);
        const auto serial = hardwareSerial(instance);
        const auto containerStability = serial.empty()
            ? ENTROPY_DEVICE_EVIDENCE_PORT_BOUND
            : ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE;
        const auto instanceStability = serial.empty()
            ? ENTROPY_DEVICE_EVIDENCE_PORT_BOUND
            : ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE;

        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_ENDPOINT_ID,
                    ENTROPY_DEVICE_EVIDENCE_VOLATILE,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_IDENTITY_ANCHOR |
                    ENTROPY_DEVICE_EVIDENCE_FLAG_OPEN_ADDRESS, endpointId);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_INTERFACE_PATH,
                    ENTROPY_DEVICE_EVIDENCE_PORT_BOUND,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_IDENTITY_ANCHOR, controllerPath);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_CONTAINER_ID,
                    containerStability,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_IDENTITY_ANCHOR, container);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_INSTANCE_ID,
                    instanceStability,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_IDENTITY_ANCHOR, instance);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_VID_PID,
                    ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_IDENTITY_ANCHOR, deviceVidPid);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_SERIAL_NUMBER,
                    ENTROPY_DEVICE_EVIDENCE_HARDWARE_IMMUTABLE,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_IDENTITY_ANCHOR, serial);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_HARDWARE_IDS,
                    ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_IDENTITY_ANCHOR, hardwareIds);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_FRIENDLY_NAME,
                    ENTROPY_DEVICE_EVIDENCE_DESCRIPTIVE,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_DIAGNOSTIC, friendly);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_DRIVER_IDENTITY,
                    ENTROPY_DEVICE_EVIDENCE_INSTALLATION_STABLE,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_DIAGNOSTIC, driver);

        if (info.active != 0) probeFormats(*endpoint, info);
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_CHANNEL_CAPABILITY,
                    ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_MATERIAL,
                    "max_channels=" + std::to_string(info.maximum_channels));
        addEvidence(info, ENTROPY_DEVICE_EVIDENCE_EXACT_FORMATS,
                    ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE,
                    ENTROPY_DEVICE_EVIDENCE_FLAG_MATERIAL,
                    formatSummary(info));
        normalizeDevice(info);
        found.push_back(info);
        endpoint->Release();
    }

    collection->Release();
    enumerator->Release();
    devices_ = std::move(found);
    return ENTROPY_OK;
}
}
