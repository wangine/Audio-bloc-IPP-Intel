#pragma once

#include <windows.h>
#include <avrt.h>

namespace entropy::host
{
struct ProAudioApi
{
    HANDLE (WINAPI* registerTask)(LPCWSTR, LPDWORD);
    BOOL (WINAPI* setPriority)(HANDLE, AVRT_PRIORITY);
    BOOL (WINAPI* revertTask)(HANDLE);
};

ProAudioApi systemProAudioApi() noexcept;

class ProAudioThreadScope final
{
public:
    explicit ProAudioThreadScope(ProAudioApi api = systemProAudioApi()) noexcept;
    ~ProAudioThreadScope();

    ProAudioThreadScope(ProAudioThreadScope&& other) noexcept;
    ProAudioThreadScope& operator=(ProAudioThreadScope&& other) noexcept;
    ProAudioThreadScope(const ProAudioThreadScope&) = delete;
    ProAudioThreadScope& operator=(const ProAudioThreadScope&) = delete;

    bool active() const noexcept { return handle_ != nullptr; }
    const char* failureReason() const noexcept { return failureReason_; }

private:
    void release() noexcept;

    ProAudioApi api_{};
    HANDLE handle_ = nullptr;
    const char* failureReason_ = "";
};
}
