#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <unknwn.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#include "iasiodrv.h"

namespace
{
// BRAVO-X x64: {2B04D35E-3C1A-4A94-9595-4E2EB6EE8A1E}
const CLSID CLSID_BravoX =
{ 0x2b04d35e, 0x3c1a, 0x4a94, { 0x95, 0x95, 0x4e, 0x2e, 0xb6, 0xee, 0x8a, 0x1e } };

std::vector<ASIOBufferInfo> gBuffers;
std::vector<std::uint8_t> gAudio;
std::atomic<std::uint64_t> gFrames { 0 };
std::atomic<std::uint64_t> gCallbacks { 0 };
long gBufferFrames = 0;
long gChannels = 0;
int gBytesPerSample = 0;

int bytesPerSample(ASIOSampleType type)
{
    switch (type)
    {
        case ASIOSTInt16LSB: return 2;
        case ASIOSTInt24LSB: return 3;
        case ASIOSTInt32LSB:
        case ASIOSTInt32LSB16:
        case ASIOSTInt32LSB18:
        case ASIOSTInt32LSB20:
        case ASIOSTInt32LSB24: return 4;
        default: return 0;
    }
}

const char* sampleTypeName(ASIOSampleType type)
{
    switch (type)
    {
        case ASIOSTInt16LSB: return "ASIOSTInt16LSB";
        case ASIOSTInt24LSB: return "ASIOSTInt24LSB";
        case ASIOSTInt32LSB: return "ASIOSTInt32LSB";
        case ASIOSTInt32LSB16: return "ASIOSTInt32LSB16";
        case ASIOSTInt32LSB18: return "ASIOSTInt32LSB18";
        case ASIOSTInt32LSB20: return "ASIOSTInt32LSB20";
        case ASIOSTInt32LSB24: return "ASIOSTInt32LSB24";
        default: return "unsupported";
    }
}

void process(long half)
{
    if (half < 0 || half > 1 || gChannels < 2 || gBytesPerSample == 0)
        return;
    const auto firstFrame = gFrames.fetch_add(static_cast<std::uint64_t>(gBufferFrames));
    const auto capacityFrames = gAudio.size() /
        static_cast<std::size_t>(gChannels * gBytesPerSample);
    const auto frames = static_cast<long>(std::min<std::uint64_t>(
        static_cast<std::uint64_t>(gBufferFrames),
        firstFrame < capacityFrames ? capacityFrames - firstFrame : 0));
    for (long frame = 0; frame < frames; ++frame)
    {
        for (long channel = 0; channel < gChannels; ++channel)
        {
            const auto* source = static_cast<const std::uint8_t*>(
                gBuffers[static_cast<std::size_t>(channel)].buffers[half]);
            if (source == nullptr)
                continue;
            const auto sourceOffset = static_cast<std::size_t>(frame * gBytesPerSample);
            const auto destOffset = static_cast<std::size_t>(
                ((firstFrame + frame) * gChannels + channel) * gBytesPerSample);
            std::memcpy(gAudio.data() + destOffset, source + sourceOffset,
                        static_cast<std::size_t>(gBytesPerSample));
        }
    }
    ++gCallbacks;
}

void bufferSwitch(long half, ASIOBool) { process(half); }
void sampleRateChanged(ASIOSampleRate) {}
long asioMessage(long selector, long value, void*, double*)
{
    if (selector == kAsioSelectorSupported)
        return value == kAsioEngineVersion || value == kAsioSupportsTimeInfo;
    if (selector == kAsioEngineVersion) return 2;
    if (selector == kAsioSupportsTimeInfo) return 1;
    return 0;
}
ASIOTime* bufferSwitchTimeInfo(ASIOTime* time, long half, ASIOBool)
{
    process(half);
    return time;
}

void u16(FILE* f, std::uint16_t v) { fwrite(&v, sizeof(v), 1, f); }
void u32(FILE* f, std::uint32_t v) { fwrite(&v, sizeof(v), 1, f); }
void writeWav(const std::filesystem::path& path, DWORD rate, WORD channels,
              WORD bits, const std::vector<std::uint8_t>& audio, std::size_t bytes)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr)
        throw std::runtime_error("cannot create WAV");
    fwrite("RIFF", 1, 4, file); u32(file, static_cast<std::uint32_t>(36 + bytes));
    fwrite("WAVEfmt ", 1, 8, file); u32(file, 16); u16(file, 1); u16(file, channels);
    u32(file, rate); u32(file, rate * channels * bits / 8);
    u16(file, static_cast<std::uint16_t>(channels * bits / 8)); u16(file, bits);
    fwrite("data", 1, 4, file); u32(file, static_cast<std::uint32_t>(bytes));
    fwrite(audio.data(), 1, bytes, file); fclose(file);
}

std::uint16_t wordAt(const std::uint8_t* sample, int lane)
{
    return static_cast<std::uint16_t>(sample[lane] | (sample[lane + 1] << 8));
}

