#include <entropy/dac/WasapiDacSink.h>

#include <entropy/dac/DacSinkCore.h>

#include <windows.h>
#include <audioclient.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace entropy::dac
{
namespace
{
template <typename T> void release(T*& pointer) noexcept
{
    if (pointer != nullptr) { pointer->Release(); pointer = nullptr; }
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
    if (text == nullptr) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0,
                                            nullptr, nullptr);
    if (count <= 1) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), count,
                        nullptr, nullptr);
    result.resize(static_cast<std::size_t>(count - 1));
    return result;
}

std::wstring wide(const char* text)
{
    if (text == nullptr) return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                            text, -1, nullptr, 0);
    if (count <= 1) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                        result.data(), count);
    result.resize(static_cast<std::size_t>(count - 1));
    return result;
}

template <std::size_t N> void copyText(char (&destination)[N],
                                       const std::string& source) noexcept
{
    const auto bytes = (std::min)(N - 1, source.size());
    std::memcpy(destination, source.data(), bytes);
    destination[bytes] = 0;
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

REFERENCE_TIME framesToReference(std::uint32_t frames,
                                 std::uint32_t sampleRate) noexcept
{
    return static_cast<REFERENCE_TIME>((10000000ULL * frames + sampleRate - 1) /
                                       sampleRate);
}
} // namespace

WasapiDacSink::WasapiDacSink() noexcept = default;
WasapiDacSink::~WasapiDacSink() { close(); }

EntropyResult WasapiDacSink::refreshDevices()
{
    if (state_.load() != ENTROPY_DAC_SINK_CLOSED)
        return ENTROPY_ERROR_INVALID_STATE;
    ComScope com;
    if (!com.usable()) return ENTROPY_ERROR_PROCESSING;
    IMMDeviceEnumerator* enumerator = nullptr;
    auto result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(result)) return ENTROPY_ERROR_PROCESSING;
    IMMDevice* defaultDevice = nullptr;
    std::string defaultId;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia,
                                                       &defaultDevice)))
    {
        LPWSTR id = nullptr;
        if (SUCCEEDED(defaultDevice->GetId(&id))) { defaultId = utf8(id); CoTaskMemFree(id); }
        defaultDevice->Release();
    }
    IMMDeviceCollection* collection = nullptr;
    result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                             &collection);
    if (FAILED(result)) { enumerator->Release(); return ENTROPY_ERROR_PROCESSING; }
    UINT count = 0;
    collection->GetCount(&count);
    std::vector<Device> found;
    found.reserve(count);
    for (UINT index = 0; index < count; ++index)
    {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(index, &device))) continue;
        LPWSTR id = nullptr;
        std::string endpointId;
        if (SUCCEEDED(device->GetId(&id))) { endpointId = utf8(id); CoTaskMemFree(id); }
        std::string name;
        IPropertyStore* properties = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties)))
        {
            PROPVARIANT value{};
            PropVariantInit(&value);
            if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
                value.vt == VT_LPWSTR)
                name = utf8(value.pwszVal);
            PropVariantClear(&value);
            properties->Release();
        }
        if (!endpointId.empty())
            found.push_back({endpointId, name, endpointId == defaultId});
        device->Release();
    }
    collection->Release();
    enumerator->Release();
    devices_ = std::move(found);
    return ENTROPY_OK;
}

std::uint32_t WasapiDacSink::deviceCount() const noexcept
{
    return static_cast<std::uint32_t>(devices_.size());
}

EntropyResult WasapiDacSink::getDevice(std::uint32_t index,
                                       EntropyDacSinkDeviceInfo& info) const noexcept
{
    if (info.struct_size < sizeof(info) || index >= devices_.size())
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto& source = devices_[index];
    info = {};
    info.struct_size = sizeof(info);
    info.backend = ENTROPY_DAC_SINK_BACKEND_WASAPI_EXCLUSIVE;
    info.active = 1;
    info.is_default = source.isDefault ? 1u : 0u;
    copyText(info.endpoint_id_utf8, source.id);
    copyText(info.display_name_utf8, source.name);
    return ENTROPY_OK;
}

