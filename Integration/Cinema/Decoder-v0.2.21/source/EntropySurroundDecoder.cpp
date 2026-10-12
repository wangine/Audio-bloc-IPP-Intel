#include "EntropySurroundCore.h"
#include "EntropySurroundIpc.h"
#include "CinemaOutput.h"
#include "CinemaDeadlineWait.h"
#include "CapturePolicy.h"
#include "CaptureScheduling.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <optional>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace std::chrono_literals;

namespace
{
std::atomic<bool> running {true};
std::atomic<std::uint64_t> captureWarnings {0};
std::atomic<bool> pacerSchedulingFailure {false};
std::atomic<bool> pacerMmcss {false};

BOOL WINAPI consoleHandler(DWORD event)
{
    if (event == CTRL_C_EVENT || event == CTRL_CLOSE_EVENT)
    {
        running = false;
        return TRUE;
    }
    return FALSE;
}

void check(HRESULT hr, std::string_view operation)
{
    if (FAILED(hr))
        throw std::runtime_error(std::format("{} failed: 0x{:08X}", operation,
                                            static_cast<unsigned>(hr)));
}

std::wstring deviceName(IMMDevice* device)
{
    ComPtr<IPropertyStore> store;
    check(device->OpenPropertyStore(STGM_READ, &store), "OpenPropertyStore");
    PROPVARIANT value{};
    PropVariantInit(&value);
    check(store->GetValue(PKEY_Device_FriendlyName, &value), "GetValue(name)");
    std::wstring result = value.vt == VT_LPWSTR ? value.pwszVal : L"(unknown)";
    PropVariantClear(&value);
    return result;
}

ComPtr<IMMDevice> findEndpoint(EDataFlow flow, std::wstring_view needle)
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                           IID_PPV_ARGS(&enumerator)), "MMDeviceEnumerator");
    ComPtr<IMMDeviceCollection> devices;
    check(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &devices),
          "EnumAudioEndpoints");
    UINT count = 0;
    devices->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> device;
        devices->Item(i, &device);
        auto name = deviceName(device.Get());
        auto lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
        std::wstring wanted(needle);
        std::transform(wanted.begin(), wanted.end(), wanted.begin(), towlower);
        if (lower.find(wanted) != std::wstring::npos)
            return device;
    }
    return nullptr;
}

WAVEFORMATEXTENSIBLE makeFormat(WORD channels)
{
    WAVEFORMATEXTENSIBLE f{};
    f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f.Format.nChannels = channels;
    f.Format.nSamplesPerSec = 48000;
    f.Format.wBitsPerSample = 16;
    f.Format.nBlockAlign = channels * 2;
    f.Format.nAvgBytesPerSec = f.Format.nSamplesPerSec * f.Format.nBlockAlign;
    f.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    f.Samples.wValidBitsPerSample = 16;
    f.dwChannelMask = channels == 2 ? KSAUDIO_SPEAKER_STEREO :
                      channels == 6 ? KSAUDIO_SPEAKER_5POINT1_SURROUND :
                                      KSAUDIO_SPEAKER_7POINT1_SURROUND;
    f.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    return f;
}

