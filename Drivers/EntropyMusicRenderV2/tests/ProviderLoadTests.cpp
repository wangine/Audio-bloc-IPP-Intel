#include "TestSupport.h"
#include "../provider/BridgeClient.h"
#include <windows.h>
int main(int argc,char** argv){
    try{CHECK(argc==2);const auto module=LoadLibraryA(argv[1]);CHECK(module);
        auto entry=reinterpret_cast<EntropyProviderEntryV2>(reinterpret_cast<void*>(GetProcAddress(module,"entropy_endpoint_provider_v2")));CHECK(entry);
        EntropyProviderApiV2 api{};api.structSize=sizeof(api);CHECK(entry(0,&api)==-1);CHECK(entry(ENTROPY_PROVIDER_ABI_V2,&api)==1);
        CHECK(api.abiVersion==ENTROPY_PROVIDER_ABI_V2 && api.structSize==336 && api.info.structSize==256);
        CHECK(api.info.capabilities==ENTROPY_CAP_RAW_PCM && api.info.roles==ENTROPY_ROLE_MUSIC);
        CHECK(api.enumerate && api.open && api.read && api.cancel && api.close);
        CHECK(!std::strcmp(api.info.providerId,entropy::music::bridge::ProviderId));for(auto v:api.reserved)CHECK(!v);for(auto v:api.info.reserved)CHECK(!v);
        void* handle=nullptr;CHECK(api.open("foreign",0,&handle)==-1 && !handle);CHECK(api.read(nullptr,nullptr,nullptr,0)==-1);api.cancel(nullptr);api.close(nullptr);
        FreeLibrary(module);std::puts("PASS isolated DLL ABI load, no audio endpoint opened");return 0;
    }catch(const std::exception& e){std::printf("FAIL %s\n",e.what());return 1;}
}
