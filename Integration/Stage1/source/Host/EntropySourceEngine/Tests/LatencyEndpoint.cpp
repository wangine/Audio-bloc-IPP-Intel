// Test-only clock-paced, lossless source. Never packaged as a native module.
#define ENTROPY_MODULE_BUILD
#include <entropy/module_abi.h>
#include <entropy/endpoint_abi.h>
#include <entropy/endpoint_catalog_abi.h>
#include <entropy/cinema_endpoint_abi.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
static uint64_t tick(){LARGE_INTEGER v;QueryPerformanceCounter(&v);return v.QuadPart;}
struct EntropyModuleOpaque {uint32_t rate=192000,role=0;uint64_t emitted=0,frequency=0;std::atomic<uint64_t> begin{0};};
static EntropyResult descriptor(EntropyModuleDescriptor* d){*d={};d->struct_size=sizeof(*d);d->abi_version=ENTROPY_MODULE_ABI_VERSION;d->module_kind=ENTROPY_MODULE_ENDPOINT;d->module_flags=ENTROPY_MODULE_FLAG_FP64;std::strcpy(d->identifier,"test.entropy.latency");return ENTROPY_OK;}
static EntropyResult create(const EntropyHostServices*,EntropyModuleHandle* h){*h=new EntropyModuleOpaque;char r[32]{};if(GetEnvironmentVariableA("ENTROPY_LATENCY_RATE",r,32))(*h)->rate=std::strtoul(r,nullptr,10);LARGE_INTEGER f;QueryPerformanceFrequency(&f);(*h)->frequency=f.QuadPart;return ENTROPY_OK;}
static void destroy(EntropyModuleHandle h){delete h;}
static EntropyResult prepare(EntropyModuleHandle,const EntropyPrepareInfo*){return ENTROPY_OK;}
static EntropyResult connect(EntropyModuleHandle h){h->emitted=0;h->begin=tick();return ENTROPY_OK;}
static EntropyResult ok(EntropyModuleHandle){return ENTROPY_OK;}
static void noop(EntropyModuleHandle){}
static EntropyResult status(EntropyModuleHandle h,EntropyEndpointStatus* s){*s={};s->struct_size=sizeof(*s);s->state=ENTROPY_ENDPOINT_ACTIVE;s->source_rate=h->rate;s->channels=2;s->container_bits=s->valid_bits=64;s->stream_generation=1;return ENTROPY_OK;}
static EntropyResult read(EntropyModuleHandle h,EntropyEndpointBlockF64* b){
 status(h,&b->status);const auto available=(tick()-h->begin.load())*h->rate/h->frequency;
 // Complete 256-frame source packets become available according to the source clock.
 auto count=(std::min)(uint64_t(b->capacity_frames),available-h->emitted);count=count/256*256;b->frame_count=uint32_t(count);
 for(uint64_t i=0;i<count;++i){auto n=h->emitted+i;double x=n==h->rate/4?0.5:(int(n%127)-63)/131072.0;b->left[i]=h->role?x/2:x;b->right[i]=h->role?x/4:-x;}
 h->emitted+=count;return ENTROPY_OK;
}
static EntropyResult cinemaStatus(EntropyModuleHandle h,EntropyCinemaEndpointStatus* s){*s={};s->struct_size=sizeof(*s);s->state=ENTROPY_ENDPOINT_ACTIVE;s->source_rate=h->rate;s->channels=8;s->sample_bits=64;s->stream_generation=1;return ENTROPY_OK;}
static EntropyResult cinemaRead(EntropyModuleHandle h,EntropyCinemaEndpointBlockF64* b){cinemaStatus(h,&b->status);const auto available=(tick()-h->begin.load())*h->rate/h->frequency;auto count=(std::min)(uint64_t(b->capacity_frames),available-h->emitted);count=count/256*256;b->frame_count=uint32_t(count);for(unsigned c=0;c<8;++c)for(uint64_t i=0;i<count;++i){auto n=h->emitted+i;double x=n==h->rate/4?0.5:(int(n%127)-63)/131072.0;b->outputs[c][i]=x*(c+1)/16;}h->emitted+=count;return ENTROPY_OK;}
static EntropyResult query(const char* id,uint32_t,void* out,uint32_t bytes){
 if(!std::strcmp(id,ENTROPY_ENDPOINT_EXTENSION_ID)&&bytes>=sizeof(EntropyEndpointApiV1)){EntropyEndpointApiV1 a{};a.struct_size=sizeof(a);a.extension_version=ENTROPY_ENDPOINT_EXTENSION_VERSION;a.minimum_buffer_frames=256;a.connect=connect;a.disconnect=noop;a.read_source_f64=read;a.get_status=status;std::memcpy(out,&a,sizeof(a));return ENTROPY_OK;}
 if(!std::strcmp(id,ENTROPY_CINEMA_ENDPOINT_EXTENSION_ID)&&bytes>=sizeof(EntropyCinemaEndpointApiV1)){EntropyCinemaEndpointApiV1 a{};a.struct_size=sizeof(a);a.extension_version=ENTROPY_CINEMA_ENDPOINT_EXTENSION_VERSION;a.minimum_buffer_frames=256;a.maximum_channels=8;a.connect=connect;a.disconnect=noop;a.read_source_f64=cinemaRead;a.get_status=cinemaStatus;std::memcpy(out,&a,sizeof(a));return ENTROPY_OK;}
 if(!std::strcmp(id,ENTROPY_ENDPOINT_CATALOG_ID)&&bytes>=sizeof(EntropyEndpointCatalogApiV1)){EntropyEndpointCatalogApiV1 a{};a.struct_size=sizeof(a);a.extension_version=ENTROPY_ENDPOINT_CATALOG_VERSION;a.cancel_read=noop;std::memcpy(out,&a,sizeof(a));return ENTROPY_OK;}return ENTROPY_ERROR_UNSUPPORTED;
}
extern "C" ENTROPY_ABI_EXPORT void entropy_latency_role(EntropyModuleHandle h,unsigned role){h->role=role;}
extern "C" ENTROPY_ABI_EXPORT uint64_t entropy_latency_begin(EntropyModuleHandle h){return h->begin.load();}
extern "C" ENTROPY_ABI_EXPORT EntropyResult entropy_module_get_api(uint32_t v,EntropyModuleApiV1* a){if(v!=ENTROPY_MODULE_ABI_VERSION)return ENTROPY_ERROR_ABI_MISMATCH;*a={};a->struct_size=sizeof(*a);a->abi_version=v;a->get_descriptor=descriptor;a->create=create;a->destroy=destroy;a->prepare=prepare;a->reset=ok;a->query_extension=query;return ENTROPY_OK;}
