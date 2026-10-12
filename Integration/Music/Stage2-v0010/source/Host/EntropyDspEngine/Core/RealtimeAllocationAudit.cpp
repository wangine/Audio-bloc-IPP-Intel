#include "RealtimeAllocationAudit.h"
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <malloc.h>
#include <windows.h>
#include <tlhelp32.h>
#include <cstring>
namespace {
thread_local unsigned phase=0;
thread_local std::uint64_t localNew[2]{},localDelete[2]{};
std::atomic<std::uint64_t> blocks{0},news[2]{},deletes[2]{},allocatingBlocks[2]{};
struct OperationCounters {bool enabled=false;std::uint64_t values[5]{};};
thread_local OperationCounters localOps;
DWORD operationTls=TLS_OUT_OF_INDEXES;
std::atomic<std::uint64_t> ops[5]{};
std::atomic<unsigned> patchedImports{0};
void countOp(unsigned kind) noexcept {
 // Never enter compiler-emulated thread_local initialization inside a heap hook.
 if(operationTls==TLS_OUT_OF_INDEXES)return;
 const auto savedError=GetLastError();
 auto* counters=static_cast<OperationCounters*>(TlsGetValue(operationTls));
 if(counters&&counters->enabled)++counters->values[kind];
 SetLastError(savedError);
}
void countNew() noexcept {if(phase)++localNew[phase-1];}
void countDelete(void* p) noexcept {if(p&&phase)++localDelete[phase-1];}
}
namespace {
decltype(&HeapAlloc) realHeapAlloc=nullptr;
decltype(&HeapFree) realHeapFree=nullptr;
decltype(&HeapReAlloc) realHeapReAlloc=nullptr;
using RtlAlloc=void* (WINAPI*)(void*,ULONG,SIZE_T);
using RtlFree=BOOLEAN (WINAPI*)(void*,ULONG,void*);
using RtlRealloc=void* (WINAPI*)(void*,ULONG,void*,SIZE_T);
RtlAlloc realRtlAlloc=nullptr;RtlFree realRtlFree=nullptr;RtlRealloc realRtlRealloc=nullptr;
void* WINAPI auditHeapAlloc(HANDLE h,DWORD f,SIZE_T n){countOp(0);return realHeapAlloc(h,f,n);}
BOOL WINAPI auditHeapFree(HANDLE h,DWORD f,void* p){if(p)countOp(1);return realHeapFree(h,f,p);}
void* WINAPI auditHeapReAlloc(HANDLE h,DWORD f,void* p,SIZE_T n){countOp(0);if(p)countOp(1);return realHeapReAlloc(h,f,p,n);}
void* WINAPI auditRtlAlloc(void* h,ULONG f,SIZE_T n){countOp(0);return realRtlAlloc(h,f,n);}
BOOLEAN WINAPI auditRtlFree(void* h,ULONG f,void* p){if(p)countOp(1);return realRtlFree(h,f,p);}
void* WINAPI auditRtlRealloc(void* h,ULONG f,void* p,SIZE_T n){countOp(0);if(p)countOp(1);return realRtlRealloc(h,f,p,n);}
decltype(&WaitForSingleObject) realWait=nullptr;
decltype(&WaitForMultipleObjects) realWaitMany=nullptr;
decltype(&Sleep) realSleep=nullptr;
decltype(&EnterCriticalSection) realEnter=nullptr;
decltype(&AcquireSRWLockExclusive) realExclusive=nullptr;
decltype(&AcquireSRWLockShared) realShared=nullptr;
decltype(&ReadFile) realRead=nullptr;decltype(&WriteFile) realWrite=nullptr;
DWORD WINAPI auditWait(HANDLE h,DWORD ms){if(ms)countOp(2);return realWait(h,ms);}
DWORD WINAPI auditWaitMany(DWORD n,const HANDLE* h,BOOL all,DWORD ms){if(ms)countOp(2);return realWaitMany(n,h,all,ms);}
void WINAPI auditSleep(DWORD ms){if(ms)countOp(2);realSleep(ms);}
void WINAPI auditEnter(LPCRITICAL_SECTION c){countOp(3);realEnter(c);}
void WINAPI auditExclusive(PSRWLOCK l){countOp(3);realExclusive(l);}
void WINAPI auditShared(PSRWLOCK l){countOp(3);realShared(l);}
BOOL WINAPI auditRead(HANDLE h,void* b,DWORD n,DWORD* done,LPOVERLAPPED o){countOp(4);return realRead(h,b,n,done,o);}
BOOL WINAPI auditWrite(HANDLE h,const void* b,DWORD n,DWORD* done,LPOVERLAPPED o){countOp(4);return realWrite(h,b,n,done,o);}
struct ImportHook {const char* name;void* replacement;};
ImportHook hooks[]={
 {"HeapAlloc",reinterpret_cast<void*>(&auditHeapAlloc)},{"HeapFree",reinterpret_cast<void*>(&auditHeapFree)},{"HeapReAlloc",reinterpret_cast<void*>(&auditHeapReAlloc)},
 {"RtlAllocateHeap",reinterpret_cast<void*>(&auditRtlAlloc)},{"RtlFreeHeap",reinterpret_cast<void*>(&auditRtlFree)},{"RtlReAllocateHeap",reinterpret_cast<void*>(&auditRtlRealloc)},
 {"WaitForSingleObject",reinterpret_cast<void*>(&auditWait)},{"WaitForMultipleObjects",reinterpret_cast<void*>(&auditWaitMany)},{"Sleep",reinterpret_cast<void*>(&auditSleep)},
 {"EnterCriticalSection",reinterpret_cast<void*>(&auditEnter)},{"AcquireSRWLockExclusive",reinterpret_cast<void*>(&auditExclusive)},{"AcquireSRWLockShared",reinterpret_cast<void*>(&auditShared)},
 {"ReadFile",reinterpret_cast<void*>(&auditRead)},{"WriteFile",reinterpret_cast<void*>(&auditWrite)}
};
}

