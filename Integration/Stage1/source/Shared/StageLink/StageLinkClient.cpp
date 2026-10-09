#include "StageLinkClient.h"
#include <algorithm>
namespace entropy::link {
bool StageLinkClient::transact(const StageLinkRequest& request,StageLinkResponse& response,uint32_t timeout,std::string& error){
 if(endpoint_.empty() || !validRequest(request,GetCurrentProcessId())){error="Invalid local control request";return false;}
 const auto deadline=GetTickCount64()+timeout; auto remaining=[&](){auto now=GetTickCount64();return now<deadline?static_cast<DWORD>(deadline-now):DWORD(0);}; HANDLE pipe=INVALID_HANDLE_VALUE; do {pipe=CreateFileW(endpoint_.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(pipe!=INVALID_HANDLE_VALUE)break;const auto code=GetLastError();if(code!=ERROR_PIPE_BUSY || !remaining() || !WaitNamedPipeW(endpoint_.c_str(),remaining())){error="Source integration endpoint unavailable: "+std::to_string(code);return false;}}while(remaining());if(pipe==INVALID_HANDLE_VALUE){error="Source endpoint connection deadline";return false;}
 ULONG serverPid=0;DWORD mode=PIPE_READMODE_MESSAGE;StageLinkResponse reply{};
 bool ok=GetNamedPipeServerProcessId(pipe,&serverPid) && SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr) && transfer(pipe,true,const_cast<StageLinkRequest*>(&request),sizeof(request),remaining()) && transfer(pipe,false,&reply,sizeof(reply),remaining());
 ok=ok && reply.magic==linkMagic && reply.version==1 && reply.bytes==sizeof(reply) && reply.sourcePid==serverPid && !std::memcmp(reply.nonce,request.nonce,16) && !reply.reserved[0] && !reply.reserved[1] && reply.status<=StageLinkStatus::Fault;
 DWORD ack=0x314B4341;if(ok)ok=transfer(pipe,true,&ack,sizeof(ack),remaining());CloseHandle(pipe);if(!ok){error="Source control response failed identity/timeout validation";return false;}response=reply;return true;
}
}
