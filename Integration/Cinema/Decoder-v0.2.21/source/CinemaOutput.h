#pragma once
#include "CinemaWdlSrc.h"
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
class CinemaBlockProcessor {
public:
 using Callback=void(*)(void*,const double* const*);
 bool prepare(unsigned channels);
 void reset() noexcept;
 bool push(const std::array<float,8>&,Callback,void*) noexcept;
 double latencySeconds() noexcept {return src_.latencySeconds();}
private:
 entropy::host::CinemaWdlSrc src_;
 unsigned channels_=0,fill_=0,assembled_=0;
 std::array<std::array<double,256>,8> input_{};
 std::array<std::array<double,1024>,8> block_{};
 std::array<std::vector<double>,8> scratch_;
};
class CinemaOutput {
public:
 CinemaOutput();~CinemaOutput();
 bool start(unsigned channels,const std::wstring& endpoint,std::string& error);
 void submit(const std::array<float,8>&) noexcept;
 void submit(const std::array<float,8>& frame,uint64_t generation) noexcept;
 uint64_t generation() const noexcept;
 void invalidate() noexcept;
 void suspend() noexcept; // Idempotent idle polling; not a stream boundary.
 void stop() noexcept;
 uint64_t dropped() const noexcept;
 uint64_t blocks() const noexcept;
 bool schedulingFailed() const noexcept;
 static std::wstring defaultEndpoint();
private:
 struct Impl;std::unique_ptr<Impl> impl_;
};
