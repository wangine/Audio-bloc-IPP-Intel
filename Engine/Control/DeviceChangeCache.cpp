#include "DeviceChangeCache.h"
#include <stdexcept>
namespace entropy::control {
DeviceChangeCache::~DeviceChangeCache(){close();}
void DeviceChangeCache::close() noexcept {if(enumerator_){enumerator_->UnregisterEndpointNotificationCallback(this);enumerator_->Release();enumerator_=nullptr;}}
HRESULT DeviceChangeCache::QueryInterface(REFIID id,void** result){
 if(!result)return E_POINTER;*result=nullptr;
 if(id==__uuidof(IUnknown)||id==__uuidof(IMMNotificationClient)){*result=static_cast<IMMNotificationClient*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
}
void DeviceChangeCache::ensure(){
 if(enumerator_)return;
 if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator_))))throw std::runtime_error("Device change monitor unavailable");
 if(FAILED(enumerator_->RegisterEndpointNotificationCallback(this))){enumerator_->Release();enumerator_=nullptr;throw std::runtime_error("Cannot register device invalidation");}
}
bool DeviceChangeCache::selectedActive(const std::vector<std::string>& endpoints){
 ensure();for(const auto& endpoint:endpoints){
  const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,endpoint.c_str(),-1,nullptr,0);if(!length)return false;
  std::wstring name(size_t(length),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,endpoint.c_str(),-1,name.data(),length);
  IMMDevice* device=nullptr;if(FAILED(enumerator_->GetDevice(name.c_str(),&device)))return false;
  DWORD state=0;const auto result=device->GetState(&state);device->Release();if(FAILED(result)||state!=DEVICE_STATE_ACTIVE)return false;
 }return true;
}
}
