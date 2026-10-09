// Synthetic endpoint used only by selection tests. No driver, device or live transport access.
#define ENTROPY_MODULE_BUILD
#include <entropy/module_abi.h>
#include <entropy/endpoint_abi.h>
#include <entropy/endpoint_catalog_abi.h>
#include <cstring>
#include <windows.h>
struct EntropyModuleOpaque {uint64_t revision=0;};
static EntropyResult descriptor(EntropyModuleDescriptor* d){*d={};d->struct_size=sizeof(*d);d->abi_version=ENTROPY_MODULE_ABI_VERSION;d->module_kind=ENTROPY_MODULE_ENDPOINT;d->module_flags=ENTROPY_MODULE_FLAG_FP64;std::strcpy(d->identifier,"audio.entropy.endpoint.music-transport");return ENTROPY_OK;}
static EntropyResult create(const EntropyHostServices*,EntropyModuleHandle* h){*h=new EntropyModuleOpaque;return ENTROPY_OK;}
static void destroy(EntropyModuleHandle h){delete h;}
static EntropyResult prepare(EntropyModuleHandle,const EntropyPrepareInfo*){return ENTROPY_OK;}
static EntropyResult ok(EntropyModuleHandle){return ENTROPY_OK;}
static void noop(EntropyModuleHandle){}
static EntropyResult bind(EntropyModuleHandle h,const EntropyEndpointBindRequestV1* b){if(!b||std::strcmp(b->endpoint_id_utf8,"entropy-music-transport:main")||b->left_channel!=0||b->right_channel!=1)return ENTROPY_ERROR_INVALID_ARGUMENT;h->revision=b->revision;return ENTROPY_OK;}
static EntropyResult status(EntropyModuleHandle h,EntropyEndpointStatus* s){*s={};s->struct_size=sizeof(*s);s->state=ENTROPY_ENDPOINT_ACTIVE;s->source_rate=192000;s->channels=2;s->container_bits=s->valid_bits=64;s->stream_generation=h->revision;return ENTROPY_OK;}
static EntropyResult read(EntropyModuleHandle h,EntropyEndpointBlockF64* b){Sleep(1);status(h,&b->status);b->frame_count=b->capacity_frames<256?b->capacity_frames:256;for(unsigned i=0;i<b->frame_count;++i){b->left[i]=0.125;b->right[i]=-0.25;}return ENTROPY_OK;}
static EntropyResult query(const char* id,uint32_t,void* out,uint32_t bytes){
 if(!std::strcmp(id,ENTROPY_ENDPOINT_EXTENSION_ID)&&bytes>=sizeof(EntropyEndpointApiV1)){EntropyEndpointApiV1 a{};a.struct_size=sizeof(a);a.extension_version=ENTROPY_ENDPOINT_EXTENSION_VERSION;a.minimum_buffer_frames=256;a.connect=ok;a.disconnect=noop;a.read_source_f64=read;a.get_status=status;std::memcpy(out,&a,sizeof(a));return ENTROPY_OK;}
 if(!std::strcmp(id,ENTROPY_ENDPOINT_CATALOG_ID)&&bytes>=sizeof(EntropyEndpointCatalogApiV1)){EntropyEndpointCatalogApiV1 a{};a.struct_size=sizeof(a);a.extension_version=ENTROPY_ENDPOINT_CATALOG_VERSION;a.bind_stopped=bind;a.cancel_read=noop;std::memcpy(out,&a,sizeof(a));return ENTROPY_OK;}
 return ENTROPY_ERROR_UNSUPPORTED;
}
extern "C" ENTROPY_ABI_EXPORT EntropyResult entropy_module_get_api(uint32_t version,EntropyModuleApiV1* a){if(version!=ENTROPY_MODULE_ABI_VERSION)return ENTROPY_ERROR_ABI_MISMATCH;*a={};a->struct_size=sizeof(*a);a->abi_version=version;a->get_descriptor=descriptor;a->create=create;a->destroy=destroy;a->prepare=prepare;a->reset=ok;a->query_extension=query;return ENTROPY_OK;}
