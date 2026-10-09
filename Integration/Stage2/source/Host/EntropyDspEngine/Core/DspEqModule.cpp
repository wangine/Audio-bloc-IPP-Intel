#include "DspEqModule.h"
#include "DspNativeRegistry.h"
#include <cmath>
#include <cstring>
#include <cstdio>
namespace entropy::stage {
EqSettings defaultEqSettings() noexcept {
 EqSettings s{};s.struct_size=sizeof(s);s.band_count=8;
 constexpr double frequencies[]={35,80,180,420,1100,2800,6500,14000};
 for(unsigned i=0;i<8;i++){auto& b=s.bands[i];b.struct_size=sizeof(b);b.enabled=1;b.detector_mode=1;b.frequency_hz=frequencies[i];b.q=1;b.threshold_db=-24;b.ratio=2;b.knee_db=6;b.attack_ms=20;b.release_ms=180;b.max_cut_db=-12;b.max_boost_db=6;}
 return s;
}
bool validEqSettings(const EqSettings& s) noexcept {
 if(s.struct_size!=sizeof(s)||s.band_count!=8||s.mode>3)return false;
 const auto bounded=[](double x,double lo,double hi){return std::isfinite(x)&&x>=lo&&x<=hi;};
 for(const auto& b:s.bands)if(b.struct_size!=sizeof(b)||b.enabled>1||b.filter_type>2||b.detector_mode>2||b.detector_source>2||b.dynamics_mode>3||
 !bounded(b.frequency_hz,20,20000)||!bounded(b.q,.1,20)||!bounded(b.static_gain_db,-18,18)||!bounded(b.threshold_db,-72,0)||!bounded(b.ratio,1,20)||!bounded(b.knee_db,0,24)||!bounded(b.attack_ms,.1,250)||!bounded(b.release_ms,5,2000)||!bounded(b.max_cut_db,-24,0)||!bounded(b.max_boost_db,0,18))return false;
 return true;
}
void DspEqModule::close() noexcept {
 if(handle_&&api_.destroy)api_.destroy(handle_);handle_=nullptr;if(library_)FreeLibrary(library_);library_=nullptr;api_={};eq_={};schema_={};snapshot_={};controlDirty_=false;
}
bool DspEqModule::prepare(const std::filesystem::path& root,std::string& error){
 close();library_=loadNativeModule(NativeModuleId::ParametricEq,root,error);if(!library_)return false;
 auto get=reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library_,ENTROPY_MODULE_GET_API_SYMBOL));api_.struct_size=sizeof(api_);
 EntropyModuleDescriptor d{};d.struct_size=sizeof(d);
 if(!get||get(ENTROPY_MODULE_ABI_VERSION,&api_)!=ENTROPY_OK||!api_.get_descriptor||!api_.create||!api_.destroy||!api_.prepare||!api_.reset||!api_.process_f64||!api_.latency_samples||!api_.query_extension||api_.get_descriptor(&d)!=ENTROPY_OK||d.max_input_channels!=2||d.max_output_channels!=2||std::strcmp(d.identifier,"audio.entropy.parametric-eq.fp64")||!(d.module_flags&ENTROPY_MODULE_FLAG_FP64)){error="EQ FP64/2IN-2OUT ABI rejected";close();return false;}
 eq_.struct_size=sizeof(eq_);
 if(api_.query_extension(ENTROPY_PARAMETRIC_EQ_EXTENSION_ID,ENTROPY_PARAMETRIC_EQ_EXTENSION_VERSION,&eq_,sizeof(eq_))!=ENTROPY_OK||!eq_.submit_settings||!eq_.read_settings||!eq_.read_telemetry||!eq_.parameter_count||!eq_.parameter_descriptor){error="EQ extension rejected";close();return false;}
 EntropyHostServices host{};host.struct_size=sizeof(host);host.abi_version=ENTROPY_MODULE_ABI_VERSION;
 EntropyPrepareInfo info{};info.struct_size=sizeof(info);info.sample_rate=192000;info.maximum_frames=1024;info.input_channels=info.output_channels=2;
 if(api_.create(&host,&handle_)!=ENTROPY_OK||!handle_||api_.prepare(handle_,&info)!=ENTROPY_OK){error="EQ native preparation failed";close();return false;}
 uint32_t count=0;if(eq_.parameter_count(handle_,&count)!=ENTROPY_OK||count!=schema_.parameters.size()){error="EQ descriptor count rejected";close();return false;}
 for(unsigned i=0;i<count;i++){auto& p=schema_.parameters[i];p.struct_size=sizeof(p);if(eq_.parameter_descriptor(handle_,i,&p)!=ENTROPY_OK||p.index!=i||!std::memchr(p.identifier,0,sizeof(p.identifier))){error="EQ descriptor rejected";close();return false;}}
 std::snprintf(schema_.version,sizeof(schema_.version),"%u.%u.%u",d.version_major,d.version_minor,d.version_patch);
 snapshot_.settings.struct_size=sizeof(EqSettings);snapshot_.telemetry.struct_size=sizeof(snapshot_.telemetry);
 if(eq_.read_settings(handle_,&snapshot_.settings)!=ENTROPY_OK||!validEqSettings(snapshot_.settings)){error="EQ defaults rejected";close();return false;}
 uint32_t latency=0;if(api_.latency_samples(handle_,&latency)!=ENTROPY_OK){error="EQ latency unavailable";close();return false;}
 snapshot_.latencyValid=1;snapshot_.latencySamples=latency;snapshot_.loaded=1;return true;
}
bool DspEqModule::apply(const EqSettings& settings,uint64_t revision) noexcept {
 if(!handle_||pending()||!validEqSettings(settings))return false;
 uint64_t sequence=0;if(eq_.submit_settings(handle_,&settings,&sequence)!=ENTROPY_OK)return false;
 snapshot_.settings=settings;snapshot_.settings.sequence=sequence;snapshot_.nativeSequence=sequence;snapshot_.revision=revision;return true;
}
bool DspEqModule::reset() noexcept {return handle_&&api_.reset(handle_)==ENTROPY_OK;}
bool DspEqModule::process(const double* l,const double* r,double* outL,double* outR) noexcept {
 if(!handle_)return false;const double* in[]={l,r};double* out[]={outL,outR};EntropyAudioBlockF64 b{};b.struct_size=sizeof(b);b.input_channels=b.output_channels=2;b.frames=1024;b.inputs=in;b.outputs=out;EntropyProcessContext context{};context.struct_size=sizeof(context);
 if(api_.process_f64(handle_,&b,&context)!=ENTROPY_OK)return false;controlDirty_=true;
 uint32_t latency=0;if(api_.latency_samples(handle_,&latency)!=ENTROPY_OK)return false;snapshot_.latencySamples=latency;snapshot_.latencyValid=1;
 return true;
}
bool DspEqModule::readControlSnapshot() noexcept {
 if(!controlDirty_)return true;
 if(!handle_||eq_.read_telemetry(handle_,&snapshot_.telemetry)!=ENTROPY_OK||eq_.read_settings(handle_,&snapshot_.settings)!=ENTROPY_OK)return false;
 if(!pending())snapshot_.appliedRevision=snapshot_.revision;controlDirty_=false;return true;
}
}
