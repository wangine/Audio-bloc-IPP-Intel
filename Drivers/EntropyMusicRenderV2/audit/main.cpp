#include "WasapiClient.h"
#include "PcmVerifier.h"
#include "StereoMetrics.h"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <filesystem>
#include <memory>
using namespace entropy::music;
using namespace entropy::music::audit;
namespace {
struct OwnedHandle{HANDLE value;~OwnedHandle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct Mapped{void* value;~Mapped(){if(value)UnmapViewOfFile(value);}};
bool readExact(HANDLE pipe,void* bytes,DWORD size){auto out=static_cast<uint8_t*>(bytes);while(size){DWORD got=0;if(!ReadFile(pipe,out,size,&got,nullptr) || !got)return false;out+=got;size-=got;}return true;}
std::string quoted(const std::string& s){std::string out="\"";for(auto c:s){if(c=='"' || c=='\\')out+='\\';if(static_cast<unsigned char>(c)<32)out+='?';else out+=c;}return out+'"';}
int collect(const std::wstring& session,const std::filesystem::path& output){
    OwnedHandle mapping{CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(SharedProgress),progressName(session).c_str())};if(!mapping.value || GetLastError()==ERROR_ALREADY_EXISTS)throw std::runtime_error("Collector session already exists");
    Mapped mapped{MapViewOfFile(mapping.value,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedProgress))};if(!mapped.value)throw std::runtime_error("Map failed");auto progress=static_cast<SharedProgress*>(mapped.value);*progress={};
    OwnedHandle pipe{CreateNamedPipeW(pipeName(session).c_str(),PIPE_ACCESS_INBOUND|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,1048576,1048576,0,nullptr)};if(pipe.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Pipe failed");
    std::cout<<"Audit collector ready. Connect isolated frozen Core; pipe="<<provider::utf8(pipeName(session))<<std::endl;
    if(!ConnectNamedPipe(pipe.value,nullptr) && GetLastError()!=ERROR_PIPE_CONNECTED)throw std::runtime_error("Connect failed");ULONG pid=0;if(!GetNamedPipeClientProcessId(pipe.value,&pid) || !pid)throw std::runtime_error("Provider client PID missing");InterlockedExchange(&progress->clientPid,static_cast<LONG>(pid));
    std::ofstream file(output,std::ios::binary);if(!file)throw std::runtime_error("Capture output failed");bool footer=false;
    for(;;){provider::AuditHeader h;if(!readExact(pipe.value,&h,sizeof(h)))break;if(h.magic!=0x41524d45 || h.version!=1 || (h.kind!=1 && h.kind!=2) || h.bytes>160+MaxPacketFrames*8)throw std::runtime_error("Invalid capture header");file.write(reinterpret_cast<const char*>(&h),sizeof(h));
        if(h.kind==2){if(h.bytes)throw std::runtime_error("Bad footer");footer=true;if(h.overflow || h.failedWrites)InterlockedExchange(&progress->errors,1);break;}
        if(h.bytes<160)throw std::runtime_error("Short capture");std::vector<uint8_t> bytes(h.bytes);if(!readExact(pipe.value,bytes.data(),h.bytes))break;
        EntropyIngressPacketV2 p;std::memcpy(&p,bytes.data(),160);if(p.payloadBytes!=h.bytes-160 || p.frames>MaxPacketFrames)throw std::runtime_error("Invalid packet length");file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!file)throw std::runtime_error("Capture disk write failed");InterlockedAdd64(&progress->frames,p.frames);
    }
    std::ofstream evidence(output.string()+".client.json");evidence<<"{\"providerClientPid\":"<<pid<<",\"footer\":"<<(footer?"true":"false")<<",\"frames\":"<<InterlockedCompareExchange64(&progress->frames,0,0)<<"}\n";
    return footer?0:2;
}
int verify(const std::filesystem::path& path,uint64_t expected,bool drained,uint32_t rate,uint32_t bits,uint64_t generation){
    PcmFormat format{64,rate,2,bits,bits==32?24u:bits,bits/4,rate*(bits/4),3,0xfffe,1,{},{}};std::memcpy(format.subFormat,PcmGuid,16);
    if(!validateFormat(format) || !generation || !expected)throw std::runtime_error("Explicit supported expected rate/bits/generation/frames required");
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Capture file absent");PcmVerifier verifier(format,expected,generation);StereoMetrics metrics;bool footer=false;uint64_t overflow=0,failed=0;
    provider::AuditHeader h;while(file.read(reinterpret_cast<char*>(&h),sizeof(h))){if(h.magic!=0x41524d45 || h.version!=1)throw std::runtime_error("Bad audit version");
        if(h.kind==2){if(h.bytes)throw std::runtime_error("Bad footer");footer=true;overflow=h.overflow;failed=h.failedWrites;if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Bytes after footer");break;}
        if(h.kind!=1 || h.bytes<160 || h.bytes>160+MaxPacketFrames*8)throw std::runtime_error("Bad audit length");std::vector<uint8_t> bytes(h.bytes);if(!file.read(reinterpret_cast<char*>(bytes.data()),h.bytes))break;EntropyIngressPacketV2 p;std::memcpy(&p,bytes.data(),160);if(p.payloadBytes!=h.bytes-160)throw std::runtime_error("Bad raw packet length");verifier.accept(p,bytes.data()+160);metrics.accumulate(p,bytes.data()+160);
    }
    auto r=verifier.finish(footer,overflow,failed,drained);auto s=metrics.result();
    std::cout<<"{\"pass\":"<<(r.pass?"true":"false")<<",\"frames\":"<<r.frames<<",\"mismatchedBytes\":"<<r.mismatchedBytes<<",\"lostFrames\":"<<r.lostFrames<<",\"auditOverflow\":"<<overflow<<",\"failedWrites\":"<<failed<<",\"reason\":"<<quoted(r.reason)<<",\"preSrc\":{\"leftPeak\":"<<s.leftPeak<<",\"rightPeak\":"<<s.rightPeak<<",\"leftRms\":"<<s.leftRms<<",\"rightRms\":"<<s.rightRms<<",\"midRms\":"<<s.midRms<<",\"sideRms\":"<<s.sideRms<<",\"pearson\":"<<(s.pearson?std::to_string(*s.pearson):"null")<<",\"uncentered\":"<<(s.uncentered?std::to_string(*s.uncentered):"null")<<"}}\n";return r.pass?0:2;
}
}
int wmain(int argc,wchar_t** argv){try{
    std::map<std::wstring,std::wstring> args;for(int i=1;i<argc;i+=2){if(i+1>=argc)throw std::runtime_error("Options require values");args[argv[i]]=argv[i+1];}
    if(args.empty()){std::cout<<"Entropy Music audit: offline tests via CTest. Live actions require --action collect|render|verify, explicit --session token and isolated Core. No audio device opened.\n";return 0;}
    const auto action=args[L"--action"];
    if(action==L"identify"){const auto devices=provider::resolveEndpoints();if(devices.size()!=1)throw std::runtime_error("Exact new endpoint unavailable");std::cout<<"{\"providerId\":"<<quoted(bridge::ProviderId)<<",\"endpointId\":"<<quoted(provider::endpointKey(devices[0]))<<",\"windowsEndpointId\":"<<quoted(provider::utf8(devices[0].windowsEndpointId))<<"}\n";return 0;}
    if(action==L"ready"){OwnedHandle mapping{OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,progressName(args[L"--session"]).c_str())};if(!mapping.value)return 2;Mapped view{MapViewOfFile(mapping.value,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedProgress))};return view.value && InterlockedCompareExchange(&static_cast<SharedProgress*>(view.value)->clientPid,0,0)?0:2;}
    if(action==L"collect")return collect(args[L"--session"],args[L"--capture"]);
    if(action==L"verify")return verify(args[L"--capture"],std::stoull(args[L"--frames"]),args[L"--drained"]==L"true",static_cast<uint32_t>(std::stoul(args[L"--rate"])),static_cast<uint32_t>(std::stoul(args[L"--bits"])),std::stoull(args[L"--generation"]));
    if(action!=L"render")throw std::runtime_error("Unknown action");RunConfig c;c.session=args[L"--session"];c.rate=static_cast<uint32_t>(std::stoul(args[L"--rate"]));c.bits=static_cast<uint32_t>(std::stoul(args[L"--bits"]));if(args.contains(L"--seconds"))c.seconds=static_cast<uint32_t>(std::stoul(args[L"--seconds"]));if(args.contains(L"--period-frames"))c.periodFrames=static_cast<uint32_t>(std::stoul(args[L"--period-frames"]));
    if(args[L"--servicing"]!=L"event" && args[L"--servicing"]!=L"polling")throw std::runtime_error("Explicit event/polling servicing required");c.eventDriven=args[L"--servicing"]==L"event";
    auto r=WasapiClient::run(c);std::cout<<"{\"mode\":\"WASAPI_EXCLUSIVE\",\"servicing\":"<<quoted(c.eventDriven?"event":"polling")<<",\"endpointId\":"<<quoted(r.endpointId)<<",\"rate\":"<<c.rate<<",\"containerBits\":"<<c.bits<<",\"bufferFrames\":"<<r.bufferFrames<<",\"emittedFrames\":"<<r.emittedFrames<<",\"clockFrames\":"<<r.clockFrames<<",\"receivedFrames\":"<<r.receivedFrames<<",\"observedAllocation\":"<<r.observedAllocation<<",\"generation\":"<<r.driver.generation<<",\"committedFrames\":"<<r.driver.committedFrames<<",\"publishedFrames\":"<<r.driver.publishedFrames<<",\"deliveredFrames\":"<<r.driver.deliveredFrames<<",\"lostFrames\":"<<r.driver.lostFrames<<",\"drained\":"<<(r.drained?"true":"false")<<"}\n";return r.drained?0:2;
}catch(const std::exception& e){std::cerr<<"Audit failed: "<<e.what()<<"\n";return 1;}}
