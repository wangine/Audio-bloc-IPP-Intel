#include "Core/ProAudioThreadScope.h"

#include <utility>

namespace entropy::host
{
ProAudioApi systemProAudioApi() noexcept
{
    return {AvSetMmThreadCharacteristicsW,
            AvSetMmThreadPriority,
            AvRevertMmThreadCharacteristics};
}

ProAudioThreadScope::ProAudioThreadScope(ProAudioApi api) noexcept
    : api_(api)
{
    if (api_.registerTask == nullptr || api_.setPriority == nullptr ||
        api_.revertTask == nullptr)
    {
        failureReason_ = "MMCSS Pro Audio API is incomplete";
        return;
    }

    DWORD taskIndex = 0;
    handle_ = api_.registerTask(L"Pro Audio", &taskIndex);
    if (handle_ == nullptr)
    {
        failureReason_ = "MMCSS Pro Audio register failed";
        return;
    }

    if (!api_.setPriority(handle_, AVRT_PRIORITY_CRITICAL))
    {
        failureReason_ = "MMCSS Pro Audio priority failed";
        release();
    }
}

ProAudioThreadScope::~ProAudioThreadScope()
{
    release();
}

ProAudioThreadScope::ProAudioThreadScope(ProAudioThreadScope&& other) noexcept
    : api_(other.api_),
      handle_(std::exchange(other.handle_, nullptr)),
      failureReason_(other.failureReason_)
{
    other.failureReason_ = "";
}

ProAudioThreadScope& ProAudioThreadScope::operator=(
    ProAudioThreadScope&& other) noexcept
{
    if (this == &other) return *this;
    release();
    api_ = other.api_;
    handle_ = std::exchange(other.handle_, nullptr);
    failureReason_ = other.failureReason_;
    other.failureReason_ = "";
    return *this;
}

void ProAudioThreadScope::release() noexcept
{
    if (handle_ == nullptr) return;
    api_.revertTask(handle_);
    handle_ = nullptr;
}
}
