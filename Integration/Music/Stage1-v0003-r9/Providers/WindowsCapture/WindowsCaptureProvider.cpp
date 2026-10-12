#include "PcmDecode.h"
#include "WindowsEndpointCatalog.h"
#include "endpoint_provider_v2.h"
#include "CapturePacketReader.h"
#include <algorithm>
#include <audioclient.h>
#include <cstring>
#include <ksmedia.h>
using Microsoft::WRL::ComPtr;
using namespace entropy::music;
namespace {
struct Stream {
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> capture;
    HANDLE ready = CreateEventW(nullptr, FALSE, FALSE, nullptr),
           stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    EntropyIngressFormatV2 format{};
    uint64_t sequence = 0;
    uint32_t mode{};
    ~Stream() {
        if (client)
            client->Stop();
        CloseHandle(ready);
        CloseHandle(stop);
    }
};
int32_t enumerate(uint32_t index, EntropyEndpointInfoV2 *out) {
    WindowsEndpointCatalog catalog;
    auto items = catalog.enumerate();
    std::vector<WindowsEndpoint> capture;
    for (auto &e : items)
        if (e.flow == eCapture)
            capture.push_back(e);
    if (index >= capture.size())
        return 0;
    auto &e = capture[index];
    if (e.id.size() >= sizeof(out->endpointId) || e.name.size() >= sizeof(out->name))
        return -1;
    *out = {};
    out->abiVersion = ENTROPY_PROVIDER_ABI_V2;
    out->structSize = sizeof(*out);
    out->roles = 3;
    out->flow = 1;
    out->state = e.state;
    out->eligible = e.captureEligible;
    out->accessModes = 6;
    strcpy_s(out->endpointId, e.id.c_str());
    strcpy_s(out->windowsEndpointId, e.id.c_str());
    strcpy_s(out->name, e.name.c_str());
    return 1;
}
int32_t open(const char *id, uint32_t mode, void **out) {
    if (mode != 1 && mode != 2)
        return -1;
    auto s = new Stream;
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    auto key = wide(id);
    if (FAILED(
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))) ||
        FAILED(enumerator->GetDevice(key.c_str(), &device)) ||
        FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void **>(s->client.GetAddressOf())))) {
        delete s;
        return -1;
    }
    ComPtr<IMMEndpoint> endpoint;
    if (FAILED(device.As(&endpoint))) {
        delete s;
        return -1;
    }
    EDataFlow flow;
    if (FAILED(endpoint->GetDataFlow(&flow)) || flow != eCapture) {
        delete s;
        return -1;
    }
    WAVEFORMATEX *mix = nullptr;
    if (FAILED(s->client->GetMixFormat(&mix))) {
        delete s;
        return -1;
    }
    auto rate = mix->nSamplesPerSec;
    CoTaskMemFree(mix);
    WAVEFORMATEXTENSIBLE requested{};
    requested.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    requested.Format.nChannels = 2;
    requested.Format.nSamplesPerSec = rate;
    requested.Format.wBitsPerSample = 32;
    requested.Format.nBlockAlign = 8;
    requested.Format.nAvgBytesPerSec = rate * 8;
    requested.Format.cbSize = 22;
    requested.Samples.wValidBitsPerSample = 24;
    requested.dwChannelMask = 3;
    requested.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    auto &f = s->format;
    f.structSize = sizeof(f);
    f.sampleRate = rate;
    f.channels = 2;
    f.containerBits = 32;
    f.validBits = 24;
    f.blockAlign = 8;
    f.averageBytesPerSecond = rate * 8;
    f.channelMask = 3;
    f.formatTag = 0xfffe;
    f.representation = ENTROPY_REP_PCM;
    memcpy(f.subFormat, &requested.SubFormat, 16);
    if (!validFormat(f)) {
        delete s;
        return -1;
    }
    auto access = mode == 2 ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
    WAVEFORMATEX *closest = nullptr;
    auto supported = s->client->IsFormatSupported(access, &requested.Format,
                                                  access == AUDCLNT_SHAREMODE_SHARED ? &closest : nullptr);
    if (closest)
        CoTaskMemFree(closest);
    if (supported != S_OK) {
        delete s;
        return -1;
    }
    const REFERENCE_TIME duration = 100000;
    auto hr = s->client->Initialize(access, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, duration,
                                    access == AUDCLNT_SHAREMODE_EXCLUSIVE ? duration : 0, &requested.Format,
                                    nullptr);
    if (FAILED(hr) || FAILED(s->client->SetEventHandle(s->ready)) ||
        FAILED(s->client->GetService(IID_PPV_ARGS(&s->capture))) || FAILED(s->client->Start())) {
        delete s;
        return -1;
    }
    s->mode = mode;
    *out = s;
    return 1;
}
int32_t read(void *h, EntropyIngressPacketV2 *out, void *payload, uint32_t capacity) {
    auto &s = *static_cast<Stream *>(h);
    if(WaitForSingleObject(s.stop,0)==WAIT_OBJECT_0)return -1;
    BYTE *bytes = nullptr;
    UINT32 frames = 0;
    DWORD flags = 0;
    UINT64 devicePosition = 0, qpc100ns = 0;
    auto packet=readCapturePacket([&] {
        auto hr=s.capture->GetBuffer(&bytes,&frames,&flags,&devicePosition,&qpc100ns);
        return hr==AUDCLNT_S_BUFFER_EMPTY?0:(FAILED(hr)?-1:1);
    },[&] {
        HANDLE waits[]={s.stop,s.ready};
        auto event=WaitForMultipleObjects(2,waits,FALSE,100);
        return event==WAIT_TIMEOUT?0:(event==WAIT_OBJECT_0+1?1:-1);
    });
    if(packet!=1)return packet;
    const auto size = uint64_t(frames) * s.format.blockAlign;
    if (size > capacity || frames > ENTROPY_MAX_PACKET_FRAMES) {
        s.capture->ReleaseBuffer(frames);
        return -1;
    }
    if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
        memset(payload, 0, static_cast<size_t>(size));
    else
        memcpy(payload, bytes, static_cast<size_t>(size));
    if(FAILED(s.capture->ReleaseBuffer(frames)))return -1;
    *out = {};
    out->abiVersion = ENTROPY_PROVIDER_ABI_V2;
    out->structSize = sizeof(*out);
    out->format = s.format;
    out->originalContainerBits = s.format.containerBits;
    out->originalValidBits = s.format.validBits;
    out->generation = 1;
    out->packetSequence = ++s.sequence;
    out->firstSourceFrame = devicePosition;
    out->frames = frames;
    out->payloadBytes = static_cast<uint32_t>(size);
    out->mode = s.mode;
    out->flags = ENTROPY_PACKET_RAW_IDENTITY;
    if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
        out->flags |= ENTROPY_PACKET_DISCONTINUITY;
    if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
        out->flags |= ENTROPY_PACKET_SILENCE;
    LARGE_INTEGER q, f;
    QueryPerformanceCounter(&q);
    QueryPerformanceFrequency(&f);
    out->qpcTimestamp = q.QuadPart;
    out->qpcFrequency = f.QuadPart;
    return 1;
}
void cancel(void *h) { SetEvent(static_cast<Stream *>(h)->stop); }
void close(void *h) { delete static_cast<Stream *>(h); }
} // namespace
extern "C" ENTROPY_PROVIDER_EXPORT int32_t entropy_endpoint_provider_v2(uint32_t abi,
                                                                        EntropyProviderApiV2 *out) {
    if (abi != ENTROPY_PROVIDER_ABI_V2 || out->structSize != sizeof(*out))
        return -1;
    *out = {};
    out->abiVersion = abi;
    out->structSize = sizeof(*out);
    out->info.abiVersion = abi;
    out->info.structSize = sizeof(out->info);
    out->info.roles = 3;
    out->info.capabilities = ENTROPY_CAP_RAW_PCM;
    strcpy_s(out->info.providerId, "windows.capture.v2");
    strcpy_s(out->info.name, "Windows stereo capture (strict PCM24-in-32)");
    out->enumerate = enumerate;
    out->open = open;
    out->read = read;
    out->cancel = cancel;
    out->close = close;
    return 1;
}
