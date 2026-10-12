#include "stage1_control_v1.h"
#include "StageLinkSecurity.h"
#include <windows.h>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    if(argc!=3 && argc!=4)return 2;
    const auto deadline=GetTickCount64()+std::stoull(argv[2])*1000;
    std::cout<<"time_ms,music_state,aux_state,music_frames_in,aux_frames_in,music_underruns,aux_underruns,music_faults,aux_faults,music_fill,aux_fill,music_rate,aux_rate,music_generation,aux_generation\n";
    do {
        Stage1ControlRequest request{};request.magic=STAGE1_CONTROL_MAGIC;
        request.abiVersion=STAGE1_CONTROL_ABI;request.structSize=sizeof(request);
        Stage1ControlResponse response{};
        HANDLE pipe=INVALID_HANDLE_VALUE;
        const auto connectDeadline=GetTickCount64()+3000;
        do {
            pipe=CreateFileW(argv[1],GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
            if(pipe!=INVALID_HANDLE_VALUE)break;
            const auto error=GetLastError();
            if(error!=ERROR_PIPE_BUSY && error!=ERROR_FILE_NOT_FOUND){std::cerr<<"Control pipe create failed: "<<error<<'\n';return 3;}
            WaitNamedPipeW(argv[1],25);
            Sleep(1);
        } while(GetTickCount64()<connectDeadline);
        if(pipe==INVALID_HANDLE_VALUE)return 3;
        bool ok=entropy::link::transfer(pipe,true,&request,sizeof(request),2000);
        if(argc==4)Sleep(std::stoul(argv[3]));
        ok=ok && entropy::link::transfer(pipe,false,&response,sizeof(response),2000);
        if(ok) {uint32_t ack=STAGE1_CONTROL_ACK;entropy::link::transfer(pipe,true,&ack,sizeof(ack),2000);}
        CloseHandle(pipe);
        if(!ok || response.magic!=STAGE1_CONTROL_MAGIC || response.structSize!=sizeof(response)){std::cerr<<"Control response incomplete\n";return 4;}
        auto& m=response.sources[0];auto& a=response.sources[1];
        std::cout<<GetTickCount64()<<','<<m.state<<','<<a.state<<','<<m.framesIn<<','<<a.framesIn<<','<<m.underruns<<','<<a.underruns<<','<<m.faults<<','<<a.faults<<','<<m.fillFrames<<','<<a.fillFrames<<','<<m.rate<<','<<a.rate<<','<<m.generation<<','<<a.generation<<'\n';
        Sleep(100);
    } while(GetTickCount64()<deadline);
    return 0;
}
