#pragma once
#include "Fixtures.h"
#include <entropy/crossover_abi.h>
#include <windows.h>
namespace stage02 {
class Golden {
    HMODULE dll=nullptr;EntropyModuleApiV1 api{};EntropyCrossoverApiV1 crossover{};EntropyModuleHandle instance=nullptr;
public:
    explicit Golden(const char* path){
        dll=LoadLibraryExA(path,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);if(!dll)throw std::runtime_error("V07 LoadLibrary error "+std::to_string(GetLastError()));
        auto fn=reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(dll,"entropy_module_get_api"));api.struct_size=sizeof(api);
        if(!fn||fn(ENTROPY_MODULE_ABI_VERSION,&api)!=ENTROPY_OK)throw std::runtime_error("V07 ABI");
        crossover.struct_size=sizeof(crossover);if(api.query_extension(ENTROPY_CROSSOVER_EXTENSION_ID,ENTROPY_CROSSOVER_EXTENSION_VERSION,&crossover,sizeof(crossover))!=ENTROPY_OK)throw std::runtime_error("V07 extension");
        EntropyHostServices host{};host.struct_size=sizeof(host);host.abi_version=ENTROPY_MODULE_ABI_VERSION;if(api.create(&host,&instance)!=ENTROPY_OK)throw std::runtime_error("V07 create");
    }
    ~Golden(){if(instance)api.destroy(instance);if(dll)FreeLibrary(dll);}
    bool set(const char* id,double v){return crossover.set_parameter(instance,id,v)==ENTROPY_OK;}
    void prepare(){EntropyPrepareInfo info{};info.struct_size=sizeof(info);info.sample_rate=192000;info.maximum_frames=1024;info.input_channels=13;info.output_channels=16;if(api.prepare(instance,&info)!=ENTROPY_OK)throw std::runtime_error("V07 prepare");}
    int process(const Buffers& in,Buffers& out,unsigned n,uint64_t at){auto b=block(in,out,n);b.input_channels=13;EntropyProcessContext ctx{};ctx.struct_size=sizeof(ctx);ctx.stream_time_samples=at;return api.process_f64(instance,&b,&ctx);}
    unsigned latency(){unsigned n=0;if(api.latency_samples(instance,&n)!=ENTROPY_OK)throw std::runtime_error("V07 latency");return n;}
    double get(const char* id){double v=NAN;if(crossover.get_parameter(instance,id,&v)!=ENTROPY_OK)throw std::runtime_error("V07 get parameter");return v;}
};
}
