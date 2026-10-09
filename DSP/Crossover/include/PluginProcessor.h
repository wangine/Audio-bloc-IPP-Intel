#pragma once
#include "NativeSupport.h"
#include <array>
#include <atomic>
#include <unordered_map>
#include <vector>
#include "RealtimeFir.h"
#include "ProgramEpochTrace.h"
#include "ParameterIds.h"
#include "FractionalDelay.h"
namespace entropy::standalone {

class EntropyCrossoverAudioProcessor final
{
public:
    static constexpr int maxOutputs = 16;
    static constexpr int mainInputs = 3;
    static constexpr int cinemaInputs = 10;

    EntropyCrossoverAudioProcessor();
    ~EntropyCrossoverAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources() {}
    void processBlock (native::AudioBuffer<double>&, native::MidiBuffer&);
    int getTotalNumInputChannels() const noexcept { return 13; }
    int getMainBusNumOutputChannels() const noexcept { return 16; }
    void getStateInformation (native::MemoryBlock&);
    void setStateInformation (const void*, int);
    bool restoreStateInformation (const void*, int);

    native::ParameterStore& getAPVTS() noexcept { return apvts; }
    static native::ParameterStore::ParameterLayout createParameterLayout();

    double getRuntimeSampleRate() const noexcept { return runtimeSampleRate.load(); }
    int getRuntimeBlockSize() const noexcept { return runtimeBlockSize.load(); }
    int getRuntimeInputChannels() const noexcept { return runtimeInputs.load(); }
    int getRuntimeOutputChannels() const noexcept { return runtimeOutputs.load(); }
    bool isCinemaBusEnabled() const noexcept;
    float getCinemaInputPeak (int channel) const noexcept
    {
        if (channel < 0 || channel >= cinemaInputs) return 0.0f;
        return cinemaInputPeaks[(size_t) channel].load (std::memory_order_relaxed);
    }
    float getMainInputPeak (int channel) const noexcept
    {
        if (channel < 0 || channel >= mainInputs) return 0.0f;
        return mainInputPeaks[(size_t) channel].load (std::memory_order_relaxed);
    }
    float getOutputPeak (int channel) const noexcept
    {
        if (channel < 0 || channel >= maxOutputs) return 0.0f;
        return outputPeaks[(size_t) channel].load (std::memory_order_relaxed);
    }
    uint32_t getFractionalDelayStatus(int channel) const noexcept {
        return channel>=0 && channel<16 ? fractionalDelayStatus[channel].load(std::memory_order_relaxed) : 0;
    }
    int getProtectionState (int channel) const noexcept { return channel >= 0 && channel < maxOutputs ? protectionState[(size_t) channel].load() : 0; }
    void resetProtectionTrip (int channel) noexcept { if (channel >= 0 && channel < maxOutputs) protectionResetRequested[(size_t) channel].store (true); }
    static constexpr int liveSpectrumSamples = 32768;
    struct LiveSpectrumStats
    {
        float peak = 0.0f;
        float maximumDelta = 0.0f;
        uint64_t sampleCount = 0;
    };
    void setLiveSpectrumChannel (int channel) noexcept
    {
        liveSpectrumRequestedChannel.store (native::jlimit (-1, maxOutputs - 1, channel),
                                            std::memory_order_relaxed);
    }
    bool copyLiveSpectrumSamples (float* destination, int sampleCount) const noexcept;
    LiveSpectrumStats consumeLiveSpectrumStats() noexcept;

    static native::String channelParam (const char* base, int channel);
    static native::StringArray roleNames();
    static native::StringArray filterNames();
    static native::StringArray bassSourceNames();
    static native::StringArray bassLinkModeNames();
    static native::StringArray bassExtractorNames();
    static native::StringArray bassProcessingNames();
    static native::StringArray firLengthNames();
    int roleForChannel (int channel) const noexcept;
    // Native-host integration only: an optional immutable view for this owner
    // block. Control compilation on other threads continues to read APVTS.
    // Unset hosts retain the original interlock behavior and all DSP formulas.
    void setHostBlockParameterView(const float* values) noexcept {hostBlockParameterOwner=values?this:nullptr;hostBlockParameterView=values;}
    bool outputRoleActive(int channel) const noexcept {return isRoleEnabledByTopology(roleForChannel(channel));}
    native::StringArray activeOutputLabels() const;

