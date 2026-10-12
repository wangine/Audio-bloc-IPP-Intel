// Targeted offline replay of the captured, unchanged listening profile.
// Opens no source transport, kernel reader, DAC or IPC server.
#include "Engine/Diagnostics/ChainAudit.h"
#include "Engine/Control/ProfileStore.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Egress/LiveSinkWorkers.h"
#include <iostream>
#include <cmath>
#include <bit>
using namespace entropy;
struct Error {double max=0,square=0;uint64_t samples=0,different=0;void add(double a,double b){if(!std::isfinite(a)||!std::isfinite(b))throw std::runtime_error("nonfinite captured/reference output");double d=a-b;max=std::max(max,std::abs(d));square+=d*d;++samples;if(std::bit_cast<uint64_t>(a)!=std::bit_cast<uint64_t>(b))++different;}};
int main(int argc,char** argv) try {
 if(argc!=3)throw std::runtime_error("PROFILE.entropy CAPTURE_DIRECTORY");
 auto graph=std::make_unique<engine::AudioGraph>();if(!graph->prepare(1024))throw std::runtime_error("prepare");graph->enableOriginalMusicAnalysis(true);
 control::ControlPlane controls(*graph);control::ProfileStore store;auto loaded=store.load(std::filesystem::u8path(argv[1]));
 auto mask=control::LiveSinkWorkers::renderLaneMask(control::LiveSinkWorkers::decode(loaded.document.at("stage05_devices").at("configuration")));
 controls.setRenderLaneMask(mask);controls.atBlockBoundary();controls.afterBlock();
 loaded.parameters.renderLaneMask=mask;controls.crossover().importBytes(loaded.crossover);controls.replace(loaded.parameters);controls.atBlockBoundary();controls.afterBlock();
 auto record=std::make_unique<chain_audit::GraphRecord>();engine::PlanarBuffer output;output.prepare(16,1024);std::array<const double*,13> input{};
 Error total[3][16]{},steady[3][16]{};uint64_t blocks=0,gaps=0,epochs=0,previousFrame=0,previousToken=0;
 auto dir=std::filesystem::u8path(argv[2]);
 std::ifstream summaryFile(dir/"graph-summary.json");if(!summaryFile)throw std::runtime_error("missing graph summary");
 auto summary=ipc::parse(std::string(std::istreambuf_iterator<char>(summaryFile),{}));
 auto expectedBlocks=summary.at("written").asUnsigned();if(!expectedBlocks||summary.at("recordBytes").asUnsigned()!=sizeof(*record)||summary.at("drops").asUnsigned()||summary.at("failed").asBool())throw std::runtime_error("incomplete graph audit");
 for(unsigned part=0;;++part){std::ifstream file(dir/("graph-"+std::to_string(part)+".bin"),std::ios::binary);if(!file)break;
  while(file.read(reinterpret_cast<char*>(record.get()),sizeof(*record))){
   if(blocks&&record->graphFrame!=previousFrame+1024)throw std::runtime_error("graph gap cannot be replayed without missing DSP history");if(blocks&&record->token!=previousToken)++epochs;previousFrame=record->graphFrame;previousToken=record->token;
   for(unsigned c=0;c<13;++c)input[c]=record->input[c];controls.atBlockBoundary();
   engine::GraphControlAccess::crossover(*graph)->setProgramInputEpochs(record->token,record->token,0,0xffffu);
   if(graph->process(input.data(),output.writePointers(),1024)!=ENTROPY_OK)throw std::runtime_error("offline process");controls.afterBlock();
   auto cross=engine::GraphControlAccess::crossoverOutput(*graph).readPointers();auto matrix=engine::GraphControlAccess::matrixOutput(*graph).readPointers();
   for(unsigned c=0;c<16;++c)for(unsigned f=0;f<1024;++f){double a[]={record->crossover[c][f],record->matrix[c][f],record->output[c][f]};double b[]={cross[c][f],matrix[c][f],output.readChannel(c)[f]};for(unsigned s=0;s<3;++s){total[s][c].add(a[s],b[s]);if(blocks>=1000)steady[s][c].add(a[s],b[s]);}}
   ++blocks;
  }if(!file.eof()||file.gcount()!=0)throw std::runtime_error("truncated graph record");
 }
 if(blocks!=expectedBlocks)throw std::runtime_error("missing graph records/parts");
 std::cout<<"{\"blocks\":"<<blocks<<",\"gaps\":"<<gaps<<",\"tokenChanges\":"<<epochs<<",\"renderMask\":"<<mask<<",\"stages\":[";
 for(unsigned s=0;s<3;++s){if(s)std::cout<<',';std::cout<<"[";for(unsigned c=0;c<16;++c){if(c)std::cout<<',';auto a=total[s][c],b=steady[s][c];std::cout<<"{\"channel\":"<<c+1<<",\"maxError\":"<<a.max<<",\"rmsError\":"<<std::sqrt(a.square/std::max(uint64_t(1),a.samples))<<",\"differentSamples\":"<<a.different<<",\"steadyMaxError\":"<<b.max<<",\"steadyDifferentSamples\":"<<b.different<<"}";}std::cout<<"]";}std::cout<<"]}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
