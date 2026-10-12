#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{
struct CoTaskMemWaveFormat
{
    WAVEFORMATEX* value = nullptr;
    ~CoTaskMemWaveFormat() { CoTaskMemFree(value); }
};

struct PropVariant
{
    PROPVARIANT value {};
    PropVariant() { PropVariantInit(&value); }
    ~PropVariant() { PropVariantClear(&value); }
};

std::wstring hrText(HRESULT hr)
{
    wchar_t* message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), 0,
                   reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring result = message != nullptr ? message : L"unknown error";
    if (message != nullptr)
        LocalFree(message);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n'))
        result.pop_back();
    return result;
}

void check(HRESULT hr, const wchar_t* operation)
{
    if (FAILED(hr))
        throw std::runtime_error(std::format("{} failed: 0x{:08X}",
            std::string(operation, operation + std::wcslen(operation)),
            static_cast<unsigned>(hr)));
}

std::wstring endpointName(IMMDevice* device)
{
    ComPtr<IPropertyStore> properties;
    check(device->OpenPropertyStore(STGM_READ, &properties), L"OpenPropertyStore");
    PropVariant name;
    check(properties->GetValue(PKEY_Device_FriendlyName, &name.value), L"GetValue");
    if (name.value.vt == VT_LPWSTR && name.value.pwszVal != nullptr)
        return name.value.pwszVal;
    return L"(unnamed)";
}

std::wstring endpointId(IMMDevice* device)
{
    wchar_t* id = nullptr;
    check(device->GetId(&id), L"GetId");
    std::wstring result = id != nullptr ? id : L"";
    CoTaskMemFree(id);
    return result;
}

ComPtr<IMMDevice> findUr23()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                           IID_PPV_ARGS(&enumerator)), L"CoCreateInstance");
    ComPtr<IMMDeviceCollection> collection;
    check(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection),
          L"EnumAudioEndpoints");
    UINT count = 0;
    check(collection->GetCount(&count), L"GetCount");
    std::wcout << L"Active capture endpoints: " << count << L"\n";
    for (UINT index = 0; index < count; ++index)
    {
        ComPtr<IMMDevice> device;
        check(collection->Item(index, &device), L"Item");
        const auto name = endpointName(device.Get());
        std::wcout << L"  [" << index << L"] " << name << L"\n";
        std::wstring lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
        if (lower.find(L"ur23") != std::wstring::npos ||
            lower.find(L"usb spdif rx") != std::wstring::npos)
            return device;
    }
    return nullptr;
}

WAVEFORMATEXTENSIBLE makePcmFormat(DWORD rate, WORD containerBits, WORD validBits)
{
    WAVEFORMATEXTENSIBLE format {};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = 2;
    format.Format.nSamplesPerSec = rate;
    format.Format.wBitsPerSample = containerBits;
    format.Format.nBlockAlign = static_cast<WORD>(2 * containerBits / 8);
    format.Format.nAvgBytesPerSec = rate * format.Format.nBlockAlign;
    format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    format.Samples.wValidBitsPerSample = validBits;
    format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    return format;
}

WAVEFORMATEXTENSIBLE makeFloatFormat()
{
    WAVEFORMATEXTENSIBLE format{};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = 2;
    format.Format.nSamplesPerSec = 48000;
    format.Format.wBitsPerSample = 32;
    format.Format.nBlockAlign = 8;
    format.Format.nAvgBytesPerSec = 384000;
    format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    format.Samples.wValidBitsPerSample = 32;
    format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    return format;
}

std::wstring formatDescription(const WAVEFORMATEX* f)
{
    WORD valid = f->wBitsPerSample;
    if (f->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        f->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
        valid = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(f)->Samples.wValidBitsPerSample;
    return std::format(L"{} Hz, {} ch, {} valid bits in {}-bit container, tag 0x{:04X}",
        f->nSamplesPerSec, f->nChannels, valid, f->wBitsPerSample, f->wFormatTag);
}

bool supportsExclusive(IAudioClient* client, const WAVEFORMATEX* format)
{
    return client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, format, nullptr) == S_OK;
}

void writeU16(FILE* file, std::uint16_t value) { fwrite(&value, sizeof(value), 1, file); }
void writeU32(FILE* file, std::uint32_t value) { fwrite(&value, sizeof(value), 1, file); }

