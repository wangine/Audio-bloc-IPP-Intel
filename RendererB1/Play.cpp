// Independent finite-file renderer. No Entropy DSP/ClockSync/ASRC/IPC library links.
#include "CoreFile.h"
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
namespace {
void check(HRESULT hr,const char* operation){if(FAILED(hr)){std::ostringstream s;s<<operation<<" HRESULT=0x"<<std::hex<<uint32_t(hr);throw std::runtime_error(s.str());}}
template<class T>struct Com {
 T* p=nullptr;~Com(){if(p)p->Release();}T* operator->()const{return p;}
 Com()=default;Com(const Com&)=delete;Com& operator=(const Com&)=delete;
};
struct Handle {HANDLE p=nullptr;~Handle(){if(p&&p!=INVALID_HANDLE_VALUE)CloseHandle(p);}Handle()=default;Handle(const Handle&)=delete;};
struct Apartment {Apartment(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"CoInitializeEx");}~Apartment(){CoUninitialize();}};
struct Priority {
 HANDLE task=nullptr;Priority(){DWORD id=0;task=AvSetMmThreadCharacteristicsW(L"Pro Audio",&id);if(!task)throw std::runtime_error("MMCSS registration failed");if(!AvSetMmThreadPriority(task,AVRT_PRIORITY_CRITICAL)){AvRevertMmThreadCharacteristics(task);task=nullptr;throw std::runtime_error("MMCSS priority failed");}}
 ~Priority(){if(task)AvRevertMmThreadCharacteristics(task);}
};
HANDLE cancelEvent=nullptr;
BOOL WINAPI onConsole(DWORD code){if(code==CTRL_C_EVENT||code==CTRL_BREAK_EVENT||code==CTRL_CLOSE_EVENT){if(cancelEvent)SetEvent(cancelEvent);return TRUE;}return FALSE;}
struct ConsoleCancel {
 Handle event;ConsoleCancel(){event.p=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event.p)throw std::runtime_error("Cancel event failed");cancelEvent=event.p;if(!SetConsoleCtrlHandler(onConsole,TRUE)){cancelEvent=nullptr;throw std::runtime_error("Console handler failed");}}
 ~ConsoleCancel(){SetConsoleCtrlHandler(onConsole,FALSE);cancelEvent=nullptr;}
};
struct StopOnExit {IAudioClient* client;bool running=false;~StopOnExit(){if(running)client->Stop();}};
unsigned parseUnsigned(const std::wstring& value){size_t used=0;const auto n=std::stoul(value,&used);if(used!=value.size()||n>65536)throw std::runtime_error("Invalid buffer size");return unsigned(n);}
}
int wmain(int argc,wchar_t** argv) try {
 std::filesystem::path file;std::wstring endpoint;unsigned requested=1024;bool authorized=false;
 for(int i=1;i<argc;++i){std::wstring a=argv[i];
  if(a==L"--pcm"&&i+1<argc)file=argv[++i];
  else if(a==L"--endpoint"&&i+1<argc)endpoint=argv[++i];
  else if(a==L"--buffer"&&i+1<argc)requested=parseUnsigned(argv[++i]);
  else if(a==L"--authorized-manual-play")authorized=true;
  else throw std::runtime_error("Usage: --pcm FILE --endpoint EXACT_ID [--buffer 1024] --authorized-manual-play");
 }
 if(!authorized||file.empty()||endpoint.empty()||!requested)throw std::runtime_error("Manual authorization, PCM and exact endpoint required");
 std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("PCM file unavailable");
 const auto size=in.tellg();b1::Header h;in.seekg(0);if(!in.read(reinterpret_cast<char*>(&h),sizeof(h)))throw std::runtime_error("Truncated PCM header");
 const auto bytes=b1::payloadBytes(h);if(size!=std::streamoff(bytes+sizeof(h)))throw std::runtime_error("PCM file size mismatch");
 std::vector<uint8_t> pcm(static_cast<size_t>(bytes));if(!in.read(reinterpret_cast<char*>(pcm.data()),std::streamsize(bytes)))throw std::runtime_error("PCM read failed");in.close();
 if(b1::hash(pcm)!=h.payloadHash)throw std::runtime_error("PCM checksum mismatch");
 if(h.containerBits==32)for(size_t n=0;n<pcm.size();n+=4)if(pcm[n])throw std::runtime_error("PCM24-in-32 must have zero low byte");
 std::wcout<<L"B1 MEMORY RENDERER: ONE DAC ONLY, no ClockSync/ASRC/FIFO\nEndpoint: "<<endpoint<<L"\nFile: "<<file.wstring()<<L"\n";
 std::cout<<h.channels<<" channels, 24 valid bits / "<<h.containerBits<<" container, 192000 Hz; "<<h.frames<<" frames.\n"
          <<"Keep the verified profile's speaker wiring and a safe amplifier level.\nType ARM to start; Ctrl+C stops this player only: "<<std::flush;
 std::string answer;std::getline(std::cin,answer);if(answer!="ARM"){std::cout<<"No DAC opened.\n";return 0;}
 Handle owner;owner.p=CreateMutexW(nullptr,FALSE,L"Local\\Entropy.Stage3.Render.Owner.V1");
 const auto ownerResult=GetLastError();if(!owner.p||ownerResult==ERROR_ALREADY_EXISTS)throw std::runtime_error("Another Stage3/B1 renderer owns output; stop it manually first");
 ConsoleCancel cancel;Apartment apartment;Com<IMMDeviceEnumerator> enumerator;Com<IMMDevice> device;Com<IAudioClient> client;Com<IAudioRenderClient> render;Com<IAudioClock> clock;
 check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&enumerator.p)),"Enumerator");
 check(enumerator->GetDevice(endpoint.c_str(),&device.p),"Exact endpoint");
 check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&client.p)),"Activate");
 WAVEFORMATEXTENSIBLE format{};format.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;format.Format.nChannels=WORD(h.channels);
 format.Format.nSamplesPerSec=h.rate;format.Format.wBitsPerSample=WORD(h.containerBits);format.Format.nBlockAlign=WORD(h.channels*(h.containerBits/8));
 format.Format.nAvgBytesPerSec=h.rate*format.Format.nBlockAlign;format.Format.cbSize=sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
 format.Samples.wValidBitsPerSample=24;format.dwChannelMask=h.channelMask;format.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
 WAVEFORMATEX* closest=nullptr;const auto supported=client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,&format.Format,&closest);if(closest)CoTaskMemFree(closest);
 if(supported!=S_OK)throw std::runtime_error("Exact PCM format unsupported; no shared-mode/format fallback");
 REFERENCE_TIME def=0,minimum=0;check(client->GetDevicePeriod(&def,&minimum),"Device period");
 const auto period=std::max<REFERENCE_TIME>(minimum,REFERENCE_TIME((uint64_t(requested)*10000000+h.rate-1)/h.rate));
 Handle ready;ready.p=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ready.p)throw std::runtime_error("Audio event failed");
 const auto initialized=client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_NOPERSIST,period,period,&format.Format,nullptr);
 if(initialized==AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED){UINT32 aligned=0;client->GetBufferSize(&aligned);std::ostringstream s;s<<"Driver requires aligned buffer "<<aligned<<" frames; no automatic reconfigure";throw std::runtime_error(s.str());}
 check(initialized,"Exclusive Initialize");check(client->SetEventHandle(ready.p),"SetEventHandle");
 UINT32 quantum=0;check(client->GetBufferSize(&quantum),"GetBufferSize");if(!quantum||quantum>65536)throw std::runtime_error("Invalid negotiated buffer");
 check(client->GetService(__uuidof(IAudioRenderClient),reinterpret_cast<void**>(&render.p)),"Render service");
 check(client->GetService(__uuidof(IAudioClock),reinterpret_cast<void**>(&clock.p)),"Clock service");
 UINT64 frequency=0;check(clock->GetFrequency(&frequency),"Clock frequency");if(!frequency)throw std::runtime_error("Zero clock frequency");
 std::cout<<"Negotiated buffer: "<<quantum<<" frames. PCM is loaded and verified in RAM.\n"<<std::flush;
 b1::Cursor cursor(pcm,format.Format.nBlockAlign);const size_t packetBytes=size_t(quantum)*format.Format.nBlockAlign;
 uint64_t submissions=0;auto submit=[&]{
  BYTE* destination=nullptr;check(render->GetBuffer(quantum,&destination),"GetBuffer");
  if(!destination||!cursor.stage({destination,packetBytes},quantum)){render->ReleaseBuffer(quantum,AUDCLNT_BUFFERFLAGS_SILENT);throw std::runtime_error("Packet staging failed");}
  check(render->ReleaseBuffer(quantum,0),"ReleaseBuffer");
  if(!cursor.commit(quantum))throw std::runtime_error("Cursor commit failed");++submissions;
 };
 bool canceled=false;
 {Priority priority;StopOnExit stop{client.p};submit();check(client->Start(),"Start");stop.running=true;
  uint64_t drainStart=0;const HANDLE events[]={cancel.event.p,ready.p};
  for(;;){const auto wait=WaitForMultipleObjects(2,events,FALSE,2000);
   if(wait==WAIT_OBJECT_0){canceled=true;break;}
   if(wait!=WAIT_OBJECT_0+1)throw std::runtime_error(wait==WAIT_TIMEOUT?"Audio event timeout; stopped without recovery/drop/repeat":"Audio event wait failed");
   if(cursor.done()){
    if(!drainStart)drainStart=GetTickCount64();UINT64 p=0;check(clock->GetPosition(&p,nullptr),"Drain position");
    if(static_cast<long double>(p)*h.rate/static_cast<long double>(frequency)>=h.frames)break;
    if(GetTickCount64()-drainStart>5000)throw std::runtime_error("Drain clock stalled");
   }
   submit();
  }
  check(client->Stop(),"Stop");stop.running=false;
 }
 std::cout<<(canceled?"MANUAL STOP":"FILE COMPLETED")<<"; sourceFrames="<<cursor.position()<<"; submissions="<<submissions
          <<". Successful submissions do not certify analog output.\n";return canceled?2:0;
}catch(const std::exception& e){std::cerr<<"B1 STOP: "<<e.what()<<'\n';return 1;}