class FrameQueue
{
public:
    explicit FrameQueue(CinemaOutput& cinema) : cinema_(cinema)
    {
        mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            0, static_cast<DWORD>(entropy::surroundipc::kMappingBytes),
            entropy::surroundipc::kMappingName);
        if (mapping_ == nullptr)
            throw std::runtime_error("Could not create Entropy Surround shared memory");
        base_ = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0,
                              entropy::surroundipc::kMappingBytes);
        if (base_ == nullptr)
            throw std::runtime_error("Could not map Entropy Surround shared memory");
        header_ = static_cast<entropy::surroundipc::RingHeader*>(base_);
        samples_ = reinterpret_cast<float*>(static_cast<BYTE*>(base_) +
            sizeof(entropy::surroundipc::RingHeader));
        const auto nextGeneration = header_->magic == entropy::surroundipc::kMagic
            ? header_->generation + 1 : 1;
        std::memset(base_, 0, entropy::surroundipc::kMappingBytes);
        header_->magic = entropy::surroundipc::kMagic;
        header_->version = entropy::surroundipc::kVersion;
        header_->sampleRate = entropy::surroundipc::kSampleRate;
        header_->channelCount = entropy::surroundipc::kChannels;
        header_->capacityFrames = entropy::surroundipc::kRingFrames;
        header_->generation = nextGeneration;
    }

    ~FrameQueue()
    {
        if (base_ != nullptr) UnmapViewOfFile(base_);
        if (mapping_ != nullptr) CloseHandle(mapping_);
    }

    void push(const std::array<float, 8>& frame, bool real = false,
              std::uint64_t sourceGeneration = 0)
    {
        if (real) cinema_.submit(frame, sourceGeneration);
        const auto absolute = static_cast<std::uint64_t>(header_->writeFrame);
        const auto ringFrame = absolute & (entropy::surroundipc::kRingFrames - 1);
        auto* destination = samples_ + ringFrame * entropy::surroundipc::kChannels;
        std::memcpy(destination, frame.data(), sizeof(float) * frame.size());
        MemoryBarrier();
        InterlockedExchange64(&header_->writeFrame,
                              static_cast<LONG64>(absolute + 1));
        InterlockedIncrement64(&header_->callbackCount);
    }

    std::uint64_t size() const
    {
        return header_ != nullptr ? static_cast<std::uint64_t>(header_->writeFrame) : 0;
    }

    void beginStreamWithSilence()
    {
        invalidateCinema(true);
        const std::array<float, 8> silence{};
        // Give Physical I/O a clean 20 ms target window before announcing a
        // new decoder generation. This prevents samples from the previous
        // movie/codec being used during reacquisition.
        for (int frame = 0; frame < 960; ++frame)
            push(silence);
        MemoryBarrier();
        InterlockedIncrement64(&header_->generation);
    }

    void invalidateCinema(bool streamBoundary = false) noexcept
    {
        if (streamBoundary) cinema_.invalidate();
        else cinema_.suspend();
    }
    std::uint64_t cinemaGeneration() const noexcept { return cinema_.generation(); }
    void noteDecodedDiscard(std::uint64_t frames) noexcept { decodedDiscarded_ += frames; }
    std::uint64_t decodedDiscarded() const noexcept { return decodedDiscarded_.load(); }
private:
    CinemaOutput& cinema_;
    std::atomic<std::uint64_t> decodedDiscarded_ {0};
    HANDLE mapping_ = nullptr;
    void* base_ = nullptr;
    entropy::surroundipc::RingHeader* header_ = nullptr;
    float* samples_ = nullptr;
};

// IEC61937 AC-3 bursts repeat every 32 ms at 48 kHz. Leave enough scheduling
// margin for USB/WASAPI jitter; 55 ms proved too tight on the movie PC and
// could discard a complete decoded AC-3 block every few seconds.
class BurstGate
{
public:
    void mark() noexcept
    {
        lastBurstMs_.store(nowMs(), std::memory_order_release);
    }

    bool locked() const noexcept
    {
        const auto last = lastBurstMs_.load(std::memory_order_acquire);
        return last != 0 && nowMs() - last <= 150;
    }


    void reset() noexcept
    {
        lastBurstMs_.store(0, std::memory_order_release);
    }

private:
    static std::int64_t nowMs() noexcept
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::atomic<std::int64_t> lastBurstMs_ {0};
};

class FfmpegDecoder
{
public:
    FfmpegDecoder(FrameQueue& output, const BurstGate& gate)
        : output_(output), gate_(gate) {}
    ~FfmpegDecoder() { stop(); }

    // Create the process and its pipe owners before WASAPI starts. No carrier
    // has been written yet, so the first observed codec can adopt this child.
    void prepare()
    {
        start();
        prepared_ = true;
    }

