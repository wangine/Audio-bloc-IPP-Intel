#include "endpoint_provider_v2.h"
#include "endpoint_stream_state_v1.h"
#include "PcmDecode.h"
#include <windows.h>
#include <array>
#include <iostream>
#include <stdexcept>
#include <cstring>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()try{
 auto dll=LoadLibraryExW(AUX_FIXTURE_PROVIDER,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
 require(dll!=nullptr,"fixture Provider load");
 auto entry=reinterpret_cast<int32_t(*)(uint32_t,EntropyProviderApiV2*)>(GetProcAddress(dll,"entropy_endpoint_provider_v2"));
 require(entry!=nullptr,"Provider entry");EntropyProviderApiV2 api{};api.structSize=sizeof(api);
 require(entry(ENTROPY_PROVIDER_ABI_V2,&api)==1,"Provider ABI");
 EntropyEndpointInfoV2 e{};require(api.enumerate(0,&e)==1,"V1 AUX bridge hidden because it has no catalog extension");
 require(e.roles==2&&!std::strcmp(e.endpointId,"entropy-aux:system-default"),"fixed AUX identity/role");
 require(api.enumerate(1,&e)==0,"AUX must not enumerate selectable capture devices");
 void* h=nullptr;require(api.open("wrong-endpoint",4,&h)<0,"unexpected AUX identity accepted");
 require(api.open("entropy-aux:system-default",4,&h)==1&&h,"V1 AUX open without catalog bind");
 // The V1 right plane starts at capacity, while V2 starts it at frame_count.
 // Deliberately read 2048 frames into a 4096-frame-capacity payload.
 std::array<std::byte,65536> payload{};std::array<double,2048> l{},r{};EntropyIngressPacketV2 p{};
 const auto payloadBytes=static_cast<uint32_t>(payload.size());
 require(api.read(h,&p,payload.data(),payloadBytes)==1,"AUX FP64 read");
 require(p.format.representation==ENTROPY_REP_F64_PLANAR&&p.mode==ENTROPY_MODE_SHARED,"FP64 payload/source Shared metadata");
 require(!(p.flags&ENTROPY_PACKET_RAW_IDENTITY)&&(p.flags&ENTROPY_PACKET_METADATA_PARTIAL),"V1 raw identity invented");
 require(bool(entropy::music::decodeStereo({p,{payload.data(),p.payloadBytes}},l.data(),r.data(),2048)),"decode AUX FP64");
 for(unsigned i=0;i<2048;i++)require(l[i]==double(i-1024.0)/8192&&r[i]==-double(i+1)/16384,"AUX stereo changed or packed with wrong stride");
 require(api.read(h,&p,payload.data(),payloadBytes)==0&&p.reserved[0]==ENTROPY_STREAM_IDLE_V1,"AUX idle fabricated PCM or RUN");
 require(api.read(h,&p,payload.data(),payloadBytes)==1&&p.mode==ENTROPY_MODE_EXCLUSIVE,"real source Exclusive metadata lost");
 require(api.read(h,&p,payload.data(),payloadBytes)==1&&p.mode==ENTROPY_MODE_UNKNOWN,"unknown source access guessed");
 api.cancel(h);require(api.read(h,&p,payload.data(),payloadBytes)<0,"AUX cancellation ignored");api.close(h);
 FreeLibrary(dll);std::cout<<"PASS original V1 AUX bridge, exact stereo FP64, idle, source access and cancellation\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
