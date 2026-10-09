#include "Stage02/Fixtures.h"
#include "Stage02/ReferenceBridge.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <memory>
int main(){
    using namespace stage02;int failures=0;
    auto check=[&](bool ok,const char* message){if(!ok){++failures;std::printf("FAIL %s\n",message);}};
    // Full delay-ring wrap, maximum per-channel delay, all 16 outputs.
    for(int program=0;program<2;++program)for(int mode=0;mode<2;++mode){
        auto graph=std::make_unique<entropy::engine::AudioGraph>();check(graph->prepare(1024),"graph prepare");
        void* reference=stage02ReferenceCreate(1024);Configuration c{program,mode,2,true,true,true};
        graph->setOfflineParameter("cinemaSource",program);graph->applyOfflineParameters();stage02ReferenceSet(reference,"cinemaSource",program);stage02ReferenceApply(reference,false);
        auto put=[&](const char* key,double value){check(graph->setOfflineParameter(key,value)&&stage02ReferenceSet(reference,key,value),"parameter");};
        configure(c,[&](const char* key,double value){put(key,value);return true;});
        // The immutable Stage02 reference retains a common base; disable it
        // explicitly to compare the surviving per-channel delay contract.
        check(stage02ReferenceSet(reference,"musicGlobalDelayMs",0)&&stage02ReferenceSet(reference,"cinemaGlobalDelayMs",0),"reference common base disabled");
        for(int ch=0;ch<16;++ch)put((std::string(program?"cinemaDelayMs":"musicDelayMs")+std::to_string(ch+1)).c_str(),800);
        check(graph->applyOfflineParameters(true)&&stage02ReferenceApply(reference,true),"prepare configured pair");
        Buffers in,out,ref;double maximum=0;uint64_t bits=0,nonfinite=0;std::array<double,16> peaks{};
        auto run=[&](unsigned n,bool cold){check(graph->process(in.read.data(),out.write.data(),n)==ENTROPY_OK&&stage02ReferenceProcess(reference,in.read.data(),ref.write.data(),n)==ENTROPY_OK,"process");for(int ch=0;ch<16;++ch)for(unsigned j=0;j<n;++j){double a=out.write[ch][j],b=ref.write[ch][j];nonfinite+=!std::isfinite(a)||!std::isfinite(b);maximum=std::max(maximum,std::abs(a-b));bits+=std::bit_cast<uint64_t>(a)!=std::bit_cast<uint64_t>(b);peaks[ch]=std::max(peaks[ch],std::abs(a));if(cold&&a!=0){check(false,"pre-reset long-delay sample leaked");return;}}};
        for(int reset=0;reset<3;++reset){
            peaks.fill(0);
            for(unsigned at=0;at<400000;){unsigned n=std::min(1024u,400000-at);for(int ch=0;ch<13;++ch)for(unsigned j=0;j<n;++j)in.write[ch][j]=0.01*std::sin(0.037*(at+j));run(n,false);at+=n;}
            for(double peak:peaks)check(peak>1e-10,"warm output active before every reset");
            const char* key=reset==1?(program?"cinemaRole":"source"):"emergencyMute";
            for(int ch=0;ch<16;++ch)put((std::string(key)+std::to_string(ch+1)).c_str(),reset==1?0:1);
            graph->applyOfflineParameters();stage02ReferenceApply(reference,false);in.zero(1024);run(1024,true);
            for(int ch=0;ch<16;++ch)put((std::string(key)+std::to_string(ch+1)).c_str(),reset==1?1:0);
            graph->applyOfflineParameters();stage02ReferenceApply(reference,false);
            for(unsigned at=0;at<400000;){unsigned n=std::min(1024u,400000-at);in.zero(n);run(n,true);at+=n;}
            peaks.fill(0);
            for(unsigned at=0;at<400000;){unsigned n=std::min(1024u,400000-at);in.zero(n);if(!at)for(int ch=0;ch<13;++ch)in.write[ch][0]=0.01;run(n,false);at+=n;}
            for(double peak:peaks)check(peak>1e-10,"reactivated delayed impulse active");
        }
        check(maximum<=1e-12&&!nonfinite,"long-delay Stage02 comparison");
        std::printf("LONG_DELAY program=%d mode=%d max_abs=%.17g bit_mismatches=%llu nonfinite=%llu\n",program,mode,maximum,bits,nonfinite);
        stage02ReferenceDestroy(reference);
    }
    entropy::output_matrix::OutputMatrixEngine matrix;entropy::headroom::HeadroomEngine headroom;prepareIdentity(matrix,headroom,1024);
    EntropyOutputMatrixState s{};s.struct_size=sizeof(s);s.generation=2;s.active_source_mask=0xffff;s.enabled_destination_mask=0xffff;
    for(int ch=0;ch<16;++ch)s.source_destination_mask[ch]=1; // Original ascending many-to-one summing.
    s.source_destination_mask[0]|=2;check(matrix.setState(s)==ENTROPY_OK,"matrix state");Buffers in,out;
    const double values[]={-0.0,1e200,-1e200,1e-200,-1e-200,0.25,-0.25};
    for(int ch=0;ch<16;++ch)for(unsigned j=0;j<1024;++j)in.write[ch][j]=values[(ch+j)%7];
    check(matrix.process(block(in,out,1024))==ENTROPY_OK,"matrix process");
    for(unsigned j=0;j<1024;++j){double sum=0.0;for(int ch=0;ch<16;++ch)sum+=in.write[ch][j];check(std::bit_cast<uint64_t>(sum)==std::bit_cast<uint64_t>(out.write[0][j]),"many-to-one bit order");check(std::bit_cast<uint64_t>(in.write[0][j])==std::bit_cast<uint64_t>(out.write[1][j]),"one-to-many signed-zero identity");}
    std::printf("Stage03 integration regression failures=%d\n",failures);return failures?1:0;
}