void reportBursts(const std::vector<std::uint8_t>& audio, std::size_t frames)
{
    for (int lane = 0; lane + 1 < gBytesPerSample; ++lane)
    {
        std::uint64_t normal = 0;
        std::uint64_t swapped = 0;
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const auto* left = audio.data() +
                (frame * static_cast<std::size_t>(gChannels)) * gBytesPerSample;
            const auto* right = left + gBytesPerSample;
            const auto a = wordAt(left, lane);
            const auto b = wordAt(right, lane);
            normal += a == 0xF872 && b == 0x4E1F;
            swapped += a == 0x72F8 && b == 0x1F4E;
        }
        std::cout << "IEC61937 lane " << lane << " Pa/Pb normal=" << normal
                  << " swapped=" << swapped << "\n";
    }
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    const int seconds = argc >= 2 ? std::max(1, _wtoi(argv[1])) : 10;
    const std::filesystem::path output = argc >= 3 ? argv[2] : L"bravo-capture.wav";
    constexpr wchar_t driverPath[] =
        L"C:\\Program Files (x86)\\SaviAudio\\BRAVO-X\\x64\\BravoASIO.dll";

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HMODULE module = LoadLibraryW(driverPath);
    if (module == nullptr) { std::cerr << "LoadLibrary failed " << GetLastError() << "\n"; return 2; }
    using GetClassObjectFn = HRESULT(__stdcall*)(REFCLSID, REFIID, void**);
    const auto getClassObject = reinterpret_cast<GetClassObjectFn>(
        GetProcAddress(module, "DllGetClassObject"));
    IClassFactory* factory = nullptr;
    HRESULT hr = getClassObject != nullptr ? getClassObject(
        CLSID_BravoX, IID_IClassFactory, reinterpret_cast<void**>(&factory)) : E_FAIL;
    if (FAILED(hr) || factory == nullptr) { std::cerr << "DllGetClassObject 0x" << std::hex << hr << "\n"; return 3; }
    IASIO* driver = nullptr;
    hr = factory->CreateInstance(nullptr, CLSID_BravoX, reinterpret_cast<void**>(&driver));
    if (FAILED(hr) || driver == nullptr) { std::cerr << "CreateInstance 0x" << std::hex << hr << "\n"; return 4; }
    if (driver->init(GetConsoleWindow()) != ASIOTrue)
    {
        char error[124] {}; driver->getErrorMessage(error);
        std::cerr << "ASIO init failed: " << error << "\n"; return 5;
    }
    char name[32] {}; driver->getDriverName(name);
    long inputs = 0, outputs = 0;
    driver->getChannels(&inputs, &outputs);
    std::cout << "Driver: " << name << " channels=" << inputs << " in / "
              << outputs << " out\n";
    if (driver->canSampleRate(48000.0) == ASE_OK)
        driver->setSampleRate(48000.0);
    ASIOSampleRate rate = 0; driver->getSampleRate(&rate);
    long minimum = 0, maximum = 0, preferred = 0, granularity = 0;
    driver->getBufferSize(&minimum, &maximum, &preferred, &granularity);
    std::cout << "Rate=" << rate << " buffers min/max/preferred/granularity="
              << minimum << "/" << maximum << "/" << preferred << "/" << granularity << "\n";
    gChannels = std::min(2L, inputs);
    if (gChannels < 2) { std::cerr << "Need two ASIO inputs\n"; return 6; }
    ASIOChannelInfo firstInfo {}; firstInfo.channel = 0; firstInfo.isInput = ASIOTrue;
    if (driver->getChannelInfo(&firstInfo) != ASE_OK) return 7;
    gBytesPerSample = bytesPerSample(firstInfo.type);
    std::cout << "Input type=" << sampleTypeName(firstInfo.type) << "\n";
    if (gBytesPerSample == 0) return 8;
    for (long channel = 0; channel < gChannels; ++channel)
    {
        ASIOChannelInfo info {}; info.channel = channel; info.isInput = ASIOTrue;
        driver->getChannelInfo(&info);
        std::cout << "Input " << channel + 1 << ": " << info.name << " type="
                  << sampleTypeName(info.type) << "\n";
    }
    gBufferFrames = preferred > 0 ? preferred : 256;
    gBuffers.resize(static_cast<std::size_t>(gChannels));
    for (long channel = 0; channel < gChannels; ++channel)
    {
        gBuffers[static_cast<std::size_t>(channel)].isInput = ASIOTrue;
        gBuffers[static_cast<std::size_t>(channel)].channelNum = channel;
    }
    const auto capacityFrames = static_cast<std::size_t>(rate * seconds + maximum * 4);
    gAudio.resize(capacityFrames * static_cast<std::size_t>(gChannels * gBytesPerSample));
    ASIOCallbacks callbacks {};
    callbacks.bufferSwitch = bufferSwitch;
    callbacks.sampleRateDidChange = sampleRateChanged;
    callbacks.asioMessage = asioMessage;
    callbacks.bufferSwitchTimeInfo = bufferSwitchTimeInfo;
    ASIOError error = driver->createBuffers(gBuffers.data(), gChannels,
                                             gBufferFrames, &callbacks);
    if (error != ASE_OK) { std::cerr << "createBuffers=" << error << "\n"; return 9; }
    error = driver->start();
    if (error != ASE_OK) { std::cerr << "start=" << error << "\n"; return 10; }
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    driver->stop(); driver->disposeBuffers();
    const auto frames = std::min<std::uint64_t>(gFrames.load(), capacityFrames);
    const auto bytes = static_cast<std::size_t>(frames * gChannels * gBytesPerSample);
    writeWav(output, static_cast<DWORD>(rate), static_cast<WORD>(gChannels),
             static_cast<WORD>(gBytesPerSample * 8), gAudio, bytes);
    std::cout << "Callbacks=" << gCallbacks.load() << " frames=" << frames
              << " output=" << output.string() << "\n";
    reportBursts(gAudio, static_cast<std::size_t>(frames));
    driver->Release(); factory->Release(); FreeLibrary(module); CoUninitialize();
    return 0;
}
