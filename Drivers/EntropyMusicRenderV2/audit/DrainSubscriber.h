#pragma once
// The frozen ABI intentionally pads cache-line-aligned atomics (C4324).
#pragma warning(push)
#pragma warning(disable:4324)
#include "StagePublishV2Protocol.h"
#pragma warning(pop)
#include "StageLinkSecurity.h"
#include "StageBusValidation.h"
#include "OutputDrainPolicy.h"
#include <windows.h>
#include <objbase.h>
#include <stdexcept>
#include <string>
namespace entropy::music::audit {
// Audit-only consumer: unlike the frozen runtime subscriber, retains its mapping
// after producer exit so all final slots can be accounted for before the footer.
class DrainSubscriber {
    using Queue=entropy::publish::wire::Queue;
    HANDLE process_=nullptr,mapping_=nullptr;Queue* queue_=nullptr;
    uint64_t read_=0;StageBusBlockHeader last_{};bool haveLast_=false,identityValid_=false,retired_=false;
public:
    enum class Result {Block,Empty,End,Invalid};
    ~DrainSubscriber(){if(queue_){if(identityValid_)queue_->connected.store(0,std::memory_order_release);UnmapViewOfFile(queue_);}if(mapping_)CloseHandle(mapping_);if(process_)CloseHandle(process_);}
    void connect(const std::wstring& endpoint,uint32_t pid){
        namespace wire=entropy::publish::wire;wire::Request request;request.clientPid=GetCurrentProcessId();GUID nonce{};
        if(FAILED(CoCreateGuid(&nonce)))throw std::runtime_error("Output nonce unavailable");std::memcpy(request.nonce,&nonce,16);
        HANDLE pipe=CreateFileW(endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        if(pipe==INVALID_HANDLE_VALUE && GetLastError()==ERROR_PIPE_BUSY && WaitNamedPipeW(endpoint.c_str(),2000))pipe=CreateFileW(endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        if(pipe==INVALID_HANDLE_VALUE)throw std::runtime_error("Output pipe unavailable");
        ULONG server=0;DWORD mode=PIPE_READMODE_MESSAGE;wire::Response response{};
        bool ok=GetNamedPipeServerProcessId(pipe,&server) && server==pid && SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr) &&
            entropy::link::transfer(pipe,true,&request,sizeof(request),2000) && entropy::link::transfer(pipe,false,&response,sizeof(response),2000);
        std::wstring name=L"Local\\Entropy.StagePublish.V2.";constexpr wchar_t hex[]=L"0123456789abcdef";
        for(auto byte:response.token){name+=hex[byte>>4];name+=hex[byte&15];}
        ok=ok && response.magic==wire::magic && response.version==wire::version && response.bytes==sizeof(response) && response.publisherPid==pid &&
            response.status==wire::Status::Ok && !std::memcmp(request.nonce,response.nonce,16) && !wire::zero(response.token,16) && wire::zero(response.reserved,sizeof(response.reserved)) &&
            response.mapping[159]==0 && name==response.mapping && entropy::stage::validateSession(response.session,static_cast<size_t>(response.session.mapping_bytes))==entropy::stage::StageBusValidationResult::Ok;
        DWORD ack=0x324b4341;if(ok)ok=entropy::link::transfer(pipe,true,&ack,sizeof(ack),500);CloseHandle(pipe);
        if(!ok)throw std::runtime_error("Output handshake identity failed");
        process_=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        mapping_=process_?OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,response.mapping):nullptr;
        queue_=mapping_?static_cast<Queue*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Queue))):nullptr;
        if(!queue_ || queue_->magic!=wire::magic || queue_->version!=wire::version || queue_->bytes!=sizeof(Queue) || queue_->capacity!=wire::capacity || queue_->publisherPid!=pid ||
            std::memcmp(queue_->token,response.token,16) || std::memcmp(&queue_->session,&response.session,sizeof(response.session)) || queue_->read.load()!=0 || queue_->connected.load()!=1)throw std::runtime_error("Output mapping identity failed");
        identityValid_=true;
    }
    const StageBusSessionHeader& session() const noexcept{return queue_->session;}
    Result read(StageBusBlockHeader& header,double* l,double* r){
        namespace wire=entropy::publish::wire;
        if(!queue_)return Result::Invalid;
        const bool exited=WaitForSingleObject(process_,0)==WAIT_OBJECT_0;DWORD exitCode=STILL_ACTIVE;if(exited && !GetExitCodeProcess(process_,&exitCode))return Result::Invalid;
        const auto state=queue_->connected.load(std::memory_order_acquire);
        if(state!=1)retired_=true;
        if(state>1 || queue_->drops.load(std::memory_order_acquire) || (exited && exitCode!=0))return Result::Invalid;
        LARGE_INTEGER qpc;QueryPerformanceCounter(&qpc);queue_->heartbeat.store(qpc.QuadPart,std::memory_order_release);
        const auto write=queue_->write.load(std::memory_order_acquire);
        if(queue_->read.load(std::memory_order_relaxed)!=read_ || write<read_ || write-read_>wire::capacity)return Result::Invalid;
        // Frozen Publisher clears both clean and failed/expired subscriptions to0.
        // No immutable terminal record distinguishes them. Preserve queued bytes,
        // but a retired mapping cannot certify complete publication, even at exit.
        if(write==read_){if(retired_)return Result::Invalid;return completeOutputBoundary(exited,exitCode,read_,write,queue_->drops.load())?Result::End:Result::Empty;}
        auto& slot=queue_->slots[read_%wire::capacity];StageBusBlockHeader candidate;std::memcpy(&candidate,&slot.header,sizeof(candidate));
        if(entropy::stage::validateBlock(queue_->session,candidate,static_cast<size_t>(queue_->session.slot_stride))!=entropy::stage::StageBusValidationResult::Ok ||
            (haveLast_ && (candidate.stream_epoch!=last_.stream_epoch || candidate.block_sequence!=last_.block_sequence+1 || candidate.first_frame!=last_.first_frame+1024)))return Result::Invalid;
        std::memcpy(l,slot.planes[0],8192);std::memcpy(r,slot.planes[1],8192);
        if(queue_->drops.load(std::memory_order_acquire))return Result::Invalid;
        header=candidate;last_=candidate;haveLast_=true;queue_->read.store(++read_,std::memory_order_release);return Result::Block;
    }
};
}