void writeWavHeader(FILE* file, const WAVEFORMATEX* format, std::uint32_t bytes)
{
    fwrite("RIFF", 1, 4, file);
    writeU32(file, 36u + bytes);
    fwrite("WAVEfmt ", 1, 8, file);
    writeU32(file, 16);
    writeU16(file, format->wFormatTag);
    writeU16(file, format->nChannels);
    writeU32(file, format->nSamplesPerSec);
    writeU32(file, format->nAvgBytesPerSec);
    writeU16(file, format->nBlockAlign);
    writeU16(file, format->wBitsPerSample);
    fwrite("data", 1, 4, file);
    writeU32(file, bytes);
}

struct BurstCounts
{
    std::uint64_t littleEndian = 0;
    std::uint64_t bigEndian = 0;
};

BurstCounts countPreambles(const std::vector<std::uint8_t>& bytes)
{
    BurstCounts counts;
    for (std::size_t i = 0; i + 3 < bytes.size(); ++i)
    {
        if (bytes[i] == 0x72 && bytes[i + 1] == 0xF8 &&
            bytes[i + 2] == 0x1F && bytes[i + 3] == 0x4E)
            ++counts.littleEndian;
        if (bytes[i] == 0xF8 && bytes[i + 1] == 0x72 &&
            bytes[i + 2] == 0x4E && bytes[i + 3] == 0x1F)
            ++counts.bigEndian;
    }
    return counts;
}

BurstCounts countFloatPreambles(const std::vector<std::uint8_t>& bytes)
{
    BurstCounts counts;
    for (std::size_t i = 0; i + 7 < bytes.size(); i += 8)
    {
        float left = 0.0f;
        float right = 0.0f;
        std::memcpy(&left, bytes.data() + i, 4);
        std::memcpy(&right, bytes.data() + i + 4, 4);
        const auto a = static_cast<std::uint16_t>(
            static_cast<std::int16_t>(std::lrint(left * 32768.0f)));
        const auto b = static_cast<std::uint16_t>(
            static_cast<std::int16_t>(std::lrint(right * 32768.0f)));
        counts.littleEndian += a == 0xF872 && b == 0x4E1F;
        counts.bigEndian += a == 0x72F8 && b == 0x1F4E;
    }
    return counts;
}

struct Candidate
{
    DWORD rate;
    WORD containerBits;
    WORD validBits;
};

std::vector<Candidate> candidates()
{
    return {
        {48000, 16, 16}, {48000, 24, 24}, {48000, 32, 24}, {48000, 32, 32},
        {44100, 16, 16}, {96000, 16, 16}, {96000, 24, 24}, {96000, 32, 24}
    };
}

void probe(IMMDevice* device)
{
    ComPtr<IAudioClient> client;
    check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client), L"Activate");
    CoTaskMemWaveFormat mix;
    check(client->GetMixFormat(&mix.value), L"GetMixFormat");
    std::wcout << L"Windows shared mix format: " << formatDescription(mix.value) << L"\n";
    std::wcout << L"Exclusive-mode PCM formats:\n";
    for (const auto candidate : candidates())
    {
        const auto format = makePcmFormat(candidate.rate, candidate.containerBits,
                                          candidate.validBits);
        const bool supported = supportsExclusive(client.Get(), &format.Format);
        std::wcout << L"  " << formatDescription(&format.Format)
                   << L": " << (supported ? L"SUPPORTED" : L"no") << L"\n";
    }
}

Candidate chooseCaptureFormat(IAudioClient* client)
{
    // The Savitech UR23 driver can reject Initialize(E_INVALIDARG) after
    // IsFormatSupported has been called on the same IAudioClient. The product
    // path is fixed at the already verified carrier format, so open it
    // directly just like EntropySurroundDecoder does.
    (void) client;
    return {48000, 16, 16};
}

