#include "CapturePacketReader.h"
#include <iostream>
#include <stdexcept>
#include <vector>
int main() {
    try {
        // One WASAPI event can represent several packets, including a split period.
        std::vector<unsigned> packets{88,353,441};
        unsigned next=0, frames=0, waits=0, notifications=1;
        auto read=[&] {if(next==packets.size())return 0;frames+=packets[next++];return 1;};
        auto wait=[&] {waits++;if(!notifications)return 0;notifications--;return 1;};
        for(unsigned i=0;i<packets.size();i++)
            if(entropy::music::readCapturePacket(read,wait)!=1) throw std::runtime_error("Queued packet was left unread while awaiting another event");
        if(frames!=882 || waits>1) throw std::runtime_error("Split capture frames not preserved");
        if(entropy::music::readCapturePacket(read,wait)!=0) throw std::runtime_error("Empty capture queue did not wait");
        auto failed=[] {return -1;};
        unsigned unexpectedWaits=0;
        auto forbiddenWait=[&] {unexpectedWaits++;return 0;};
        if(entropy::music::readCapturePacket(failed,forbiddenWait)!=-1) throw std::runtime_error("Read error was hidden");
        if(unexpectedWaits)throw std::runtime_error("Wait after read error");
        std::cout<<"PASS complete split capture queue, empty wait, read error\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
