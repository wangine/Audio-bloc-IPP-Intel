#include "Fp64Capture.h"
#include "Fp64Verifier.h"
#include "WasapiClient.h"
#include "StagePublishV2.h"
#include "StageBusValidation.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace entropy::music::audit {
namespace {
struct CaptureHeader {uint32_t magic=0x464d5245,version=1,kind=0,bytes=0;uint64_t errors=0,reserved=0;};
static_assert(sizeof(CaptureHeader)==32);
struct Progress {
    HANDLE mapping=nullptr;SharedProgress* view=nullptr;
    explicit Progress(const std::wstring& session){
        mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,progressName(session).c_str());
        if(mapping)view=static_cast<SharedProgress*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedProgress)));
        if(!view){if(mapping)CloseHandle(mapping);throw std::runtime_error("Raw collector progress absent");}
    }
    ~Progress(){UnmapViewOfFile(view);CloseHandle(mapping);}
};
bool nativeSession(const StageBusSessionHeader& s,uint32_t pid){
    return s.magic==STAGE_BUS_MAGIC && s.abi_version==STAGE_BUS_ABI && s.header_bytes==256 && s.layout_id==MUSIC_STEREO2_V1 &&
        s.channels==2 && s.sample_rate==192000 && s.sample_format==STAGE_BUS_BINARY64_LE && s.planar==STAGE_BUS_PLANAR && s.producer_pid==pid &&
        entropy::stage::validateSession(s,static_cast<size_t>(s.mapping_bytes))==entropy::stage::StageBusValidationResult::Ok;
}
}
int collectFp64(const std::wstring& session,const std::wstring& endpoint,uint32_t corePid,const std::filesystem::path& output,const std::filesystem::path& stop){
    if(!corePid || !endpoint.starts_with(L"\\\\.\\pipe\\Entropy.Stage1.Audit."))throw std::runtime_error("Explicit isolated Core endpoint/PID required");
    Progress progress(session);entropy::publish::Subscriber subscriber;std::string error;
    if(!subscriber.connect(endpoint,error))throw std::runtime_error("FP64 subscriber connect failed: "+error);
    const auto& source=subscriber.session();if(!nativeSession(source,corePid))throw std::runtime_error("FP64 session differs from isolated Core");
    std::ofstream file(output,std::ios::binary);if(!file)throw std::runtime_error("FP64 capture file failed");
    auto write=[&](uint32_t kind,const void* bytes,uint32_t count,uint64_t errors=0){CaptureHeader h;h.kind=kind;h.bytes=count;h.errors=errors;file.write(reinterpret_cast<const char*>(&h),sizeof(h));if(count)file.write(static_cast<const char*>(bytes),count);if(!file)throw std::runtime_error("FP64 capture disk write failed");};
    write(1,&source,sizeof(source));InterlockedExchange(&progress.view->outputReady,1);
    std::array<std::array<double,1024>,2> storage{};double* planes[]{storage[0].data(),storage[1].data()};StageBusBlockHeader h{};uint64_t errors=0;
    for(;;){
        LARGE_INTEGER qpc;QueryPerformanceCounter(&qpc);subscriber.heartbeat(qpc.QuadPart);
        const auto result=subscriber.read(h,planes);
        if(result==entropy::publish::ReadResult::Block){
            CaptureHeader record;record.kind=2;record.bytes=sizeof(h)+sizeof(storage);file.write(reinterpret_cast<const char*>(&record),sizeof(record));file.write(reinterpret_cast<const char*>(&h),sizeof(h));file.write(reinterpret_cast<const char*>(storage.data()),sizeof(storage));if(!file)throw std::runtime_error("FP64 capture disk write failed");
            if(h.validity_mask&1)InterlockedAdd64(&progress.view->outputFrames,1024);
        }else if(result==entropy::publish::ReadResult::Empty){if(std::filesystem::exists(stop))break;Sleep(1);}
        else {if(!std::filesystem::exists(stop) || result!=entropy::publish::ReadResult::Disconnected){++errors;InterlockedExchange(&progress.view->outputErrors,1);}break;}
    }
    write(3,nullptr,0,errors);subscriber.close();return errors?2:0;
}
int verifyFp64(const std::filesystem::path& input,uint64_t frames,uint32_t bits,uint64_t generation,uint32_t corePid){
    PcmFormat f{64,192000,2,bits,bits==16?16u:24u,bits/4,192000*(bits/4),3,0xfffe,1,{},{}};std::memcpy(f.subFormat,PcmGuid,16);Fp64Verifier verifier(f,frames,generation);
    std::ifstream file(input,std::ios::binary);if(!file)throw std::runtime_error("FP64 capture absent");CaptureHeader record{};StageBusSessionHeader session{};
    if(!file.read(reinterpret_cast<char*>(&record),sizeof(record)) || record.magic!=0x464d5245 || record.version!=1 || record.kind!=1 || record.bytes!=sizeof(session) || record.errors || record.reserved || !file.read(reinterpret_cast<char*>(&session),sizeof(session)) || !nativeSession(session,corePid))throw std::runtime_error("Invalid FP64 capture session");
    bool footer=false;uint64_t errors=0;std::array<std::array<double,1024>,2> storage{};
    while(file.read(reinterpret_cast<char*>(&record),sizeof(record))){
        if(record.magic!=0x464d5245 || record.version!=1 || record.reserved)throw std::runtime_error("Invalid FP64 capture record");
        if(record.kind==3){if(record.bytes || file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid FP64 footer");footer=true;errors=record.errors;break;}
        StageBusBlockHeader block{};if(record.kind!=2 || record.bytes!=sizeof(block)+sizeof(storage) || record.errors)throw std::runtime_error("Invalid FP64 block length");
        if(!file.read(reinterpret_cast<char*>(&block),sizeof(block)) || !file.read(reinterpret_cast<char*>(storage.data()),sizeof(storage)))break;
        if(std::memcmp(block.session_id,session.session_id,16))throw std::runtime_error("Wrong FP64 output session");
        verifier.accept(block,storage[0].data(),storage[1].data());
    }
    const bool pass=verifier.finish(footer,errors);std::cout<<"{\"pass\":"<<(pass?"true":"false")<<",\"frames\":"<<verifier.frames()<<",\"mismatchedSamples\":"<<verifier.mismatchedSamples()<<",\"invalidClockFrames\":"<<verifier.invalidFrames()<<",\"interiorInvalidFrames\":"<<verifier.interiorInvalidFrames()<<",\"exactBinary64\":true,\"inputRate\":192000,\"outputRate\":192000,\"footer\":"<<(footer?"true":"false")<<",\"subscriberErrors\":"<<errors<<",\"reason\":\""<<verifier.reason()<<"\"}\n";return pass?0:2;
}
}
