// Read-only diagnostic of the existing AUX shared render path.
// Emits 0.001 peak (-60 dBFS) stereo for 12 seconds; optional positive control.
// No endpoint settings, driver, Stage1, Stage2 or Stage3 code is changed.
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <avrt.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstdint>
using Microsoft::WRL::ComPtr;
static void check(HRESULT h){if(FAILED(h))throw h;}
struct Stats {
    uint64_t frames{}; double l{},r{},m{},s{};
    void add(double a,double b){++frames;l+=a*a;r+=b*b;m+=(a+b)*(a+b)*.25;s+=(a-b)*(a-b)*.25;}
};
int wmain(int argc,wchar_t** argv){
    if(argc!=2 && argc!=3)return 2;
    const bool leftOnly=argc==3 && wcscmp(argv[2],L"left-only")==0;
    if(argc==3 && !leftOnly)return 2;
    HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(apartment))return 3;
    try {
        ComPtr<IMMDeviceEnumerator> enumeration;ComPtr<IMMDevice> device;
        check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumeration)));
        check(enumeration->GetDevice(argv[1],&device));
        ComPtr<IAudioClient> render,loopback;
        check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(render.GetAddressOf())));
        check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(loopback.GetAddressOf())));
        WAVEFORMATEX* mix=nullptr;check(render->GetMixFormat(&mix));
        WAVEFORMATEXTENSIBLE format{};
        const auto formatSize=sizeof(WAVEFORMATEX)+mix->cbSize;
        if(formatSize>sizeof(format)){CoTaskMemFree(mix);throw E_INVALIDARG;}
        memcpy(&format,mix,formatSize);CoTaskMemFree(mix);
        const auto& f=format.Format;
        if(f.nChannels!=2 || f.wBitsPerSample!=32 ||
           !(f.wFormatTag==WAVE_FORMAT_IEEE_FLOAT || (f.wFormatTag==WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(format.SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))))throw E_INVALIDARG;
        check(render->Initialize(AUDCLNT_SHAREMODE_SHARED,0,1000000,0,&f,nullptr));
        check(loopback->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,&f,nullptr));
        ComPtr<IAudioRenderClient> writer;ComPtr<IAudioCaptureClient> reader;
        check(render->GetService(IID_PPV_ARGS(&writer)));check(loopback->GetService(IID_PPV_ARGS(&reader)));
        UINT32 capacity{};check(render->GetBufferSize(&capacity));
        uint64_t emitted=0;auto fill=[&](UINT32 n){BYTE* b=nullptr;check(writer->GetBuffer(n,&b));auto p=reinterpret_cast<float*>(b);for(UINT32 i=0;i<n;++i){float a=static_cast<float>(.001*std::sin(6.283185307179586*997.0*(emitted+i)/f.nSamplesPerSec));p[2*i]=a;p[2*i+1]=leftOnly?0:-a;}check(writer->ReleaseBuffer(n,0));emitted+=n;};
        DWORD task{};HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
        if(!mmcss)throw E_FAIL;
        AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_HIGH);
        Stats measured;fill(capacity);check(loopback->Start());check(render->Start());
        const auto deadline=GetTickCount64()+12000;
        do {
            UINT32 padding{};check(render->GetCurrentPadding(&padding));if(padding>capacity)throw E_FAIL;
            if(padding<capacity)fill(capacity-padding);
            UINT32 next{};check(reader->GetNextPacketSize(&next));while(next){BYTE* b=nullptr;UINT32 n{};DWORD flags{};check(reader->GetBuffer(&b,&n,&flags,nullptr,nullptr));auto p=reinterpret_cast<const float*>(b);if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT))for(UINT32 i=0;i<n;++i)measured.add(p[2*i],p[2*i+1]);check(reader->ReleaseBuffer(n));check(reader->GetNextPacketSize(&next));}
            Sleep(2);
        } while(GetTickCount64()<deadline);
        render->Stop();loopback->Stop();AvRevertMmThreadCharacteristics(mmcss);
        if(!measured.frames)throw E_FAIL;
        std::cout<<"{\"leftOnly\":"<<(leftOnly?"true":"false")<<",\"sharedMixRate\":"<<f.nSamplesPerSec<<",\"emittedFrames\":"<<emitted<<",\"testPeak\":0.001,\"loopbackFrames\":"<<measured.frames<<",\"loopbackLrms\":"<<std::sqrt(measured.l/measured.frames)<<",\"loopbackRrms\":"<<std::sqrt(measured.r/measured.frames)<<",\"loopbackMidRms\":"<<std::sqrt(measured.m/measured.frames)<<",\"loopbackSideRms\":"<<std::sqrt(measured.s/measured.frames)<<"}\n";
        CoUninitialize();return 0;
    } catch(HRESULT h){std::cerr<<"HRESULT 0x"<<std::hex<<static_cast<uint32_t>(h)<<'\n';CoUninitialize();return 1;}
}
