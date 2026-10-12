#include "endpoint_provider_v2.h"
#include "endpoint_stream_state_v1.h"
#include <algorithm>
#include <cstring>
#include <windows.h>
#ifndef SYNTHETIC_ID
#define SYNTHETIC_ID "synthetic.music"
#endif
#ifdef SYNTHETIC_BAD_ABI
#define FIXTURE_ABI (ENTROPY_PROVIDER_ABI_V2 + 1)
#else
#define FIXTURE_ABI ENTROPY_PROVIDER_ABI_V2
#endif
struct Stream {
    uint64_t sequence = 0, frame = 0;
    bool burst44 = false;
    unsigned lifecycle = 0;
    bool resumedGeneration=false;
    LARGE_INTEGER origin{}, frequency{};
    HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, nullptr),
           timer = CreateWaitableTimerExW(nullptr, nullptr, 2, TIMER_ALL_ACCESS);
    Stream() {
        QueryPerformanceCounter(&origin);
        QueryPerformanceFrequency(&frequency);
    }
    ~Stream() {
        CloseHandle(stop);
        CloseHandle(timer);
    }
};
int32_t enumerate(uint32_t index, EntropyEndpointInfoV2 *out) {
    if (index > 4)
        return 0;
    *out = {};
    out->abiVersion = ENTROPY_PROVIDER_ABI_V2;
    out->structSize = sizeof(*out);
    out->roles = 3;
    out->eligible = 1;
    out->state = 1;
    out->accessModes = 2;
    const char* ids[]={"fixture","burst44","idle-running","idle-unknown","new-generation-running"};
    strcpy_s(out->endpointId,ids[index]);
    strcpy_s(out->name, "Synthetic stereo fixture (offline only)");
    return 1;
}
int32_t open(const char *id, uint32_t, void **out) {
    if (std::strcmp(id, "fixture") && std::strcmp(id, "burst44") && std::strcmp(id,"idle-running") && std::strcmp(id,"idle-unknown") && std::strcmp(id,"new-generation-running"))
        return -1;
    auto stream = new Stream;
    stream->burst44 = !std::strcmp(id, "burst44");
    stream->lifecycle=!std::strcmp(id,"idle-running")?1:!std::strcmp(id,"idle-unknown")?2:!std::strcmp(id,"new-generation-running")?3:0;
    *out = stream;
    return 1;
}
int32_t read(void *opaque, EntropyIngressPacketV2 *out, void *payload, uint32_t capacity) {
    auto &s = *static_cast<Stream *>(opaque);
    const uint32_t frames = s.burst44 ? 2048 : 1024;
    const uint32_t rate = s.burst44 ? 44100 : 192000;
    if (capacity < frames * 8)
        return -1;
    LARGE_INTEGER now, due;
    QueryPerformanceCounter(&now);
    const auto elapsed=(now.QuadPart-s.origin.QuadPart)*1000/s.frequency.QuadPart;
    if(s.lifecycle==3 && elapsed>=100) {
        if(elapsed<1000 || elapsed>=1500) {
            *out={};out->abiVersion=ENTROPY_PROVIDER_ABI_V2;out->structSize=sizeof(*out);
            out->flags=ENTROPY_PACKET_STREAM_STATE_V1;
            out->reserved[0]=elapsed<500?ENTROPY_STREAM_IDLE_V1:ENTROPY_STREAM_RUNNING_V1;
            out->generation=elapsed<500?1:2;
            Sleep(2);return 0;
        }
        if(!s.resumedGeneration) {s.resumedGeneration=true;s.sequence=0;s.frame=0;}
    }
    if(s.lifecycle && s.lifecycle!=3 && elapsed>=100) {
        *out={};out->abiVersion=ENTROPY_PROVIDER_ABI_V2;out->structSize=sizeof(*out);
        const bool idle=(now.QuadPart-s.origin.QuadPart)*1000/s.frequency.QuadPart<500;
        if(idle || s.lifecycle==1) {
            out->flags=ENTROPY_PACKET_STREAM_STATE_V1;
            out->reserved[0]=idle?ENTROPY_STREAM_IDLE_V1:ENTROPY_STREAM_RUNNING_V1;
        }
        Sleep(2);return 0;
    }
    if (s.burst44 && s.sequence == 8)
        s.origin = now;
    const uint64_t pacedFrame = s.burst44 ? (s.frame > 16384 ? s.frame - 16384 : 0) : s.frame;
    auto target = s.origin.QuadPart +
                  static_cast<int64_t>(static_cast<long double>(pacedFrame) * s.frequency.QuadPart / rate);
    if(s.resumedGeneration)target+=s.frequency.QuadPart;
    due.QuadPart =
        -((std::max)(int64_t(1), static_cast<int64_t>(static_cast<long double>(target - now.QuadPart) *
                                                      10000000 / s.frequency.QuadPart)));
    if (!SetWaitableTimer(s.timer, &due, 0, nullptr, nullptr, FALSE))
        return -1;
    HANDLE waits[] = {s.stop, s.timer};
    if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
        return -1;
    *out = {};
    out->abiVersion = ENTROPY_PROVIDER_ABI_V2;
    out->structSize = sizeof(*out);
    out->generation = s.lifecycle==3 && elapsed>=1000?2:1;
    out->packetSequence = ++s.sequence;
    out->firstSourceFrame = s.frame;
    s.frame += frames;
    out->frames = frames;
    out->payloadBytes = frames * 8;
    out->flags = ENTROPY_PACKET_RAW_IDENTITY;
    out->originalContainerBits = 32;
    out->originalValidBits = 24;
    LARGE_INTEGER q, f;
    QueryPerformanceCounter(&q);
    QueryPerformanceFrequency(&f);
    out->qpcTimestamp = q.QuadPart;
    out->qpcFrequency = f.QuadPart;
    auto &fmt = out->format;
    fmt.structSize = sizeof(fmt);
    fmt.sampleRate = rate;
    fmt.channels = 2;
    fmt.containerBits = 32;
    fmt.validBits = 24;
    fmt.blockAlign = 8;
    fmt.averageBytesPerSecond = rate * 8;
    fmt.channelMask = 3;
    fmt.formatTag = 0xfffe;
    fmt.representation = ENTROPY_REP_PCM;
    const uint8_t guid[] = {1, 0, 0, 0, 0, 0, 16, 0, 128, 0, 0, 170, 0, 56, 155, 113};
    memcpy(fmt.subFormat, guid, 16);
    auto *p = static_cast<int32_t *>(payload);
    for (unsigned i = 0; i < frames; i++) {
        p[i * 2] = 268435456;
        p[i * 2 + 1] = -134217728;
    }
    return 1;
}
void cancel(void *h) { SetEvent(static_cast<Stream *>(h)->stop); }
void close(void *h) { delete static_cast<Stream *>(h); }
extern "C" ENTROPY_PROVIDER_EXPORT int32_t entropy_endpoint_provider_v2(uint32_t abi,
                                                                        EntropyProviderApiV2 *out) {
    if (abi != ENTROPY_PROVIDER_ABI_V2 || out->structSize != sizeof(*out))
        return -1;
    *out = {};
    out->abiVersion = FIXTURE_ABI;
    out->structSize = sizeof(*out);
    out->info.abiVersion = abi;
    out->info.structSize = sizeof(out->info);
    out->info.roles = 3;
    out->info.capabilities = ENTROPY_CAP_RAW_PCM | ENTROPY_CAP_STREAM_STATE_V1;
    strcpy_s(out->info.providerId, SYNTHETIC_ID);
    strcpy_s(out->info.name, "Offline fixture");
    out->enumerate = enumerate;
    out->open = open;
    out->read = read;
    out->cancel = cancel;
    out->close = close;
    return 1;
}
