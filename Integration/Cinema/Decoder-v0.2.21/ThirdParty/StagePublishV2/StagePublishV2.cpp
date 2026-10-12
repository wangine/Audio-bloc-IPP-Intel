#include "StagePublishV2.h"
#include "StagePublishV2Protocol.h"
#include "StageLinkSecurity.h"
#include "StageBusHashes.h"
#include "StageBusValidation.h"
#include <windows.h>
#include <objbase.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

namespace entropy::publish {
namespace {
bool validSession(const StageBusSessionHeader& s) noexcept {
    return stage::validateSession(s,s.mapping_bytes)==stage::StageBusValidationResult::Ok;
}
bool validBlock(const StageBusSessionHeader& s,const StageBusBlockHeader& b) noexcept {
    const bool music=s.layout_id==MUSIC_STEREO2_V1||s.layout_id==MUSIC_LRC3_V1;
    const bool cinema=s.layout_id==CINEMA_51_6_V1||s.layout_id==CINEMA_71_8_V1;
    const unsigned mask=music?3u:cinema?4u:7u;
    if(music&&(b.source_generation[2]||b.source_state[2]||b.source_rate[2]))return false;
    if(cinema&&(b.music_session_revision||b.source_generation[0]||b.source_generation[1]||
        b.source_state[0]||b.source_state[1]||b.source_rate[0]||b.source_rate[1]))return false;
    return !std::memcmp(s.session_id,b.session_id,16) && b.stream_epoch && b.block_sequence &&
        b.control_revision && b.frames==1024 && !b.reserved &&
        !((b.validity_mask|b.discontinuity_mask)&~mask) && b.first_frame<=UINT64_MAX-1024;
}
std::uint64_t qpcNow() noexcept {LARGE_INTEGER v{};QueryPerformanceCounter(&v);return v.QuadPart;}
bool newToken(std::uint8_t* token) {GUID guid{};if(FAILED(CoCreateGuid(&guid)))return false;std::memcpy(token,&guid,16);return true;}
std::wstring mappingName(const std::uint8_t* token) {
    constexpr wchar_t hex[]=L"0123456789abcdef";std::wstring result=L"Local\\Entropy.StagePublish.V2.";
    for(unsigned i=0;i<16;i++){result+=hex[token[i]>>4];result+=hex[token[i]&15];}return result;
}
struct Subscription {
    HANDLE mapping=nullptr,process=nullptr;wire::Queue* queue=nullptr;
    std::uint64_t write=0,initialHeartbeat=0;
    void clear() noexcept {
        if(queue){queue->connected.store(0,std::memory_order_release);UnmapViewOfFile(queue);}
        if(mapping)CloseHandle(mapping);if(process)CloseHandle(process);
        mapping=process=nullptr;queue=nullptr;write=initialHeartbeat=0;
    }
};
}
std::wstring endpointName() {
    auto name=entropy::link::endpointName();auto pos=name.find(L"StageLink.V1");
    if(pos==std::wstring::npos)return {};name.replace(pos,12,L"StagePublish.V2");return name;
}
std::wstring sourceEndpointName() {
    auto name=entropy::link::endpointName();auto pos=name.find(L"StageLink.V1");
    if(pos==std::wstring::npos)return {};name.replace(pos,12,L"StagePublish.Source.V2");return name;
}
struct Publisher::Impl {
    std::array<Subscription,wire::subscribers> subscriptions{};
    StageBusSessionHeader session{};
    std::atomic<std::uint64_t> dropped{0};
    HANDLE pipe=INVALID_HANDLE_VALUE,stop=nullptr;
    std::thread worker;
    std::mutex mutex;std::condition_variable cv;
    wire::Request request{};wire::Response response{};
    bool pending=false,replied=false;
    ~Impl(){for(auto& s:subscriptions)s.clear();}
    void serve() {
        while(WaitForSingleObject(stop,0)!=WAIT_OBJECT_0) {
            OVERLAPPED op{};op.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!op.hEvent)break;
            BOOL connected=ConnectNamedPipe(pipe,&op);auto code=connected?ERROR_SUCCESS:GetLastError();
            if(code==ERROR_IO_PENDING){HANDLE events[]={op.hEvent,stop};
                if(WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0){DWORD n=0;connected=GetOverlappedResult(pipe,&op,&n,FALSE);}
                else{CancelIoEx(pipe,&op);DWORD n=0;GetOverlappedResult(pipe,&op,&n,TRUE);connected=FALSE;}}
            else if(code==ERROR_PIPE_CONNECTED)connected=TRUE;
            CloseHandle(op.hEvent);if(!connected)break;
            wire::Request incoming{};ULONG verifiedPid=0;
            if(GetNamedPipeClientProcessId(pipe,&verifiedPid) &&
                entropy::link::transfer(pipe,false,&incoming,sizeof(incoming),500,stop) && wire::validRequest(incoming,verifiedPid)) {
                wire::Response outgoing{};bool available=false;
                {std::unique_lock lock(mutex);request=incoming;pending=true;replied=false;
                    cv.wait_for(lock,std::chrono::milliseconds(1500),[&]{return replied || WaitForSingleObject(stop,0)==WAIT_OBJECT_0;});
                    available=replied;if(available)outgoing=response;pending=replied=false;}
                if(available && entropy::link::transfer(pipe,true,&outgoing,sizeof(outgoing),500,stop)) {
                    DWORD ack=0;entropy::link::transfer(pipe,false,&ack,sizeof(ack),500,stop);
                }
            }
            DisconnectNamedPipe(pipe);
        }
    }
};
Publisher::Publisher():impl_(new Impl) {} Publisher::~Publisher(){stop();}
bool Publisher::start(const std::wstring& endpoint,std::string& error) {
    auto& p=*impl_;if(endpoint.empty() || p.worker.joinable()){error="Invalid/already owned passive publication endpoint";return false;}
    entropy::link::PipeSecurity security;if(!security.valid()){error="Current-user publication ACL unavailable";return false;}
    p.pipe=CreateNamedPipeW(endpoint.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,security.attributes());
    if(p.pipe==INVALID_HANDLE_VALUE){error="Publication endpoint unavailable or already owned";return false;}
    p.stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!p.stop){CloseHandle(p.pipe);p.pipe=INVALID_HANDLE_VALUE;error="Publication stop event failed";return false;}
    try{p.worker=std::thread([&p]{p.serve();});}catch(...){CloseHandle(p.stop);CloseHandle(p.pipe);p.stop=nullptr;p.pipe=INVALID_HANDLE_VALUE;error="Publication control worker failed";return false;}
    error.clear();return true;
}
void Publisher::stop() {
    auto& p=*impl_;if(p.stop)SetEvent(p.stop);p.cv.notify_all();if(p.worker.joinable())p.worker.join();
    for(auto& s:p.subscriptions)s.clear();if(p.pipe!=INVALID_HANDLE_VALUE)CloseHandle(p.pipe);if(p.stop)CloseHandle(p.stop);
    p.pipe=INVALID_HANDLE_VALUE;p.stop=nullptr;p.pending=p.replied=false;
}
void Publisher::setSession(const StageBusSessionHeader& session) noexcept {
    auto& p=*impl_;if(std::memcmp(&p.session,&session,sizeof(session))){for(auto& s:p.subscriptions)s.clear();p.session=session;}
}
bool Publisher::needsService() noexcept {
    auto& p=*impl_;std::lock_guard lock(p.mutex);return p.pending && !p.replied;
}
void Publisher::service() noexcept {
    auto& p=*impl_;const auto now=qpcNow();
    for(auto& s:p.subscriptions)if(s.queue){
        const auto beat=s.queue->heartbeat.load(std::memory_order_acquire);
        const auto last=beat?beat:s.initialHeartbeat;
        if(s.queue->connected.load(std::memory_order_acquire)!=1 ||
            WaitForSingleObject(s.process,0)==WAIT_OBJECT_0 ||
            (now>=last && (now-last)/p.session.qpc_frequency>=5))s.clear();
    }
    std::lock_guard lock(p.mutex);if(!p.pending || p.replied)return;
    wire::Response reply{};reply.publisherPid=GetCurrentProcessId();std::memcpy(reply.nonce,p.request.nonce,16);
    if(validSession(p.session)){
        Subscription* selected=nullptr;for(auto& s:p.subscriptions)if(!s.queue){selected=&s;break;}
        reply.status=selected?wire::Status::Invalid:wire::Status::Full;
        if(selected)try {
            entropy::link::PipeSecurity security;
            if(security.valid() && newToken(reply.token)){
                const auto name=mappingName(reply.token);
                auto& s=*selected;s.process=OpenProcess(SYNCHRONIZE,FALSE,p.request.clientPid);
                s.mapping=s.process?CreateFileMappingW(INVALID_HANDLE_VALUE,security.attributes(),PAGE_READWRITE,0,sizeof(wire::Queue),name.c_str()):nullptr;
                const auto creationError=GetLastError();
                if(s.mapping && creationError!=ERROR_ALREADY_EXISTS)s.queue=static_cast<wire::Queue*>(MapViewOfFile(s.mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(wire::Queue)));
                if(s.queue){new(s.queue) wire::Queue;s.queue->publisherPid=reply.publisherPid;s.queue->session=p.session;
                    std::memcpy(s.queue->token,reply.token,16);s.initialHeartbeat=now;
                    reply.session=p.session;std::memcpy(reply.mapping,name.c_str(),(name.size()+1)*sizeof(wchar_t));reply.status=wire::Status::Ok;}
                else s.clear();
            }
        }catch(...){selected->clear();reply.status=wire::Status::Invalid;}
    }
    p.response=reply;p.replied=true;p.cv.notify_one();
}
void Publisher::publish(const StageBusBlockHeader& block,const double* const* planes) noexcept {
    auto& p=*impl_;const bool valid=validBlock(p.session,block) && planes;
    bool pointers=valid;if(pointers)for(unsigned c=0;c<p.session.channels;c++)pointers=pointers && planes[c];
    for(auto& s:p.subscriptions){auto* q=s.queue;if(!q || q->connected.load(std::memory_order_acquire)!=1)continue;
        const auto read=q->read.load(std::memory_order_acquire);
        if(!pointers || q->write.load(std::memory_order_relaxed)!=s.write || read>s.write || s.write-read>wire::capacity || s.write==UINT64_MAX){q->connected.store(2,std::memory_order_release);continue;}
        if(s.write-read==wire::capacity){q->drops.fetch_add(1,std::memory_order_release);p.dropped.fetch_add(1,std::memory_order_relaxed);continue;}
        auto& slot=q->slots[s.write%wire::capacity];std::memcpy(&slot.header,&block,sizeof(block));
        for(unsigned c=0;c<p.session.channels;c++)std::memcpy(slot.planes[c],planes[c],1024*sizeof(double));
        q->write.store(++s.write,std::memory_order_release);
    }
}
std::uint64_t Publisher::dropped() const noexcept{return impl_->dropped.load(std::memory_order_relaxed);}
struct Subscriber::Impl {
    HANDLE mapping=nullptr,process=nullptr;wire::Queue* queue=nullptr;
    StageBusSessionHeader session{};StageBusBlockHeader last{};std::uint64_t read=0;bool cursor=false,readable=false;
};
Subscriber::Subscriber():impl_(new Impl) {} Subscriber::~Subscriber(){close();}
void Subscriber::close() noexcept {
    auto& s=*impl_;if(s.queue){s.queue->connected.store(0,std::memory_order_release);UnmapViewOfFile(s.queue);}
    if(s.mapping)CloseHandle(s.mapping);if(s.process)CloseHandle(s.process);
    s.queue=nullptr;s.mapping=s.process=nullptr;s.read=0;s.cursor=s.readable=false;
}
bool Subscriber::connect(const std::wstring& endpoint,std::string& error) {
    close();wire::Request request{};request.clientPid=GetCurrentProcessId();
    if(endpoint.empty() || !newToken(request.nonce)){error="Invalid passive endpoint/nonce";return false;}
    HANDLE pipe=CreateFileW(endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
    if(pipe==INVALID_HANDLE_VALUE && GetLastError()==ERROR_PIPE_BUSY && WaitNamedPipeW(endpoint.c_str(),2000))
        pipe=CreateFileW(endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
    if(pipe==INVALID_HANDLE_VALUE){error="Passive publication endpoint unavailable: "+std::to_string(GetLastError());return false;}
    ULONG serverPid=0;DWORD mode=PIPE_READMODE_MESSAGE;wire::Response response{};
    bool ok=GetNamedPipeServerProcessId(pipe,&serverPid) && SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr) &&
        entropy::link::transfer(pipe,true,&request,sizeof(request),2000) && entropy::link::transfer(pipe,false,&response,sizeof(response),2000);
    ok=ok && response.magic==wire::magic && response.version==wire::version && response.bytes==sizeof(response) &&
        response.publisherPid==serverPid && !std::memcmp(request.nonce,response.nonce,16) && wire::zero(response.reserved,sizeof(response.reserved)) &&
        response.status==wire::Status::Ok && !wire::zero(response.token,16) && validSession(response.session) && response.mapping[159]==0;
    if(ok)ok=mappingName(response.token)==response.mapping;
    DWORD ack=0x324B4341;if(ok)ok=entropy::link::transfer(pipe,true,&ack,sizeof(ack),500);CloseHandle(pipe);
    if(!ok){error="Passive subscription identity, capacity or session validation failed";return false;}
    auto& s=*impl_;s.process=OpenProcess(SYNCHRONIZE,FALSE,serverPid);
    s.mapping=s.process?OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,response.mapping):nullptr;
    s.queue=s.mapping?static_cast<wire::Queue*>(MapViewOfFile(s.mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(wire::Queue))):nullptr;
    ok=s.queue && s.queue->magic==wire::magic && s.queue->version==wire::version && s.queue->bytes==sizeof(wire::Queue) &&
        s.queue->capacity==wire::capacity && s.queue->publisherPid==serverPid && !std::memcmp(s.queue->token,response.token,16) &&
        !std::memcmp(&s.queue->session,&response.session,sizeof(response.session)) && s.queue->connected.load(std::memory_order_acquire)==1 &&
        s.queue->read.load(std::memory_order_acquire)==0;
    if(!ok){close();error="Passive mapping validation failed";return false;}
    s.session=response.session;s.readable=true;heartbeat(qpcNow());error.clear();return true;
}
ReadResult Subscriber::read(StageBusBlockHeader& block,double* const* planes) noexcept {
    auto& s=*impl_;auto* q=s.queue;if(!q || !s.readable)return ReadResult::Disconnected;
    // Retire logically here; mapping/process handle cleanup belongs to close() on control.
    const auto retire=[&](ReadResult result) noexcept {s.readable=false;q->connected.store(0,std::memory_order_release);return result;};
    const auto state=q->connected.load(std::memory_order_acquire);
    if(state!=1)return retire(state==2?ReadResult::Invalid:ReadResult::Disconnected);
    if(q->drops.load(std::memory_order_acquire))return retire(ReadResult::Gap);
    const auto write=q->write.load(std::memory_order_acquire);
    if(q->read.load(std::memory_order_relaxed)!=s.read || write<s.read || write-s.read>wire::capacity)return retire(ReadResult::Invalid);
    if(write==s.read)return ReadResult::Empty;
    auto& slot=q->slots[s.read%wire::capacity];StageBusBlockHeader candidate{};std::memcpy(&candidate,&slot.header,sizeof(candidate));
    if(!validBlock(s.session,candidate) || !planes)return retire(ReadResult::Invalid);
    for(unsigned c=0;c<s.session.channels;c++)if(!planes[c])return retire(ReadResult::Invalid);
    // Source discontinuity_mask is immutable telemetry, which can be cumulative.
    // Only observed transport/source cursor discontinuity is a delivery Gap.
    if(s.cursor && (candidate.stream_epoch!=s.last.stream_epoch ||
        s.last.block_sequence==UINT64_MAX || candidate.block_sequence!=s.last.block_sequence+1 ||
        candidate.first_frame!=s.last.first_frame+1024))return retire(ReadResult::Gap);
    for(unsigned c=0;c<s.session.channels;c++)std::memcpy(planes[c],slot.planes[c],1024*sizeof(double));
    if(q->drops.load(std::memory_order_acquire))return retire(ReadResult::Gap);
    block=candidate;s.last=candidate;s.cursor=true;q->read.store(++s.read,std::memory_order_release);return ReadResult::Block;
}
void Subscriber::heartbeat(std::uint64_t qpc) noexcept {
    auto& s=*impl_;if(!s.queue)return;
    // Called by the connector/control worker, never the graph callback.
    if(WaitForSingleObject(s.process,0)==WAIT_OBJECT_0)s.queue->connected.store(0,std::memory_order_release);
    else s.queue->heartbeat.store(qpc,std::memory_order_release);
}
const StageBusSessionHeader& Subscriber::session() const noexcept{return impl_->session;}
}
