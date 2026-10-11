#include "TestSupport.h"
#include "../provider/BridgeClient.h"
#include <thread>
#include <chrono>
#include <memory>
using namespace entropy::music;
using namespace entropy::music::provider;
namespace entropy::music::provider {struct BridgeClientTestProbe {static void barrier(BridgeClient& c,void(*hook)(void*),void* context){c.exitHook_=hook;c.exitContext_=context;}static bool readerReleased(BridgeClient& c){std::unique_lock guard(c.reader_,std::try_to_lock);return guard.owns_lock();}};}
DeviceIdentity identity(){DeviceIdentity d{bridge::HardwareId,bridge::ServiceName,L"private-interface",L"windows-render-id",{}, {},{},0,1};std::memcpy(d.interfaceGuid.data(),bridge::InterfaceGuid,16);d.containerId[0]=d.endpointContainerId[0]=1;return d;}
struct FakeBridge:BridgeTransport {
    std::mutex mutex;std::condition_variable changed;bool claimed=false,cancelled=false,block=false,entered=false;
    std::vector<uint8_t> packet;
    void(*onClose)(void*)=nullptr;void* closeContext=nullptr;
    bool open(const DeviceIdentity&) override{std::lock_guard guard(mutex);if(claimed)return false;claimed=true;cancelled=false;return true;}
    IoResult read(void* out,uint32_t capacity,uint32_t& copied) override{
        std::unique_lock guard(mutex);entered=true;changed.notify_all();if(block)changed.wait(guard,[&](){return cancelled;});
        copied=0;if(cancelled)return IoResult::Cancelled;if(packet.empty())return IoResult::NoData;
        if(capacity<packet.size())return IoResult::TooSmall;std::memcpy(out,packet.data(),packet.size());copied=static_cast<uint32_t>(packet.size());packet.clear();return IoResult::Success;
    }
    bool info(bridge::Info& out) override {out={};out.header={bridge::Magic,bridge::Version,sizeof(out),2};std::memcpy(out.interfaceGuid,bridge::InterfaceGuid,16);out.driverBuild=1;return true;}
    void cancel() override{std::lock_guard guard(mutex);cancelled=true;changed.notify_all();}
    void close() override{std::lock_guard guard(mutex);if(onClose)onClose(closeContext);claimed=false;}
    void make(){bridge::Packet p{{bridge::Magic,bridge::Version,176,1},format(96000,32),{7,9,42,123456,10000000,0,2,RawIdentity,0,0},16,0,0};packet.resize(176);std::memcpy(packet.data(),&p,sizeof(p));for(size_t i=160;i<176;++i)packet[i]=static_cast<uint8_t>(i);}
};
void ProviderAbiExactRaw(){
    CHECK(sizeof(EntropyIngressFormatV2)==64 && sizeof(EntropyIngressPacketV2)==160 && sizeof(EntropyProviderInfoV2)==256 && sizeof(EntropyEndpointInfoV2)==1376 && sizeof(EntropyProviderApiV2)==336);
    FakeBridge fake;fake.make();BridgeClient client(fake);CHECK(client.open(identity()));
    uint8_t out[32]{};EntropyIngressPacketV2 p{};CHECK(client.read(p,out,32)==1);
    CHECK(p.abiVersion==ENTROPY_PROVIDER_ABI_V2 && p.format.sampleRate==96000 && p.format.containerBits==32 && p.format.validBits==24);
    CHECK(p.generation==7 && p.packetSequence==9 && p.firstSourceFrame==42 && p.qpcTimestamp==123456);
    CHECK(p.flags==ENTROPY_PACKET_RAW_IDENTITY && p.mode==ENTROPY_MODE_UNKNOWN && p.originalContainerBits==32 && p.originalValidBits==24);
    CHECK(!p.reserved[0] && !p.reserved[1]);for(unsigned i=0;i<16;++i)CHECK(out[i]==160+i);
}
void ReadTooSmallDoesNotConsume(){FakeBridge fake;fake.make();BridgeClient client(fake);CHECK(client.open(identity()));uint8_t out[16]{};EntropyIngressPacketV2 p{};CHECK(client.read(p,out,15)==-1);CHECK(fake.packet.size()==176);CHECK(client.read(p,out,16)==1);}
void CloseCancelsPendingRead(){
    FakeBridge fake;fake.block=true;BridgeClient client(fake);CHECK(client.open(identity()));int32_t result=123;
    std::thread reader([&](){uint8_t out[16]{};EntropyIngressPacketV2 p{};result=client.read(p,out,16);});
    {std::unique_lock lock(fake.mutex);fake.changed.wait(lock,[&](){return fake.entered;});}
    client.close();reader.join();CHECK(result==0 && !fake.claimed);
}
void SecondReaderRefused(){FakeBridge fake;BridgeClient first(fake),second(fake);CHECK(first.open(identity()));CHECK(!second.open(identity()));first.close();CHECK(second.open(identity()));}
void WrongDeviceRejected(){
    CHECK(validIdentity(identity()));FakeBridge fake;for(unsigned bad=0;bad<6;++bad){auto d=identity();switch(bad){case 0:d.hardwareId=L"ROOT\\EntropyAudioVirtual";break;case 1:d.service=L"EntropyMusicTransport";break;case 2:d.interfaceGuid[0]^=1;break;case 3:d.containerId[1]=1;break;case 4:d.flow=1;break;case 5:d.state=2;break;}BridgeClient client(fake);CHECK(!client.open(d));}
}
void AccessModeDoesNotInventWasapiMode(){FakeBridge fake;fake.make();BridgeClient client(fake);CHECK(client.open(identity()));uint8_t out[16]{};EntropyIngressPacketV2 p{};CHECK(client.read(p,out,16)==1);CHECK(p.mode==0 && !(p.flags&ENTROPY_PACKET_METADATA_PARTIAL));}
void CloseWaitsUntilReaderMutexReleased(){
    struct Barrier {std::mutex lock;std::condition_variable cv;bool entered=false,release=false;};Barrier b;FakeBridge fake;fake.make();auto client=std::make_unique<BridgeClient>(fake);CHECK(client->open(identity()));
    BridgeClientTestProbe::barrier(*client,[](void* context){auto& p=*static_cast<Barrier*>(context);std::unique_lock guard(p.lock);p.entered=true;p.cv.notify_all();p.cv.wait(guard,[&](){return p.release;});},&b);
    struct CloseCheck {BridgeClient* client;bool released=false;} check{client.get()};fake.closeContext=&check;fake.onClose=[](void* context){auto& p=*static_cast<CloseCheck*>(context);p.released=BridgeClientTestProbe::readerReleased(*p.client);};
    auto pointer=client.get();std::thread reader([&](){uint8_t bytes[16]{};EntropyIngressPacketV2 p{};pointer->read(p,bytes,16);});{std::unique_lock guard(b.lock);b.cv.wait(guard,[&](){return b.entered;});}
    client.reset();{std::lock_guard guard(b.lock);b.release=true;b.cv.notify_all();}reader.join();CHECK(check.released);
}
int main(){const Case tests[]={{"ProviderAbiExactRaw",ProviderAbiExactRaw},{"ReadTooSmallDoesNotConsume",ReadTooSmallDoesNotConsume},{"CloseCancelsPendingRead",CloseCancelsPendingRead},{"SecondReaderRefused",SecondReaderRefused},{"WrongDeviceRejected",WrongDeviceRejected},{"AccessModeDoesNotInventWasapiMode",AccessModeDoesNotInventWasapiMode},{"CloseWaitsUntilReaderMutexReleased",CloseWaitsUntilReaderMutexReleased}};return runTests(tests,7);}
