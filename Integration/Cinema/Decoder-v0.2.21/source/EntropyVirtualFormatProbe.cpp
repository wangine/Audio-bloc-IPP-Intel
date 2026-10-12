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

namespace
{
std::wstring nameOf(IMMDevice* device)
{
    ComPtr<IPropertyStore> store;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &store))) return L"(unknown)";
    PROPVARIANT value{};
    PropVariantInit(&value);
    std::wstring result = L"(unknown)";
    if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) &&
        value.vt == VT_LPWSTR && value.pwszVal != nullptr)
        result = value.pwszVal;
    PropVariantClear(&value);
    return result;
}

WAVEFORMATEXTENSIBLE makeFormat(WORD channels, bool floatingPoint)
{
    WAVEFORMATEXTENSIBLE f{};
    f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f.Format.nChannels = channels;
    f.Format.nSamplesPerSec = 48000;
    f.Format.wBitsPerSample = floatingPoint ? 32 : 16;
    f.Format.nBlockAlign = channels * f.Format.wBitsPerSample / 8;
    f.Format.nAvgBytesPerSec = f.Format.nSamplesPerSec * f.Format.nBlockAlign;
    f.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    f.Samples.wValidBitsPerSample = f.Format.wBitsPerSample;
    if (channels == 2) f.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    if (channels == 6) f.dwChannelMask = KSAUDIO_SPEAKER_5POINT1_SURROUND;
    if (channels == 8) f.dwChannelMask = KSAUDIO_SPEAKER_7POINT1_SURROUND;
    f.SubFormat = floatingPoint ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
    return f;
}

const wchar_t* resultText(HRESULT hr)
{
    if (hr == S_OK) return L"YES";
    if (hr == S_FALSE) return L"closest";
    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) return L"no";
    return L"error";
}
}

int wmain()
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 2;
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&enumerator)))) return 3;
    ComPtr<IMMDeviceCollection> devices;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) return 4;
    UINT count = 0;
    devices->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> device;
        devices->Item(i, &device);
        const auto name = nameOf(device.Get());
        std::wstring lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
        if (lower.find(L"cable") == std::wstring::npos &&
            lower.find(L"vbmatrix") == std::wstring::npos)
            continue;

        ComPtr<IAudioClient> client;
        if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                    reinterpret_cast<void**>(client.GetAddressOf()))))
            continue;
        WAVEFORMATEX* mix = nullptr;
        client->GetMixFormat(&mix);
        std::wcout << L"\n" << name;
        if (mix != nullptr)
        {
            std::wcout << L"  [mix " << mix->nSamplesPerSec << L" Hz / "
                       << mix->nChannels << L" ch / " << mix->wBitsPerSample << L" bit]";
            CoTaskMemFree(mix);
        }
        std::wcout << L"\n";
        for (WORD ch : {WORD(2), WORD(6), WORD(8)})
        {
            for (bool fp : {false, true})
            {
                auto f = makeFormat(ch, fp);
                WAVEFORMATEX* closest = nullptr;
                const HRESULT shared = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                    &f.Format, &closest);
                CoTaskMemFree(closest);
                const HRESULT exclusive = client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                    &f.Format, nullptr);
                std::wcout << L"  " << ch << L"ch " << (fp ? L"float32" : L"pcm16")
                           << L": shared=" << resultText(shared)
                           << L" (0x" << std::hex << static_cast<unsigned>(shared) << L")"
                           << L", exclusive=" << resultText(exclusive)
                           << L" (0x" << static_cast<unsigned>(exclusive) << std::dec << L")\n";
            }
        }
    }
    CoUninitialize();
    return 0;
}
