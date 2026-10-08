#pragma once
#include <filesystem>
#include <string>
namespace entropy::egress_host {
std::string fileSha256(const std::filesystem::path&);
bool offlineGateMatches(const std::filesystem::path& gate,const std::string& executableHash);
int hardwareTest(int argc,char** argv);
}
