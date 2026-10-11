#include "ProviderRegistry.h"
#include "SourceDomain.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <objbase.h>
#include <avrt.h>
using namespace entropy::music;
int main(int argc, char** argv) {
    if (argc != 6) return 2;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DWORD taskIndex=0;
    HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&taskIndex);
    if(!mmcss)return 5;
    if(!AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_HIGH))return 6;
    ProviderRegistry registry;
    registry.discover(argv[1]);
    auto provider = registry.find(argv[2]);
    if (!provider) return 3;
    void* handle{};
    if (provider->api.open(argv[3], std::stoul(argv[4]), &handle) != 1) return 4;
    SourceDomain domain;
    std::vector<std::byte> payload(ENTROPY_MAX_PACKET_FRAMES * 16);
    LARGE_INTEGER frequency, begin, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&begin);
    uint64_t count=0, frames=0, lastFirst=0, lastFrames=0, generation=0, empty=0, gaps=0;
    double maximumReadMs=0;
    bool failed=false;
    const double seconds=std::stod(argv[5]);
    do {
        EntropyIngressPacketV2 packet{};
        packet.abiVersion=ENTROPY_PROVIDER_ABI_V2; packet.structSize=sizeof(packet);
        LARGE_INTEGER a,b;
        QueryPerformanceCounter(&a);
        auto rc=provider->api.read(handle, &packet, payload.data(), static_cast<uint32_t>(payload.size()));
        QueryPerformanceCounter(&b);
        auto ms=1000.0*(b.QuadPart-a.QuadPart)/frequency.QuadPart;
        maximumReadMs=(std::max)(maximumReadMs,ms);
        if(rc<0) {std::cout << "{\"event\":\"read_failed\",\"after_packets\":"<<count<<",\"seconds\":"<<double(b.QuadPart-begin.QuadPart)/frequency.QuadPart<<"}\n";failed=true;break;}
        if(!rc) {empty++; Sleep(1);}
        else {
            if(packet.frames!=packet.format.sampleRate/100 || (packet.flags&ENTROPY_PACKET_DISCONTINUITY))
                std::cout<<"{\"event\":\"packet_detail\",\"packet\":"<<packet.packetSequence<<",\"first\":"<<packet.firstSourceFrame<<",\"frames\":"<<packet.frames<<",\"flags\":"<<packet.flags<<",\"seconds\":"<<double(b.QuadPart-begin.QuadPart)/frequency.QuadPart<<"}\n";
            if(count && packet.firstSourceFrame!=lastFirst+lastFrames) {
                gaps++;
                std::cout<<"{\"event\":\"position_gap\",\"expected\":"<<lastFirst+lastFrames<<",\"actual\":"<<packet.firstSourceFrame<<",\"flags\":"<<packet.flags<<",\"seconds\":"<<double(b.QuadPart-begin.QuadPart)/frequency.QuadPart<<"}\n";
            }
            if(generation!=packet.generation) {
                if(!domain.prepare(packet.format,packet.generation)) {failed=true;break;}
                generation=packet.generation;
            }
            if(!domain.processPacket({packet,{payload.data(),packet.payloadBytes}})) {
                std::cout<<"{\"event\":\"domain_rejected\",\"packet\":"<<packet.packetSequence<<",\"flags\":"<<packet.flags<<",\"frames\":"<<packet.frames<<"}\n";
                failed=true;break;
            }
            StereoBlock block;
            while(domain.pop1024(block)) {}
            count++; frames+=packet.frames;lastFirst=packet.firstSourceFrame;lastFrames=packet.frames;
        }
        QueryPerformanceCounter(&now);
    } while(double(now.QuadPart-begin.QuadPart)/frequency.QuadPart<seconds);
    provider->api.cancel(handle);provider->api.close(handle);
    std::cout<<std::setprecision(12)<<"{\"event\":\"summary\",\"packets\":"<<count<<",\"frames\":"<<frames<<",\"empty_reads\":"<<empty<<",\"position_gaps\":"<<gaps<<",\"max_read_ms\":"<<maximumReadMs<<",\"failed\":"<<(failed?"true":"false")<<"}\n";
    CoUninitialize();
    AvRevertMmThreadCharacteristics(mmcss);
    return failed?1:0;
}