    void saveChannelSnapshot (int channel);
    void saveAllSnapshots();

public:
    // Control thread only, serialized with all other APVTS mutations.
    int getReportedFirLatencySamples() const noexcept { return reportedFirLatency.load(std::memory_order_relaxed); }
    void serviceUpdates();
    struct ControlCommandTag {
        uint64_t session = 0;
        uint64_t sequence = 0;
        uint32_t kind = 0;
        uint64_t payloadHash = 0;
    };
    struct ControlCommandStatus {
        ControlCommandTag accepted{};
        ControlCommandTag applied{};
        ControlCommandTag failed{};
        uint32_t reason = 0;
    };
    bool publishControlSnapshot(ControlCommandTag tag);
    void publishControlAccepted(ControlCommandTag tag) noexcept;
    void publishControlFailure(ControlCommandTag tag, uint32_t reason) noexcept;
    ControlCommandStatus controlCommandStatus() const noexcept;
    // Processing owner only, immediately after processBlock. Not a UI getter.
    struct ProgramBlockState {
        uint64_t session = 0, sequence = 0, revision = 0;
        uint32_t program = 0, requiredInputs = 3;
        uint64_t outputToken = 0;
        uint32_t firstTransitionFrame = UINT32_MAX, transitions = 0;
    };
    ProgramBlockState programBlockState() const noexcept { return programBlock; }
    void setProgramInputEpochs(uint64_t session, uint64_t music, uint64_t cinema, uint32_t outputMask) noexcept {
        outputEpochSourceMask=outputMask;
        inputMusicSession=session; inputMusicGeneration=music; inputCinemaGeneration=cinema;
    }
    uint32_t requiredProgramInputs() const noexcept {
        return pending.load(std::memory_order_acquire) >= 0 || fading != nullptr
            ? 3u : programBlock.requiredInputs;
    }
private:
    struct Biquad
    {
        IirEpochTrace epoch;
        void reset() noexcept { z1 = z2 = 0.0; epoch.reset(); }
        uint64_t trace(uint64_t input) noexcept {return epoch.process(input,b0,b1,b2,a1,a2);}
        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;
    };

    struct ChannelDSP
    {
        std::array<Biquad, 4> hp, lp;
        Biquad allPass;
        std::array<Biquad, 4> previousHp, previousLp;
        Biquad previousAllPass;
        bool iirTransition = false;
        bool modeTransition = false;
        bool firAllPassTransition = false;
        bool firAllPassHistoryValid = false;
        std::array<double,2> firAllPassInputs{};
        std::array<uint64_t,2> firAllPassInputEpochs{};
        std::vector<double> modeIirBuffer;
        std::vector<uint64_t> modeIirEpochs;
        entropy::standalone::RealtimeFir fir;
        native::SmoothedValue<double, native::ValueSmoothingTypes::Linear> gain;
        native::SmoothedValue<double, native::ValueSmoothingTypes::Linear> delaySamples;
        std::vector<double> delayBuffer;
        std::vector<uint64_t> delayEpochs;
        FractionalDelay fractionalDelay;
        uint64_t delayedEpoch=0;
        FirEpochTrace firEpoch;
        int writePosition = 0;
        size_t delayValidSamples = 0;
        double peak = 0.0;
        uint64_t tripFrames = 0;
        bool protectionTripped = false;
        bool safetySilenced = false;
    };

    static void clearFirAllPassHistory(ChannelDSP& c) noexcept {
        c.firAllPassInputs.fill(0.0);c.firAllPassInputEpochs.fill(0);c.firAllPassHistoryValid=false;
    }
    static void recordFirAllPass(ChannelDSP& c,double input,uint64_t inputEpoch) noexcept {
        c.firAllPassInputs[0]=c.firAllPassInputs[1];c.firAllPassInputs[1]=input;
        c.firAllPassInputEpochs[0]=c.firAllPassInputEpochs[1];c.firAllPassInputEpochs[1]=inputEpoch;
        c.firAllPassHistoryValid=true;
    }
    float parameter (const native::String& id) const noexcept;
    void compileCoefficients (int channel, double sampleRate);
    static void setLowPass (Biquad&, double sampleRate, double frequency, double q);
    static void setHighPass (Biquad&, double sampleRate, double frequency, double q);
    static void setAllPass (Biquad&, double sampleRate, double frequency, double q);
    void updateFir (int channel, double sampleRate);

