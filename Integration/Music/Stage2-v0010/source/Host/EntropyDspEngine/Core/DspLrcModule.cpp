#include "DspLrcModule.h"
#include "DspNativeRegistry.h"
#include <cstring>
namespace entropy::stage {
void DspLrcModule::close() noexcept {if(handle_&&api_.destroy)api_.destroy(handle_);handle_=nullptr;if(library_)FreeLibrary(library_);library_=nullptr;api_={};extension_={};}
bool DspLrcModule::prepare(const std::filesystem::path&root,std::string& error){
 close();library_=loadNativeModule(NativeModuleId::LrcAligner,root,error);if(!library_)return false;
 auto get=reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library_,ENTROPY_MODULE_GET_API_SYMBOL));api_.struct_size=sizeof(api_);EntropyModuleDescriptor d{};d.struct_size=sizeof(d);
 if(!get||get(ENTROPY_MODULE_ABI_VERSION,&api_)!=ENTROPY_OK||!api_.get_descriptor||!api_.create||!api_.destroy||!api_.prepare||!api_.reset||!api_.process_f64||!api_.latency_samples||!api_.query_extension||api_.get_descriptor(&d)!=ENTROPY_OK||std::strcmp(d.identifier,nativeModule(NativeModuleId::LrcAligner)->identifier)||d.max_input_channels!=3||d.max_output_channels!=3||!(d.module_flags&ENTROPY_MODULE_FLAG_FP64)){error="LRC ABI/FP64/3IN-3OUT rejected";close();return false;}
 extension_.struct_size=sizeof(extension_);if(api_.query_extension(ENTROPY_LRC_EXTENSION_ID,ENTROPY_LRC_EXTENSION_VERSION,&extension_,sizeof(extension_))!=ENTROPY_OK||!extension_.set_state||!extension_.get_state||!extension_.get_telemetry){error="LRC extension rejected";close();return false;}
 EntropyHostServices services{};services.struct_size=sizeof(services);services.abi_version=ENTROPY_MODULE_ABI_VERSION;EntropyPrepareInfo p{};p.struct_size=sizeof(p);p.sample_rate=192000;p.maximum_frames=1024;p.input_channels=p.output_channels=3;
 if(api_.create(&services,&handle_)!=ENTROPY_OK||!handle_||api_.prepare(handle_,&p)!=ENTROPY_OK||!configure(false,{},0)){error="LRC preparation failed";close();return false;}return true;
}
bool DspLrcModule::configure(bool align,const std::array<uint64_t,3>&delays,uint64_t target) noexcept {
 if(!handle_||target>UINT32_MAX)return false;for(auto v:delays)if(v>ENTROPY_LRC_MAX_DELAY)return false;
 EntropyLrcAlignerStateV1 s{};s.struct_size=sizeof(s);s.mode=align?ENTROPY_LRC_ALIGN_STATIC:ENTROPY_LRC_BYPASS_EXACT;s.delay_l_samples=static_cast<uint32_t>(delays[0]);s.delay_r_samples=static_cast<uint32_t>(delays[1]);s.delay_c_samples=static_cast<uint32_t>(delays[2]);s.target_latency_samples=static_cast<uint32_t>(target);
 if(extension_.set_state(handle_,&s)!=ENTROPY_OK||api_.reset(handle_)!=ENTROPY_OK)return false;EntropyLrcAlignerStateV1 actual{};actual.struct_size=sizeof(actual);return extension_.get_state(handle_,&actual)==ENTROPY_OK&&!std::memcmp(&s,&actual,sizeof(s));
}
bool DspLrcModule::reset() noexcept {return handle_&&api_.reset(handle_)==ENTROPY_OK;}
bool DspLrcModule::process(const double*const*in,double*const*out) noexcept {if(!handle_)return false;EntropyAudioBlockF64 b{};b.struct_size=sizeof(b);b.frames=1024;b.input_channels=b.output_channels=3;b.inputs=in;b.outputs=out;EntropyProcessContext context{};context.struct_size=sizeof(context);return api_.process_f64(handle_,&b,&context)==ENTROPY_OK;}
}
