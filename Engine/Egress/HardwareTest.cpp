#include "HardwareTest.h"
#include "NativeEgress.h"
#include <entropy/dac/WasapiDacSink.h>
#include <WindowsDeviceInventory.h>
#include <DeviceIdentityResolver.h>
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>

namespace entropy::egress_host {
std::string fileSha256(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    std::string result;std::ifstream file(path,std::ios::binary);
    if(!file || BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return result;
    ULONG size=0,copied=0;
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&copied,0)>=0){
        std::vector<uint8_t> object(size);std::array<uint8_t,65536> buffer{};
        if(BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0)>=0){
            bool good=true;
            while(file){file.read(reinterpret_cast<char*>(buffer.data()),buffer.size());auto count=file.gcount();
                if(count && BCryptHashData(hash,buffer.data(),static_cast<ULONG>(count),0)<0){good=false;break;}}
            std::array<uint8_t,32> digest{};
            if(good && !file.bad() && BCryptFinishHash(hash,digest.data(),digest.size(),0)>=0){
                constexpr char hex[]="0123456789ABCDEF";for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}}
            BCryptDestroyHash(hash);
        }
    }
    BCryptCloseAlgorithmProvider(algorithm,0);return result;
}
bool offlineGateMatches(const std::filesystem::path& path,const std::string& exeHash){
    std::ifstream f(path);std::string magic,hash,profile,extra;
    return exeHash.size()==64 && std::getline(f,magic) && std::getline(f,hash) && std::getline(f,profile) &&
        magic=="ENTROPY_STAGE04_OFFLINE_PASS_V1" && hash==exeHash &&
        profile=="FIRST_DAC_INTERNAL_SINE_192000_2CH_IIR_HEADROOM20DB" && !std::getline(f,extra);
}
namespace {
std::atomic<bool> cancelled{false};
BOOL WINAPI consoleStop(DWORD signal){if(signal==CTRL_C_EVENT||signal==CTRL_BREAK_EVENT||signal==CTRL_CLOSE_EVENT){cancelled=true;return TRUE;}return FALSE;}
struct WasapiAdapter final:Sink {
    entropy::dac::WasapiDacSink sink;
    EntropyResult open(const EntropyDacSinkConfig& c) override{return sink.open(c);}
    EntropyResult start() noexcept override{return sink.start();}
    EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{return sink.waitBuffer(r);}
    EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{return sink.submit(b);}
    EntropyDacSinkStatus status() const noexcept override{return sink.status();}
    void stop() noexcept override{sink.stop();}
    void close() noexcept override{sink.close();}
};
}
int hardwareTest(int argc,char** argv){
    // All argument/report checks precede device enumeration and COM initialization.
    std::string endpoint,gate,format;bool explicitArm=false;unsigned frames=256;
    for(int i=2;i<argc;++i){std::string_view arg=argv[i];
        if(arg=="--arm")explicitArm=true;
        else if(i+1<argc && arg=="--endpoint")endpoint=argv[++i];
        else if(i+1<argc && arg=="--offline-gate")gate=argv[++i];
        else if(i+1<argc && arg=="--format")format=argv[++i];
        else if(i+1<argc && arg=="--frames"){
            std::string_view value=argv[++i];if(value=="128")frames=128;else if(value=="256")frames=256;else if(value=="512")frames=512;else if(value=="1024")frames=1024;else return 2;
        }else return 2;
    }
    if(!explicitArm||endpoint.empty()||endpoint.size()>=ENTROPY_DAC_SINK_ENDPOINT_ID_BYTES||gate.empty()||
       (format!="packed24"&&format!="24in32")){
        std::fprintf(stderr,"BLOCKED hardware: explicit --arm, --endpoint, --format and matching --offline-gate required; no device opened\n");return 2;}
    std::array<wchar_t,32768> exePath{};const auto length=GetModuleFileNameW(nullptr,exePath.data(),static_cast<DWORD>(exePath.size()));
    if(!length||length==exePath.size()||!offlineGateMatches(gate,fileSha256(std::filesystem::path(exePath.data())))){
        std::fprintf(stderr,"BLOCKED hardware: missing/stale offline PASS gate for this executable; no device opened\n");return 2;}
    entropy::device::WindowsDeviceInventory inventory;
    if(inventory.refresh()!=ENTROPY_OK)return 3;
    auto device=std::find_if(inventory.devices().begin(),inventory.devices().end(),[&](const auto& d){return d.active && endpoint==d.endpoint_id_utf8;});
    if(device==inventory.devices().end()){std::fprintf(stderr,"FAIL endpoint not active, no fallback device selected\n");return 3;}
    auto binding=entropy::device::captureBinding(*device,"DAC_A",1);
    auto resolution=entropy::device::resolveBinding(binding,inventory.devices());
    if(resolution.state!=ENTROPY_DEVICE_VERIFIED){std::fprintf(stderr,"FAIL explicit endpoint identity verification\n");return 3;}
    FirstDacGraph graph;ProbeConfig config;config.frames=frames;config.format=format=="packed24"?0:1;
    if(!graph.prepare(config)||!graph.arm())return 3;
    EntropyDacSinkConfig dac{};dac.struct_size=sizeof(dac);dac.backend=ENTROPY_DAC_SINK_BACKEND_WASAPI_EXCLUSIVE;
    dac.sample_rate=192000;dac.channels=2;dac.output_format=config.format;dac.requested_buffer_frames=frames;dac.revision=1;
    std::memcpy(dac.endpoint_id_utf8,endpoint.c_str(),endpoint.size()+1);
    cancelled=false;SetConsoleCtrlHandler(consoleStop,TRUE);
    std::atomic<bool> ready{false},started{false},finished{false};std::atomic<int> result{0};
    std::thread producer,consumer;
    try {
    producer=std::thread([&]{try {
        for(unsigned written=0;written<4096 && !cancelled && !finished;written+=frames)
            if(!graph.produce()){result=4;finished=true;return;}
        ready.store(true,std::memory_order_release);
        while(!started.load(std::memory_order_acquire)&&!finished&&!cancelled)std::this_thread::yield();
        auto next=std::chrono::steady_clock::now();const auto period=std::chrono::nanoseconds(uint64_t(frames)*1000000000/192000);
        while(!finished&&!cancelled){
            // Scheduler pacing is outside the DSP processing function.
            next+=period;std::this_thread::sleep_until(next);
            if(finished||cancelled)break;
            if(!graph.produce()){if(!finished&&!cancelled)result=4;finished=true;break;}
        }
    }catch(...){graph.pipeline().fail(Input);result=7;finished=true;}});
    consumer=std::thread([&]{try {
        WasapiAdapter sink;SinkOwner owner(graph.pipeline(),sink);
        while(!ready.load(std::memory_order_acquire)&&!finished&&!cancelled)std::this_thread::yield();
        if(finished||cancelled){finished=true;return;}
        if(!owner.open(dac)||!owner.start()){result=5;owner.stop();finished=true;return;}
        started.store(true,std::memory_order_release);
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!finished&&!cancelled&&std::chrono::steady_clock::now()<end){if(!owner.cycle()){result=6;break;}}
        finished=true;graph.disarm();owner.stop();
    }catch(...){graph.pipeline().fail(Device);result=7;finished=true;}});
    }catch(...){graph.pipeline().fail(Device);result=7;finished=true;}
    if(consumer.joinable())consumer.join();finished=true;
    if(producer.joinable())producer.join();graph.disarm();SetConsoleCtrlHandler(consoleStop,FALSE);
    std::printf("Hardware test stopped: result=%d faults=%u xruns=%llu; no automatic restart\n",result.load(),graph.pipeline().faults(),(unsigned long long)graph.pipeline().xruns());
    return result.load();
}
}
