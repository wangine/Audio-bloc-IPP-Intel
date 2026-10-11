#pragma once
#include <windows.h>
#include <string>
namespace entropy::link {
std::wstring endpointName();
class PipeSecurity {public:PipeSecurity();~PipeSecurity();bool valid() const noexcept{return descriptor_!=nullptr;}SECURITY_ATTRIBUTES* attributes() noexcept{return &attributes_;}private:PSECURITY_DESCRIPTOR descriptor_=nullptr;SECURITY_ATTRIBUTES attributes_{};};
bool transfer(HANDLE pipe,bool write,void* data,DWORD bytes,DWORD timeout,HANDLE stop=nullptr) noexcept;
}
