// Compiled with entropy=stage02_reference and pristine external source paths.
#include "ReferenceBridge.h"
#include <PluginProcessor.h>
#include <OutputMatrixEngine.h>
#include <HeadroomEngine.h>
#include <array>
#include <vector>
#include <cmath>
namespace {
struct Reference {
    entropy::standalone::EntropyCrossoverAudioProcessor crossover;
    entropy::output_matrix::OutputMatrixEngine matrix;
    entropy::headroom::HeadroomEngine headroom;
    std::array<std::vector<double>,16> first, second;
    std::array<double*,16> a{},b{};
    std::array<const double*,16> ca{},cb{};
    unsigned maximum;
    explicit Reference(unsigned n):maximum(n) {
        for(int c=0;c<16;++c) {first[c].resize(n);second[c].resize(n);a[c]=first[c].data();b[c]=second[c].data();ca[c]=a[c];cb[c]=b[c];}
        crossover.prepareToPlay(192000,n);
        if(!matrix.prepare(192000,n)||!headroom.prepare(192000,n))throw std::runtime_error("reference prepare");
        EntropyOutputMatrixState m{};m.struct_size=sizeof(m);m.generation=1;m.active_source_mask=m.enabled_destination_mask=0xffff;
        for(unsigned c=0;c<16;++c)m.source_destination_mask[c]=1u<<c;
        EntropyHeadroomState h{};h.struct_size=sizeof(h);h.revision=1;h.group_count=1;h.active_channel_mask=0xffff;h.transition_ms=10;h.groups[0].struct_size=sizeof(h.groups[0]);h.groups[0].bounded=1;
        if(matrix.setState(m)!=ENTROPY_OK||headroom.setState(h)!=ENTROPY_OK||headroom.setMode(ENTROPY_HEADROOM_MODE_BIT_PERFECT)!=ENTROPY_OK)throw std::runtime_error("reference fixture");
    }
};
}
void* stage02ReferenceCreate(unsigned n){return new Reference(n);}
void stage02ReferenceDestroy(void* p){delete static_cast<Reference*>(p);}
bool stage02ReferenceSet(void* p,const char* id,double v){auto& r=*static_cast<Reference*>(p);auto* a=r.crossover.getAPVTS().getParameter(id);if(!a||!std::isfinite(v))return false;a->setValueNotifyingHost(a->convertTo0to1(static_cast<float>(v)));return true;}
bool stage02ReferenceApply(void* p,bool reset){auto& r=*static_cast<Reference*>(p);if(reset)r.crossover.prepareToPlay(192000,r.maximum);else r.crossover.serviceUpdates();return true;}
int stage02ReferenceProcess(void* p,const double* const* in,double* const* out,unsigned n){
    auto& r=*static_cast<Reference*>(p);for(int c=0;c<16;++c)if(c<13)std::copy_n(in[c],n,r.a[c]);else std::fill_n(r.a[c],n,0.0);
    entropy::native::AudioBuffer<double> v(r.a.data(),16,n);entropy::native::MidiBuffer midi;r.crossover.processBlock(v,midi);
    EntropyAudioBlockF64 block{};block.struct_size=sizeof(block);block.frames=n;block.input_channels=block.output_channels=16;block.inputs=r.ca.data();block.outputs=r.b.data();auto result=r.matrix.process(block);if(result!=ENTROPY_OK)return result;block.inputs=r.cb.data();block.outputs=out;return r.headroom.process(block);
}
unsigned stage02ReferenceLatency(void* p){return static_cast<Reference*>(p)->crossover.getReportedFirLatencySamples();}
double stage02ReferenceGet(void* p,const char* id){auto* v=static_cast<Reference*>(p)->crossover.getAPVTS().getRawParameterValue(id);return v?v->load():NAN;}
