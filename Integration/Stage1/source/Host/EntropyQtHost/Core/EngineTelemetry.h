#pragma once

#include <cstddef>
#include <cstdint>
#include "SourceDeliveryDiagnostics.h"
#include <entropy/loudness_abi.h>
#include <entropy/loudness_display_abi.h>
#include <entropy/parametric_eq_abi.h>
#include <entropy/crossover_abi.h>

namespace entropy::host
{
inline constexpr wchar_t engineTelemetryMappingName[] =
#if defined(ENTROPY_ISOLATED_LOUDNESS_TEST)
    L"Local\\EntropyAudio.Tests.LoudnessUi.Telemetry.v6";
#elif defined(ENTROPY_ISOLATED_PARAMETRIC_EQ_TEST)
    L"Local\\EntropyAudio.Tests.ParametricEq.Telemetry.v7";
#else
    L"Local\\EntropyAudio.ModularHost.Telemetry.v11";
#endif
inline constexpr std::uint32_t engineTelemetryMagic = 0x31485445u;
inline constexpr std::uint32_t engineTelemetryVersion = 0x000b0000u;
inline constexpr wchar_t engineTelemetryMappingNameV10[] =
    L"Local\\EntropyAudio.ModularHost.Telemetry.v10";
inline constexpr std::uint32_t engineTelemetryVersionV10 = 0x000a0000u;
inline constexpr wchar_t engineControlMappingName[] =
#if defined(ENTROPY_ISOLATED_LOUDNESS_TEST)
    L"Local\\EntropyAudio.Tests.LoudnessUi.Control.v4";
#elif defined(ENTROPY_ISOLATED_PARAMETRIC_EQ_TEST)
    L"Local\\EntropyAudio.Tests.ParametricEq.Control.v5";
#else
    L"Local\\EntropyAudio.ModularHost.Control.v6";
#endif
inline constexpr std::uint32_t engineControlMagic = 0x31435445u;
inline constexpr std::uint32_t engineControlVersion = 0x00060000u;
inline constexpr wchar_t engineControlMappingNameV5[] =
    L"Local\\EntropyAudio.ModularHost.Control.v5";
inline constexpr std::uint32_t engineControlVersionV5 = 0x00050000u;

inline constexpr std::uint32_t centerParameterCount = 22;
inline constexpr std::uint32_t crossoverParameterCapacity = 640;
inline constexpr std::uint32_t crossoverResponsePointCount = 256;
inline constexpr std::uint32_t crossoverSpectrumPointCount = 256;
enum CenterParameterIndex : std::uint32_t
{
    CenterMode, CenterFftSize, CenterThreshold, CenterSelectivity, CenterFloor,
    CenterAttackMs, CenterReleaseMs, CenterSmoothingHz, CenterMidWeight,
    CenterLevelWeight, CenterPhaseWeight, CenterCoherenceWeight, CenterDetectorMode,
    CenterLookAheadMs, CenterLimiterEnable, CenterLimiterCeilingDb,
    CenterLimiterReleaseMs, CenterLrAutoAlign, CenterLrManualOffsetMs,
    CenterLrBypass, CenterEnable, CenterGain
};

enum EngineMuteMask : std::uint32_t
{
    EngineMuteTidal = 1u << 0,
    EngineMuteAux = 1u << 1,
    EngineMuteCinema = 1u << 2
};

enum EngineOptionalStageState : std::uint32_t
{
    EngineOptionalActive = 0,
    EngineOptionalBypassed = 1,
    EngineOptionalUnavailableWire = 2,
    EngineOptionalFaultForcedBypass = 3,
    EngineOptionalEmptySlotWire = 4
};

inline constexpr std::uint32_t engineOptionalStageCount = 5;

struct EngineOptionalStageTelemetry
{
    std::uint32_t state = EngineOptionalUnavailableWire;
    std::int32_t faultCode = 0;
    std::uint32_t slotIndex = 0;
    std::uint32_t reserved = 0;
    char instanceId[48]{};
    char moduleId[80]{};
    char detail[160]{};
};

enum EngineSrcRole : std::uint32_t
{
    EngineSrcTidal = 0,
    EngineSrcAux = 1,
    EngineSrcCinema = 2,
    EngineSrcRoleCount = 3
};

struct EngineInputTelemetry
{
    char role[16]{};
    std::uint32_t state = 0;
    std::uint32_t bridgeConnected = 0;
    std::uint32_t sourceRate = 0;
    std::uint32_t channels = 0;
    std::uint32_t validBits = 0;
    std::uint32_t containerBits = 0;
    std::uint32_t livePathPassed = 0;
    std::uint32_t muted = 0;
    std::uint32_t capturedFrames = 0;
    std::uint32_t convertedFrames = 0;
    std::uint32_t matrixFrames = 0;
    std::uint32_t srcEngine = 0;
    std::uint32_t srcLatencyFrames = 0;
    std::uint32_t srcControlAvailable = 0;
    std::uint32_t endpointAvailableFrames = 0;
    std::uint32_t sourceQueueDepthFrames = 0;
    std::uint32_t srcReserved = 0;
    std::uint64_t generation = 0;
    std::uint64_t lostFrames = 0;
    std::uint64_t sourceFifoProducedFrames = 0;
    std::uint64_t sourceFifoConsumedFrames = 0;
    std::uint64_t sourceFifoUnderruns = 0;
    std::uint64_t sourceFifoOverflows = 0;
    std::uint32_t sourceFifoCapacityFrames = 0;
    std::uint32_t sourceFifoHighWaterFrames = 0;
    double endpointPeak = 0.0;
    double matrixPeak = 0.0;
    double endpointPeakLeft = 0.0;
    double endpointPeakRight = 0.0;
    double matrixPeakLeft = 0.0;
    double matrixPeakRight = 0.0;
    double endpointReadUs = 0.0;
    double srcProcessUs = 0.0;
    double endpointChannelPeaks[8]{};
    double matrixChannelPeaks[8]{};
    char detail[128]{};
};

inline constexpr std::uint32_t engineMaximumSrcEngines = 8;

struct EngineSrcEngineCapability
{
    std::uint32_t engine = 0;
    std::uint32_t flags = 0;
    char identifier[48]{};
    char name[64]{};
};

struct EngineModuleParameterTelemetry
{
    char identifier[64]{};
    double value = 0.0;
};

inline constexpr std::uint32_t engineMaximumDacDomains = 2;
inline constexpr std::uint32_t engineMaximumDestinationLanes = 16;
inline constexpr std::uint32_t engineDestinationUnassigned = 0xffffffffu;

struct EngineDacDomainControl
{
    std::uint32_t enabled = 0;
    std::uint32_t sampleRate = 0;
    std::uint32_t channels = 0;
    std::uint32_t outputFormat = 0;
    std::uint32_t destinationLaneForChannel[engineMaximumDestinationLanes]{};
    std::uint64_t approvalRevision = 0;
    char logicalDeviceId[64]{};
    char verifiedEndpointIdUtf8[768]{};
    char verifiedFingerprint[64]{};
};

struct EngineDacDomainTelemetry
{
    std::uint32_t configured = 0;
    std::uint32_t bindingState = 0;
    std::uint32_t sinkState = 0;
    std::uint32_t faultFlags = 0;
    std::uint32_t sampleRate = 0;
    std::uint32_t channels = 0;
    std::uint32_t clockState = 0;
    std::uint32_t bufferFrames = 0;
    std::uint64_t submittedFrames = 0;
    std::uint64_t renderedBuffers = 0;
    std::uint64_t devicePositionFrames = 0;
    std::uint64_t xrunCount = 0;
    std::uint64_t clockInputFrames = 0;
    std::uint64_t clockNominalFrames = 0;
    std::uint64_t clockOutputFrames = 0;
    std::uint64_t clockStarvedCallbacks = 0;
    std::uint64_t clockResyncCount = 0;
    double fifoFillFrames = 0.0;
    double correctionPpm = 0.0;
    std::uint32_t guardianState = 0;
    std::uint32_t guardianFaultFlags = 0;
    std::uint32_t guardianLookaheadFrames = 0;
    std::uint32_t guardianEmergencyFadeFrames = 0;
    std::uint32_t guardianOversampleFactor = 0;
    std::uint64_t guardianInterventionCount = 0;
    double guardianCeilingDbtp = 0.0;
    double guardianMaximumTruePeak = 0.0;
    double guardianTripTruePeak = 0.0;

