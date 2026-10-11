#include "WasapiClient.h"
#include "PcmPattern.h"
#include "OutputDrainPolicy.h"
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <stdexcept>
#include <sstream>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace entropy::music::audit {
namespace {
void check(HRESULT h,const char* operation){if(FAILED(h)){std::ostringstream text;text<<operation<<" failed HRESULT=0x"<<std::hex<<static_cast<uint32_t>(h);throw std::runtime_error(text.str());}}
struct Apartment {HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);Apartment(){check(result,"COM");}~Apartment(){CoUninitialize();}};
struct Handle {HANDLE value=nullptr;~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct View {void* value=nullptr;~View(){if(value)UnmapViewOfFile(value);}};
struct Started {IAudioClient* client;bool active=false;~Started(){if(active)client->Stop();}};
struct AudioPriority {DWORD index=0;HANDLE value=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);~AudioPriority(){if(value)AvRevertMmThreadCharacteristics(value);}};
WAVEFORMATEXTENSIBLE wave(const PcmFormat& f){WAVEFORMATEXTENSIBLE w{};w.Format={0xfffe,2,f.sampleRate,f.averageBytesPerSecond,static_cast<WORD>(f.blockAlign),static_cast<WORD>(f.containerBits),22};w.Samples.wValidBitsPerSample=static_cast<WORD>(f.validBits);w.dwChannelMask=3;std::memcpy(&w.SubFormat,PcmGuid,16);return w;}
void validateSession(const std::wstring& s){if(s.empty() || s.size()>64)throw std::runtime_error("Session token required");for(auto c:s)if(!((c>=L'0' && c<=L'9') || (c>=L'a' && c<=L'z') || (c>=L'A' && c<=L'Z') || c==L'-'))throw std::runtime_error("Invalid session token");}
}
std::wstring pipeName(const std::wstring& session){validateSession(session);return L"\\\\.\\pipe\\EntropyMusicAudit-"+session;}
std::wstring progressName(const std::wstring& session){validateSession(session);return L"Local\\EntropyMusicAudit-"+session;}
RunReport WasapiClient::run(const RunConfig& c){
    Apartment apartment;pipeName(c.session);if(!c.seconds || c.seconds>1800 || c.pauseMs>2000)throw std::runtime_error("Duration must be 1..1800 seconds and pause 0..2000ms");
    PcmFormat f{64,c.rate,2,c.bits,c.bits==16?16u:24u,c.bits/4,c.rate*(c.bits/4),3,0xfffe,1,{},{}};std::memcpy(f.subFormat,PcmGuid,16);if(!validateFormat(f))throw std::runtime_error("Unsupported format; no fallback");
    const auto devices=provider::resolveEndpoints();if(devices.size()!=1 || !provider::validIdentity(devices[0]))throw std::runtime_error("Exact Entropy Music Render V2 identity absent or ambiguous; no audio opened");
    const auto& identity=devices[0];RunReport report;report.endpointId=provider::utf8(identity.windowsEndpointId);
    Handle mapping{OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,progressName(c.session).c_str())};if(!mapping.value)throw std::runtime_error("Audit collector not ready");
    View mapped{MapViewOfFile(mapping.value,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedProgress))};if(!mapped.value)throw std::runtime_error("Audit progress mapping unavailable");auto progress=static_cast<SharedProgress*>(mapped.value);
    if(!InterlockedCompareExchange(&progress->clientPid,0,0))throw std::runtime_error("Frozen Core audit provider must connect before rendering");
    Handle privateDevice{CreateFileW(identity.interfacePath.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr)};if(privateDevice.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Private diagnostics unavailable");
    auto trace=[&](bridge::Trace& result){bridge::Header request{bridge::Magic,bridge::Version,sizeof(request),4};DWORD copied=0;
        if(!DeviceIoControl(privateDevice.value,bridge::IoctlTrace,&request,sizeof(request),&result,sizeof(result),&copied,nullptr) || copied!=sizeof(result) || result.header.kind!=4)throw std::runtime_error("Loaded driver trace unavailable");};
    trace(report.traceBefore);
    auto pausedInfo=[&](){
        bridge::Header request{bridge::Magic,bridge::Version,sizeof(request),2};bridge::Info result{};DWORD copied=0;
        for(unsigned attempt=0;attempt<100;++attempt){
            if(DeviceIoControl(privateDevice.value,bridge::IoctlInfo,&request,sizeof(request),&result,sizeof(result),&copied,nullptr) && copied==sizeof(result) && result.header.magic==bridge::Magic && result.header.version==bridge::Version && result.generation && result.state==1)return result;
            Sleep(1);
        }
        throw std::runtime_error("Paused driver diagnostics unavailable or not paused");
    };
    ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<IMMDevice> device;ComPtr<IAudioClient> client;ComPtr<IAudioRenderClient> renderer;ComPtr<IAudioClock> clock;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)),"MMDeviceEnumerator");check(enumerator->GetDevice(identity.windowsEndpointId.c_str(),&device),"exact endpoint");
    check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())),"IAudioClient");auto w=wave(f);
    check(client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,&w.Format,nullptr),"exact Exclusive format");
    REFERENCE_TIME defaultPeriod=0,minimum=0;check(client->GetDevicePeriod(&defaultPeriod,&minimum),"GetDevicePeriod");
    REFERENCE_TIME period=c.periodFrames?static_cast<REFERENCE_TIME>((static_cast<uint64_t>(c.periodFrames)*10000000+c.rate/2)/c.rate):defaultPeriod;
    if(period<minimum)throw std::runtime_error("Requested period below device minimum");const DWORD flags=c.eventDriven?AUDCLNT_STREAMFLAGS_EVENTCALLBACK:0;
    auto initialized=client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,flags,c.eventDriven?period:period*2,period,&w.Format,nullptr);
    if(initialized==AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED){UINT32 aligned=0;check(client->GetBufferSize(&aligned),"aligned buffer size");period=static_cast<REFERENCE_TIME>((static_cast<uint64_t>(aligned)*10000000+c.rate/2)/c.rate);client.Reset();check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())),"aligned client");initialized=client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,flags,c.eventDriven?period:period*2,period,&w.Format,nullptr);}
    check(initialized,"Exclusive Initialize");check(client->GetBufferSize(&report.bufferFrames),"GetBufferSize");if(!report.bufferFrames || report.bufferFrames>MaxPacketFrames)throw std::runtime_error("Unsupported buffer bound");
    Handle event{CreateEventW(nullptr,FALSE,FALSE,nullptr)};if(!event.value)throw std::runtime_error("Event allocation failed");if(c.eventDriven)check(client->SetEventHandle(event.value),"SetEventHandle");
    check(client->GetService(IID_PPV_ARGS(&renderer)),"Render service");check(client->GetService(IID_PPV_ARGS(&clock)),"Audio clock");UINT64 frequency=0;check(clock->GetFrequency(&frequency),"Audio clock frequency");if(!frequency)throw std::runtime_error("Invalid audio clock");
    uint64_t target=static_cast<uint64_t>(c.rate)*c.seconds;if(c.eventDriven)target=((target+report.bufferFrames-1)/report.bufferFrames)*report.bufferFrames;
    if(c.completeFp64Tail && !completeFp64Tail(c.rate,target))throw std::runtime_error("Actual WASAPI period creates an incomplete1024-frame FP64 tail; refused before PCM prefill");
    trace(report.traceAfterInitialize);
    LARGE_INTEGER commitFrequency{},lastCommit{};QueryPerformanceFrequency(&commitFrequency);
    auto fill=[&](uint32_t count){BYTE* bytes=nullptr;check(renderer->GetBuffer(count,&bytes),"GetBuffer");for(uint32_t i=0;i<count;++i)PcmPattern::frame(report.emittedFrames+i,f,bytes+i*f.blockAlign);check(renderer->ReleaseBuffer(count,0),"ReleaseBuffer");LARGE_INTEGER now{};QueryPerformanceCounter(&now);if(lastCommit.QuadPart){auto ms=1000.0*(now.QuadPart-lastCommit.QuadPart)/commitFrequency.QuadPart;if(ms>report.maxCommitIntervalMs){report.maxCommitIntervalMs=ms;report.maxCommitIntervalFirstFrame=report.emittedFrames;}}lastCommit=now;report.emittedFrames+=count;};
    AudioPriority priority;if(!priority.value)throw std::runtime_error("Audit MMCSS registration failed");
    fill(static_cast<uint32_t>(target<report.bufferFrames?target:report.bufferFrames));trace(report.traceAfterPrefill);check(client->Start(),"Start after prefill");Started started{client.Get(),true};
    const auto deadline=GetTickCount64()+static_cast<uint64_t>(c.seconds)*1000+c.pauseMs+5000;
    while(GetTickCount64()<deadline){
        if(c.pauseMs && !report.pause.observed && report.clockFrames>=target/2 && report.receivedFrames>=target/2){
            check(client->Stop(),"Pause without Reset");started.active=false;
            UINT64 ignoredQpc=0;check(clock->GetPosition(&report.pause.clockBefore,&ignoredQpc),"paused clock before");
            const auto before=pausedInfo();report.pause.sourceBefore=before.sourceFrames;report.pause.generationBefore=before.generation;
            Sleep(c.pauseMs);
            check(clock->GetPosition(&report.pause.clockAfter,&ignoredQpc),"paused clock after");
            const auto after=pausedInfo();report.pause.sourceAfter=after.sourceFrames;report.pause.generationAfter=after.generation;report.pause.observed=1;
            if(!report.pause.valid())throw std::runtime_error("Clock advanced or generation changed while paused");
            check(client->Start(),"Resume without Reset");started.active=true;
        }
        if(report.emittedFrames<target){
            UINT32 available=report.bufferFrames;
            if(c.eventDriven){if(WaitForSingleObject(event.value,2000)!=WAIT_OBJECT_0)throw std::runtime_error("Event timeout");}
            else {UINT32 padding=0;check(client->GetCurrentPadding(&padding),"GetCurrentPadding");if(padding>report.bufferFrames)throw std::runtime_error("Invalid padding");available-=padding;}
            if(available){const auto remaining=target-report.emittedFrames;fill(static_cast<uint32_t>(remaining<available?remaining:available));}
        }
        UINT64 position=0,qpc=0;check(clock->GetPosition(&position,&qpc),"Clock position");report.clockFrames=(position/frequency)*c.rate+(position%frequency)*c.rate/frequency;
        report.receivedFrames=static_cast<uint64_t>(InterlockedCompareExchange64(&progress->frames,0,0));
        bridge::Header request{bridge::Magic,bridge::Version,sizeof(request),2};DWORD copied=0;bridge::Info info{};
        if(DeviceIoControl(privateDevice.value,bridge::IoctlInfo,&request,sizeof(request),&info,sizeof(info),&copied,nullptr) && copied==sizeof(info) && info.header.magic==bridge::Magic && info.header.version==bridge::Version){report.driver=info;if(info.allocationMode)report.observedAllocation=info.allocationMode;}
        if(report.emittedFrames==target && report.clockFrames>=target && report.receivedFrames>=target){report.drained=report.receivedFrames==target && !InterlockedCompareExchange(&progress->errors,0,0);break;}
        // WaitForSingleObject is the event-driven pacing. Sleep(1) can round to
        // 15.6ms and introduce underruns into a 10ms test stream.
        if(!c.eventDriven)Sleep(std::max<DWORD>(1,static_cast<DWORD>(period/40000)));
        else if(report.emittedFrames==target)WaitForSingleObject(event.value,20);
    }
    trace(report.traceAfter);check(client->Stop(),"Stop after observed drain");started.active=false;
    // A finite client without an OS EOS may have an unwritten interval after
    // its requested tail. Interior gaps are rejected by complete frame indices.
    if(report.driver.state==4 || report.driver.lostFrames || report.observedAllocation!=1 || report.driver.qpcFrequency==0 || (c.pauseMs && !report.pause.valid()))report.drained=false;return report;
}
}
