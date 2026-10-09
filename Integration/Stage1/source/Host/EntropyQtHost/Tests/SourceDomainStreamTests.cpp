#include "Core/SourceDomainStream.h"
#include <cstdlib>
#include <iostream>
void require(bool v,const char* m){if(!v){std::cerr<<"FAIL "<<m<<"\n";std::exit(1);}}
int main() {
    entropy::host::SourceDomainConfig c{};
    c.channels=2; c.destinationRate=192000; c.dspBlockFrames=1024;
    c.fifoCapacityFrames=131072; c.prerollFrames=32768;
    if(!entropy::host::SourceDomainStream::validConfig(c)) { std::cerr<<"FAIL valid config\n"; return 1; }
    c.destinationRate=96000;
    if(entropy::host::SourceDomainStream::validConfig(c)) { std::cerr<<"FAIL accepted non-192k\n"; return 1; }
    c.destinationRate=192000; c.dspBlockFrames=512;
    if(entropy::host::SourceDomainStream::validConfig(c)) { std::cerr<<"FAIL accepted non-1024\n"; return 1; }
    c.dspBlockFrames=1024;
    require(c.captureFifoCapacityFrames >= c.endpointReadFrames * 8,
            "capture FIFO must absorb endpoint/SRC scheduling separation");
    require(c.outputTargetFrames < c.fifoCapacityFrames,
            "converted FIFO requires a bounded producer target below capacity");
    std::cout<<"PASS SourceDomainStream config\n";
}
