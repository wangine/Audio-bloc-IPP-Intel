#pragma once
#include <atomic>
#include <mmdeviceapi.h>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl/client.h>
namespace entropy::music {
struct WindowsEndpoint {
    std::string id, name;
    uint32_t flow, state;
    bool captureEligible;
};
class WindowsEndpointCatalog final : public IMMNotificationClient {
    std::atomic<ULONG> references_{1};
    std::atomic<bool> dirty_{true};
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;

  public:
    WindowsEndpointCatalog();
    ~WindowsEndpointCatalog();
    std::vector<WindowsEndpoint> enumerate();
    bool changed() { return dirty_.exchange(false); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **) override;
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { return --references_; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
        dirty_ = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
        dirty_ = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        dirty_ = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override {
        dirty_ = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        dirty_ = true;
        return S_OK;
    }
};
std::string utf8(const wchar_t *);
std::wstring wide(const std::string &);
} // namespace entropy::music