    // Live DAC-worker timing. These fields are observability only; they never
    // feed ClockSync, Guardian, PCM conversion, scheduling or gain control.
    std::uint64_t egressTimingCallbacks = 0;
    std::uint64_t egressDeadlineMisses = 0;
    std::uint64_t egressLastThreadCycles = 0;
    std::uint64_t egressMaxThreadCycles = 0;
    double egressCallbackIntervalUs = 0.0;
    double egressMaxCallbackIntervalUs = 0.0;
    double egressClockUs = 0.0;
    double egressGuardianUs = 0.0;
    double egressPcmUs = 0.0;
    double egressSubmitUs = 0.0;
    double egressTotalUs = 0.0;
    double egressMaxTotalUs = 0.0;
    double egressMaxClockUs = 0.0;
    double egressMaxGuardianUs = 0.0;
    double egressMaxPcmUs = 0.0;
    double egressMaxSubmitUs = 0.0;
    char detail[192]{};
};

// Read-only buffering diagnostics. Module ABIs and control IPC are unchanged.
struct EngineSourceBufferTelemetry
{
    std::uint32_t lowWaterFrames = 0, prerollFrames = 0, targetFrames = 0;
    std::uint32_t captureFillFrames = 0, captureCapacityFrames = 0;
    std::uint32_t captureLowWaterFrames = 0, captureHighWaterFrames = 0;
    std::uint64_t captureUnderruns = 0, captureOverflows = 0;
    std::uint64_t srcResets = 0, faults = 0;
};
struct EngineClockBufferTelemetry
{
    std::uint32_t capacityFrames = 0, faultFlags = 0;
    std::uint32_t latencySinkFrames = 0, latencyAvailable = 0;
    double targetFrames = 0.0;
};

struct EngineTelemetrySnapshot
{
    std::uint32_t magic = engineTelemetryMagic;
    std::uint32_t version = engineTelemetryVersion;
    std::uint32_t structSize = 0;
    std::uint32_t enginePid = 0;
    std::uint64_t sequence = 0;
    std::uint64_t heartbeat = 0;
    std::uint32_t graphReady = 0;
    std::uint32_t outputGateClosed = 1;
    std::uint32_t inputCount = 3;
    std::uint32_t reserved = 0;
    double engineTargetCycleUs = 5333.333;
    double engineCycleUs = 0.0;
    double engineCycleMaxUs = 0.0;
    double engineMaxLatenessUs = 0.0;
    std::uint64_t engineDeadlineMisses = 0;
    std::uint64_t engineTimingReserved = 0;
    EngineInputTelemetry inputs[3]{};
    std::uint32_t centerLive = 0;
    std::uint32_t centerTelemetryAvailable = 0;
    std::uint32_t centerFrames = 0;
    std::uint32_t centerLatencySamples = 0;
    std::uint32_t centerBlockSize = 0;
    std::uint32_t centerReserved = 0;
    double centerOutputPeaks[3]{};
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
    double authoritativeMatrixUs = 0.0;
    double authoritativeCenterUs = 0.0;
    double authoritativeCrossoverUs = 0.0;
    double authoritativeOutputMatrixUs = 0.0;
    double authoritativeHeadroomUs = 0.0;
    double authoritativeDownstreamUs = 0.0;
    std::uint32_t centerControlAvailable = 0;
    std::uint32_t centerControlReserved = 0;
    std::uint64_t centerControlAppliedSequence = 0;
    double centerParameterValues[centerParameterCount]{};
    char centerDetail[128]{};
    std::uint32_t crossoverTelemetryAvailable = 0;
    std::uint32_t crossoverControlAvailable = 0;
    std::uint32_t crossoverLatencySamples = 0;
    std::uint32_t crossoverBlockSize = 0;
    double crossoverSampleRate = 0.0;
    std::uint32_t crossoverCinemaProgram = 0;
    std::uint32_t crossoverReserved = 0;
    double crossoverMusicInputPeaks[3]{};
    double crossoverCinemaInputPeaks[10]{};
    double crossoverOutputPeaks[16]{};
    std::uint32_t crossoverProtectionAvailable = 0;
    std::uint32_t crossoverProtectionGlobalTrip = 0;
    std::uint32_t crossoverProtectionEnabledMask = 0;
    std::uint32_t crossoverProtectionWarningMask = 0;
    std::uint32_t crossoverProtectionTrippedMask = 0;
    std::uint32_t crossoverProtectionReserved[3]{};
    std::uint32_t crossoverProtectionState[16]{};
    std::uint64_t crossoverControlAppliedSequence = 0;
    char crossoverLastParameter[48]{};
    double crossoverLastValue = 0.0;
    std::uint32_t crossoverParameterCount = 0;
    std::uint32_t crossoverParameterReserved = 0;
    EngineModuleParameterTelemetry crossoverParameters[crossoverParameterCapacity]{};
    std::uint32_t crossoverViewTelemetryAvailable = 0;
    std::uint32_t crossoverResponsePoints = 0;
    std::uint32_t crossoverSpectrumPoints = 0;
    std::int32_t crossoverMonitoredChannel = -1;
    double crossoverResponseMinimumHz = 20.0;
    double crossoverResponseMaximumHz = 60000.0;
    float crossoverResponseDb[16][crossoverResponsePointCount]{};
    float crossoverSpectrumFrequencyHz[crossoverSpectrumPointCount]{};
    float crossoverSpectrumDb[crossoverSpectrumPointCount]{};
    std::uint32_t outputMatrixAvailable = 0;
    std::uint32_t outputMatrixGeneration = 0;
    std::uint32_t outputMatrixActiveSourceMask = 0;
    std::uint32_t outputMatrixEnabledDestinationMask = 0;
    std::uint32_t outputMatrixSummingDestinationMask = 0;
    std::uint32_t outputMatrixOrphanedSourceMask = 0;
    std::uint32_t outputMatrixRouteCount = 0;
    std::uint32_t outputMatrixReserved = 0;
    std::uint32_t outputMatrixSourceDestinationMask[16]{};
    std::uint32_t outputMatrixLaneStatus[16]{};
    double outputMatrixDestinationPeaks[16]{};
    char outputMatrixRoleId[16][48]{};
    char outputMatrixRoleLabel[16][96]{};
    char outputMatrixLaneLabel[16][96]{};
    std::uint64_t outputMatrixAppliedSequence = 0;
    std::uint32_t headroomAvailable = 0;
    std::uint32_t headroomStateRevision = 0;
    std::uint32_t headroomAppliedRevision = 0;
    std::uint32_t headroomGroupCount = 0;
    std::uint32_t headroomActiveChannelMask = 0;
    std::uint32_t headroomFaultFlags = 0;
    std::uint32_t headroomModeAvailable = 0;
    std::uint32_t headroomRequestedMode = 0;
    std::uint32_t headroomEffectiveMode = 0;
    std::uint32_t headroomModeTransitionActive = 0;
    double headroomModeTransitionProgress = 0.0;
    double headroomFixedAttenuationDb = -6.0;
    double headroomFixedTargetGain = 0.5011872336272722;
    double headroomMeasuredInputPeak = 0.0;
    double headroomMeasuredInputTruePeak = 0.0;
    double headroomAutoAnalysisReductionDb = 0.0;
    std::uint32_t headroomAnalyzerState = 0;
    std::uint32_t headroomLimitingChannelMask = 0;
    std::uint32_t headroomTruePeakStatus = 0;
    std::uint32_t headroomTelemetryReserved = 0;
    double headroomChannelInputPeak[16]{};
    std::uint32_t headroomHostEngineRate = 0;
    std::uint32_t headroomTransitionSamples = 0;
    std::uint64_t headroomProcessedFrames = 0;
    std::uint64_t headroomNonfiniteSampleCount = 0;
    double headroomTransitionMs = 0.0;
    std::uint32_t headroomTimingAvailable = 0;
    double headroomAutoAttackMs = 500.0;
    double headroomAutoReleaseMs = 4000.0;
    std::uint32_t headroomChannelGroup[16]{};
    std::uint32_t headroomGroupBounded[16]{};
    double headroomDeclaredPositiveDb[16]{};
    double headroomGuaranteedInputDb[16]{};
    double headroomPreventiveReserveDb[16]{};
    double headroomCurrentGain[16]{};
    double headroomTargetGain[16]{};
    double headroomGainReductionDb[16]{};
    double headroomRequiredDb[16]{};
    double headroomReserveMarginDb[16]{};
    double headroomTransitionProgress[16]{};
    std::uint32_t headroomGroupStatus[16]{};
    char headroomLaneLabel[16][96]{};
    std::uint64_t headroomAppliedSequence = 0;
    std::uint32_t outputArmed = 0;
    std::uint32_t outputDomainCount = 0;
    std::uint32_t outputFaultLatched = 0;
    std::uint32_t outputFirstFaultAvailable = 0;
    std::uint32_t outputFirstFaultDomain = 0;
    char outputFirstFaultDetail[512]{};
    std::uint32_t outputReserved = 0;
    std::uint64_t outputAppliedSequence = 0;
    EngineDacDomainTelemetry outputDomains[engineMaximumDacDomains]{};
    char outputDetail[256]{};
    std::uint64_t projectAppliedSequence = 0;
    std::uint32_t projectStatus = 0;
    std::uint32_t projectReserved = 0;
    wchar_t projectPath[512]{};
    char projectDetail[192]{};

