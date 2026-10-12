#pragma once
#include "StageBusValidation.h"
#include <filesystem>
#include <string>
namespace entropy::stage {
struct DspSessionDiscovery {
 static bool readExternal(const std::filesystem::path&,StageBusSessionHeader&,std::string&);
 static bool write(const std::filesystem::path&,const StageBusSessionHeader&,const char* owner,std::string&,const std::wstring& publishEndpoint={});
};
}
