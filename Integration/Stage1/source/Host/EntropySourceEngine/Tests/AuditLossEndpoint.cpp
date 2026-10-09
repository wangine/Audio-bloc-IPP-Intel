#define ENTROPY_MODULE_BUILD
#include <entropy/module_abi.h>
#include <entropy/endpoint_abi.h>
#include <entropy/endpoint_catalog_abi.h>
#include <entropy/cinema_endpoint_abi.h>
#include <windows.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdlib>
static unsigned env(const char*key,unsigned fallback){char b[64]{};return GetEnvironmentVariableA(key,b,64)?std::strtoul(b,nullptr,10):fallback;}
static uint64_t tick(){LARGE_INTEGER t;QueryPerformanceCounter(&t);return t.QuadPart;}
struct EntropyModuleOpaque {unsigned rate=48000,pattern=0,target=0,absent=0;uint64_t emitted=0,total=0,start=0,hz=0,revision=1;};
static double noise(uint64_t n,unsigned seed){uint32_t x=uint32_t(n)^seed;x^=x>>16;x*=0x7feb352d;x^=x>>15;x*=0x846ca68b;x^=x>>16;return (int(x&65535)-32768)/262144.0;}
static double sample(uint64_t n,unsigned c,unsigned p){double l=noise(n,1234567),r=noise(n,9876543);if(p==1)return c==0&&n==512?.25:0;if(p==2)return c==1&&n==512?.25:0;if(p==3)return l;if(p==4)return c?-l:l;if(p==5)return .04*std::sin(n*(c?17:11)*.031)+.02*std::sin(n*.91)+(c?r:l)*.02;return c?r:l;}
static EntropyResult descriptor(EntropyModuleDescriptor*d){*d={};d->struct_size=sizeof(*d);d->abi_version=ENTROPY_MODULE_ABI_VERSION;d->module_kind=ENTROPY_MODULE_ENDPOINT;d->module_flags=ENTROPY_MODULE_FLAG_FP64;std::strcpy(d->identifier,AUDIT_ROLE==0?"audio.entropy.endpoint.music-transport":AUDIT_ROLE==1?"audio.entropy.endpoint.aux":"audio.entropy.endpoint.cinema");return ENTROPY_OK;}
static EntropyResult create(const EntropyHostServices*,EntropyModuleHandle*h){*h=new EntropyModuleOpaque;auto&s=**h;s.rate=env("AUDIT_RATE",192000);s.pattern=env("AUDIT_PATTERN",0);s.target=env("AUDIT_TARGET",0);s.absent=AUDIT_ROLE==2&&env("AUDIT_CINEMA_ABSENT",0);s.total=s.rate/2/256*256;LARGE_INTEGER f;QueryPerformanceFrequency(&f);s.hz=f.QuadPart;return ENTROPY_OK;}
static void destroy(EntropyModuleHandle h){delete h;}
static EntropyResult prepare(EntropyModuleHandle,const EntropyPrepareInfo*){return ENTROPY_OK;}
static EntropyResult connect(EntropyModuleHandle h){h->emitted=0;h->start=tick();return ENTROPY_OK;}
static EntropyResult ok(EntropyModuleHandle){return ENTROPY_OK;}
static void noop(EntropyModuleHandle){}
static EntropyResult bind(EntropyModuleHandle h,const EntropyEndpointBindRequestV1*b){h->revision=b->revision;return ENTROPY_OK;}
static EntropyResult status(EntropyModuleHandle h,EntropyEndpointStatus*s){*s={};s->struct_size=sizeof(*s);s->state=ENTROPY_ENDPOINT_ACTIVE;s->source_rate=h->rate;s->channels=2;s->valid_bits=s->container_bits=64;s->stream_generation=h->revision;return ENTROPY_OK;}
static unsigned available(EntropyModuleHandle h,unsigned cap){auto now=(tick()-h->start)*h->rate/h->hz;auto limit=(std::min)(h->total,now);auto n=(std::min)(uint64_t(cap),limit>h->emitted?limit-h->emitted:0);return unsigned(n/256*256);}
static EntropyResult read(EntropyModuleHandle h,EntropyEndpointBlockF64*b){status(h,&b->status);unsigned n=available(h,b->capacity_frames);b->frame_count=n;for(unsigned i=0;i<n;++i){b->left[i]=double(h->emitted+i+1)/1048576;b->right[i]=-double(h->emitted+i+1)/1048576;}h->emitted+=n;return ENTROPY_OK;}
static EntropyResult cs(EntropyModuleHandle h,EntropyCinemaEndpointStatus*s){*s={};s->struct_size=sizeof(*s);s->state=ENTROPY_ENDPOINT_ACTIVE;s->source_rate=h->absent?0:h->rate;s->channels=8;s->sample_bits=64;s->stream_generation=h->revision;return ENTROPY_OK;}
static EntropyResult cr(EntropyModuleHandle h,EntropyCinemaEndpointBlockF64*b){cs(h,&b->status);b->frame_count=h->absent?0:available(h,b->capacity_frames);for(unsigned c=0;c<8;++c)for(unsigned i=0;i<b->frame_count;++i)b->outputs[c][i]=double(h->emitted+i+1)/1048576*(c+1);h->emitted+=b->frame_count;return ENTROPY_OK;}
static EntropyResult query(const char*id,uint32_t,void*out,uint32_t bytes){if(!std::strcmp(id,ENTROPY_ENDPOINT_EXTENSION_ID)&&bytes>=sizeof(EntropyEndpointApiV1)){EntropyEndpointApiV1 a{};a.struct_size=sizeof(a);a.extension_version=ENTROPY_ENDPOINT_EXTENSION_VERSION;a.minimum_buffer_frames=256;a.connect=connect;a.disconnect=noop;a.get_status=status;a.read_source_f64=read;std::memcpy(out,&a,sizeof(a));return ENTROPY_OK;}if(!std::strcmp(id,ENTROPY_CINEMA_ENDPOINT_EXTENSION_ID)&&bytes>=sizeof(EntropyCinemaEndpointApiV1)){EntropyCinemaEndpointApiV1 a{};a.struct_size=sizeof(a);a.extension_version=ENTROPY_CINEMA_ENDPOINT_EXTENSION_VERSION;a.maximum_channels=8;a.minimum_buffer_frames=256;a.connect=connect;a.disconnect=noop;a.get_status=cs;a.read_source_f64=cr;std::memcpy(out,&a,sizeof(a));return ENTROPY_OK;}if(!std::strcmp(id,ENTROPY_ENDPOINT_CATALOG_ID)&&bytes>=sizeof(EntropyEndpointCatalogApiV1)){EntropyEndpointCatalogApiV1 a{};a.struct_size=sizeof(a);a.extension_version=ENTROPY_ENDPOINT_CATALOG_VERSION;a.bind_stopped=bind;a.cancel_read=noop;std::memcpy(out,&a,sizeof(a));return ENTROPY_OK;}return ENTROPY_ERROR_UNSUPPORTED;}
extern "C" ENTROPY_ABI_EXPORT EntropyResult entropy_module_get_api(uint32_t v,EntropyModuleApiV1*a){if(v!=ENTROPY_MODULE_ABI_VERSION)return ENTROPY_ERROR_ABI_MISMATCH;*a={};a->struct_size=sizeof(*a);a->abi_version=v;a->get_descriptor=descriptor;a->create=create;a->destroy=destroy;a->prepare=prepare;a->reset=ok;a->query_extension=query;return ENTROPY_OK;}