    void start()
    {
        if(!pacerWait_.valid())throw std::runtime_error("High resolution pacer timer unavailable");
        SECURITY_ATTRIBUTES security {sizeof(security), nullptr, TRUE};
        HANDLE childInput = nullptr;
        HANDLE childOutput = nullptr;
        if (!CreatePipe(&childInput, &inputWrite_, &security, 1 << 20) ||
            !CreatePipe(&outputRead_, &childOutput, &security, 1 << 20))
            throw std::runtime_error("CreatePipe failed");
        SetHandleInformation(inputWrite_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(outputRead_, HANDLE_FLAG_INHERIT, 0);

        HANDLE nullError = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, 0, nullptr);
        STARTUPINFOW startup {sizeof(startup)};
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = childInput;
        startup.hStdOutput = childOutput;
        startup.hStdError = nullError;
        std::array<wchar_t, 32768> modulePath{};
        GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        auto ffmpegPath = std::filesystem::path(modulePath.data()).parent_path() / L"ffmpeg.exe";
        if (!std::filesystem::exists(ffmpegPath))
            ffmpegPath = L"C:\\Program Files\\Labcenter Electronics\\Proteus 9 Professional\\Tools\\Python\\ffmpeg.exe";
        if (!std::filesystem::exists(ffmpegPath))
            throw std::runtime_error("ffmpeg.exe was not found beside the decoder or in the known system location");
        std::wstring command = L"\"" + ffmpegPath.wstring() +
            L"\" -hide_banner -loglevel error -probesize 32 -analyzeduration 0 "
            L"-fflags nobuffer -flags low_delay -f spdif -i pipe:0 -map 0:a:0 "
            L"-ar 48000 -ac 6 -c:a pcm_f32le -f f32le pipe:1";
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                            nullptr, nullptr, &startup, &process))
            throw std::runtime_error(std::format("Could not start FFmpeg: {}", GetLastError()));
        process_ = process.hProcess;
        CloseHandle(process.hThread);
        CloseHandle(childInput);
        CloseHandle(childOutput);
        CloseHandle(nullError);
        pacerRunning_ = true;
        pacer_ = std::thread([this] { paceLoop(); });
        reader_ = std::thread([this] { readLoop(); });
    }

    void restart(entropy::surround::Codec codec)
    {
        if (prepared_ && isAlive())
        {
            prepared_ = false;
            streamCodec_.store(codec, std::memory_order_release);
            return;
        }
        stop();
        streamCodec_.store(codec, std::memory_order_release);
        start();
    }

    bool write(const BYTE* bytes, DWORD count)
    {
        if (inputWrite_ == nullptr || bytes == nullptr || count == 0) return false;
        while (count != 0 && running)
        {
            DWORD written = 0;
            if (!WriteFile(inputWrite_, bytes, count, &written, nullptr) || written == 0)
                return false;
            bytes += written;
            count -= written;
        }
        return count == 0;
    }

    bool isAlive() const noexcept
    {
        return process_ != nullptr &&
               WaitForSingleObject(process_, 0) == WAIT_TIMEOUT;
    }

    void stop()
    {
        prepared_ = false;
        pacerRunning_ = false;
        pcmReady_.notify_all();
        if (inputWrite_ != nullptr) { CloseHandle(inputWrite_); inputWrite_ = nullptr; }
        if (process_ != nullptr)
        {
            // Never join the pipe reader while FFmpeg may still own the write
            // side. The old order could deadlock forever during a codec
            // transition, leaving the decoder alive and UR23 exclusively held.
            if (WaitForSingleObject(process_, 500) == WAIT_TIMEOUT)
            {
                TerminateProcess(process_, 0);
                WaitForSingleObject(process_, 1000);
            }
        }
        if (reader_.joinable()) reader_.join();
        if (pacer_.joinable()) pacer_.join();
        {
            std::lock_guard lock(pcmMutex_);
            pcmQueue_.clear();
        }
        if (outputRead_ != nullptr) { CloseHandle(outputRead_); outputRead_ = nullptr; }
        if (process_ != nullptr)
        {
            CloseHandle(process_);
            process_ = nullptr;
        }
    }

    std::atomic<std::uint64_t> decodedFrames {0};

