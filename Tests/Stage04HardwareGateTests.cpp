#include "Engine/Egress/HardwareTest.h"
#include <cstdio>
#include <fstream>
int main(int argc,char** argv){
    using namespace entropy::egress_host;
    const std::filesystem::path root=argc>1?argv[1]:"stage04-gate-tests";
    std::filesystem::create_directories(root);
    auto abc=root/"abc.txt";{std::ofstream f(abc,std::ios::binary);f<<"abc";}
    const std::string expected="BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD";
    if(fileSha256(abc)!=expected)return 1;
    auto gate=root/"test.gate";
    if(offlineGateMatches(gate,expected))return 2;
    {std::ofstream f(gate);f<<"ENTROPY_STAGE04_OFFLINE_PASS_V1\n"<<expected<<"\nFIRST_DAC_INTERNAL_SINE_192000_2CH_IIR_HEADROOM20DB\n";}
    if(!offlineGateMatches(gate,expected)||offlineGateMatches(gate,std::string(64,'0')))return 3;
    {std::ofstream f(gate,std::ios::app);f<<"unexpected\n";}
    if(offlineGateMatches(gate,expected))return 4;
    char a[]="EntropyAudioCore",b[]="--hardware-test";char* missing[]={a,b};
    if(hardwareTest(2,missing)!=2)return 5;
    char arm[]="--arm",ep[]="--endpoint",id[]="must-never-open",fmt[]="--format",packed[]="packed24",g[]="--offline-gate";
    std::string stale=(root/"missing.gate").string();char* args[]={a,b,arm,ep,id,fmt,packed,g,stale.data()};
    if(hardwareTest(9,args)!=2)return 6;
    std::puts("PASS hardware gate: SHA256 vector, stale/malformed gate and missing authorization rejected before device access");
}
