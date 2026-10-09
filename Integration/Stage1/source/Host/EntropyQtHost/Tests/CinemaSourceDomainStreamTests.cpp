#include "Core/CinemaSourceDomainStream.h"

#include <iostream>

int main()
{
    entropy::host::SourceDomainConfig config;
    config.channels = 8;
    if (entropy::host::CinemaSourceDomainStream::validConfig(config))
    {
        std::cerr << "FAIL accepted CINEMA endpoint read larger than its negotiated 1024-frame maximum\n";
        return 1;
    }
    config.endpointReadFrames = 1024;
    if (!entropy::host::CinemaSourceDomainStream::validConfig(config))
    {
        std::cerr << "FAIL valid coherent CINEMA contract\n";
        return 2;
    }
    config.channels = 2;
    if (entropy::host::CinemaSourceDomainStream::validConfig(config))
    {
        std::cerr << "FAIL accepted stereo CINEMA contract\n";
        return 3;
    }
    config.channels = 8;
    config.destinationRate = 96000;
    if (entropy::host::CinemaSourceDomainStream::validConfig(config))
    {
        std::cerr << "FAIL accepted non-192k destination\n";
        return 4;
    }
    config.destinationRate = 192000;
    config.dspBlockFrames = 512;
    if (entropy::host::CinemaSourceDomainStream::validConfig(config))
    {
        std::cerr << "FAIL accepted non-1024 DSP cadence\n";
        return 5;
    }
    std::cout << "PASS coherent 8-channel CINEMA source-domain contract\n";
    return 0;
}
