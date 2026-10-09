#pragma once
#include <array>
#include <cstdint>
#include <entropy/crossover_program_abi.h>
namespace entropy::host {
struct AudioBlockReport
{
    // Host-only diagnostic identity; no DLL ABI change.
    std::uint64_t diagnosticBlockId = 0, diagnosticFrameOffset = 0;
    std::array<std::uint64_t,3> diagnosticGeneration{};
    bool attempted = false;
    bool passed = false;
    bool outputGateClosed = true;
    bool bridgeConnected = false;
    std::uint32_t state = 0;
    std::uint32_t sourceRate = 0;
    std::uint32_t channels = 0;
    std::uint32_t validBits = 0;
    std::uint32_t containerBits = 0;
    std::uint32_t capturedFrames = 0;
    std::uint32_t convertedFrames = 0;
    std::uint32_t matrixFrames = 0;
    std::uint32_t srcEngine = 0;
    std::uint32_t srcLatencyFrames = 0;
    std::uint64_t streamGeneration = 0;
    std::uint64_t lostFrames = 0;
    double endpointPeak = 0.0;
    double matrixPeak = 0.0;
    double endpointReadUs = 0.0;
    double srcProcessUs = 0.0;
    std::uint32_t endpointAvailableFrames = 0;
    std::uint32_t sourceQueueDepthFrames = 0;
    double endpointPeakLeft = 0.0;
    double endpointPeakRight = 0.0;
    double matrixPeakLeft = 0.0;
    double matrixPeakRight = 0.0;
    std::array<double, 8> endpointChannelPeaks{};
    std::array<double, 8> matrixChannelPeaks{};
    // Non-owning source-domain handoff. Storage is owned by the caller.
    std::array<const double*, 8> convertedAudioPointers{};
    std::uint32_t convertedAudioPointerFrames = 0;
    bool centerPassed = false;
    bool centerTelemetryAvailable = false;
    std::uint32_t centerFrames = 0;
    std::array<double, 3> centerOutputPeaks{};
    std::uint32_t centerLatencySamples = 0;
    std::uint32_t centerBlockSize = 0;
    double centerPeakDb = -60.0;
    double centerRmsDb = -60.0;
    double centerMidActivity = 0.0;
    double centerLevelActivity = 0.0;
    double centerPhaseActivity = 0.0;
    double centerCoherenceActivity = 0.0;
    double centerStereoCorrelation = 0.0;
    double centerDetectorConfidence = 0.0;
    double centerDspLoadPercent = 0.0;
    double centerProcessTimeUs = 0.0;
    EntropyCrossoverProgramBlock appliedProgram{};
    bool crossoverPassed = false;
    std::uint32_t crossoverFrames = 0;
    std::array<double, 16> crossoverOutputPeaks{};
    bool outputMatrixPassed = false;
    std::uint32_t outputMatrixFrames = 0;
    std::array<double, 16> outputMatrixDestinationPeaks{};
    // Non-owning authoritative Output Matrix block used to prove/test the
    // optional Headroom exact-wire boundary. Valid until the next downstream
    // cycle, like finalAudioPointers.
    std::array<const double*, 16> outputMatrixAudioPointers{};
    bool headroomPassed = false;
    std::uint32_t headroomFrames = 0;
    std::array<double, 16> finalAudioPeaks{};
    double matrixProcessUs = 0.0;
    double programMixProcessUs = 0.0;
    double programPeakLeft = 0.0;
    double programPeakRight = 0.0;
    std::uint32_t programOverloadMask = 0;
    double centerProcessUs = 0.0;
    double crossoverProcessUs = 0.0;
    double outputMatrixProcessUs = 0.0;
    double headroomProcessUs = 0.0;
    double downstreamProcessUs = 0.0;
    // Host-internal handoff to the hardware output coordinator.
    // finalAudioPointers references fixed authoritative scratch valid until
    // the next call on its owning executor (or legacy diagnostic probe). This avoids
    // 16 heap allocations/copies on every 1024-frame realtime cycle.
    std::array<const double*, 16> finalAudioPointers{};
    std::uint32_t finalAudioFrames = 0;
    // Host-internal numeric diagnostics; no text ownership on the audio path.
    std::uint32_t faultCode = 0;
    std::uint32_t detailCode = 0;
};

}
