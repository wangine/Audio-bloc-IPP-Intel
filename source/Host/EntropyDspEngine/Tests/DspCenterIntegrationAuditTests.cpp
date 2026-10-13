#include "DspCenterModule.h"
#include "TestSupport.h"
#include <cmath>
#include <cstring>
struct Reference {
 HMODULE library=LoadLibraryW((std::filesystem::path(CENTER_MODULE_ROOT)/"EntropyCenterExtractorV2FP64.dll").c_str());
 EntropyModuleApiV1 api{};EntropyCenterApiV1 center{};EntropyModuleHandle handle=nullptr;
 ~Reference(){if(handle)api.destroy(handle);if(library)FreeLibrary(library);}
 bool prepare(const CenterSettings& s){if(!library)return false;api.struct_size=sizeof(api);auto get=reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library,ENTROPY_MODULE_GET_API_SYMBOL));if(!get||get(ENTROPY_MODULE_ABI_VERSION,&api)!=ENTROPY_OK)return false;
 EntropyHostServices services{};services.struct_size=sizeof(services);services.abi_version=ENTROPY_MODULE_ABI_VERSION;if(api.create(&services,&handle)!=ENTROPY_OK)return false;EntropyPrepareInfo info{};info.struct_size=sizeof(info);info.sample_rate=192000;info.maximum_frames=1024;info.input_channels=2;info.output_channels=1;if(api.prepare(handle,&info)!=ENTROPY_OK)return false;
 center.struct_size=sizeof(center);if(api.query_extension(ENTROPY_CENTER_V2_EXTENSION_ID,ENTROPY_CENTER_V2_EXTENSION_VERSION,&center,sizeof(center))!=ENTROPY_OK)return false;
 for(unsigned i=0;i<16;i++)if(center.set_parameter(handle,centerIds[i],s.values[i])!=ENTROPY_OK)return false;
 return center.set_parameter(handle,"limiterEnable",0)==ENTROPY_OK&&api.reset(handle)==ENTROPY_OK;
 }
 bool process(const double* l,const double* r,double* c){const double* in[]={l,r};double* out[]={c};EntropyAudioBlockF64 b{};b.struct_size=sizeof(b);b.inputs=in;b.outputs=out;b.frames=1024;b.input_channels=2;b.output_channels=1;EntropyProcessContext context{};context.struct_size=sizeof(context);return api.process_f64(handle,&b,&context)==ENTROPY_OK;}
};
int main(){std::string error;uint64_t checked=0;
 for(unsigned mode=0;mode<2;mode++)for(unsigned fft=0;fft<5;fft++){
  CenterSettings s;s.values[0]=mode;s.values[1]=fft;s.values[13]=1.25;s.values[14]=0;
  DspCenterModule integrated;CHECK(integrated.prepare(CENTER_MODULE_ROOT,error));CHECK(integrated.apply(s,2));CHECK(integrated.reset());Reference direct;CHECK(direct.prepare(s));Planes<2> in;std::array<double,1024> a{},b{};
  for(unsigned block=0;block<128;block++){
   for(unsigned i=0;i<1024;i++){const double t=block*1024+i;in.data[0][i]=.2*std::sin(t*.03)+.08*std::sin(t*.071);in.data[1][i]=.2*std::sin(t*.03)+.07*std::sin(t*.11);}
   CHECK(integrated.process(in.in[0],in.in[1],a.data()));CHECK(direct.process(in.in[0],in.in[1],b.data()));CHECK(!std::memcmp(a.data(),b.data(),8192));checked+=1024;
  }
 }
 // Known mono amplitude: spectral reconstruction at 0 dB adds no boost.
 for(unsigned fft=0;fft<5;fft++){
  DspCenterModule center;CHECK(center.prepare(CENTER_MODULE_ROOT,error));CenterSettings s;s.values[1]=fft;CHECK(center.apply(s,2));CHECK(center.reset());Planes<2> in;std::array<double,1024> out{};double energy=0,inputEnergy=0;
  for(unsigned block=0;block<256;block++){
   for(unsigned i=0;i<1024;i++){const double t=block*1024+i;in.data[0][i]=in.data[1][i]=.25*std::sin(t*.03);}
   CHECK(center.process(in.in[0],in.in[1],out.data()));if(block>=128)for(unsigned i=0;i<1024;i++){energy+=out[i]*out[i];inputEnergy+=in.data[0][i]*in.data[0][i];}
  }
  double ratio=std::sqrt(energy/inputEnergy);std::cout<<"mono FFT="<<(512u<<fft)<<" C/L RMS ratio="<<ratio<<"\n";CHECK(ratio>.98&&ratio<1.002);
 }
 auto response=[&](double threshold){DspCenterModule center;std::string err;if(!center.prepare(CENTER_MODULE_ROOT,err))return -1.;CenterSettings settings;settings.values[2]=threshold;settings.values[4]=0;settings.values[12]=2;settings.values[5]=1;if(!center.apply(settings,2)||!center.reset())return -1.;Planes<2> in;std::array<double,1024> out{};double energy=0;
 for(unsigned block=0;block<64;block++){for(unsigned i=0;i<1024;i++){auto sample=std::sin((block*1024+i)*.03);in.data[0][i]=.4*sample;in.data[1][i]=.1*sample;}if(!center.process(in.in[0],in.in[1],out.data()))return -1.;if(block>32)for(auto value:out)energy+=value*value;}return std::sqrt(energy/(31*1024));};
 const auto permissive=response(0),selective=response(.45);std::cout<<"real detector threshold response C RMS: "<<permissive<<" -> "<<selective<<"\n";CHECK(permissive>.005&&selective>=0&&selective<permissive*.05);
 std::cout<<"PASS direct native ABI reference: "<<checked<<" exact C samples across SUM/SPECTRAL and all FFT sizes; no integration gain\n";
}