private:
    void paceLoop()
    {
        // This owner supplies the fixed-rate PCM consumed by the real DACs.
        // Timer precision alone cannot meet its deadline at ordinary priority.
        std::optional<CaptureScheduling> audioScheduling;
        try { audioScheduling.emplace(); }
        catch (...) {
            output_.invalidateCinema();
            pacerSchedulingFailure = true;
            pacerRunning_ = false;
            running = false;
            return;
        }
        pacerMmcss = true;
        struct SchedulingStatus {
            ~SchedulingStatus() { pacerMmcss = false; }
        } schedulingStatus;
        // AC-3 decoders naturally release 1536 samples every 32 ms. Publishing
        // those blocks directly makes a real-time consumer underrun between
        // bursts. Prebuffer one AC-3 frame, then publish deterministic 128-frame
        // blocks. 128 divides AC-3 (1536) and common DTS frame sizes exactly.
        constexpr std::size_t prebufferFrames = 1536;
        constexpr std::size_t blockFrames = 128;
        constexpr std::size_t maximumFrames = 9600;
        bool primed = false;
        auto next = std::chrono::steady_clock::now();
        std::array<std::array<float, 8>, blockFrames> block{};

        while (pacerRunning_)
        {
            std::unique_lock lock(pcmMutex_);
            if (!primed)
            {
                pcmReady_.wait_for(lock, 20ms, [this]
                {
                    return !pacerRunning_ || pcmQueue_.size() >= prebufferFrames;
                });
                if (!pacerRunning_) break;
                if (!gate_.locked())
                {
                    pcmQueue_.clear();
                    continue;
                }
                if (pcmQueue_.size() < prebufferFrames) continue;
                primed = true;
                next = std::chrono::steady_clock::now();
            }

            auto sourceGeneration = output_.cinemaGeneration();
            if (!gate_.locked())
            {
                output_.invalidateCinema();
                pcmQueue_.clear();
                primed = false;
                continue;
            }
            if (pcmQueue_.size() < blockFrames)
            {
                output_.invalidateCinema();
                // Re-prime instead of publishing a partial/corrupt block.
                primed = false;
                lock.unlock();
                const std::array<float, 8> silence{};
                for (std::size_t i = 0; i < blockFrames; ++i)
                    output_.push(silence);
                next += 2666667ns;
                const auto now = std::chrono::steady_clock::now();
                if (next + 20ms < now) next = now;
                if(!pacerWait_.until(next)){output_.invalidateCinema();pacerSchedulingFailure=true;pacerRunning_=false;running=false;}
                continue;
            }

            // Bound latency if a decoder or scheduler stalls and later catches up.
            if (pcmQueue_.size() > maximumFrames)
            {
                output_.invalidateCinema(true);
                output_.noteDecodedDiscard(pcmQueue_.size() - prebufferFrames);
                pcmQueue_.erase(pcmQueue_.begin(),
                                pcmQueue_.begin() +
                                    static_cast<std::ptrdiff_t>(pcmQueue_.size() - prebufferFrames));
                sourceGeneration = output_.cinemaGeneration();
                // Capture resets the IEC gate before retiring its generation.
                // Re-check after our own catch-up boundary so that old queued
                // PCM cannot acquire a token from concurrent recovery.
                if (!gate_.locked())
                {
                    pcmQueue_.clear();
                    primed = false;
                    continue;
                }
            }
            for (auto& frame : block)
            {
                frame = pcmQueue_.front();
                pcmQueue_.pop_front();
            }
            const auto queued = pcmQueue_.size();
            lock.unlock();

            for (const auto& frame : block) output_.push(frame, true, sourceGeneration);
            // Small bounded correction holds the decoder FIFO near its target
            // without altering pitch perceptibly or accumulating long latency.
            const auto interval = queued > prebufferFrames + 256 ? 2653333ns
                                : queued + 256 < prebufferFrames ? 2680000ns
                                                                 : 2666667ns;
            next += interval;
            const auto now = std::chrono::steady_clock::now();
            if (next + 20ms < now) next = now;
            if(!pacerWait_.until(next)){output_.invalidateCinema();pacerSchedulingFailure=true;pacerRunning_=false;running=false;}
        }
    }

    void readLoop()
    {
        std::vector<std::uint8_t> pending;
        pending.reserve(65536);
        std::array<std::uint8_t, 65536> bytes{};
        while (running)
        {
            DWORD got = 0;
            if (!ReadFile(outputRead_, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr) || got == 0)
                break;
            pending.insert(pending.end(), bytes.begin(), bytes.begin() + got);
            constexpr std::size_t frameBytes = 6 * sizeof(float);
            const auto complete = pending.size() / frameBytes;
            for (std::size_t frame = 0; frame < complete; ++frame)
            {
                std::array<float, 8> out{};
                std::memcpy(out.data(), pending.data() + frame * frameBytes, frameBytes);
                // Never publish stale decoder output after IEC lock is lost.
                // Also contain corrupt/non-finite decoder samples before they
                // can reach Matrix or a power amplifier.
                if (gate_.locked())
                {
                    for (auto& sample : out)
                        if (!std::isfinite(sample)) sample = 0.0f;
                    entropy::surround::applyDecodedCodecCalibration(
                        out, streamCodec_.load(std::memory_order_acquire));
                    for (auto& sample : out)
                        sample = std::clamp(sample, -1.0f, 1.0f);
                    {
                        std::lock_guard lock(pcmMutex_);
                        pcmQueue_.push_back(out);
                    }
                }
            }
            pcmReady_.notify_one();
            decodedFrames += complete;
            pending.erase(pending.begin(), pending.begin() + complete * frameBytes);
        }
    }

    FrameQueue& output_;
    const BurstGate& gate_;
    HANDLE inputWrite_ = nullptr;
    HANDLE outputRead_ = nullptr;
    HANDLE process_ = nullptr;
    std::thread reader_;
    std::thread pacer_;
    std::atomic<bool> pacerRunning_ {false};
    CinemaDeadlineWait pacerWait_;
    bool prepared_ = false; // only the capture owner calls prepare/restart
    std::atomic<entropy::surround::Codec> streamCodec_
        {entropy::surround::Codec::noSignal};
    std::mutex pcmMutex_;
    std::condition_variable pcmReady_;
    std::deque<std::array<float, 8>> pcmQueue_;
};

