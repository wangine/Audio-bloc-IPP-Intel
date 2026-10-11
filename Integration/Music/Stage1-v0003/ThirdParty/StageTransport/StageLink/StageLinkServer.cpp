#include "StageLinkServer.h"
#include <chrono>
namespace entropy::link {
bool StageLinkServer::start(std::string& error){if(thread_.joinable() || endpoint_.empty()){error="Control server already started or invalid endpoint";return false;}PipeSecurity security;if(!security.valid()){error="Current-user pipe ACL unavailable";return false;}pipe_=CreateNamedPipeW(endpoint_.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,security.attributes());if(pipe_==INVALID_HANDLE_VALUE){error="Control endpoint already owned or unavailable";return false;}stop_=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stop_){CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;return false;}try{thread_=std::thread(&StageLinkServer::worker,this);}catch(...){CloseHandle(stop_);CloseHandle(pipe_);stop_=nullptr;pipe_=INVALID_HANDLE_VALUE;return false;}return true;}
void StageLinkServer::stop(){if(stop_)SetEvent(stop_);cv_.notify_all();if(thread_.joinable())thread_.join();if(pipe_!=INVALID_HANDLE_VALUE)CloseHandle(pipe_);if(stop_)CloseHandle(stop_);pipe_=INVALID_HANDLE_VALUE;stop_=nullptr;std::lock_guard lock(mutex_);pending_=taken_=replied_=false;}
bool StageLinkServer::tryTake(StageLinkRequest& r,uint32_t& pid){std::lock_guard lock(mutex_);if(!pending_ || taken_)return false;r=request_;pid=pid_;taken_=true;return true;}
void StageLinkServer::complete(const StageLinkResponse& reply){std::lock_guard lock(mutex_);if(pending_ && taken_ && !replied_ && !std::memcmp(reply.nonce,request_.nonce,16)){response_=reply;replied_=true;cv_.notify_one();}}
void StageLinkServer::worker(){
 while(WaitForSingleObject(stop_,0)!=WAIT_OBJECT_0){OVERLAPPED connect{};connect.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!connect.hEvent)break;BOOL connected=ConnectNamedPipe(pipe_,&connect);auto code=connected?ERROR_SUCCESS:GetLastError();if(code==ERROR_IO_PENDING){HANDLE events[]={connect.hEvent,stop_};if(WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0){DWORD n=0;connected=GetOverlappedResult(pipe_,&connect,&n,FALSE);}else {CancelIoEx(pipe_,&connect);DWORD n;GetOverlappedResult(pipe_,&connect,&n,TRUE);connected=FALSE;}}else if(code==ERROR_PIPE_CONNECTED)connected=TRUE;CloseHandle(connect.hEvent);if(!connected)break;
  StageLinkRequest request{};ULONG pid=0;if(GetNamedPipeClientProcessId(pipe_,&pid) && transfer(pipe_,false,&request,sizeof(request),500,stop_) && validRequest(request,pid)){
   StageLinkResponse reply{};bool available=false;{std::unique_lock lock(mutex_);request_=request;pid_=pid;pending_=true;taken_=replied_=false;cv_.wait_for(lock,std::chrono::milliseconds(1500),[&]{return replied_ || WaitForSingleObject(stop_,0)==WAIT_OBJECT_0;});available=replied_;if(available)reply=response_;pending_=taken_=replied_=false;}
   if(available && transfer(pipe_,true,&reply,sizeof(reply),500,stop_)){DWORD ack=0;transfer(pipe_,false,&ack,sizeof(ack),500,stop_);}
  }
  DisconnectNamedPipe(pipe_);
 }
}
}