    bool isRoleEnabledByTopology (int role) const noexcept;
    bool isRoleEnabledByLiveTopology (int role) const noexcept;
    int firGroupDelayForChannel (int channel) const noexcept;
    void refreshReportedLatency();
    static int stagesForFilterType (int filterType) noexcept;
    static double processFilter (std::array<Biquad, 4>& stages, int filterType, double x) noexcept;
    static uint64_t traceFilterEpoch(std::array<Biquad,4>& stages,int filterType,uint64_t token) noexcept {
        for(int i=0;i<stagesForFilterType(filterType);++i) token=stages[(size_t)i].trace(token);
        return token;
    }
    double readDelayedSample (ChannelDSP&, double input, double delaySamples, uint64_t epoch) noexcept;
    void captureLiveSpectrumBlock (int channel, const double* samples, int sampleCount) noexcept;
    void captureSceneState (bool cinema);
    void restoreSceneState (bool cinema);
    void writeSceneStatesToTree();
    void readSceneStatesFromTree();
    static std::vector<native::String> sceneParameterIds();
    struct ComplexPcaLane;
    void buildBassLane (const native::AudioBuffer<double>& source, native::AudioBuffer<double>& destination,
                        int samples, int feed, int extractorMode,
                        int& rightPolarity, int& centerPolarity, ComplexPcaLane& pcaState);
    struct ComplexPcaBand
    {
        std::array<std::complex<double>, 3> envelope {};
        std::array<std::array<std::complex<double>, 3>, 3> covariance {};
        std::array<std::complex<double>, 3> eigenvector {
            std::complex<double> (0.5773502691896258, 0.0),
            std::complex<double> (0.5773502691896258, 0.0),
            std::complex<double> (0.5773502691896258, 0.0)
        };
        double phase = 0.0;
    };
    struct ComplexPcaLane
    {
        std::array<ComplexPcaBand, 5> bands;
    };
    void resetComplexPca (ComplexPcaLane&);
    void buildComplexPcaLane (const native::AudioBuffer<double>& source, native::AudioBuffer<double>& destination,
                              int samples, int feed, ComplexPcaLane&);

    native::ParameterStore apvts;
    std::array<ChannelDSP, maxOutputs> channels;

    native::AudioBuffer<double> inputScratch, cinemaScratch, bassLaneScratch, scoopLaneScratch, cinemaScoopLaneScratch, cinemaMixScratch;
    native::SmoothedValue<double, native::ValueSmoothingTypes::Linear> cinemaSourceMix;
    std::array<native::SmoothedValue<double, native::ValueSmoothingTypes::Linear>, mainInputs> musicTrimGains;
    std::array<native::SmoothedValue<double, native::ValueSmoothingTypes::Linear>, cinemaInputs> cinemaTrimGains;
    native::SmoothedValue<double, native::ValueSmoothingTypes::Linear> cinemaScoopLrcGain, cinemaScoopLfeGain;
    native::SmoothedValue<double, native::ValueSmoothingTypes::Linear> cinemaLfeReferenceGain;
    int bassRightPolarity = 1, bassCenterPolarity = 1;
    int scoopRightPolarity = 1, scoopCenterPolarity = 1;
    int cinemaScoopRightPolarity = 1, cinemaScoopCenterPolarity = 1;
    ComplexPcaLane bassPca, scoopPca, cinemaScoopPca;
    // 0..800 ms global causal base plus -800..+800 ms channel trim.
    // The effective value is clamped to 0..1600 ms.
    static constexpr double maxDelaySeconds = 1.6;
    static constexpr int maximumExpectedBlockSize = 4096;

