#include "SourceSelectionController.h"
namespace entropy::host
{
bool SourceSelectionController::selectMusic(const InputSourceKey &key,
                                            std::string &error)
{
    error.clear();
    if (!SourceStartupPolicy::permitted(key))
    {
        error = "Unsupported exact MUSIC identity/access; no fallback";
        return false;
    }
    const auto match = findInputSource(catalog_, key);
    if (match.status != InputSourceFindStatus::found)
    {
        error = match.status == InputSourceFindStatus::ambiguous ? "MUSIC identity ambiguous"
                : match.status == InputSourceFindStatus::absent  ? "MUSIC provider absent"
                                                                 : "MUSIC identity incompatible";
        return false;
    }
    if (!match.entry || !match.entry->selectable)
    {
        error = "Exact MUSIC endpoint unavailable; waiting without fallback";
        return false;
    }
    if (leftChannel == rightChannel || leftChannel > 63 || rightChannel > 63 ||
        (key == SourceStartupPolicy::musicKey() && (leftChannel != 0 || rightChannel != 1)))
    {
        error = "MUSIC channels incompatible with exact provider; Transport requires L=0/R=1";
        return false;
    }
    if (!domain_.clearMusic(std::chrono::milliseconds(500), error))
        return false;
    selected_ = key;
    ++revision_;
    auto runtime = std::make_unique<SourceGraphRuntime>();
    if (!runtime->prepareMusic(root_, key, revision_, leftChannel, rightChannel, error))
        return false;
    return domain_.activateMusic(std::move(runtime), engine, revision_, error);
}
} // namespace entropy::host
