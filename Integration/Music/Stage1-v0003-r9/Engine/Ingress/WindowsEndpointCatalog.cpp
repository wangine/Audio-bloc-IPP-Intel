#include "WindowsEndpointCatalog.h"
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
namespace entropy::music {
std::string utf8(const wchar_t *s) {
    if (!s)
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if (n < 1)
        return {};
    std::string v(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, v.data(), n, nullptr, nullptr);
    v.pop_back();
    return v;
}
std::wstring wide(const std::string &s) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), -1, nullptr, 0);
    if (n < 1)
        return {};
    std::wstring v(n, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), -1, v.data(), n);
    v.pop_back();
    return v;
}
WindowsEndpointCatalog::WindowsEndpointCatalog() {
    if (SUCCEEDED(
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator_))))
        enumerator_->RegisterEndpointNotificationCallback(this);
}
WindowsEndpointCatalog::~WindowsEndpointCatalog() {
    if (enumerator_)
        enumerator_->UnregisterEndpointNotificationCallback(this);
}
HRESULT WindowsEndpointCatalog::QueryInterface(REFIID id, void **out) {
    if (!out)
        return E_POINTER;
    *out = nullptr;
    if (id == __uuidof(IUnknown) || id == __uuidof(IMMNotificationClient)) {
        *out = static_cast<IMMNotificationClient *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}
std::vector<WindowsEndpoint> WindowsEndpointCatalog::enumerate() {
    std::vector<WindowsEndpoint> result;
    if (!enumerator_)
        return result;
    for (auto flow : {eRender, eCapture}) {
        Microsoft::WRL::ComPtr<IMMDeviceCollection> list;
        if (FAILED(enumerator_->EnumAudioEndpoints(flow, DEVICE_STATEMASK_ALL, &list)))
            continue;
        UINT count = 0;
        list->GetCount(&count);
        for (UINT i = 0; i < count; i++) {
            Microsoft::WRL::ComPtr<IMMDevice> device;
            if (FAILED(list->Item(i, &device)))
                continue;
            LPWSTR id = nullptr;
            DWORD state = 0;
            device->GetState(&state);
            if (FAILED(device->GetId(&id)))
                continue;
            WindowsEndpoint e{utf8(id),
                              {},
                              static_cast<uint32_t>(flow),
                              state,
                              flow == eCapture && state == DEVICE_STATE_ACTIVE};
            CoTaskMemFree(id);
            Microsoft::WRL::ComPtr<IPropertyStore> props;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                PROPVARIANT v;
                PropVariantInit(&v);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR)
                    e.name = utf8(v.pwszVal);
                PropVariantClear(&v);
            }
            result.push_back(std::move(e));
        }
    }
    return result;
}
} // namespace entropy::music
