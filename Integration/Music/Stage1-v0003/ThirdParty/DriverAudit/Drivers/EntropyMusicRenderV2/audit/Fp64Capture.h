#pragma once
#include <filesystem>
#include <string>
namespace entropy::music::audit {
int collectFp64(const std::wstring& session,const std::wstring& endpoint,uint32_t corePid,const std::filesystem::path& output,const std::filesystem::path& stop);
int verifyFp64(const std::filesystem::path& input,uint64_t frames,uint32_t bits,uint64_t generation,uint32_t corePid);
}