    std::array<std::atomic<float>, maxOutputs> outputPeaks {};
    std::array<std::atomic<uint32_t>, maxOutputs> fractionalDelayStatus {};
    std::array<std::atomic<float>, mainInputs> mainInputPeaks {};
    std::array<std::atomic<float>, cinemaInputs> cinemaInputPeaks {};
    std::array<std::atomic<int>, maxOutputs> protectionState {};
    std::array<std::atomic<bool>, maxOutputs> protectionResetRequested {};
    // Passive post-DSP/pre-Output-Matrix monitor. It is a one-way copy from the
    // audio path; the analyzer cannot write audio or alter filter/delay state.
    std::array<std::atomic<float>, liveSpectrumSamples> liveSpectrumRing {};
    std::atomic<int> liveSpectrumRequestedChannel { -1 };
    std::atomic<uint64_t> liveSpectrumPublishedSamples { 0 };
    std::atomic<float> liveSpectrumPeakHold { 0.0f };
    std::atomic<float> liveSpectrumDeltaHold { 0.0f };
    std::atomic<uint64_t> liveSpectrumStatSamples { 0 };
    int liveSpectrumActiveChannel = -1;
    uint64_t liveSpectrumProducerSamples = 0;
    float liveSpectrumPreviousSample = 0.0f;
    bool liveSpectrumHasPreviousSample = false;
    std::atomic<int> reportedFirLatency { 0 };
    static_assert(std::atomic<int>::is_always_lock_free);
    std::atomic<double> runtimeSampleRate { 0.0 };
    std::atomic<int> runtimeBlockSize { 0 }, runtimeInputs { 0 }, runtimeOutputs { 0 };
    std::array<std::unordered_map<std::string, float>, 2> sceneStates;
    bool sceneStatesInitialised = false;
    bool activeSceneCinema = false;


    ProgramBlockState programBlock{};
    uint64_t inputMusicSession=0, inputMusicGeneration=0, inputCinemaGeneration=0;
    uint64_t epochMusicSession=0, epochGeneration=0, inputEpochToken=0, previousInputEpochToken=0;
    std::vector<uint64_t> blockOutputEpochs;
    bool traceOutputEpoch = false;
    uint32_t outputEpochSourceMask=0xffff;
    struct alignas(64) Config {
        std::array<float,parameterCount> values{};
        std::array<std::array<Biquad,4>,16> hp{},lp{};
        std::array<double,16> effectiveHpHz{},effectiveLpHz{};
        std::array<Biquad,16> ap{};
        std::array<FirKernel,16> kernels{};
        std::array<uint64_t,16> kernelKey{};
        std::array<FractionalDelayDesign,16> delays{};
        int latency=0;
        ControlCommandTag controlTag{};
    };
    std::array<Config,3> configs;
    std::array<std::atomic<int>,3> slotState{};
    std::atomic<int> pending{-1};
    std::array<std::atomic<float>*,parameterCount> raw{};
    Config *active=nullptr,*fading=nullptr,*building=nullptr;
    int activeIndex=-1,fadingIndex=-1,fadePosition=9600;
    FixedFFT kernelFft;
    std::array<float,parameterCount> lastCompiled{}; bool hasCompiled=false;
    struct CommandEventMailbox {
        std::atomic<uint64_t> revision { 0 };
        std::atomic<uint64_t> session { 0 };
        std::atomic<uint64_t> sequence { 0 };
        std::atomic<uint64_t> payloadHash { 0 };
        std::atomic<uint32_t> kind { 0 };
        std::atomic<uint32_t> reason { 0 };
        void publish(ControlCommandTag, uint32_t eventReason = 0) noexcept;
        ControlCommandTag read(uint32_t* eventReason = nullptr) const noexcept;
    };
    CommandEventMailbox acceptedCommand;
    CommandEventMailbox appliedCommand;
    CommandEventMailbox failedCommand;
    inline static thread_local const Config* tlsConfig=nullptr;
    float rt(int index) const noexcept {if(tlsConfig)return tlsConfig->values[index];return raw[index]?raw[index]->load(std::memory_order_relaxed):0.f;}
    inline static thread_local const EntropyCrossoverAudioProcessor* hostBlockParameterOwner=nullptr;
    inline static thread_local const float* hostBlockParameterView=nullptr;
    float liveRt(int index) const noexcept {if(hostBlockParameterOwner==this&&hostBlockParameterView)return hostBlockParameterView[index];return raw[index]?raw[index]->load(std::memory_order_relaxed):0.f;}
    void beginRt() noexcept;
    void endRt(int samples) noexcept;

    EntropyCrossoverAudioProcessor(const EntropyCrossoverAudioProcessor&) = delete;
    EntropyCrossoverAudioProcessor& operator=(const EntropyCrossoverAudioProcessor&) = delete;
};

}