    // Appended capability catalog: preserves all previous field offsets while
    // allowing Qt to discover exactly what the loaded SRC DLL exposes.
    std::uint32_t srcCapabilityAvailable = 0;
    std::uint32_t srcEngineCount = 0;
    std::uint32_t srcCapabilityReserved[2]{};
    EngineSrcEngineCapability srcEngines[engineMaximumSrcEngines]{};
    std::uint32_t loudnessAvailable=0,loudnessParameterCount=0,loudnessMicrophoneCount=0,loudnessCommandStatus=0;
    std::uint64_t loudnessRequestSequence=0;
    std::int32_t loudnessCommandResult=0;
    std::uint32_t loudnessReserved=0;
    EntropyLoudnessSettings loudnessSettings{};
    EntropyLoudnessTelemetry loudnessTelemetry{};
    EntropyLoudnessParameter loudnessParameters[64]{};
    EntropyLoudnessMicrophone loudnessMicrophones[32]{};
    char loudnessVersion[32]{};
    std::uint32_t loudnessDisplayAvailable=0;
    EntropyLoudnessDisplay loudnessDisplay{};
    std::uint32_t optionalStageCount=engineOptionalStageCount;
    std::uint32_t optionalStageReserved[3]{};
    EngineOptionalStageTelemetry optionalStages[engineOptionalStageCount]{};
    std::uint32_t parametricEqAvailable = 0;
    std::uint32_t parametricEqParameterCount = 0;
    std::uint32_t parametricEqCommandStatus = 0;
    std::uint32_t parametricEqSlotState = EngineOptionalUnavailableWire;
    std::uint64_t parametricEqRequestSequence = 0;
    std::int32_t parametricEqCommandResult = 0;
    std::uint32_t parametricEqReserved = 0;
    EntropyParametricEqSettingsV1 parametricEqSettings{};
    EntropyParametricEqTelemetryV1 parametricEqTelemetry{};
    EntropyParametricEqParameterV1 parametricEqParameters[121]{};
    char parametricEqVersion[32]{};
    EngineSourceBufferTelemetry sourceBuffers[3]{};
    EngineClockBufferTelemetry clockBuffers[engineMaximumDacDomains]{};
    SourceDeliveryTelemetry sourceDelivery[3]{};
    std::uint64_t engineRecoveryReanchors = 0;
    std::uint64_t crossoverCommandSession = 0;
    std::uint64_t crossoverCommandRequestedSequence = 0;
    std::uint64_t crossoverCommandAcceptedSequence = 0;
    std::uint64_t crossoverCommandAppliedSequence = 0;
    std::uint64_t crossoverCommandFailedSequence = 0;
    std::uint32_t crossoverCommandState = ENTROPY_CROSSOVER_COMMAND_IDLE;
    std::uint32_t crossoverCommandReason = ENTROPY_CROSSOVER_COMMAND_REASON_NONE;
    std::uint32_t crossoverCommandKind = 0;
    std::uint32_t crossoverShadowFailureMask = 0;
};
inline constexpr std::size_t engineTelemetryV10StructSize =
    offsetof(EngineTelemetrySnapshot, crossoverCommandSession);

struct EngineControlSnapshot
{
    std::uint32_t magic = engineControlMagic;
    std::uint32_t version = engineControlVersion;
    std::uint32_t structSize = 0;
    std::uint32_t muteMask = 0;
    std::uint64_t sequence = 0;
    std::uint64_t srcSequence = 0;
    std::uint32_t srcRole = 0;
    std::uint32_t srcEngine = 0;
    std::uint64_t parameterSequence = 0;
    char moduleIdentifier[64]{};
    char parameterIdentifier[48]{};
    double parameterValue = 0.0;
    std::uint32_t parameterAction = 0;
    std::uint32_t parameterReserved = 0;
    std::int32_t crossoverMonitorChannel = -1;
    std::uint32_t crossoverMonitorSequence = 0;
    std::uint64_t crossoverBulkSequence = 0;
    std::uint32_t crossoverBulkAction = 0;
    std::uint32_t crossoverBulkProgram = 0;
    std::uint32_t crossoverBulkSide = 0;
    std::uint32_t crossoverBulkReserved = 0;
    double crossoverBulkValue = 0.0;
    std::uint64_t outputMatrixSequence = 0;
    std::uint32_t outputMatrixAction = 0;
    std::uint32_t outputMatrixSource = 0;
    std::uint32_t outputMatrixDestination = 0;
    std::uint32_t outputMatrixEnabled = 0;
    std::uint64_t headroomSequence = 0;
    std::uint32_t headroomAction = 0;
    std::uint32_t headroomIndex = 0;
    std::uint32_t headroomSecondaryIndex = 0;
    std::uint32_t headroomReserved = 0;
    double headroomValue = 0.0;
    std::uint64_t hardwareOutputSequence = 0;
    std::uint32_t hardwareOutputAction = 0;
    std::uint32_t hardwareOutputReserved = 0;
    EngineDacDomainControl hardwareOutputDomains[engineMaximumDacDomains]{};
    std::uint64_t projectSequence = 0;
    std::uint32_t projectAction = 0;
    std::uint32_t projectReserved = 0;
    wchar_t projectPath[512]{};
    // Bounded one-command mailbox: 0 free, 1 producer, 2 ready, 3 awaiting ACK.
    std::uint32_t loudnessBusy=0,loudnessAction=0;
    std::uint64_t loudnessSequence=0;
    EntropyLoudnessSettings loudnessSettings{};
    char loudnessEndpoint[512]{};
    std::uint32_t parametricEqBusy = 0;
    std::uint32_t parametricEqAction = 0;
    std::uint64_t parametricEqSequence = 0;
    EntropyParametricEqSettingsV1 parametricEqSettings{};
    std::uint64_t crossoverCommandSession = 0;
};
inline constexpr std::size_t engineControlV5StructSize =
    offsetof(EngineControlSnapshot, crossoverCommandSession);

bool writeEngineLoudnessRequest(std::uint32_t action,const EntropyLoudnessSettings&,
                               const char* endpoint,std::uint64_t& sequence) noexcept;
bool writeEngineParametricEqSettings(
    const EntropyParametricEqSettingsV1& settings,
    std::uint64_t& sequence) noexcept;

enum EngineParameterAction : std::uint32_t
{
    EngineParameterSet = 0,
    EngineParameterBeginGesture = 1,
    EngineParameterEndGesture = 2
};

enum EngineCrossoverBulkAction : std::uint32_t
{
    EngineCrossoverBulkNone = 0,
    EngineCrossoverApplySideDelay = 1,
    EngineCrossoverApplyLeftToRight = 2,
    EngineCrossoverResetProtectionTrip = 3,
    EngineCrossoverResetAllProtectionTrips = 4
};

enum EngineProjectAction : std::uint32_t
{
    EngineProjectNone = 0,
    EngineProjectSave = 1,
    EngineProjectLoad = 2
};

enum EngineHardwareOutputAction : std::uint32_t
{
    EngineHardwareOutputNone = 0,
    EngineHardwareOutputArm = 1,
    EngineHardwareOutputDisarm = 2,
    EngineHardwareOutputConfigure = 3
};

enum EngineOutputMatrixAction : std::uint32_t
{
    EngineOutputMatrixNone = 0,
    EngineOutputMatrixSetRoute = 1,
    EngineOutputMatrixClearAll = 2
};

enum EngineOutputLaneStatus : std::uint32_t
{
    EngineOutputLaneUnassigned = 0,
    EngineOutputLaneOnline = 1,
    EngineOutputLaneOffline = 2
};

enum EngineHeadroomAction : std::uint32_t
{
    EngineHeadroomNone = 0,
    EngineHeadroomSetTransitionMs = 1,
    EngineHeadroomSetGroupCount = 2,
    EngineHeadroomSetChannelGroup = 3,
    EngineHeadroomSetPreventiveReserve = 4,
    EngineHeadroomClearFault = 5,
    EngineHeadroomSetOperatingMode = 6,
    EngineHeadroomSetAutoAttackMs = 7,
    EngineHeadroomSetAutoReleaseMs = 8
};

enum EngineProjectStatus : std::uint32_t
{
    EngineProjectIdle = 0,
    EngineProjectBusy = 1,
    EngineProjectSaved = 2,
    EngineProjectLoaded = 3,
    EngineProjectFailed = 4
};

bool readEngineTelemetry(EngineTelemetrySnapshot& snapshot) noexcept;
bool readEngineControl(EngineControlSnapshot& snapshot) noexcept;
bool writeEngineMuteMask(std::uint32_t muteMask) noexcept;
bool writeEngineSrcEngine(std::uint32_t role, std::uint32_t engine,
                          std::uint64_t& sequence) noexcept;
bool writeEngineParameterIntent(const char* moduleIdentifier,
                                const char* parameterIdentifier,
                                double value,
                                const wchar_t* mappingName = engineControlMappingName) noexcept;
bool writeEngineParameterGesture(const char* moduleIdentifier,
                                 const char* parameterIdentifier,
                                 bool begin,
                                 const wchar_t* mappingName = engineControlMappingName) noexcept;
bool writeEngineCrossoverMonitorChannel(int channel) noexcept;
bool writeEngineCrossoverApplySideDelay(bool cinemaProgram,
                                        bool rightSide,
                                        double delayMs) noexcept;
bool writeEngineCrossoverApplyLeftToRight(bool cinemaProgram) noexcept;
bool writeEngineCrossoverResetProtectionTrip(std::uint32_t outputIndex) noexcept;
bool writeEngineCrossoverResetAllProtectionTrips() noexcept;
bool writeEngineOutputMatrixRoute(std::uint32_t source,
                                  std::uint32_t destination,
                                  bool enabled,
                                  std::uint64_t& sequence) noexcept;
bool writeEngineOutputMatrixClearAll(std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomTransitionMs(double milliseconds,
                                     std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomAutoAttackMs(double milliseconds, std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomAutoReleaseMs(double milliseconds, std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomGroupCount(std::uint32_t groupCount,
                                   std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomChannelGroup(std::uint32_t channel,
                                     std::uint32_t group,
                                     std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomPreventiveReserve(std::uint32_t group,
                                          double decibels,
                                          std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomClearFault(std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomOperatingMode(std::uint32_t mode,
                                      std::uint64_t& sequence) noexcept;
bool writeEngineHeadroomOperatingPolicy(std::uint32_t mode,
                                        double manualGainDb,
                                        std::uint64_t& sequence) noexcept;
bool writeEngineHardwareOutputArm(
    const EngineDacDomainControl* domains,
    std::uint32_t domainCount,
    std::uint64_t& sequence) noexcept;
bool writeEngineHardwareOutputConfiguration(
    const EngineDacDomainControl* domains,
    std::uint32_t domainCount,
    std::uint64_t& sequence) noexcept;
bool writeEngineHardwareOutputDisarm(std::uint64_t& sequence) noexcept;
bool writeEngineProjectRequest(EngineProjectAction action,
                               const wchar_t* path,
                               std::uint64_t& sequence) noexcept;
} // namespace entropy::host
