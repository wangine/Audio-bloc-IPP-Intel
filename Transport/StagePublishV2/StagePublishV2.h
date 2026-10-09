#pragma once
#include <entropy/stage_bus_v1.h>
#include <cstdint>
#include <memory>
#include <string>

namespace entropy::publish {
// This endpoint conveys completed source blocks; it has no demand/lease operations.
std::wstring endpointName();
enum class ReadResult { Block, Empty, Gap, Invalid, Disconnected };

class Publisher {
public:
    Publisher();
    ~Publisher();
    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;
    bool start(const std::wstring& endpoint, std::string& error);
    void stop();
    // These three methods have one owner: the Stage2 processing/control worker.
    void setSession(const StageBusSessionHeader& session) noexcept;
    void service() noexcept;
    void publish(const StageBusBlockHeader& block, const double* const* planes) noexcept;
    std::uint64_t dropped() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class Subscriber {
public:
    Subscriber();
    ~Subscriber();
    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;
    bool connect(const std::wstring& endpoint, std::string& error);
    void close() noexcept;
    // Gap/Invalid are terminal locally. Reconnect creates an empty subscription.
    // Block copies exactly eleven planes of 1024 binary64 values.
    ReadResult read(StageBusBlockHeader& block, double* const* planes) noexcept;
    // Control/connector only: also checks publisher process liveness.
    void heartbeat(std::uint64_t qpc) noexcept;
    const StageBusSessionHeader& session() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace entropy::publish