void captureLoop(IMMDevice* endpoint, FfmpegDecoder* decoder, FrameQueue& output,
                 bool output71, std::atomic<entropy::surround::Codec>& codec,
                 std::atomic<std::uint64_t>& bursts, BurstGate& burstGate,
                 std::atomic<std::uint64_t>& recoveries,
                 std::ofstream* carrierCapture)
{
    CaptureScheduling scheduling;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IAudioClient> client;
    check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                             reinterpret_cast<void**>(client.GetAddressOf())), "Activate(UR23)");
    auto format = makeFormat(2);
    REFERENCE_TIME defaultPeriod = 0, minimumPeriod = 0;
    check(client->GetDevicePeriod(&defaultPeriod, &minimumPeriod),
          "GetDevicePeriod(UR23)");
    std::cout << "UR23 periods: default=" << defaultPeriod
              << " minimum=" << minimumPeriod << " (100 ns units)\n";
    bool eventDriven = true;
    HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK, defaultPeriod, defaultPeriod, &format.Format, nullptr);
    std::cout << std::format("UR23 event initialize: 0x{:08X}\n",
                            static_cast<unsigned>(hr));
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        UINT32 aligned = 0;
        client->GetBufferSize(&aligned);
        client.Reset();
        endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(client.GetAddressOf()));
        const auto duration = static_cast<REFERENCE_TIME>(10000000.0 * aligned / 48000.0 + 0.5);
        hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK, duration, duration, &format.Format, nullptr);
        std::cout << std::format("UR23 aligned event initialize: 0x{:08X}\n",
                                static_cast<unsigned>(hr));
    }
    if (FAILED(hr))
    {
        // The Windows USB class driver on some PCs advertises the format but
        // rejects event-driven exclusive mode with E_INVALIDARG. A polling
        // exclusive stream uses the same bit-perfect format and is also less
        // sensitive to missed USB event notifications.
        client.Reset();
        check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(client.GetAddressOf())),
              "Activate(UR23 polling fallback)");
        constexpr REFERENCE_TIME pollingBuffer = 200000; // 20 ms
        hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, 0,
                                pollingBuffer, 0, &format.Format, nullptr);
        std::cout << std::format("UR23 polling initialize: 0x{:08X}\n",
                                static_cast<unsigned>(hr));
        eventDriven = false;
    }
    if (FAILED(hr))
    {
        // Recent Microsoft USB Audio builds can report EXTENSIBLE support but
        // reject it at Initialize. IEC61937 is carried as ordinary 2-channel
        // 16-bit PCM, so the equivalent legacy descriptor remains bit-perfect.
        WAVEFORMATEX legacy{};
        legacy.wFormatTag = WAVE_FORMAT_PCM;
        legacy.nChannels = 2;
        legacy.nSamplesPerSec = 48000;
        legacy.wBitsPerSample = 16;
        legacy.nBlockAlign = 4;
        legacy.nAvgBytesPerSec = 192000;
        legacy.cbSize = 0;
        client.Reset();
        check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(client.GetAddressOf())),
              "Activate(UR23 legacy fallback)");
        hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                defaultPeriod, defaultPeriod, &legacy, nullptr);
        eventDriven = true;
        std::cout << std::format("UR23 legacy PCM event initialize: 0x{:08X}\n",
                                static_cast<unsigned>(hr));
        if (FAILED(hr))
        {
            client.Reset();
            check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                     reinterpret_cast<void**>(client.GetAddressOf())),
                  "Activate(UR23 legacy polling fallback)");
            hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, 0,
                                    200000, 0, &legacy, nullptr);
            eventDriven = false;
            std::cout << std::format("UR23 legacy PCM polling initialize: 0x{:08X}\n",
                                    static_cast<unsigned>(hr));
        }
    }
    check(hr, "Initialize(UR23 exclusive)");
    HANDLE event = nullptr;
    if (eventDriven)
    {
        event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event == nullptr)
            throw std::runtime_error("CreateEvent(UR23) failed");
        check(client->SetEventHandle(event), "SetEventHandle(UR23)");
    }
    ComPtr<IAudioCaptureClient> capture;
    check(client->GetService(IID_PPV_ARGS(&capture)), "GetService(UR23 capture)");
    entropy::surround::MatrixSurround upmixer;
    const std::array<float, 8> silence{};
    auto decoderCodec = entropy::surround::Codec::noSignal;
    bool encodedActive = false;
    auto lastDecoderProgress = std::chrono::steady_clock::now();
    auto lastDecoderRestart = lastDecoderProgress - 1s;
    std::uint64_t observedDecodedFrames = 0;
    entropy::surround::PcmLockGate pcmGate;
    const auto leaveEncodedStream = [&]
    {
        if (!encodedActive) return;
        // Flush the old decoded generation before the optical source changes
        // to pause/silence/PCM. Without this edge reset, queued decoded samples
        // and residual carrier words can meet at the Matrix reader and produce
        // a short burst at Pause/Play.
        encodedActive = false;
        burstGate.reset();
        pcmGate.reset();
        upmixer.reset();
        output.beginStreamWithSilence();
        codec = entropy::surround::Codec::noSignal;
    };
    if (decoder != nullptr) decoder->prepare();
    check(client->Start(), "Start(UR23 capture)");
    auto lastPacket = std::chrono::steady_clock::now();
    while (running)
    {
        if (eventDriven ? WaitForSingleObject(event, 100) != WAIT_OBJECT_0
                        : (std::this_thread::sleep_for(2ms), false))
        {
            if (std::chrono::steady_clock::now() - lastPacket > 300ms)
            {
                output.invalidateCinema();
                leaveEncodedStream();
                pcmGate.reset();
                upmixer.reset();
                codec = entropy::surround::Codec::noSignal;
                for (int i = 0; i < 480; ++i) output.push(silence);
                lastPacket = std::chrono::steady_clock::now();
            }
            continue;
        }
        // GetNextPacketSize is a Shared-only API. In Exclusive mode acquire
        // directly, and wait for the next pass when no packet is available.
        while (running)
        {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            const HRESULT bufferResult = capture->GetBuffer(
                &data, &frames, &flags, nullptr, nullptr);
            if (bufferResult == AUDCLNT_S_BUFFER_EMPTY ||
                bufferResult == AUDCLNT_E_BUFFER_ERROR)
                break;
            check(bufferResult, "GetBuffer(UR23)");
            if (frames == 0) break;
            if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0)
                captureWarnings.fetch_add(1, std::memory_order_relaxed);
            if (stereoTransportDiscontinuity(burstGate.locked(), flags))
                output.invalidateCinema(true);
            const auto sourceGeneration = output.cinemaGeneration();
            lastPacket = std::chrono::steady_clock::now();
            if (carrierCapture != nullptr && carrierCapture->is_open())
            {
                const auto byteCount = static_cast<std::streamsize>(frames) * 4;
                if (data != nullptr && (flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0)
                    carrierCapture->write(reinterpret_cast<const char*>(data), byteCount);
                else
                {
                    static const std::array<char, 16384> zeros{};
                    auto remaining = byteCount;
                    while (remaining > 0)
                    {
                        const auto chunk = std::min<std::streamsize>(remaining, zeros.size());
                        carrierCapture->write(zeros.data(), chunk);
                        remaining -= chunk;
                    }
                }
            }
            if (data != nullptr && (flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0)
            {
                const auto status = entropy::surround::inspectIec61937(data, frames * 4ull);
                if (status.found)
                {
                    pcmGate.reset();
                    upmixer.reset();
                    const auto now = std::chrono::steady_clock::now();
                    const auto decodedNow = decoder != nullptr
                        ? decoder->decodedFrames.load(std::memory_order_acquire) : 0;
                    if (decodedNow != observedDecodedFrames)
                    {
                        observedDecodedFrames = decodedNow;
                        lastDecoderProgress = now;
                    }
                    const bool decoderStalled = encodedActive && decoder != nullptr &&
                        now - lastDecoderProgress > 1500ms;
                    const bool decoderExited = encodedActive && decoder != nullptr &&
                        !decoder->isAlive();
                    const bool needsRestart = !burstGate.locked() ||
                        decoderCodec != status.codec ||
                        decoderStalled || decoderExited;
                    if (decoder != nullptr && needsRestart &&
                        now - lastDecoderRestart > 250ms)
                    {
                        // FFmpeg's SPDIF demuxer locks to the first stream.
                        // Recreate it after a stream gap or codec change and
                        // also when the child exits or bursts continue without
                        // decoded progress. A WASAPI warning alone is not a
                        // codec/stream boundary: the retained UR23 DTS carrier
                        // has continuous 2048-byte bursts and decodes strictly
                        // across that warning. Recreating FFmpeg there loses a
                        // partial encoded frame and retires healthy CINEMA PCM.
                        // Keep reporting the warning; retain real recovery and
                        // the separate PCM discontinuity policy above.
                        burstGate.reset();
                        output.beginStreamWithSilence();
                        decoder->restart(status.codec);
                        if (encodedActive)
                            recoveries.fetch_add(1, std::memory_order_relaxed);
                        decoderCodec = status.codec;
                        observedDecodedFrames = decoder->decodedFrames.load(
                            std::memory_order_acquire);
                        lastDecoderProgress = now;
                        lastDecoderRestart = now;
                    }
                    burstGate.mark();
                    encodedActive = true;
                    codec = status.codec;
                    ++bursts;
                }
                const bool encodedNow = burstGate.locked();
                if (!encodedNow) leaveEncodedStream();
                if (decoder != nullptr && encodedNow)
                {
                    if (!decoder->write(data, frames * 4))
                    {
                        // Fail closed. The next IEC burst will recreate the
                        // FFmpeg process because decoderCodec is invalidated.
                        burstGate.reset();
                        decoderCodec = entropy::surround::Codec::noSignal;
                        output.beginStreamWithSilence();
                    }
                }
                else if (decoder == nullptr && encodedNow)
                {
                    // A manual stereo mode must never publish an IEC61937
                    // carrier as PCM. Mute immediately and let the GUI tell
                    // the user that FULL AUTO is required for this stream.
                    for (UINT32 i = 0; i < frames; ++i)
                        output.push(silence);
                }
                // FULL AUTO: PCM must be both waveform-plausible and stable
                // for 250 ms before it is allowed to reach Matrix. UR23 was
                // captured repeating one stale non-zero stereo frame while
                // optical lock was absent; treating that as PCM caused the
                // loud crackle/hum on unplug and a short burst on reconnect.
                auto* pcm = reinterpret_cast<const std::int16_t*>(data);
                const auto pcmQuality = !encodedNow
                    ? entropy::surround::classifyPcm16Stereo(pcm, frames)
                    : entropy::surround::PcmPacketQuality::invalid;
                const bool stereoNow = !encodedNow &&
                    pcmGate.observe(pcm, frames);
                if (stereoNow)
                {
                    codec = entropy::surround::Codec::pcmStereo;
                    for (UINT32 i = 0; i < frames; ++i)
                        output.push(upmixer.process(pcm[i * 2] / 32768.0f,
                                                    pcm[i * 2 + 1] / 32768.0f, output71), true,
                                    sourceGeneration);
                }
                else if (!encodedNow)
                {
                    output.invalidateCinema();
                    if (pcmQuality == entropy::surround::PcmPacketQuality::invalid)
                        upmixer.reset();
                    codec = entropy::surround::Codec::noSignal;
                    for (UINT32 i = 0; i < frames; ++i)
                        output.push(silence);
                }
            }
            else if (validStereoSilence(pcmGate.locked(), burstGate.locked(),
                                        encodedActive, flags))
            {
                // SILENT is real zero-valued PCM only after confirmed stereo
                // lock. Preserve its frames and the existing upmixer tail.
                codec = entropy::surround::Codec::pcmStereo;
                for (UINT32 i = 0; i < frames; ++i)
                    output.push(upmixer.process(0.0f, 0.0f, output71), true,
                                sourceGeneration);
            }
            else
            {
                // IEC61937 contains legitimate zero-filled intervals between
                // bursts, which a USB driver may flag as silent. Only declare
                // the encoded stream ended after the burst hold has expired.
                if (!burstGate.locked()) { output.invalidateCinema(); leaveEncodedStream(); }
                pcmGate.reset();
                upmixer.reset();
                codec = entropy::surround::Codec::noSignal;
                for (UINT32 i = 0; i < frames; ++i)
                    output.push(silence);
            }
            check(capture->ReleaseBuffer(frames), "ReleaseBuffer(UR23)");
        }
    }
    client->Stop();
    if (event != nullptr) CloseHandle(event);
    CoUninitialize();
}
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        std::cout << std::unitbuf;
        std::wcout << std::unitbuf;
        HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE,
                                         L"Local\\EntropySurroundDecoder.Stop");
        if (stopEvent == nullptr)
            throw std::runtime_error("Could not create decoder stop event");
        ResetEvent(stopEvent);
        bool forceStereo = false;
        bool captureOnly = false;
        bool output71 = false;
        int seconds = 0;
        std::filesystem::path carrierCapturePath;
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring_view arg = argv[i];
            if (arg == L"--stereo") forceStereo = true;
            else if (arg == L"--capture-only") captureOnly = true;
            else if (arg == L"--capture-carrier" && i + 1 < argc)
                carrierCapturePath = argv[++i];
            else if (arg == L"--7.1") output71 = true;
            else if (arg == L"--seconds" && i + 1 < argc) seconds = _wtoi(argv[++i]);
        }
        check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "CoInitializeEx");
        SetConsoleCtrlHandler(consoleHandler, TRUE);
        auto ur23 = findEndpoint(eCapture, L"UR23");
        if (ur23 == nullptr) throw std::runtime_error("UR23 capture endpoint not found");
        std::wcout << L"Entropy Surround Decoder v" ENTROPY_SURROUND_VERSION
                   << L"\nInput : " << deviceName(ur23.Get())
                   << L"\nInput format: 48000 Hz / 16-bit stereo carrier"
                   << L"\nOutput: Entropy Matrix shared memory (8 channels)"
                   << L"\nMode: "
                   << (forceStereo
                        ? (output71 ? L"PCM Stereo only -> 7.1"
                                    : L"PCM Stereo only -> 5.1")
                        : L"FULL AUTO - PCM Stereo / Dolby Digital / DTS")
                   << L"\nOutput format: 48000 Hz / float32 / "
                   << (output71 ? L"7.1" : L"5.1 (BL/BR silent)") << L"\n";

        CinemaOutput cinema;
        std::string cinemaError;
        if (!cinema.start(output71 ? 8 : 6, CinemaOutput::defaultEndpoint(), cinemaError))
            throw std::runtime_error("CINEMA publication: " + cinemaError);
        FrameQueue queue(cinema);
        BurstGate burstGate;
        FfmpegDecoder ffmpeg(queue, burstGate);
        std::ofstream carrierCapture;
        if (!carrierCapturePath.empty())
        {
            carrierCapture.open(carrierCapturePath, std::ios::binary | std::ios::trunc);
            if (!carrierCapture)
                throw std::runtime_error("Could not create raw carrier capture");
        }
        std::atomic<entropy::surround::Codec> codec {
            entropy::surround::Codec::noSignal};
        std::atomic<std::uint64_t> bursts {0};
        std::atomic<std::uint64_t> recoveries {0};
        std::mutex workerErrorMutex;
        std::exception_ptr workerError;
        const auto guarded = [&](auto&& work) {
            try { work(); }
            catch (...)
            {
                std::lock_guard lock(workerErrorMutex);
                if (workerError == nullptr) workerError = std::current_exception();
                running = false;
            }
        };
        std::thread capture([&] {
            guarded([&] {
                captureLoop(ur23.Get(), (forceStereo || captureOnly) ? nullptr : &ffmpeg,
                            queue, output71, codec, bursts, burstGate,
                            recoveries,
                            carrierCapture.is_open() ? &carrierCapture : nullptr);
            });
        });
        const auto deadline = seconds > 0
            ? std::chrono::steady_clock::now() + std::chrono::seconds(seconds)
            : std::chrono::steady_clock::time_point::max();
        std::cout << "Running; Ctrl+C stops the decoder.\n";
        while (running && std::chrono::steady_clock::now() < deadline)
        {
            if(cinema.schedulingFailed()){running=false;break;}
            if (WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0)
            {
                running = false;
                break;
            }
            std::this_thread::sleep_for(1s);
            std::cout << "Codec=" << entropy::surround::codecName(codec.load())
                      << (codec.load() == entropy::surround::Codec::pcmStereo
                              ? " | Input PCM=48k/16 stereo"
                              : " | Carrier=IEC61937 48k/16")
                      << " | Output PCM=192k/f64/" << (output71 ? "7.1" : "5.1")
                      << " | IEC bursts=" << bursts.load()
                      << " | decoded=" << ffmpeg.decodedFrames.load()
                      << " | recoveries=" << recoveries.load()
                      << " | published=" << queue.size()
                      << " | cinemaBlocks=" << cinema.blocks()
                      << " | cinemaDrops=" << cinema.dropped()
                      << " | captureWarnings=" << captureWarnings.load(std::memory_order_relaxed)
                      << " | pacerMmcss=" << pacerMmcss.load(std::memory_order_relaxed)
                      << " | decodedDiscarded=" << queue.decodedDiscarded() << "\n";
        }
        running = false;
        capture.join();
        ffmpeg.stop();
        if(pacerSchedulingFailure||cinema.schedulingFailed())
            throw std::runtime_error("High resolution audio scheduling failed; decoder stopped");
        if (workerError != nullptr) std::rethrow_exception(workerError);
        CloseHandle(stopEvent);
        CoUninitialize();
        return 0;
    }
    catch (const std::exception& error)
    {
        running = false;
        std::cerr << "ERROR: " << error.what() << "\n";
        CoUninitialize();
        return 1;
    }
}

