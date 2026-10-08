#include "CommandServer.h"
#include <sddl.h>
#include <array>
#include <vector>
#include <stdexcept>
namespace entropy::ipc {
namespace {
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
std::wstring userAcl(){
    Handle token;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value))throw std::runtime_error("Cannot query server user");
    DWORD size=0;GetTokenInformation(token.value,TokenUser,nullptr,0,&size);std::vector<unsigned char> storage(size);
    if(!size||!GetTokenInformation(token.value,TokenUser,storage.data(),size,&size))throw std::runtime_error("Cannot read server user");
    wchar_t* sid=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(storage.data())->User.Sid,&sid))throw std::runtime_error("Cannot encode server user");
    std::wstring acl=L"D:P(A;;GA;;;"+std::wstring(sid)+L")";LocalFree(sid);return acl;
}
// The owner waits only for its own overlapped transport operation or shutdown.
bool await(HANDLE pipe,HANDLE stop,OVERLAPPED& op,DWORD& count,bool started,DWORD timeout=INFINITE){
    if(started)return true;if(GetLastError()!=ERROR_IO_PENDING)return false;
    HANDLE events[]{stop,op.hEvent};auto wait=WaitForMultipleObjects(2,events,FALSE,timeout);
    if(wait!=WAIT_OBJECT_0+1){CancelIoEx(pipe,&op);GetOverlappedResult(pipe,&op,&count,TRUE);return false;}
    return GetOverlappedResult(pipe,&op,&count,FALSE)!=FALSE;
}
}
std::wstring CommandServer::pipePath(const std::wstring& endpoint){
    if(endpoint.empty()||endpoint.size()>128||endpoint.rfind(L"EntropyAudioCore.",0)!=0)throw std::invalid_argument("Invalid isolated AudioCore endpoint");
    for(auto c:endpoint)if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'_'||c==L'-'))throw std::invalid_argument("Invalid endpoint character");
    return L"\\\\.\\pipe\\"+endpoint;
}
bool CommandServer::start(const std::wstring& endpoint,Handler handler,Disconnect disconnect,std::string& error){
    if(thread_.joinable()){error="Command server already started";return false;}
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    try {
        auto path=pipePath(endpoint),acl=userAcl();
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))throw std::runtime_error("Cannot create same-user pipe ACL");
        SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};
        pipe_=CreateNamedPipeW(path.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,&attributes);
        LocalFree(descriptor);descriptor=nullptr;
        if(pipe_==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot own isolated command endpoint: "+std::to_string(GetLastError()));
        stop_=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stop_)throw std::runtime_error("Cannot create server shutdown event");
        handler_=std::move(handler);disconnect_=std::move(disconnect);thread_=std::thread(&CommandServer::worker,this);error.clear();return true;
    }catch(const std::exception& e){if(descriptor)LocalFree(descriptor);error=e.what();stop();return false;}
}
void CommandServer::stop() noexcept {
    if(stop_)SetEvent(stop_);if(thread_.joinable())thread_.join();
    if(pipe_!=INVALID_HANDLE_VALUE){CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;}
    if(stop_){CloseHandle(stop_);stop_=nullptr;}
}
void CommandServer::worker() noexcept {
    Handle event;event.value=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event.value)return;
    std::array<unsigned char,16384> bytes{};
    while(WaitForSingleObject(stop_,0)!=WAIT_OBJECT_0){
        ResetEvent(event.value);OVERLAPPED op{};op.hEvent=event.value;DWORD count=0;
        bool connected=ConnectNamedPipe(pipe_,&op)!=FALSE;
        if(!connected&&GetLastError()==ERROR_PIPE_CONNECTED)connected=true;
        else if(!connected)connected=await(pipe_,stop_,op,count,false);
        if(!connected){if(WaitForSingleObject(stop_,0)==WAIT_OBJECT_0)break;DisconnectNamedPipe(pipe_);continue;}
        try {
            Framer reader;auto lastTraffic=GetTickCount64();
            while(WaitForSingleObject(stop_,0)!=WAIT_OBJECT_0){
                ResetEvent(event.value);op={};op.hEvent=event.value;count=0;
                auto idle=GetTickCount64()-lastTraffic;if(idle>=kControlLeaseMs)break;
                bool started=ReadFile(pipe_,bytes.data(),DWORD(bytes.size()),&count,&op)!=FALSE;
                if(!await(pipe_,stop_,op,count,started,DWORD(kControlLeaseMs-idle))||count==0)break;
                reader.push(std::span(bytes.data(),count));
                for(const auto& request:reader.take()){
                    lastTraffic=GetTickCount64(); // Renew only a complete valid frame, not partial bytes.
                    auto reply=handler_(request);reply.requestId=request.requestId;auto encoded=encode(reply);size_t done=0;
                    while(done<encoded.size()){
                        ResetEvent(event.value);op={};op.hEvent=event.value;count=0;
                        started=WriteFile(pipe_,encoded.data()+done,DWORD(encoded.size()-done),&count,&op)!=FALSE;
                        if(!await(pipe_,stop_,op,count,started,kControlLeaseMs)||!count)throw std::runtime_error("Peer disconnected");done+=count;
                    }
                }
            }
        }catch(...){} // Framing failure closes the controlling session, never audio processing.
        try{if(disconnect_)disconnect_();}catch(...){}
        DisconnectNamedPipe(pipe_);
    }
}
}