EntropyResult WasapiDacSink::open(const EntropyDacSinkConfig& requested)
{
    if (!validConfig(requested))
    {
        faults_.fetch_or(ENTROPY_DAC_SINK_FAULT_INVALID_CONFIG);
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    }
    if (state_.load() != ENTROPY_DAC_SINK_CLOSED)
        return ENTROPY_ERROR_INVALID_STATE;
    const auto comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
        return ENTROPY_ERROR_PROCESSING;
    ownsCom_ = comResult == S_OK || comResult == S_FALSE;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    const auto endpoint = wide(requested.endpoint_id_utf8);
    HRESULT result = endpoint.empty() ? E_INVALIDARG :
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(result)) result = enumerator->GetDevice(endpoint.c_str(), &device);
    if (SUCCEEDED(result)) result = device->Activate(__uuidof(IAudioClient),
        CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient_));
    release(device);
    release(enumerator);
    if (FAILED(result)) { setDriverFailure(result, isDeviceLoss(result)); close(); return ENTROPY_ERROR_PROCESSING; }

    WAVEFORMATEXTENSIBLE format{};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = static_cast<WORD>(requested.channels);
    format.Format.nSamplesPerSec = requested.sample_rate;
    format.Format.wBitsPerSample = requested.output_format == ENTROPY_PCM24_PACKED_LE ? 24 : 32;
    format.Format.nBlockAlign = static_cast<WORD>(requested.channels *
        (format.Format.wBitsPerSample / 8));
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
    format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    format.Samples.wValidBitsPerSample = 24;
    format.dwChannelMask = channelMask(requested.channels);
    format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    WAVEFORMATEX* closest = nullptr;
    result = audioClient_->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                              &format.Format, &closest);
    if (closest != nullptr) CoTaskMemFree(closest);
    if (result != S_OK)
    {
        faults_.fetch_or(ENTROPY_DAC_SINK_FAULT_FORMAT_UNSUPPORTED);
        lastResult_.store(static_cast<std::int32_t>(result));
        close();
        return ENTROPY_ERROR_UNSUPPORTED;
    }

    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (event_ == nullptr) { close(); return ENTROPY_ERROR_PROCESSING; }
    REFERENCE_TIME defaultPeriod = 0, minimumPeriod = 0;
    result = audioClient_->GetDevicePeriod(&defaultPeriod, &minimumPeriod);
    if (FAILED(result)) { setDriverFailure(result, false); close(); return ENTROPY_ERROR_PROCESSING; }
    REFERENCE_TIME period = requested.requested_buffer_frames != 0
        ? framesToReference(requested.requested_buffer_frames, requested.sample_rate)
        : (std::max)(defaultPeriod, minimumPeriod);
    period = (std::max)(period, minimumPeriod);
    result = audioClient_->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
        period, period, &format.Format, nullptr);
    if (FAILED(result)) { setDriverFailure(result, isDeviceLoss(result)); close(); return ENTROPY_ERROR_PROCESSING; }
    result = audioClient_->SetEventHandle(static_cast<HANDLE>(event_));
    if (SUCCEEDED(result)) result = audioClient_->GetBufferSize(&bufferFrames_);
    if (SUCCEEDED(result)) result = audioClient_->GetService(__uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&renderClient_));
    if (SUCCEEDED(result)) audioClient_->GetService(__uuidof(IAudioClock),
        reinterpret_cast<void**>(&audioClock_));
    if (audioClock_ != nullptr) audioClock_->GetFrequency(&clockFrequency_);
    if (FAILED(result) || bufferFrames_ == 0)
    {
        setDriverFailure(FAILED(result) ? result : E_FAIL, isDeviceLoss(result));
        close();
        return ENTROPY_ERROR_PROCESSING;
    }
    streamLatency100ns_=0;latencyValid_=false;freshClock_=false;precisePositionFrames_=0;
    REFERENCE_TIME latency=0;
    if(SUCCEEDED(audioClient_->GetStreamLatency(&latency))){streamLatency100ns_=static_cast<uint64_t>(latency);latencyValid_=true;}
    config_ = requested;
    config_.struct_size = sizeof(config_);
    blockAlignment_ = format.Format.nBlockAlign;
    pending_ = false;
    faults_.store(0);
    lastResult_.store(S_OK);
    submittedFrames_.store(0);
    renderedBuffers_.store(0);
    xruns_.store(0);
    timeouts_.store(0);
    deviceLosses_.store(0);
    devicePosition_.store(0);
    deviceQpc_.store(0);
    state_.store(ENTROPY_DAC_SINK_OPEN);
    return ENTROPY_OK;
}

