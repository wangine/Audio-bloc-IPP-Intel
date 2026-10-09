#include "SourceStartupPolicy.h"
#include <entropy/endpoint_catalog_abi.h>
namespace entropy::host
{
InputSourceKey SourceStartupPolicy::musicKey()
{
    return {"audio.entropy.endpoint.music-transport", "entropy-music-transport:main",
            ENTROPY_ACCESS_MUSIC_TRANSPORT};
}
bool SourceStartupPolicy::permitted(const InputSourceKey &key) noexcept
{
    return (key.providerId == "audio.entropy.endpoint.music-transport" &&
            key.endpointId == "entropy-music-transport:main" &&
            key.accessMode == ENTROPY_ACCESS_MUSIC_TRANSPORT) ||
           (key.providerId == "audio.entropy.endpoint.tidal" && !key.endpointId.empty() &&
            key.accessMode == ENTROPY_ACCESS_PRIVATE_TAP);
}
} // namespace entropy::host
