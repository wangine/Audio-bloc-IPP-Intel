#include "Configuration.h"
#include "WindowsEndpointCatalog.h"
namespace entropy::music {
Configuration Configuration::load(const std::filesystem::path &path) {
    Configuration c;
    c.path = std::filesystem::absolute(path);
    for (unsigned i = 0; i < 2; i++) {
        auto section = i ? L"aux" : L"music";
        wchar_t value[1024]{};
        auto read = [&](const wchar_t *key) {
            GetPrivateProfileStringW(section, key, L"", value, 1024, c.path.c_str());
            return utf8(value);
        };
        c.sources[i].key.providerId = read(L"providerId");
        c.sources[i].key.endpointId = read(L"endpointId");
        c.sources[i].mode = GetPrivateProfileIntW(section, L"mode", 1, c.path.c_str());
    }
    return c;
}
bool Configuration::save() const {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return false;
    for (unsigned i = 0; i < 2; i++) {
        auto section = i ? L"aux" : L"music";
        auto &s = sources[i];
        if (!WritePrivateProfileStringW(section, L"providerId", wide(s.key.providerId).c_str(),
                                        path.c_str()) ||
            !WritePrivateProfileStringW(section, L"endpointId", wide(s.key.endpointId).c_str(),
                                        path.c_str()) ||
            !WritePrivateProfileStringW(section, L"mode", std::to_wstring(s.mode).c_str(), path.c_str()))
            return false;
    }
    return true;
}
} // namespace entropy::music