EntropyResult WasapiDacSink::close() noexcept
{
    stop();
    releaseInterfaces();
    config_ = {};
    bufferFrames_ = 0;
    blockAlignment_ = 0;
    clockFrequency_ = 0;
    pending_ = false;
    state_.store(ENTROPY_DAC_SINK_CLOSED);
    if (ownsCom_) { CoUninitialize(); ownsCom_ = false; }
    return ENTROPY_OK;
}

EntropyResult WasapiDacSink::start() noexcept
{
    if (state_.load() != ENTROPY_DAC_SINK_OPEN || renderClient_ == nullptr)
        return ENTROPY_ERROR_INVALID_STATE;
    BYTE* initial = nullptr;
    auto result = renderClient_->GetBuffer(bufferFrames_, &initial);
    if (SUCCEEDED(result)) result = renderClient_->ReleaseBuffer(
        bufferFrames_, AUDCLNT_BUFFERFLAGS_SILENT);
    if (SUCCEEDED(result)) result = audioClient_->Start();
    if (FAILED(result))
    {
        setDriverFailure(result, isDeviceLoss(result));
        return ENTROPY_ERROR_PROCESSING;
    }
    state_.store(ENTROPY_DAC_SINK_RUNNING);
    return ENTROPY_OK;
}

EntropyResult WasapiDacSink::stop() noexcept
{
    if (state_.load() == ENTROPY_DAC_SINK_RUNNING && audioClient_ != nullptr)
    {
        const auto result = audioClient_->Stop();
        if (FAILED(result)) setDriverFailure(result, isDeviceLoss(result));
        else state_.store(ENTROPY_DAC_SINK_OPEN);
    }
    pending_ = false;
    return ENTROPY_OK;
}

EntropyResult WasapiDacSink::waitBuffer(EntropyDacSinkBufferRequest& request) noexcept
{
    if (request.struct_size < sizeof(request) ||
        state_.load() != ENTROPY_DAC_SINK_RUNNING || event_ == nullptr || pending_)
        return ENTROPY_ERROR_INVALID_STATE;
    const auto timeout = request.timeout_ms;
    request.frames = 0;
    request.bytes = 0;
    // Cancel has priority if audio and DISARM become ready together.
    HANDLE handles[2]={static_cast<HANDLE>(cancelEvent_),static_cast<HANDLE>(event_)};
    const auto wait=cancelEvent_?WaitForMultipleObjects(2,handles,FALSE,timeout):WaitForSingleObject(handles[1],timeout);
    if(cancelEvent_&&wait==WAIT_OBJECT_0)return ENTROPY_ERROR_INVALID_STATE;
    const auto audioReady=cancelEvent_?WAIT_OBJECT_0+1:WAIT_OBJECT_0;
    if (wait == WAIT_TIMEOUT)
    {
        timeouts_.fetch_add(1);
        xruns_.fetch_add(1);
        faults_.fetch_or(ENTROPY_DAC_SINK_FAULT_XRUN);
        lastResult_.store(static_cast<std::int32_t>(HRESULT_FROM_WIN32(ERROR_TIMEOUT)));
        return ENTROPY_ERROR_PROCESSING;
    }
    if (wait != audioReady)
    {
        setDriverFailure(HRESULT_FROM_WIN32(GetLastError()), false);
        return ENTROPY_ERROR_PROCESSING;
    }
    pending_ = true;
#if ENTROPY_A06_R04_DIAG
    quality_.value.called=0;quality_.value.fresh=0;quality_.value.stale=1;
#endif
    request.frames = bufferFrames_;
    request.bytes = bufferFrames_ * blockAlignment_;
    freshClock_=false;request.device_position_frames=0;request.device_qpc_position=0;
    if (audioClock_ != nullptr)
    {
        UINT64 position = 0, qpc = 0;
        const auto clockResult=audioClock_->GetPosition(&position, &qpc);
#if ENTROPY_A06_R04_DIAG
        quality_.observe(static_cast<std::int32_t>(clockResult),position,qpc);
#endif
        if (clockResult==S_OK && clockFrequency_!=0 && qpc!=0 && qpc>deviceQpc_.load())
        {
            const auto frames = clockFrequency_ != 0
                ? static_cast<std::uint64_t>((static_cast<long double>(position) *
                    config_.sample_rate) / clockFrequency_)
                : 0;
            precisePositionFrames_=static_cast<double>(static_cast<long double>(position)*config_.sample_rate/clockFrequency_);
            devicePosition_.store(frames);
            deviceQpc_.store(qpc);freshClock_=true;
        }
    }
    if(freshClock_){request.device_position_frames=devicePosition_.load();request.device_qpc_position=deviceQpc_.load();}
    return ENTROPY_OK;
}

