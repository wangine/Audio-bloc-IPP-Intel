#define ENTROPY_MODULE_BUILD 1
#include <entropy/endpoint_abi.h>
#include <cstring>
namespace {
struct State { unsigned reads=0; bool connected=false; };
EntropyResult ENTROPY_ABI_CALL create(const EntropyHostServices*,EntropyModuleHandle* out){*out=reinterpret_cast<EntropyModuleHandle>(new State);return ENTROPY_OK;}
void ENTROPY_ABI_CALL destroy(EntropyModuleHandle h){delete reinterpret_cast<State*>(h);}
EntropyResult ENTROPY_ABI_CALL prepare(EntropyModuleHandle,const EntropyPrepareInfo* p){return p->output_channels==2&&p->maximum_frames>=2048?ENTROPY_OK:ENTROPY_ERROR_INVALID_ARGUMENT;}
EntropyResult ENTROPY_ABI_CALL connect(EntropyModuleHandle h){reinterpret_cast<State*>(h)->connected=true;return ENTROPY_OK;}
void ENTROPY_ABI_CALL disconnect(EntropyModuleHandle h){reinterpret_cast<State*>(h)->connected=false;}
EntropyResult ENTROPY_ABI_CALL read(EntropyModuleHandle h,EntropyEndpointBlockF64* b){
 auto& s=*reinterpret_cast<State*>(h);if(!s.connected||b->capacity_frames<2048)return ENTROPY_ERROR_INVALID_STATE;
 b->frame_count=s.reads==1?0:2048;b->status={};b->status.struct_size=sizeof(b->status);
 b->status.state=s.reads==1?ENTROPY_ENDPOINT_IDLE:ENTROPY_ENDPOINT_ACTIVE;
 b->status.source_rate=192000;b->status.channels=2;b->status.container_bits=32;b->status.valid_bits=24;b->status.stream_generation=17;
 b->status.discontinuity_flags=s.reads==0?ENTROPY_ENDPOINT_FLAG_SHARED_MIX_RATE:s.reads==2?ENTROPY_ENDPOINT_FLAG_EXCLUSIVE_NATIVE_RATE:ENTROPY_ENDPOINT_FLAG_MODE_UNKNOWN;
 for(unsigned i=0;i<b->frame_count;i++){b->left[i]=double(i-1024.0)/8192;b->right[i]=-double(i+1)/16384;}
 ++s.reads;return ENTROPY_OK;
}
EntropyResult ENTROPY_ABI_CALL status(EntropyModuleHandle,EntropyEndpointStatus* out){out->state=ENTROPY_ENDPOINT_IDLE;return ENTROPY_OK;}
EntropyResult ENTROPY_ABI_CALL extension(const char* id,uint32_t version,void* out,uint32_t size){
 if(std::strcmp(id,ENTROPY_ENDPOINT_EXTENSION_ID))return ENTROPY_ERROR_UNSUPPORTED; // V1 only, deliberately no catalog.
 if(version!=ENTROPY_ENDPOINT_EXTENSION_VERSION||size<sizeof(EntropyEndpointApiV1))return ENTROPY_ERROR_ABI_MISMATCH;
 auto& a=*static_cast<EntropyEndpointApiV1*>(out);a={};a.struct_size=sizeof(a);a.extension_version=version;a.minimum_buffer_frames=2048;
 a.connect=connect;a.disconnect=disconnect;a.read_source_f64=read;a.get_status=status;return ENTROPY_OK;
}
}
extern "C" ENTROPY_ABI_EXPORT EntropyResult ENTROPY_ABI_CALL entropy_module_get_api(uint32_t version,EntropyModuleApiV1* out){
 if(version!=ENTROPY_MODULE_ABI_VERSION)return ENTROPY_ERROR_ABI_MISMATCH;
 *out={};out->struct_size=sizeof(*out);out->abi_version=version;out->create=create;out->destroy=destroy;out->prepare=prepare;out->query_extension=extension;return ENTROPY_OK;
}
