#include "Core/SourceDomainRuntime.h"

#include <chrono>
#include <iostream>
#include <string>

int main()
{
    entropy::host::SourceDomainRuntime runtime;
    if (runtime.engineRate() != 192000 || runtime.blockFrames() != 1024)
    {
        std::cerr << "FAIL fixed contract\n";
        return 1;
    }

    entropy::host::SourceBlock1024 block;
    if (runtime.musicPrepared() ||
        runtime.musicReady() ||
        runtime.musicSessionRevision() != 0 ||
        runtime.popMusic1024(block))
    {
        std::cerr << "FAIL fresh MUSIC must be unbound\n";
        return 2;
    }

    std::string error;
    if (!runtime.clearMusic(std::chrono::milliseconds(1), error) ||
        !error.empty() ||
        runtime.musicPrepared() ||
        runtime.musicSessionRevision() != 0)
    {
        std::cerr << "FAIL empty MUSIC clear must be idempotent\n";
        return 3;
    }

    if (!runtime.restartMusic(nullptr, 0) ||
        runtime.musicPrepared() ||
        runtime.popMusic1024(block))
    {
        std::cerr << "FAIL null legacy MUSIC restart must remain optional\n";
        return 4;
    }

    std::cout
        << "PASS SourceDomainRuntime optional MUSIC / session revision / "
        << "reader-safe clear contract\n";
    return 0;
}
