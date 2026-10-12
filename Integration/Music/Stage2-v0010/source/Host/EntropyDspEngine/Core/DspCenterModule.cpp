#include "DspCenterModule.h"
#include "DspNativeRegistry.h"
#include <cmath>
#include <cstring>
namespace entropy::stage {
bool validCenterSettings(const CenterSettings& s) noexcept {
 for(unsigned i=0;i<16;i++){
  auto v=s.values[i];if(!std::isfinite(v)||v<centerMin[i]||v>centerMax[i])return false;
  if((i==0||i==1||i==12||i==15)&&v!=std::floor(v))return false;
 }return true;
}
void DspCenterModule::close() noexcept {
 if(handle_ && api_.destroy)api_.destroy(handle_);handle_=nullptr;
 if(library_)FreeLibrary(library_);library_=nullptr;api_={};center_={};telemetry_={};snapshot_={};configured_=false;
}
bool DspCenterModule::prepare(const std::filesystem::path& root,std::string& error) {
 close();library_=loadNativeModule(NativeModuleId::CenterV2,root,error);if(!library_)return false;
 auto get=reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library_,ENTROPY_MODULE_GET_API_SYMBOL));api_.struct_size=sizeof(api_);
 EntropyModuleDescriptor descriptor{};descriptor.struct_size=sizeof(descriptor);
 if(!get || get(ENTROPY_MODULE_ABI_VERSION,&api_)!=ENTROPY_OK || !api_.get_descriptor || !api_.create || !api_.destroy || !api_.prepare || !api_.process_f64 || !api_.reset || !api_.latency_samples || !api_.query_extension || api_.get_descriptor(&descriptor)!=ENTROPY_OK || descriptor.max_input_channels!=2 || descriptor.max_output_channels!=1 || std::strcmp(descriptor.identifier,"audio.entropy.center-extractor.fp64-v2") || !(descriptor.module_flags&ENTROPY_MODULE_FLAG_FP64)){
  error="Center V2 ABI/2IN-1OUT/FP64 contract rejected";close();return false;
 }
 center_.struct_size=sizeof(center_);telemetry_.struct_size=sizeof(telemetry_);
 if(api_.query_extension(ENTROPY_CENTER_V2_EXTENSION_ID,ENTROPY_CENTER_V2_EXTENSION_VERSION,&center_,sizeof(center_))!=ENTROPY_OK || api_.query_extension(ENTROPY_CENTER_TELEMETRY_EXTENSION_ID,ENTROPY_CENTER_TELEMETRY_EXTENSION_VERSION,&telemetry_,sizeof(telemetry_))!=ENTROPY_OK || !center_.set_parameter || !center_.get_parameter || !telemetry_.get_telemetry){error="Center V2 extensions rejected";close();return false;}
 EntropyHostServices services{};services.struct_size=sizeof(services);services.abi_version=ENTROPY_MODULE_ABI_VERSION;
 if(api_.create(&services,&handle_)!=ENTROPY_OK || !handle_){error="Center V2 creation failed";close();return false;}
 EntropyPrepareInfo info{};info.struct_size=sizeof(info);info.sample_rate=192000;info.maximum_frames=1024;info.input_channels=2;info.output_channels=1;
 if(api_.prepare(handle_,&info)!=ENTROPY_OK || center_.set_parameter(handle_,"limiterEnable",0)!=ENTROPY_OK || !apply(CenterSettings{},1)) {error="Center V2 preparation/limiter disable failed";close();return false;}
 snapshot_.loaded=1;return true;
}
bool DspCenterModule::set(unsigned index,double value) noexcept {
 if(index>=16 || !handle_)return false;
 auto next=snapshot_.settings;next.values[index]=value;if(!validCenterSettings(next))return false;
 if(center_.set_parameter(handle_,centerIds[index],value)!=ENTROPY_OK)return false;
 if(center_.get_parameter(handle_,centerIds[index],&snapshot_.settings.values[index])!=ENTROPY_OK)return false;
 if(index==0||index==1||index==13)snapshot_.latencyValid=0;
 snapshot_.revision++;return true;
}
bool DspCenterModule::apply(const CenterSettings& s,uint64_t revision) noexcept {
 if(!handle_ || !validCenterSettings(s))return false;
 // The accepted V2 stores controls as float values. Skip unchanged native controls;
 // detector/gain edits do not invalidate audio telemetry or the latency contract.
 for(unsigned i=0;i<16;i++){
  const double nativeValue=static_cast<double>(static_cast<float>(s.values[i]));
  if(!configured_ || snapshot_.settings.values[i]!=nativeValue)if(!set(i,s.values[i]))return false;
 }
 if(center_.set_parameter(handle_,"limiterEnable",0)!=ENTROPY_OK)return false;
 double limiter=1;if(center_.get_parameter(handle_,"limiterEnable",&limiter)!=ENTROPY_OK || limiter!=0)return false;
 configured_=true;snapshot_.revision=revision;return true;
}
bool DspCenterModule::reset() noexcept {
 if(!handle_ || api_.reset(handle_)!=ENTROPY_OK)return false;snapshot_.latencyValid=0;snapshot_.telemetry={};return true;
}
bool DspCenterModule::process(const double* left,const double* right,double* out) noexcept {
 if(!handle_)return false;
 const double* inputs[]={left,right};double* outputs[]={out};EntropyAudioBlockF64 block{};block.struct_size=sizeof(block);block.input_channels=2;block.output_channels=1;block.frames=1024;block.inputs=inputs;block.outputs=outputs;
 EntropyProcessContext context{};context.struct_size=sizeof(context);
 if(api_.process_f64(handle_,&block,&context)!=ENTROPY_OK)return false;
 snapshot_.telemetry.struct_size=sizeof(snapshot_.telemetry);
 if(telemetry_.get_telemetry(handle_,&snapshot_.telemetry)!=ENTROPY_OK)return false;
 uint32_t latency=0;if(api_.latency_samples(handle_,&latency)!=ENTROPY_OK || latency!=snapshot_.telemetry.latency_samples)return false;
 snapshot_.latencyValid=1;return true;
}
}
