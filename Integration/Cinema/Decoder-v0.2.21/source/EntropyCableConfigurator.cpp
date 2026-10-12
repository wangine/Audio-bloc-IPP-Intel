#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <iostream>
#include <string>

using Microsoft::WRL::ComPtr;

struct DeviceShareMode { DWORD mode; };
struct PolicyConfig : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, const PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

namespace
{
constexpr GUID policyClass {0x870af99c, 0x171d, 0x4f9e,
    {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};
constexpr GUID policyInterface {0xf8679f50, 0x850a, 0x41cf,
    {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};

std::wstring nameOf(IMMDevice* device)
{
    ComPtr<IPropertyStore> store;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &store))) return L"";
    PROPVARIANT value{};
    PropVariantInit(&value);
    std::wstring result;
    if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR)
        result = value.pwszVal;
    PropVariantClear(&value);
    return result;
}

ComPtr<IMMDevice> findCableOutput()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     IID_PPV_ARGS(&enumerator));
    ComPtr<IMMDeviceCollection> devices;
    enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &devices);
    UINT count = 0;
    devices->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> device;
        devices->Item(i, &device);
        auto name = nameOf(device.Get());
        if (name == L"CABLE Output (VB-Audio Virtual Cable)") return device;
    }
    return nullptr;
}

WAVEFORMATEXTENSIBLE format8()
{
    WAVEFORMATEXTENSIBLE f{};
    f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f.Format.nChannels = 8;
    f.Format.nSamplesPerSec = 48000;
    f.Format.wBitsPerSample = 16;
    f.Format.nBlockAlign = 16;
    f.Format.nAvgBytesPerSec = 768000;
    f.Format.cbSize = 22;
    f.Samples.wValidBitsPerSample = 16;
    f.dwChannelMask = KSAUDIO_SPEAKER_7POINT1_SURROUND;
    f.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    return f;
}
}

int wmain(int argc, wchar_t** argv)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto device = findCableOutput();
    if (device == nullptr)
    {
        std::cerr << "CABLE Output capture endpoint not found.\n";
        return 2;
    }
    wchar_t* id = nullptr;
    device->GetId(&id);
    ComPtr<IAudioClient> audio;
    device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                     reinterpret_cast<void**>(audio.GetAddressOf()));
    WAVEFORMATEX* before = nullptr;
    audio->GetMixFormat(&before);
    std::wcout << L"Endpoint: " << nameOf(device.Get()) << L"\nCurrent: "
               << before->nSamplesPerSec << L" Hz, " << before->nChannels
               << L" ch, " << before->wBitsPerSample << L" bit\n";
    CoTaskMemFree(before);

    if (argc < 2 || std::wstring_view(argv[1]) != L"--set-8ch")
    {
        std::cout << "No change made. Use --set-8ch to configure 48 kHz / 8 channels.\n";
        CoTaskMemFree(id);
        CoUninitialize();
        return 0;
    }

    ComPtr<PolicyConfig> policy;
    const HRESULT create = CoCreateInstance(policyClass, nullptr, CLSCTX_ALL,
        policyInterface, reinterpret_cast<void**>(policy.GetAddressOf()));
    if (FAILED(create))
    {
        std::cerr << "PolicyConfig unavailable: 0x" << std::hex << create << "\n";
        return 3;
    }
    auto desired = format8();
    const HRESULT changed = policy->SetDeviceFormat(id, &desired.Format, &desired.Format);
    std::cout << "SetDeviceFormat result: 0x" << std::hex << changed << "\n";
    CoTaskMemFree(id);
    CoUninitialize();
    return FAILED(changed) ? 4 : 0;
}
