#pragma once
#include <entropy/lrc_aligner_abi.h>
#include <windows.h>
#include <filesystem>
#include <array>
#include <string>
namespace entropy::stage {
class DspLrcModule final {
public:
 DspLrcModule()=default;DspLrcModule(const DspLrcModule&)=delete;~DspLrcModule(){close();}
 bool prepare(const std::filesystem::path&,std::string&);
 bool configure(bool,const std::array<uint64_t,3>&,uint64_t) noexcept;
 bool process(const double*const*,double*const*) noexcept;
 bool reset() noexcept;
 void close() noexcept;
 bool loaded() const noexcept{return handle_!=nullptr;}
private:
 HMODULE library_=nullptr;EntropyModuleHandle handle_=nullptr;
 EntropyModuleApiV1 api_{};EntropyLrcAlignerApiV1 extension_{};
};
}
