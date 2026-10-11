#pragma once
#include <entropy/stage_bus_v1.h>
#include <string>
namespace entropy::input {
enum class Domain { Music, Cinema };
bool validateSessionForDomain(const StageBusSessionHeader&,Domain) noexcept;
bool validateBlockForDomain(const StageBusSessionHeader&,const StageBusBlockHeader&,Domain) noexcept;
std::wstring defaultEndpoint(Domain);
bool localEndpoint(const std::wstring&) noexcept;
}
