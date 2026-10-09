#include "StartupProfile.h"
#include <windows.h>
#include <shlobj.h>
#include <stdexcept>
namespace entropy::control {
std::filesystem::path motherProfilePath(){
    PWSTR folder=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,KF_FLAG_DONT_VERIFY,nullptr,&folder)))throw std::runtime_error("Cannot resolve Windows Documents folder");
    std::filesystem::path path(folder);CoTaskMemFree(folder);return path/L"entropy.entropy";
}
std::string profilePathUtf8(const std::filesystem::path& path){auto bytes=path.u8string();return {bytes.begin(),bytes.end()};}
}
