#pragma once

namespace entropy::host
{

inline constexpr wchar_t engineShutdownEventName[] =
    L"Local\\Entropy.Audio.Alpha01.EngineShutdown.v1";
inline constexpr wchar_t hostShowEventName[] =
    L"Local\\Entropy.Audio.Alpha01.HostShow.v1";

enum class LifecycleEvent
{
    engineShutdown,
    hostShow
};

class LifecycleEventOwner final
{
public:
    explicit LifecycleEventOwner(LifecycleEvent event) noexcept;
    explicit LifecycleEventOwner(const wchar_t* isolatedEventName) noexcept;
    ~LifecycleEventOwner();

    LifecycleEventOwner(const LifecycleEventOwner&) = delete;
    LifecycleEventOwner& operator=(const LifecycleEventOwner&) = delete;

    LifecycleEventOwner(LifecycleEventOwner&& other) noexcept;
    LifecycleEventOwner& operator=(LifecycleEventOwner&& other) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool signaled() const noexcept;
    bool reset() noexcept;

private:
    void* handle_ = nullptr;
};

[[nodiscard]] bool requestEngineShutdown() noexcept;
[[nodiscard]] bool requestExistingHostShow() noexcept;
[[nodiscard]] bool engineShutdownRequested() noexcept;

} // namespace entropy::host
