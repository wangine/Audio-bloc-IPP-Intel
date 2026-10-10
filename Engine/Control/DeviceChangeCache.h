#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <mmdeviceapi.h>
namespace entropy::control {
// Notifications invalidate evidence; the callback performs no scan or audio work.
// The preparation owner alone queries COM endpoints and rebuilds evidence.
class DeviceChangeCache final : public IMMNotificationClient {
public:
 ~DeviceChangeCache();
 void ensure();void close() noexcept;
 uint64_t generation() const noexcept {return generation_.load();}
 bool selectedActive(const std::vector<std::string>&);
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override;
 ULONG STDMETHODCALLTYPE AddRef() override {return refs_.fetch_add(1)+1;}
 ULONG STDMETHODCALLTYPE Release() override {return refs_.fetch_sub(1)-1;}
 HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD) override {return changed();}
 HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {return changed();}
 HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {return changed();}
 HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow,ERole,LPCWSTR) override {return changed();}
 HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY) override {return changed();}
private:
 HRESULT changed() noexcept {generation_.fetch_add(1);return S_OK;}
 std::atomic<ULONG> refs_{1};std::atomic<uint64_t> generation_{1};IMMDeviceEnumerator* enumerator_=nullptr;
};
}
