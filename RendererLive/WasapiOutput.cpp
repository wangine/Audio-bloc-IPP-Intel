#include "WasapiOutput.h"
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ks.h>
#include <ksmedia.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
namespace entropy::live {
namespace {
template<class T>struct Com {T* p=nullptr;~Com(){reset();}void reset(){if(p){p->Release();p=nullptr;}}T* operator->()const{return p;}};
void checked(HRESULT hr,const char* where){if(FAILED(hr)){std::ostringstream out;out<<where<<" HRESULT=0x"<<std::hex<<uint32_t(hr);throw std::runtime_error(out.str());}}
std::wstring wide(const std::string& value){
 if(value.empty()||value.size()>4096||value.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid exact endpoint");
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(!n)throw std::invalid_argument("Invalid endpoint UTF8");
 std::wstring out(size_t(n),L'\0');if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),n))throw std::runtime_error("Endpoint conversion");return out;
}
DWORD channelMask(unsigned n){switch(n){case 1:return 4;case 2:return 3;case 4:return 0x33;case 6:return 0x60f;case 8:return 0x63f;default:return 0;}}
REFERENCE_TIME duration(unsigned n){return REFERENCE_TIME((uint64_t(n)*10000000+96000)/192000);}
}
struct WasapiOutput::Impl {
 bool apartment=false,running=false;HANDLE event=nullptr;
 Com<IMMDeviceEnumerator> enumerator;Com<IMMDevice> device;Com<IAudioClient> client;Com<IAudioRenderClient> render;Com<IAudioClock> clock;
 WAVEFORMATEXTENSIBLE format{};DeviceObservation info{};
 ~Impl(){if(running&&client.p)client->Stop();clock.reset();render.reset();client.reset();device.reset();enumerator.reset();if(event)CloseHandle(event);if(apartment)CoUninitialize();}
 bool result(HRESULT hr) noexcept {info.lastResult=int32_t(hr);return SUCCEEDED(hr);}
 void clockObservation() noexcept {
  UINT64 position=0,qpc=0;info.clockValid=false;
  if(clock.p&&info.clockFrequency&&SUCCEEDED(clock->GetPosition(&position,&qpc))){
   const long double frames=static_cast<long double>(position)*192000.0L/static_cast<long double>(info.clockFrequency);
   if(frames>=0&&frames<=static_cast<long double>(UINT64_MAX)){info.position=uint64_t(frames);info.qpc100ns=qpc;info.clockValid=true;}
  }
 }
};
WasapiOutput::WasapiOutput():impl_(std::make_unique<Impl>()){}
WasapiOutput::~WasapiOutput()=default;
void WasapiOutput::open(const DeviceConfig& config){
 auto& s=*impl_;if(s.client.p)throw std::logic_error("Device already open");
 if(!config.channels||config.channels>16||config.format>1||!config.requestedFrames||config.requestedFrames>4096)throw std::invalid_argument("Unsupported output contract");
 const auto apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);checked(apartment,"COM owner");s.apartment=true;
 checked(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&s.enumerator.p)),"Device enumerator");
 const auto endpoint=wide(config.endpoint);checked(s.enumerator->GetDevice(endpoint.c_str(),&s.device.p),"Exact saved endpoint");
 auto activate=[&]{checked(s.device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&s.client.p)),"AudioClient");};activate();
 auto& f=s.format;f.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;f.Format.nChannels=WORD(config.channels);f.Format.nSamplesPerSec=192000;
 f.Format.wBitsPerSample=WORD(config.format?32:24);f.Format.nBlockAlign=WORD(config.channels*(config.format?4:3));f.Format.nAvgBytesPerSec=192000*f.Format.nBlockAlign;
 f.Format.cbSize=sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);f.Samples.wValidBitsPerSample=24;f.dwChannelMask=channelMask(config.channels);f.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
 if(s.client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,&f.Format,nullptr)!=S_OK)throw std::runtime_error("Exact saved PCM24 format unsupported; no fallback");
 REFERENCE_TIME standard=0,minimum=0;checked(s.client->GetDevicePeriod(&standard,&minimum),"Device period");
 auto period=(std::max)(minimum,duration(config.requestedFrames));
 auto initialize=[&]{return s.client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_NOPERSIST,period,period,&f.Format,nullptr);};
 auto hr=initialize();
 if(hr==AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED){
  UINT32 aligned=0;checked(s.client->GetBufferSize(&aligned),"Aligned buffer size");if(!aligned||aligned>4096)throw std::runtime_error("Aligned buffer exceeds prepared capacity");
  s.client.reset();activate();period=duration(aligned);hr=initialize(); // documented one-time alignment retry; same format and endpoint
 }
 checked(hr,"Exclusive event Initialize");
 s.event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!s.event)throw std::runtime_error("Audio event creation failed");
 checked(s.client->SetEventHandle(s.event),"Set event");UINT32 quantum=0;checked(s.client->GetBufferSize(&quantum),"Negotiated quantum");
 if(!quantum||quantum>4096)throw std::runtime_error("Negotiated quantum exceeds prepared capacity");s.info.quantum=quantum;
 checked(s.client->GetService(__uuidof(IAudioRenderClient),reinterpret_cast<void**>(&s.render.p)),"Render service");
 if(SUCCEEDED(s.client->GetService(__uuidof(IAudioClock),reinterpret_cast<void**>(&s.clock.p)))){UINT64 hz=0;if(SUCCEEDED(s.clock->GetFrequency(&hz)))s.info.clockFrequency=hz;}
}
bool WasapiOutput::start() noexcept {auto& s=*impl_;if(!s.client.p||s.running)return false;if(!s.result(s.client->Start()))return false;s.running=true;return true;}
WasapiOutput::Wait WasapiOutput::wait(void* cancel,unsigned milliseconds) noexcept {
 auto& s=*impl_;if(!s.running||!cancel)return Wait::Failed;const HANDLE events[]={HANDLE(cancel),s.event};
 const auto result=WaitForMultipleObjects(2,events,FALSE,milliseconds);
 if(result==WAIT_OBJECT_0)return Wait::Canceled;
 if(result==WAIT_OBJECT_0+1){s.clockObservation();return Wait::Ready;}
 if(result==WAIT_TIMEOUT){++s.info.timeouts;s.info.lastResult=int32_t(HRESULT_FROM_WIN32(WAIT_TIMEOUT));}
 else s.info.lastResult=int32_t(HRESULT_FROM_WIN32(GetLastError()));return Wait::Failed;
}
bool WasapiOutput::submit(const uint8_t* data,unsigned frames,unsigned bytes) noexcept {
 auto& s=*impl_;if(!s.render.p||!data||frames!=s.info.quantum||bytes!=frames*s.format.Format.nBlockAlign)return false;
 BYTE* destination=nullptr;if(!s.result(s.render->GetBuffer(frames,&destination)))return false;
 if(!destination){s.render->ReleaseBuffer(frames,AUDCLNT_BUFFERFLAGS_SILENT);s.info.lastResult=int32_t(E_POINTER);return false;}
 std::memcpy(destination,data,bytes);if(!s.result(s.render->ReleaseBuffer(frames,0)))return false;
 s.info.submitted+=frames;return true;
}
void WasapiOutput::stop() noexcept {auto& s=*impl_;if(s.running&&s.client.p){s.result(s.client->Stop());s.running=false;}}
DeviceObservation WasapiOutput::observation() const noexcept {return impl_->info;}
}
