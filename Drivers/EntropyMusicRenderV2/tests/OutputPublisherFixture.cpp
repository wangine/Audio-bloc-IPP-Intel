// Synthetic publication only: never opens WASAPI or the installed driver.
#include "StagePublishV2.h"
#include "StageBusValidation.h"
#include "../audit/WasapiClient.h"
#include "../audit/PcmPattern.h"
#include <windows.h>
#include <array>
#include <fstream>
#include <filesystem>
#include <stdexcept>
int wmain(int argc,wchar_t** argv){
    try{
        if(argc!=5)return 2;
        const std::wstring token=argv[1],kind=argv[2];
        HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(L"Local\\EntropyMusicAudit-"+token).c_str());
        auto* progress=mapping?static_cast<entropy::music::audit::SharedProgress*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(entropy::music::audit::SharedProgress))):nullptr;
        if(!progress)throw std::runtime_error("Fixture progress absent");
        uint8_t uuid[16]{1};LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
        auto session=entropy::stage::makeSession(MUSIC_STEREO2_V1,uuid,frequency.QuadPart);session.producer_pid=GetCurrentProcessId();
        entropy::publish::Publisher publisher;publisher.setSession(session);std::string error;
        if(!publisher.start(L"\\\\.\\pipe\\Entropy.Stage1.Audit."+token,error))throw std::runtime_error(error);
        std::ofstream(std::filesystem::path(argv[3])).put('1');
        const auto deadline=GetTickCount64()+5000;
        while(!InterlockedCompareExchange(&progress->outputReady,0,0) && GetTickCount64()<deadline){publisher.service();Sleep(1);}
        if(!progress->outputReady)throw std::runtime_error("Fixture subscriber absent");
        std::array<double,1024> l{},r{};uint8_t bytes[4]{};
        entropy::music::PcmFormat format{64,192000,2,16,16,4,768000,3,0xfffe,1,{},{}};
        for(unsigned i=0;i<1024;++i){entropy::music::audit::PcmPattern::frame(i,format,bytes);int16_t a,b;std::memcpy(&a,bytes,2);std::memcpy(&b,bytes+2,2);l[i]=a/32768.0;r[i]=b/32768.0;}
        StageBusBlockHeader block{};std::memcpy(block.session_id,uuid,16);block.stream_epoch=1;block.block_sequence=1;block.control_revision=1;block.frames=1024;block.validity_mask=1;block.source_generation[0]=7;block.source_rate[0]=192000;
        const double* planes[]{l.data(),r.data()};publisher.publish(block,planes);
        const auto receivedDeadline=GetTickCount64()+3000;
        while(InterlockedCompareExchange64(&progress->outputFrames,0,0)<1024 && GetTickCount64()<receivedDeadline)Sleep(1);
        if(progress->outputFrames!=1024)throw std::runtime_error("Fixture first block not received");
        // Empty queue and expected frames are not producer completion.
        Sleep(250);
        if(kind==L"abortfinal")std::ofstream(std::filesystem::path(argv[4])).put('1');
        if(kind==L"error-retire"){block.block_sequence=2;block.first_frame=1024;block.frames=0;publisher.publish(block,planes);}
        if(kind==L"retire" || kind==L"error-retire"){
            session.session_id[0]=2;publisher.setSession(session);
            std::memcpy(block.session_id,session.session_id,16);block.frames=1024;block.block_sequence=2;block.first_frame=1024;publisher.publish(block,planes);
        }
        if(kind==L"extra"){block.block_sequence=2;block.first_frame=1024;publisher.publish(block,planes);}
        if(kind==L"clean-retire")publisher.stop();
        // A process exit with the intact queue is distinguishable from frozen
        // Publisher.stop(), which erases both normal and error terminal states.
        ExitProcess(kind==L"badexit"?3:0);
    }catch(...){return 2;}
}
