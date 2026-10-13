#include "StageLinkSecurity.h"
#include "StageLinkProtocol.h"
#include <sddl.h>
#include <vector>
#include <objbase.h>
namespace entropy::link {
namespace {std::wstring sid(){HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return {};DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<unsigned char> data(size);bool ok=GetTokenInformation(token,TokenUser,data.data(),size,&size);CloseHandle(token);LPWSTR string=nullptr;if(!ok || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&string))return {};std::wstring value=string;LocalFree(string);return value;}}
std::wstring endpointName(){auto value=sid();return value.empty()?std::wstring():L"\\\\.\\pipe\\Entropy.StageLink.V1."+value;}
PipeSecurity::PipeSecurity(){auto value=sid();if(value.empty())return;auto acl=L"D:P(A;;GA;;;"+value+L")";if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor_,nullptr))return;attributes_={sizeof(attributes_),descriptor_,FALSE};}
PipeSecurity::~PipeSecurity(){if(descriptor_)LocalFree(descriptor_);}
StageLinkRequest makeRequest(StageLinkAction action){StageLinkRequest r;r.action=action;r.clientPid=GetCurrentProcessId();GUID nonce{};if(SUCCEEDED(CoCreateGuid(&nonce)))std::memcpy(r.nonce,&nonce,16);return r;}
bool transfer(HANDLE pipe,bool write,void* data,DWORD bytes,DWORD timeout,HANDLE stop) noexcept {
 OVERLAPPED op{};op.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!op.hEvent)return false;DWORD done=0;BOOL ok=write?WriteFile(pipe,data,bytes,&done,&op):ReadFile(pipe,data,bytes,&done,&op);
 if(!ok && GetLastError()==ERROR_IO_PENDING){HANDLE events[]={op.hEvent,stop};auto wait=WaitForMultipleObjects(stop?2:1,events,FALSE,timeout);if(wait==WAIT_OBJECT_0)ok=GetOverlappedResult(pipe,&op,&done,FALSE);else {CancelIoEx(pipe,&op);GetOverlappedResult(pipe,&op,&done,TRUE);ok=FALSE;}}
 CloseHandle(op.hEvent);return ok && done==bytes;
}
}
