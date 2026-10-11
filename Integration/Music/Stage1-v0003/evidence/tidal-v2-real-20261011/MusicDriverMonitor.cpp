// Diagnostic only: INFO/TRACE IOCTLs, no reader claim, PCM read, or state changes.
#include "provider/EndpointIdentity.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <cstdlib>
using namespace entropy::music;
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 2;
    const unsigned seconds=static_cast<unsigned>(std::wcstoul(argv[1],nullptr,10));
    if(!seconds || seconds>600)return 2;
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return 3;
    const auto endpoints=provider::resolveEndpoints();
    if(endpoints.size()!=1 || !provider::validIdentity(endpoints[0]))return 4;
    HANDLE device=CreateFileW(endpoints[0].interfacePath.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    if(device==INVALID_HANDLE_VALUE)return 5;
    std::cout<<"tick_ms,info_ok,error,state,generation,rate,bits,source_frames,packet_count,committed,published,delivered,lost,cancelled,uncommitted,lifetime_lost,lifetime_cancelled,terminated_lost,terminated_cancelled,write_calls,write_success,write_failure,last_write,last_flags,last_eos,last_status\n";
    const auto deadline=GetTickCount64()+seconds*1000ull;
    do {
        bridge::Header request{bridge::Magic,bridge::Version,sizeof(request),2};
        bridge::Info info{};DWORD copied{};
        const bool ok=DeviceIoControl(device,bridge::IoctlInfo,&request,sizeof(request),&info,sizeof(info),&copied,nullptr) && copied==sizeof(info);
        const auto error=ok?0:GetLastError();
        request.kind=4;bridge::Trace trace{};
        const bool traceOk=DeviceIoControl(device,bridge::IoctlTrace,&request,sizeof(request),&trace,sizeof(trace),&copied,nullptr) && copied==sizeof(trace);
        if(!traceOk){CloseHandle(device);return 6;}
        std::cout<<GetTickCount64()<<','<<ok<<','<<error<<','<<info.state<<','<<info.generation<<','<<info.format.sampleRate<<','<<info.format.validBits<<','<<info.sourceFrames<<','<<info.packetCount<<','<<info.committedFrames<<','<<info.publishedFrames<<','<<info.deliveredFrames<<','<<info.lostFrames<<','<<info.cancelledFrames<<','<<info.currentUnderrunFrames<<','<<info.lifetimeLost<<','<<info.lifetimeCancelled<<','<<info.terminatedLost<<','<<info.terminatedCancelled<<','<<trace.counters[1]<<','<<trace.counters[2]<<','<<trace.counters[3]<<','<<trace.lastPacketNumber<<','<<trace.lastFlags<<','<<trace.lastEosBytes<<','<<trace.lastStatus<<'\n';
        std::cout.flush();Sleep(100);
    } while(GetTickCount64()<deadline);
    CloseHandle(device);CoUninitialize();return 0;
}