void capture(IMMDevice* device, double seconds, const std::filesystem::path& path)
{
    ComPtr<IAudioClient> client;
    check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client), L"Activate");
    const auto candidate = chooseCaptureFormat(client.Get());
    auto format = makePcmFormat(candidate.rate, candidate.containerBits, candidate.validBits);
    WAVEFORMATEX activeFormat = format.Format;
    bool sharedFloat = false;
    bool eventDriven = true;
    CoTaskMemWaveFormat sharedMix;

    REFERENCE_TIME defaultPeriod = 0;
    REFERENCE_TIME minimumPeriod = 0;
    check(client->GetDevicePeriod(&defaultPeriod, &minimumPeriod), L"GetDevicePeriod");
    REFERENCE_TIME bufferDuration = defaultPeriod;
    constexpr DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, streamFlags,
                                    bufferDuration, bufferDuration,
                                    &format.Format, nullptr);
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        UINT32 alignedFrames = 0;
        check(client->GetBufferSize(&alignedFrames), L"GetBufferSize(alignment)");
        client.Reset();
        check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client),
              L"Activate(aligned)");
        bufferDuration = static_cast<REFERENCE_TIME>(
            (10'000'000.0 * alignedFrames / candidate.rate) + 0.5);
        hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, streamFlags,
                                bufferDuration, bufferDuration,
                                &format.Format, nullptr);
    }
    if (FAILED(hr))
    {
        std::wcerr << L"Exclusive initialize failed: 0x" << std::hex
                   << static_cast<unsigned>(hr) << L" " << hrText(hr)
                   << L"; trying shared float diagnostic.\n" << std::dec;
        client.Reset();
        check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client),
              L"Activate(shared)");
        auto cleanFloat = makeFloatFormat();
        activeFormat = cleanFloat.Format;
        sharedFloat = true;
        check(client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0,
                                 &cleanFloat.Format, nullptr),
              L"Initialize(shared clean float)");
        eventDriven = false;
    }
    else
    {
        activeFormat = format.Format;
    }

    ComPtr<IAudioCaptureClient> captureClient;
    check(client->GetService(IID_PPV_ARGS(&captureClient)), L"GetService");
    HANDLE captureEvent = nullptr;
    if (eventDriven)
    {
        captureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (captureEvent == nullptr)
            check(HRESULT_FROM_WIN32(GetLastError()), L"CreateEvent");
        check(client->SetEventHandle(captureEvent), L"SetEventHandle");
    }
    check(client->Start(), L"Start");

    std::vector<std::uint8_t> bytes;
    const std::size_t estimated = static_cast<std::size_t>(
        seconds * activeFormat.nSamplesPerSec * activeFormat.nBlockAlign);
    bytes.reserve(estimated + estimated / 4);
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(static_cast<long long>(seconds * 1000.0));
    std::uint64_t discontinuities = 0;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (eventDriven)
            WaitForSingleObject(captureEvent, 100);
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        UINT32 packetFrames = 0;
        check(captureClient->GetNextPacketSize(&packetFrames), L"GetNextPacketSize");
        if (packetFrames == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        check(captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr), L"GetBuffer");
        const std::size_t packetBytes = static_cast<std::size_t>(frames) * activeFormat.nBlockAlign;
        const auto oldSize = bytes.size();
        bytes.resize(oldSize + packetBytes);
        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr)
            std::memset(bytes.data() + oldSize, 0, packetBytes);
        else
            std::memcpy(bytes.data() + oldSize, data, packetBytes);
        if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0)
            ++discontinuities;
        check(captureClient->ReleaseBuffer(frames), L"ReleaseBuffer");
    }
    client->Stop();
    if (captureEvent != nullptr)
        CloseHandle(captureEvent);

    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr)
        throw std::runtime_error("Could not create capture WAV");
    writeWavHeader(file, &activeFormat, static_cast<std::uint32_t>(bytes.size()));
    fwrite(bytes.data(), 1, bytes.size(), file);
    fclose(file);

    const auto bursts = sharedFloat ? countFloatPreambles(bytes) : countPreambles(bytes);
    std::wcout << L"Captured: " << path.wstring() << L"\n"
               << L"Mode: " << (sharedFloat ? L"SHARED FLOAT DIAGNOSTIC" : L"EXCLUSIVE") << L"\n"
               << L"Format: " << formatDescription(&activeFormat) << L"\n"
               << L"Audio bytes: " << bytes.size() << L"\n"
               << L"Discontinuities: " << discontinuities << L"\n"
               << L"IEC61937 Pa/Pb little-endian hits: " << bursts.littleEndian << L"\n"
               << L"IEC61937 Pa/Pb big-endian hits: " << bursts.bigEndian << L"\n";
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), L"CoInitializeEx");
        const auto uninitialize = [] { CoUninitialize(); };
        struct Guard { decltype(uninitialize)& fn; ~Guard() { fn(); } } guard { uninitialize };

        auto device = findUr23();
        if (device == nullptr)
        {
            std::wcerr << L"UR23 capture endpoint was not found.\n";
            return 2;
        }
        std::wcout << L"Selected: " << endpointName(device.Get()) << L"\n"
                   << L"Endpoint ID: " << endpointId(device.Get()) << L"\n";
        probe(device.Get());

        if (argc >= 2 && std::wstring_view(argv[1]) == L"--capture")
        {
            const double seconds = argc >= 3 ? std::wcstod(argv[2], nullptr) : 10.0;
            const std::filesystem::path output = argc >= 4 ? argv[3] : L"ur23-capture.wav";
            capture(device.Get(), seconds, output);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << "\n";
        return 1;
    }
}