EntropyResult WasapiDacSink::submit(EntropyDacSinkPcmBlock& block) noexcept
{
    block.accepted_frames = 0;
    if (state_.load() != ENTROPY_DAC_SINK_RUNNING || !pending_ ||
        block.frames != bufferFrames_)
        return ENTROPY_ERROR_INVALID_STATE;
    BYTE* destination = nullptr;
    auto result = renderClient_->GetBuffer(bufferFrames_, &destination);
    if (FAILED(result))
    {
        pending_ = false;
        setDriverFailure(result, isDeviceLoss(result));
        return ENTROPY_ERROR_PROCESSING;
    }
    if (!copyExact(destination, static_cast<std::size_t>(bufferFrames_) * blockAlignment_,
                   block, config_))
    {
        renderClient_->ReleaseBuffer(bufferFrames_, AUDCLNT_BUFFERFLAGS_SILENT);
        pending_ = false;
        faults_.fetch_or(ENTROPY_DAC_SINK_FAULT_INVALID_CONFIG);
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    }
    result = renderClient_->ReleaseBuffer(bufferFrames_, 0);
    pending_ = false;
    if (FAILED(result))
    {
        setDriverFailure(result, isDeviceLoss(result));
        return ENTROPY_ERROR_PROCESSING;
    }
    block.accepted_frames = block.frames;
    submittedFrames_.fetch_add(block.frames);
    renderedBuffers_.fetch_add(1);
    lastResult_.store(S_OK);
    return ENTROPY_OK;
}

EntropyDacSinkStatus WasapiDacSink::status() const noexcept
{
    EntropyDacSinkStatus value{};
    value.struct_size = sizeof(value);
    value.state = state_.load();
    value.fault_flags = faults_.load();
    value.last_driver_result = lastResult_.load();
    value.applied_revision = config_.revision;
    value.sample_rate = config_.sample_rate;
    value.channels = config_.channels;
    value.output_format = config_.output_format;
    value.buffer_frames = bufferFrames_;
    value.block_alignment_bytes = blockAlignment_;
    value.submitted_frames = submittedFrames_.load();
    value.rendered_buffers = renderedBuffers_.load();
    value.xrun_count = xruns_.load();
    value.timeout_count = timeouts_.load();
    value.device_loss_count = deviceLosses_.load();
    value.device_position_frames = devicePosition_.load();
    value.device_qpc_position = deviceQpc_.load();
    return value;
}

void WasapiDacSink::releaseInterfaces() noexcept
{
    release(audioClock_);
    release(renderClient_);
    release(audioClient_);
    if (event_ != nullptr) { CloseHandle(static_cast<HANDLE>(event_)); event_ = nullptr; }
}

bool WasapiDacSink::isDeviceLoss(long result) noexcept
{
    return result == AUDCLNT_E_DEVICE_INVALIDATED ||
           result == AUDCLNT_E_RESOURCES_INVALIDATED ||
           result == AUDCLNT_E_SERVICE_NOT_RUNNING;
}

void WasapiDacSink::setDriverFailure(long result, bool deviceLost) noexcept
{
    lastResult_.store(static_cast<std::int32_t>(result));
    if (deviceLost)
    {
        faults_.fetch_or(ENTROPY_DAC_SINK_FAULT_DEVICE_LOST);
        deviceLosses_.fetch_add(1);
        state_.store(ENTROPY_DAC_SINK_DEVICE_LOST);
    }
    else
    {
        faults_.fetch_or(ENTROPY_DAC_SINK_FAULT_DRIVER);
        state_.store(ENTROPY_DAC_SINK_FAULTED);
    }
}
} // namespace entropy::dac
