#pragma once
#include "ProgramIngress.h"
namespace entropy::input {
class ProgramSelection {
public:
 bool request(uint32_t program,const ProgramIngress&,std::string& error);
 void accepted(uint32_t p) noexcept {desired_=p;}
 void quiescent(uint32_t p) noexcept {desired_=p;applied_=p;}
 void afterBlock(bool settled,uint32_t actual=2) noexcept {auto target=desired_.load();if(settled&&(actual==2||actual==target))applied_=target;}
 uint32_t program() const noexcept {return desired_.load();}
 std::array<uint32_t,2> snapshot() const noexcept {return {desired_.load(),applied_.load()};}
 void restore(std::array<uint32_t,2> s) noexcept {desired_=s[0];applied_=s[1];}
 uint32_t requiredDomains() const noexcept {return (1u<<desired_.load())|(1u<<applied_.load());}
private: std::atomic<uint32_t> desired_{0},applied_{0};
};
}
