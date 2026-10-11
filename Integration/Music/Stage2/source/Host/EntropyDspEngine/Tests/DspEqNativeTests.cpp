#include "DspEqModule.h"
#include "TestSupport.h"
#include <cmath>
#include <limits>
int main(){
 auto path=std::filesystem::path(CENTER_MODULE_ROOT)/"EntropyParametricEQ.dll";auto library=LoadLibraryW(path.c_str());CHECK(library);
 auto get=reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library,ENTROPY_MODULE_GET_API_SYMBOL));EntropyModuleApiV1 api{};api.struct_size=sizeof(api);CHECK(get&&get(ENTROPY_MODULE_ABI_VERSION,&api)==ENTROPY_OK);
 EntropyParametricEqApiV1 ext{};ext.struct_size=sizeof(ext);CHECK(api.query_extension(ENTROPY_PARAMETRIC_EQ_EXTENSION_ID,ENTROPY_PARAMETRIC_EQ_EXTENSION_VERSION,&ext,sizeof(ext))==ENTROPY_OK);
 uint64_t checked=0;std::array<double,4> responses{};
 for(unsigned mode=0;mode<4;mode++)for(unsigned filter=0;filter<3;filter++){
  auto settings=defaultEqSettings();settings.mode=mode;auto& band=settings.bands[0];band.frequency_hz=1000;band.filter_type=filter;band.static_gain_db=6;band.threshold_db=-36;band.ratio=4;band.attack_ms=1;band.release_ms=50;
  DspEqModule integrated;std::string error;CHECK(integrated.prepare(CENTER_MODULE_ROOT,error));CHECK(integrated.schema().parameters.size()==121);CHECK(integrated.apply(settings,99));CHECK(integrated.pending());
  EntropyModuleHandle reference{};EntropyHostServices host{};host.struct_size=sizeof(host);host.abi_version=ENTROPY_MODULE_ABI_VERSION;CHECK(api.create(&host,&reference)==ENTROPY_OK);EntropyPrepareInfo prep{};prep.struct_size=sizeof(prep);prep.sample_rate=192000;prep.maximum_frames=1024;prep.input_channels=prep.output_channels=2;CHECK(api.prepare(reference,&prep)==ENTROPY_OK);uint64_t serial=0;CHECK(ext.submit_settings(reference,&settings,&serial)==ENTROPY_OK);
  Planes<2> in,a,b;double energy=0;
  for(unsigned block=0;block<96;block++){
   for(unsigned i=0;i<1024;i++){double t=block*1024+i;in.data[0][i]=.2*std::sin(t*2*3.141592653589793*1000/192000);in.data[1][i]=.13*std::sin(t*.07)+1e-17;}
   CHECK(integrated.process(in.in[0],in.in[1],a.out[0],a.out[1]));CHECK(integrated.readControlSnapshot());EntropyAudioBlockF64 native{};native.struct_size=sizeof(native);native.input_channels=native.output_channels=2;native.frames=1024;native.inputs=in.in.data();native.outputs=b.out.data();EntropyProcessContext context{};context.struct_size=sizeof(context);CHECK(api.process_f64(reference,&native,&context)==ENTROPY_OK);
   for(unsigned c=0;c<2;c++){CHECK(!std::memcmp(a.in[c],b.in[c],8192));if(mode==0)CHECK(!std::memcmp(a.in[c],in.in[c],8192));for(auto x:a.data[c])CHECK(std::isfinite(x));}checked+=2048;
   if(block>=64)for(auto x:a.data[0])energy+=x*x;
  }
  CHECK(integrated.snapshot().appliedRevision==99&&!integrated.pending());CHECK(integrated.snapshot().latencySamples==0);CHECK(integrated.snapshot().telemetry.processed_frames==96*1024);if(filter==0)responses[mode]=std::sqrt(energy/(32*1024));api.destroy(reference);
 }
 CHECK(responses[1]>responses[0]*1.9);CHECK(responses[2]<responses[0]*.95);CHECK(responses[3]<responses[1]*.95);
 auto invalid=defaultEqSettings();invalid.bands[0].q=std::numeric_limits<double>::quiet_NaN();CHECK(!validEqSettings(invalid));DspEqModule missing;std::string error;CHECK(!missing.prepare(std::filesystem::temp_directory_path()/"entropy-no-eq-module",error));
 std::cout<<"PASS "<<checked<<" exact original native LR samples across 4 modes x 3 filters; real static/dynamic response; 121 descriptors, latency0, native ack; no gain/SRC adapter\n";FreeLibrary(library);
}
