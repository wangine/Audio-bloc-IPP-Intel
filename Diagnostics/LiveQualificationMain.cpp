#include "Engine/Control/LiveQualification.h"
#include "Engine/Control/ProfileStore.h"
#include <iostream>
int main(int argc,char** argv) try {
 using namespace entropy;
 if(argc<2){std::cout<<"Explicit OFFLINE diagnostic: EntropyLiveQualification.exe PROFILE.entropy [MUSIC|CINEMA]\nNo DAC, source transport or IPC server is opened.\n";return 0;}
 if(argc>4)throw std::invalid_argument("Expected profile and optional MUSIC/CINEMA or --change-json JSON");
 auto graph=std::make_unique<engine::AudioGraph>();if(!graph->prepare(1024))throw std::runtime_error("Graph prepare");
 control::ControlPlane controls(*graph);control::ProfileStore store;auto profile=store.load(std::filesystem::u8path(argv[1]));
 controls.crossover().importBytes(profile.crossover);controls.replace(profile.parameters);controls.atBlockBoundary();controls.afterBlock();
 ipc::Value change;if(argc==3){std::string program=argv[2];if(program!="MUSIC"&&program!="CINEMA")throw std::invalid_argument("MUSIC/CINEMA only");change=ipc::Value::Object{{"action","program"},{"program",program=="CINEMA"?1:0}};}
 if(argc==4){if(std::string(argv[2])!="--change-json")throw std::invalid_argument("Expected --change-json");change=ipc::parse(argv[3]);}
 std::cout<<ipc::dump(control::liveProfilePreflight(controls,change))<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
