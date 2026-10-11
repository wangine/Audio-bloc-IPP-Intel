#pragma once
#include "TestSupport.h"
#include "StagePublishV2.h"
#include <future>
inline StageBusBlockHeader musicHeader(const StageBusSessionHeader& s,uint64_t sequence){
 auto b=header(request(s,sequence));b.validity_mask=3;b.source_generation[2]=0;return b;
}
template<class F> bool musicConnect(entropy::publish::Publisher& publisher,F&& f){
 auto pending=std::async(std::launch::async,std::forward<F>(f));auto end=qpc()+frequency()*4;
 while(pending.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready && qpc()<end){publisher.service();Sleep(1);}
 return pending.get();
}
inline std::wstring musicEndpoint(const wchar_t* suffix){return L"\\\\.\\pipe\\Entropy.Music.Test."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(qpc())+suffix;}