void* operator new(std::size_t n){countNew();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{countDelete(p);std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
void* operator new(std::size_t n,std::align_val_t a){countNew();if(auto* p=_aligned_malloc(n?n:1,static_cast<std::size_t>(a)))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void* p,std::align_val_t)noexcept{countDelete(p);_aligned_free(p);}
void operator delete[](void* p,std::align_val_t a)noexcept{::operator delete(p,a);}
void operator delete(void* p,std::size_t,std::align_val_t a)noexcept{::operator delete(p,a);}
void operator delete[](void* p,std::size_t,std::align_val_t a)noexcept{::operator delete(p,a);}
namespace entropy::host::allocation_audit {
bool installHooks() noexcept {
 if(operationTls==TLS_OUT_OF_INDEXES)operationTls=TlsAlloc();
 if(operationTls==TLS_OUT_OF_INDEXES)return false;
 const auto kernel=GetModuleHandleW(L"kernel32.dll");const auto nt=GetModuleHandleW(L"ntdll.dll");
#define LOAD(name,field) field=reinterpret_cast<decltype(field)>(GetProcAddress(kernel,name));if(!field)return false
 LOAD("HeapAlloc",realHeapAlloc);LOAD("HeapFree",realHeapFree);LOAD("HeapReAlloc",realHeapReAlloc);
 LOAD("WaitForSingleObject",realWait);LOAD("WaitForMultipleObjects",realWaitMany);LOAD("Sleep",realSleep);
 LOAD("EnterCriticalSection",realEnter);LOAD("AcquireSRWLockExclusive",realExclusive);LOAD("AcquireSRWLockShared",realShared);
 LOAD("ReadFile",realRead);LOAD("WriteFile",realWrite);
#undef LOAD
 realRtlAlloc=reinterpret_cast<RtlAlloc>(GetProcAddress(nt,"RtlAllocateHeap"));
 realRtlFree=reinterpret_cast<RtlFree>(GetProcAddress(nt,"RtlFreeHeap"));
 realRtlRealloc=reinterpret_cast<RtlRealloc>(GetProcAddress(nt,"RtlReAllocateHeap"));
 if(!realRtlAlloc||!realRtlFree||!realRtlRealloc)return false;
 const auto list=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());if(list==INVALID_HANDLE_VALUE)return false;
 MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);bool ok=true;
 if(Module32FirstW(list,&entry))do{
  // Do not patch the implementation of the intercepted APIs themselves.
  if(!_wcsicmp(entry.szModule,L"kernel32.dll")||!_wcsicmp(entry.szModule,L"kernelbase.dll")||!_wcsicmp(entry.szModule,L"ntdll.dll"))continue;
  const auto moduleBefore=patchedImports.load();
  auto* base=entry.modBaseAddr;auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE)continue;
  auto* pe=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);if(pe->Signature!=IMAGE_NT_SIGNATURE)continue;
  const auto dir=pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!dir.VirtualAddress)continue;
  for(auto* d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+dir.VirtualAddress);d->Name;++d){
   if(!d->OriginalFirstThunk)continue;
   auto* names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+d->OriginalFirstThunk);
   auto* addresses=reinterpret_cast<IMAGE_THUNK_DATA*>(base+d->FirstThunk);
   for(;names->u1.AddressOfData;++names,++addresses){
    if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
    const auto* name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData)->Name;
    for(const auto& hook:hooks)if(std::strcmp(reinterpret_cast<const char*>(name),hook.name)==0){
     auto* slot=&addresses->u1.Function;const auto replacement=reinterpret_cast<ULONG_PTR>(hook.replacement);
     if(*slot==replacement)break;
     DWORD before=0,ignored=0;
     if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&before)){ok=false;break;}
     InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),hook.replacement);
     if(!VirtualProtect(slot,sizeof(*slot),before,&ignored))ok=false;
     patchedImports.fetch_add(1,std::memory_order_relaxed);break;
    }
   }
  }
  if(patchedImports.load()!=moduleBefore)std::fprintf(stderr,"A02_IMPORT_MODULE name=%ls hooks=%u\n",entry.szModule,patchedImports.load()-moduleBefore);
 }while(Module32NextW(list,&entry));
 CloseHandle(list);return ok&&patchedImports.load()!=0;
}
void reset()noexcept{phase=0;blocks=0;for(unsigned i=0;i<2;++i){news[i]=0;deletes[i]=0;allocatingBlocks[i]=0;}for(auto& v:ops)v=0;}
Snapshot snapshot()noexcept{Snapshot s;s.cppNew=news[0]+news[1];s.cppDelete=deletes[0]+deletes[1];s.heapAlloc=ops[0];s.heapFree=ops[1];s.wait=ops[2];s.lock=ops[3];s.io=ops[4];return s;}
bool selfTest(){
 if(!installHooks())return false;
 CRITICAL_SECTION critical;InitializeCriticalSection(&critical);
 HANDLE file=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
 reset();begin();
 void* (*volatile allocate)(std::size_t)=::operator new;
 void (*volatile release)(void*)=::operator delete;
 auto* p=allocate(64);release(p);
 p=HeapAlloc(GetProcessHeap(),0,64);HeapFree(GetProcessHeap(),0,p);
 EnterCriticalSection(&critical);LeaveCriticalSection(&critical);Sleep(1);
 DWORD written=0;WriteFile(file,"x",1,&written,nullptr);
 audioPhase();p=allocate(128);release(p);end();
 CloseHandle(file);DeleteCriticalSection(&critical);report();const auto s=snapshot();
 return blocks.load()==1&&s.cppNew==2&&s.cppDelete==2&&s.heapAlloc>0&&s.heapFree>0&&s.wait>0&&s.lock>0&&s.io>0;
}
void begin()noexcept{localNew[0]=localNew[1]=localDelete[0]=localDelete[1]=0;for(auto& v:localOps.values)v=0;localOps.enabled=true;if(operationTls!=TLS_OUT_OF_INDEXES)TlsSetValue(operationTls,&localOps);phase=1;}
void audioPhase()noexcept{phase=2;}
void end()noexcept{if(!phase)return;phase=0;localOps.enabled=false;for(unsigned i=0;i<2;++i){news[i].fetch_add(localNew[i],std::memory_order_relaxed);deletes[i].fetch_add(localDelete[i],std::memory_order_relaxed);allocatingBlocks[i].fetch_add(localNew[i]||localDelete[i],std::memory_order_relaxed);}for(unsigned i=0;i<5;++i)ops[i].fetch_add(localOps.values[i],std::memory_order_relaxed);blocks.fetch_add(1,std::memory_order_release);}
void report()noexcept{std::fprintf(stderr,"A02_HOST_CPP_AUDIT blocks=%llu control_new=%llu control_delete=%llu control_nonzero_blocks=%llu audio_new=%llu audio_delete=%llu audio_nonzero_blocks=%llu coverage=host_cpp_operators\n",(unsigned long long)blocks.load(std::memory_order_acquire),(unsigned long long)news[0].load(),(unsigned long long)deletes[0].load(),(unsigned long long)allocatingBlocks[0].load(),(unsigned long long)news[1].load(),(unsigned long long)deletes[1].load(),(unsigned long long)allocatingBlocks[1].load());const auto s=snapshot();std::fprintf(stderr,"A02_WINDOWS_IMPORT_AUDIT hooks=%u heap_alloc_calls=%llu heap_free_calls=%llu wait_calls=%llu lock_calls=%llu io_calls=%llu coverage=loaded_named_imports_excludes_dynamic_resolution_syscalls_and_new_images\n",patchedImports.load(),(unsigned long long)s.heapAlloc,(unsigned long long)s.heapFree,(unsigned long long)s.wait,(unsigned long long)s.lock,(unsigned long long)s.io);std::fflush(stderr);}
}
