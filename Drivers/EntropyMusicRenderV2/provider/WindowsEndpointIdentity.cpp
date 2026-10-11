#include "EndpointIdentity.h"
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace entropy::music::provider {
std::string utf8(const std::wstring& text){
    if(text.empty())return {};
    const auto bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if(!bytes)return {};std::string out(bytes,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),bytes,nullptr,nullptr);return out;
}
std::string endpointKey(const DeviceIdentity& d){return std::string(bridge::ProviderId)+":"+utf8(d.windowsEndpointId);}
namespace {
struct Apartment {HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~Apartment(){if(SUCCEEDED(result))CoUninitialize();}};
std::wstring property(HDEVINFO set,SP_DEVINFO_DATA& data,DWORD key){
    wchar_t buffer[512]{};DWORD type=0,size=0;
    if(!SetupDiGetDeviceRegistryPropertyW(set,&data,key,&type,reinterpret_cast<BYTE*>(buffer),sizeof(buffer),&size) || (type!=REG_SZ && type!=REG_MULTI_SZ))return {};
    if(!size || size>sizeof(buffer) || buffer[std::size(buffer)-1])return {};return buffer;
}
}
std::vector<DeviceIdentity> resolveEndpoints(){
    std::vector<DeviceIdentity> result;Apartment apartment;if(FAILED(apartment.result) && apartment.result!=RPC_E_CHANGED_MODE)return result;
    GUID guid;std::memcpy(&guid,bridge::InterfaceGuid,16);
    const auto set=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);if(set==INVALID_HANDLE_VALUE)return result;
    ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<IMMDeviceCollection> endpoints;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator))) || FAILED(enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&endpoints))){SetupDiDestroyDeviceInfoList(set);return result;}
    UINT count=0;endpoints->GetCount(&count);
    for(DWORD index=0;;++index) {
        SP_DEVICE_INTERFACE_DATA iface{sizeof(iface)};if(!SetupDiEnumDeviceInterfaces(set,nullptr,&guid,index,&iface))break;
        DWORD size=0;SetupDiGetDeviceInterfaceDetailW(set,&iface,nullptr,0,&size,nullptr);if(size<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size>65536)continue;
        std::vector<uint8_t> storage(size);auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());detail->cbSize=sizeof(*detail);SP_DEVINFO_DATA data{sizeof(data)};
        if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,size,nullptr,&data))continue;
        DeviceIdentity candidate;candidate.interfacePath=detail->DevicePath;candidate.hardwareId=property(set,data,SPDRP_HARDWAREID);candidate.service=property(set,data,SPDRP_SERVICE);std::memcpy(candidate.interfaceGuid.data(),bridge::InterfaceGuid,16);
        DEVPROPTYPE type=0;GUID container{};
        if(!SetupDiGetDevicePropertyW(set,&data,&DEVPKEY_Device_ContainerId,&type,reinterpret_cast<BYTE*>(&container),sizeof(container),nullptr,0) || type!=DEVPROP_TYPE_GUID)continue;
        std::memcpy(candidate.containerId.data(),&container,16);
        for(UINT endpoint=0;endpoint<count;++endpoint){
            ComPtr<IMMDevice> device;ComPtr<IPropertyStore> properties;if(FAILED(endpoints->Item(endpoint,&device)) || FAILED(device->OpenPropertyStore(STGM_READ,&properties)))continue;
            PROPVARIANT value{};PropVariantInit(&value);
            if(SUCCEEDED(properties->GetValue(PKEY_Device_ContainerId,&value)) && value.vt==VT_CLSID && value.puuid){
                std::memcpy(candidate.endpointContainerId.data(),value.puuid,16);
                LPWSTR id=nullptr;if(SUCCEEDED(device->GetId(&id))){candidate.windowsEndpointId=id;CoTaskMemFree(id);candidate.state=1;candidate.flow=0;if(validIdentity(candidate))result.push_back(candidate);}
            }
            PropVariantClear(&value);
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    // The product is a single endpoint. Ambiguous correlation is refused.
    if(result.size()!=1)result.clear();return result;
}
}
