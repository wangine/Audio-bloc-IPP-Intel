#include <windows.h>
#include "PluginProcessor.h"
#include <string_view>
#include <cmath>
namespace entropy::standalone {

namespace
{
constexpr int musicGainBase = parameterIndex ("musicGainDb1");
constexpr int cinemaGainBase = parameterIndex ("cinemaGainDb1");
constexpr int musicTrimBase = parameterIndex ("musicTrimDb1");
constexpr int cinemaTrimBase = parameterIndex ("cinemaTrimDb1");
constexpr int protectorOnBase = parameterIndex ("protectorOn1");
constexpr int musicDelayBase = parameterIndex ("musicDelayMs1");
constexpr int cinemaDelayBase = parameterIndex ("cinemaDelayMs1");
constexpr int safetyMinBase = parameterIndex ("safetyMinHz1");
constexpr int safetyMaxBase = parameterIndex ("safetyMaxHz1");
constexpr int tripHoldBase = parameterIndex ("tripHoldMs1");
constexpr int fractionalDelayModeBase = parameterIndex ("fractionalDelayMode1");
static_assert (parameterCount == 478,
               "Crossover parameter indices must match the canonical layout");
}

namespace
{
constexpr float minFilterHz = 2.0f;
constexpr float maxFilterHz = 60000.0f;
constexpr double qButterworth2 = 0.7071067811865476;
constexpr double qButterworth4a = 0.541196100146197;
constexpr double qButterworth4b = 1.306562964876377;
constexpr double qButterworth6a = 0.5176380902050415;
constexpr double qButterworth6b = 0.7071067811865476;
constexpr double qButterworth6c = 1.9318516525781366;
constexpr double qButterworth8a = 0.5097955791041592;
constexpr double qButterworth8b = 0.6013448869350453;
constexpr double qButterworth8c = 0.8999762231364156;
constexpr double qButterworth8d = 2.5629154477415064;
}

native::String EntropyCrossoverAudioProcessor::channelParam (const char* base, int channel)
{
    return native::String (base) + native::String (channel + 1);
}

native::StringArray EntropyCrossoverAudioProcessor::roleNames()
{
    return { "OFF",
             "L WAY 1", "L WAY 2", "L WAY 3", "L WAY 4", "L WAY 5",
             "R WAY 1", "R WAY 2", "R WAY 3", "R WAY 4", "R WAY 5",
             "CENTER WAY 1", "CENTER WAY 2",
             "LEGACY CENTER WAY 3 (INACTIVE)", "LEGACY CENTER WAY 4 (INACTIVE)", "LEGACY CENTER WAY 5 (INACTIVE)",
             "SUB 1", "SCOOP 1", "SUB 2", "SCOOP 2",
             "CINEMA SL", "CINEMA SR", "CINEMA BL", "CINEMA BR",
             "CINEMA TSL", "CINEMA TSR" };
}

native::StringArray EntropyCrossoverAudioProcessor::filterNames()
{
    return { "BW12", "BW24", "LR24", "LR48", "BW36", "BW48" };
}

native::StringArray EntropyCrossoverAudioProcessor::bassSourceNames()
{
    return { "L+R", "L+R+C", "LEFT", "RIGHT", "CENTER" };
}

native::StringArray EntropyCrossoverAudioProcessor::bassLinkModeNames()
{
    return { "INDEPENDENT", "COMMON SOURCE", "COMMON EXTRACTOR", "FULL LINK" };
}

native::StringArray EntropyCrossoverAudioProcessor::bassExtractorNames()
{
    return { "CLASSIC MATRIX", "AVERAGED MATRIX", "PHASE SAFE", "COMPLEX MULTIBAND PCA" };
}

native::StringArray EntropyCrossoverAudioProcessor::bassProcessingNames()
{
    return { "IIR", "FIR LINEAR PHASE" };
}

native::StringArray EntropyCrossoverAudioProcessor::firLengthNames()
{
    return { "2049 taps", "4097 taps", "8193 taps" };
}

EntropyCrossoverAudioProcessor::EntropyCrossoverAudioProcessor()
    : apvts (createParameterLayout())
{
}

EntropyCrossoverAudioProcessor::~EntropyCrossoverAudioProcessor()
{

}

native::ParameterStore::ParameterLayout EntropyCrossoverAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<native::RangedAudioParameter>> p;
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "cinemaSource", 1 }, "Program Source", native::StringArray { "MUSIC L/R/C", "CINEMA 7.1.2" }, 0));
    p.push_back (std::make_unique<native::AudioParameterInt> (native::ParameterID { "selectedChannel", 1 }, "Selected Channel", 1, maxOutputs, 1));
    p.push_back (std::make_unique<native::AudioParameterInt> (native::ParameterID { "lrWays", 1 }, "L/R Ways", 1, 5, 4));
    p.push_back (std::make_unique<native::AudioParameterInt> (native::ParameterID { "centerWays", 1 }, "Center Ways", 1, 2, 2));
    p.push_back (std::make_unique<native::AudioParameterInt> (native::ParameterID { "subSpeakers", 1 }, "Sub Speakers", 1, 2, 1));
    p.push_back (std::make_unique<native::AudioParameterInt> (native::ParameterID { "scoopSpeakers", 1 }, "Scoop Speakers", 1, 2, 1));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "lrBandLimit", 1 }, "L/R Working Band", native::StringArray { "40 kHz", "60 kHz" }, 0));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "fineDelayRatio", 1 }, "Fine Delay Ratio", native::StringArray { "1/10", "1/100", "1/1000" }, 1));
    for (int ch = 0; ch < mainInputs; ++ch)
    {
        const auto n = native::String (ch + 1);
        p.push_back (std::make_unique<native::AudioParameterFloat> (
            native::ParameterID { "musicTrimDb" + n, 1 }, "MUSIC " + n + " Input Trim",
            native::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, "dB"));
    }
    for (int ch = 0; ch < cinemaInputs; ++ch)
    {
        const auto n = native::String (ch + 1);
        p.push_back (std::make_unique<native::AudioParameterFloat> (
            native::ParameterID { "cinemaTrimDb" + n, 1 }, "CINEMA " + n + " Input Trim",
            native::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, "dB"));
    }
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "bassFeed", 1 }, "Bass Source", bassSourceNames(), 0));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "scoopFeed", 1 }, "Scoop Bass Source", bassSourceNames(), 0));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "bassLinkMode", 1 }, "Bass Link Mode", bassLinkModeNames(), 0));
    p.push_back (std::make_unique<native::AudioParameterChoice> (
        native::ParameterID { "cinemaScoopSource", 1 }, "Cinema Scoop Source",
        native::StringArray { "L/R/C EXTRACT", "LFE", "L/R/C + LFE" }, 0));
    p.push_back (std::make_unique<native::AudioParameterBool> (
        native::ParameterID { "cinemaLfeFilterBypass", 1 }, "Cinema LFE Filter Bypass", false));
    p.push_back (std::make_unique<native::AudioParameterBool> (
        native::ParameterID { "cinemaLfeReferenceGain", 1 }, "Cinema LFE +10 dB Reference Gain", false));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "bassExtractor", 1 }, "Bass Extractor", bassExtractorNames(), 1));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "scoopExtractor", 1 }, "Scoop Extractor", bassExtractorNames(), 1));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "bassProcess", 1 }, "Bass Processing", bassProcessingNames(), 0));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "scoopProcess", 1 }, "Scoop Processing", bassProcessingNames(), 0));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "bassFirLength", 1 }, "Bass FIR Length", firLengthNames(), 1));
    p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { "scoopFirLength", 1 }, "Scoop FIR Length", firLengthNames(), 1));
    p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { "bassAllPass", 1 }, "Bass All-Pass Alignment", false));
    p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { "scoopAllPass", 1 }, "Scoop All-Pass Alignment", false));
    p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { "bassAPFreq", 1 }, "Bass All-Pass Frequency", native::NormalisableRange<float> (2.0f, 1000.0f, 0.1f, 0.32f), 80.0f, "Hz"));
    p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { "scoopAPFreq", 1 }, "Scoop All-Pass Frequency", native::NormalisableRange<float> (2.0f, 1000.0f, 0.1f, 0.32f), 120.0f, "Hz"));
    p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { "bassAPQ", 1 }, "Bass All-Pass Q", native::NormalisableRange<float> (0.2f, 4.0f, 0.01f), 0.707f));
    p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { "scoopAPQ", 1 }, "Scoop All-Pass Q", native::NormalisableRange<float> (0.2f, 4.0f, 0.01f), 0.707f));

    for (int ch = 0; ch < maxOutputs; ++ch)
    {
        const auto n = native::String (ch + 1);
        int defaultRole = 0;
        if (ch < 4) defaultRole = 1 + ch;
        else if (ch < 8) defaultRole = 6 + (ch - 4);
        else if (ch < 10) defaultRole = 11 + (ch - 8);
        else if (ch == 10) defaultRole = 16;
        else if (ch == 11) defaultRole = 17;

        p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { channelParam ("source", ch), 1 }, "CH " + n + " Role", roleNames(), defaultRole));
        const int defaultCinemaRole = ch >= 12 ? 20 + (ch - 12) : defaultRole;
        p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { channelParam ("cinemaRole", ch), 1 }, "CINEMA CH " + n + " Role", roleNames(), defaultCinemaRole));
        p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { channelParam ("filterType", ch), 1 }, "CH " + n + " Filter Algorithm", filterNames(), 2));
        // gainDb is retained only so states made before 1.9.10 can be
        // migrated without changing their sound. Runtime/UI use the two
        // explicit scene gains below.
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("gainDb", ch), 1 }, "CH " + n + " Legacy Gain", native::NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f, "dB"));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("musicGainDb", ch), 1 }, "MUSIC CH " + n + " Gain", native::NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f, "dB"));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("cinemaGainDb", ch), 1 }, "CINEMA CH " + n + " Gain", native::NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f, "dB"));
        p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { channelParam ("mute", ch), 1 }, "CH " + n + " Mute", false));
        p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { channelParam ("solo", ch), 1 }, "CH " + n + " Solo", false));
        p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { channelParam ("polarity", ch), 1 }, "CH " + n + " Polarity Invert", false));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("delayMs", ch), 1 }, "CH " + n + " Channel Delay", native::nonnegativeDelayRange(), 0.0f, "ms"));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("musicDelayMs", ch), 1 }, "MUSIC CH " + n + " Channel Delay", native::nonnegativeDelayRange(), 0.0f, "ms"));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("cinemaDelayMs", ch), 1 }, "CINEMA CH " + n + " Channel Delay", native::nonnegativeDelayRange(), 0.0f, "ms"));
        const bool defaultBassLane = ch == 10 || ch == 11;
        p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { channelParam ("processMode", ch), 1 }, "CH " + n + " Processing", bassProcessingNames(), 0));
        p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { channelParam ("firLength", ch), 1 }, "CH " + n + " FIR Length", firLengthNames(), 1));
        p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { channelParam ("allPass", ch), 1 }, "CH " + n + " All-Pass Alignment", false));
        p.push_back (std::make_unique<native::AudioParameterChoice> (native::ParameterID { channelParam ("fractionalDelayMode", ch), 1 }, "CH " + n + " Fractional Delay", native::StringArray {"Linear (reference)", "Thiran 1", "Thiran 3", "Thiran 5"}, 0));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("apFreq", ch), 1 }, "CH " + n + " All-Pass Frequency", native::NormalisableRange<float> (2.0f, 60000.0f, 0.1f, 0.28f), defaultBassLane ? 80.0f : 1000.0f, "Hz"));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("apQ", ch), 1 }, "CH " + n + " All-Pass Q", native::NormalisableRange<float> (0.2f, 4.0f, 0.01f), 0.707f));
        p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { channelParam ("hpOn", ch), 1 }, "CH " + n + " High Pass", false));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("hpHz", ch), 1 }, "CH " + n + " High Pass Frequency", native::NormalisableRange<float> (minFilterHz, maxFilterHz, 0.1f, 0.28f), defaultBassLane ? 20.0f : 80.0f, "Hz"));
        p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { channelParam ("lpOn", ch), 1 }, "CH " + n + " Low Pass", false));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("lpHz", ch), 1 }, "CH " + n + " Low Pass Frequency", native::NormalisableRange<float> (minFilterHz, maxFilterHz, 0.1f, 0.28f), defaultBassLane ? 200.0f : 20000.0f, "Hz"));
        p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { channelParam ("protectorOn", ch), 1 }, "CH " + n + " Protector", false));
        p.push_back (std::make_unique<native::AudioParameterBool> (native::ParameterID { channelParam ("emergencyMute", ch), 1 }, "CH " + n + " Emergency Mute", false));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("safetyMinHz", ch), 1 }, "CH " + n + " Safe Minimum", native::NormalisableRange<float> (2.0f, 60000.0f, 0.1f, 0.25f), 2.0f, "Hz"));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("safetyMaxHz", ch), 1 }, "CH " + n + " Safe Maximum", native::NormalisableRange<float> (2.0f, 96000.0f, 0.1f, 0.25f), 96000.0f, "Hz"));
        p.push_back (std::make_unique<native::AudioParameterFloat> (native::ParameterID { channelParam ("tripHoldMs", ch), 1 }, "CH " + n + " Trip Hold", native::NormalisableRange<float> (5.0f, 1000.0f, 1.0f, 0.4f), 50.0f, "ms"));
    }
    return p;
}

int EntropyCrossoverAudioProcessor::roleForChannel (int channel) const noexcept
{
    channel = native::jlimit (0, maxOutputs - 1, channel);
    const bool cinema = rt (parameterIndex ("cinemaSource")) >= 0.5f;
    return (int) std::lround (rt ((cinema ? parameterIndex("cinemaRole1") : parameterIndex("source1")) + channel));
}

native::StringArray EntropyCrossoverAudioProcessor::activeOutputLabels() const
{
    native::StringArray labels;
    const auto names = roleNames();
    for (int channel = 0; channel < maxOutputs; ++channel)
    {
        const int role = native::jlimit(0, names.size()-1, roleForChannel(channel));
        labels.add(isRoleEnabledByTopology(role)
            ? "CH" + native::String(channel+1) + " " + names[role].replace("CENTER", "C").replace(" WAY ", " way")
            : native::String());
    }
    return labels;
}

void EntropyCrossoverAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto nextProgramSession = programBlock.session + 1;
    programBlock = {};
    programBlock.session = nextProgramSession;
    inputEpochToken=previousInputEpochToken=epochMusicSession=epochGeneration=0;
    inputMusicSession=inputMusicGeneration=inputCinemaGeneration=0;
    std::fill(blockOutputEpochs.begin(),blockOutputEpochs.end(),0);
    // Prepare/reset is quiescent. Rebuild the requested graph with clean
    // histories; a departing transition must not survive an explicit reset.
    {
        const int discarded = pending.exchange(-1, std::memory_order_relaxed);
        if (discarded >= 0 && configs[(size_t) discarded].controlTag.sequence != 0)
            failedCommand.publish(configs[(size_t) discarded].controlTag, 4u); // superseded
        active = fading = building = nullptr;
        activeIndex = fadingIndex = -1;
        fadePosition = 9600;
        for (auto& state : slotState) state.store(0, std::memory_order_relaxed);
        hasCompiled = false;
    }
    runtimeSampleRate = sampleRate;
    runtimeBlockSize = samplesPerBlock;
    runtimeInputs = getTotalNumInputChannels();
    runtimeOutputs = getMainBusNumOutputChannels();
    visualization.applied.store(0,std::memory_order_release);
    visualization.monitorIdentity.fetch_add(32,std::memory_order_acq_rel);
    viewCaptureRevision.fetch_add(1,std::memory_order_acq_rel);
    viewCaptureQpc.store(0,std::memory_order_relaxed);
    liveSpectrumActiveChannel = -1;
    liveSpectrumProducerSamples = 0;
    liveSpectrumPublishedSamples.store (0, std::memory_order_release);
    liveSpectrumPeakHold.store (0.0f, std::memory_order_relaxed);
    liveSpectrumDeltaHold.store (0.0f, std::memory_order_relaxed);
    liveSpectrumStatSamples.store (0, std::memory_order_relaxed);
    liveSpectrumPreviousSample = 0.0f;
    liveSpectrumHasPreviousSample = false;
    for (auto& sample : liveSpectrumRing) sample.store (0.0f, std::memory_order_relaxed);
    viewCaptureRevision.fetch_add(1,std::memory_order_release);

    const int scratchSize = native::jmax (samplesPerBlock, maximumExpectedBlockSize);
    blockOutputEpochs.assign((size_t)scratchSize,0);
    for(int i=0;i<entropy::standalone::parameterCount;++i) raw[i]=apvts.getRawParameterValue(entropy::standalone::parameterIds[i]);
    inputScratch.setSize (mainInputs, scratchSize, false, false, true);
    cinemaScratch.setSize (cinemaInputs, scratchSize, false, false, true);
    bassLaneScratch.setSize (1, scratchSize, false, false, true);
    scoopLaneScratch.setSize (1, scratchSize, false, false, true);
    cinemaScoopLaneScratch.setSize (1, scratchSize, false, false, true);
    cinemaMixScratch.setSize (1, scratchSize, false, false, true);
    cinemaSourceMix.reset (sampleRate, 0.03);
    cinemaSourceMix.setCurrentAndTargetValue (rt (parameterIndex ("cinemaSource")) >= 0.5f ? 1.0f : 0.0f);
    for (int ch = 0; ch < mainInputs; ++ch)
    {
        auto& trim = musicTrimGains[(size_t) ch];
        trim.reset (sampleRate, 0.02);
        trim.setCurrentAndTargetValue (native::Decibels::decibelsToGain (
            (double) rt (musicTrimBase + ch)));
    }
    for (int ch = 0; ch < cinemaInputs; ++ch)
    {
        auto& trim = cinemaTrimGains[(size_t) ch];
        trim.reset (sampleRate, 0.02);
        trim.setCurrentAndTargetValue (native::Decibels::decibelsToGain (
            (double) rt (cinemaTrimBase + ch)));
    }
    cinemaScoopLrcGain.reset (sampleRate, 0.05);
    cinemaScoopLfeGain.reset (sampleRate, 0.05);
    cinemaLfeReferenceGain.reset (sampleRate, 0.02);
    const int cinemaScoopSource = native::jlimit (0, 2, (int) std::lround (rt (parameterIndex ("cinemaScoopSource"))));
    cinemaScoopLrcGain.setCurrentAndTargetValue (cinemaScoopSource == 1 ? 0.0f : 1.0f);
    cinemaScoopLfeGain.setCurrentAndTargetValue (cinemaScoopSource == 0 ? 0.0f : 1.0f);
    cinemaLfeReferenceGain.setCurrentAndTargetValue (
        rt (parameterIndex ("cinemaLfeReferenceGain")) >= 0.5f ? native::Decibels::decibelsToGain (10.0) : 1.0);
    bassRightPolarity = bassCenterPolarity = scoopRightPolarity = scoopCenterPolarity
        = cinemaScoopRightPolarity = cinemaScoopCenterPolarity = 1;
    resetComplexPca (bassPca);
    resetComplexPca (scoopPca);
    resetComplexPca (cinemaScoopPca);

    const int delaySize = native::jmax (4, (int) std::ceil (sampleRate * maxDelaySeconds) + 4096 + samplesPerBlock + 8);
    for (int ch = 0; ch < maxOutputs; ++ch)
    {
        auto& c = channels[(size_t) ch];
        c.delayBuffer.assign ((size_t) delaySize, 0.0);
        c.delayEpochs.assign ((size_t) delaySize, 0);
        c.firEpoch.reset();
        c.writePosition = 0;
        c.delayValidSamples = 0;
        c.fractionalDelay.reset();
        fractionalDelayStatus[ch].store(0,std::memory_order_relaxed);
        for (auto& q : c.hp) q.reset();
        for (auto& q : c.lp) q.reset();
        c.allPass.reset();
        for (auto& q : c.previousHp) q.reset();
        for (auto& q : c.previousLp) q.reset();
        c.previousAllPass.reset();
        c.iirTransition = false;
        c.modeTransition = false;
        c.firAllPassTransition = false;
        clearFirAllPassHistory(c);
        c.modeIirBuffer.assign((size_t)scratchSize,0.0);
        c.modeIirEpochs.assign((size_t)scratchSize,0);
        c.fir.reset();
        c.gain.reset (sampleRate, 0.03);
        const int role = roleForChannel (ch);
        const bool startupSilence = ! isRoleEnabledByTopology (role)
            || rt (parameterIndex ("mute1") + ch) >= 0.5f
            || rt (parameterIndex ("emergencyMute1") + ch) >= 0.5f;
        const bool cinemaScene = rt (parameterIndex ("cinemaSource")) >= 0.5f;
        const double sceneGainDb = rt ((cinemaScene ? cinemaGainBase : musicGainBase) + ch);
        double startupGain = startupSilence
            ? 0.0
            : native::Decibels::decibelsToGain (sceneGainDb, -100.0);
        if (rt (parameterIndex ("polarity1") + ch) >= 0.5f)
            startupGain = -startupGain;
        c.gain.setCurrentAndTargetValue (startupGain);
        c.delaySamples.reset (sampleRate, 0.04);
        const int sceneDelayBase = cinemaScene ? cinemaDelayBase : musicDelayBase;
        const double effectiveDelayMs = native::jlimit (0.0, maxDelaySeconds * 1000.0,
            (double) rt (sceneDelayBase + ch));
        c.delaySamples.setCurrentAndTargetValue (effectiveDelayMs * 0.001 * sampleRate);
        c.tripFrames = 0;
        c.protectionTripped = false;
        protectionState[(size_t) ch].store (0);
        c.safetySilenced = startupSilence;
    }

    serviceUpdates();
    // prepare is quiescent; expose the prepared graph before its first block.
    const int preparedSlot = pending.load(std::memory_order_relaxed);
    reportedFirLatency.store(active ? active->latency
        : (preparedSlot >= 0 ? configs[(size_t) preparedSlot].latency : 0),
        std::memory_order_relaxed);
}

float EntropyCrossoverAudioProcessor::parameter (const native::String& id) const noexcept
{
    if (auto* value = apvts.getRawParameterValue (id)) return value->load();
    return 0.0f;
}

void EntropyCrossoverAudioProcessor::setLowPass (Biquad& q, double sr, double frequency, double Q)
{
    frequency = native::jlimit (2.0, sr * 0.45, frequency);
    const double w0 = native::MathConstants<double>::twoPi * frequency / sr;
    const double alpha = std::sin (w0) / (2.0 * Q);
    const double cosw = std::cos (w0);
    const double a0 = 1.0 + alpha;
    q.b0 = (1.0 - cosw) * 0.5 / a0;
    q.b1 = (1.0 - cosw) / a0;
    q.b2 = q.b0;
    q.a1 = -2.0 * cosw / a0;
    q.a2 = (1.0 - alpha) / a0;
}

void EntropyCrossoverAudioProcessor::setHighPass (Biquad& q, double sr, double frequency, double Q)
{
    frequency = native::jlimit (2.0, sr * 0.45, frequency);
    const double w0 = native::MathConstants<double>::twoPi * frequency / sr;
    const double alpha = std::sin (w0) / (2.0 * Q);
    const double cosw = std::cos (w0);
    const double a0 = 1.0 + alpha;
    q.b0 = (1.0 + cosw) * 0.5 / a0;
    q.b1 = -(1.0 + cosw) / a0;
    q.b2 = q.b0;
    q.a1 = -2.0 * cosw / a0;
    q.a2 = (1.0 - alpha) / a0;
}

void EntropyCrossoverAudioProcessor::setAllPass (Biquad& q, double sr, double frequency, double Q)
{
    frequency = native::jlimit (2.0, sr * 0.45, frequency);
    Q = native::jlimit (0.2, 4.0, Q);
    const double w0 = native::MathConstants<double>::twoPi * frequency / sr;
    const double alpha = std::sin (w0) / (2.0 * Q);
    const double cosw = std::cos (w0);
    const double a0 = 1.0 + alpha;
    q.b0 = (1.0 - alpha) / a0;
    q.b1 = -2.0 * cosw / a0;
    q.b2 = 1.0;
    q.a1 = -2.0 * cosw / a0;
    q.a2 = (1.0 - alpha) / a0;
}

void EntropyCrossoverAudioProcessor::updateFir (int channel, double sampleRate)
{
    const int role = roleForChannel (channel);
    if (role == 0) return;
    const int mode = native::jlimit (0, 1, (int) std::lround (rt (parameterIndex ("processMode1") + channel)));
    if (mode != 1) return;

    const bool bassLane = role >= 16 && role <= 19;
    const bool cinemaLfeLane = rt (parameterIndex ("cinemaSource")) >= 0.5f && (role == 16 || role == 18);
    const double requestedMax = cinemaLfeLane ? 240.0 : (bassLane ? 5000.0 : (((role >= 1 && role <= 10) && (int) std::lround (rt (parameterIndex ("lrBandLimit"))) == 0) ? 40000.0 : 60000.0));
    const double maxLane = native::jmax (2.0, native::jmin (requestedMax, sampleRate * 0.45));
    const int lengthIndex = native::jlimit (0, 2, (int) std::lround (rt (parameterIndex ("firLength1") + channel)));
    const int taps = lengthIndex == 0 ? 2049 : (lengthIndex == 1 ? 4097 : 8193);
    const bool hpOn = rt (parameterIndex ("hpOn1") + channel) >= 0.5f;
    const bool lpOn = rt (parameterIndex ("lpOn1") + channel) >= 0.5f;
    const double hp = native::jlimit (2.0, maxLane, (double) rt (parameterIndex ("hpHz1") + channel)); // lower edge
    const double lp = native::jlimit (2.0, maxLane, (double) rt (parameterIndex ("lpHz1") + channel)); // upper edge
    building->effectiveHpHz[channel]=hp;
    building->effectiveLpHz[channel]=lp;

    native::AudioBuffer<double> ir (1, taps);
    auto* dst = ir.getWritePointer (0);
    const int mid = (taps - 1) / 2;
    const double twoPi = native::MathConstants<double>::twoPi;
    const auto idealLowPass = [sampleRate, twoPi] (double cutoff, int n) -> double
    {
        if (n == 0) return 2.0 * cutoff / sampleRate;
        return std::sin (twoPi * cutoff * (double) n / sampleRate) / (native::MathConstants<double>::pi * (double) n);
    };
    for (int i = 0; i < taps; ++i)
    {
        const int n = i - mid;
        double v = 0.0;
        if (hpOn && lpOn)
        {
            // HP is always the lower edge and LP is always the upper edge.
            // If the user crosses them, use the ordered pair rather than producing an inverted response.
            const double lower = native::jmin (hp, lp);
            const double upper = native::jmax (hp, lp);
            v = idealLowPass (upper, n) - idealLowPass (lower, n);
        }
        else if (lpOn) v = idealLowPass (lp, n);
        else if (hpOn) v = (n == 0 ? 1.0 : 0.0) - idealLowPass (hp, n);
        else v = n == 0 ? 1.0 : 0.0;
        const double phase = twoPi * (double) i / (double) (taps - 1);
        const double window = 0.42 - 0.5 * std::cos (phase) + 0.08 * std::cos (2.0 * phase);
        dst[i] = v * window;
    }
    building->kernels[(size_t)channel].build(dst,taps,kernelFft);
}

bool EntropyCrossoverAudioProcessor::isRoleEnabledByTopology (int role) const noexcept
{
    const int lrWays = native::jlimit (1, 5, (int) std::lround (rt (parameterIndex ("lrWays"))));
    const int centerWays = native::jlimit (1, 2, (int) std::lround (rt (parameterIndex ("centerWays"))));
    if (role >= 1 && role <= 5) return role <= lrWays;
    if (role >= 6 && role <= 10) return role - 5 <= lrWays;
    if (role >= 11 && role <= 15) return role - 10 <= centerWays;
    if (role == 16) return true;
    if (role == 17) return true;
    if (role == 18) return (int) std::lround (rt (parameterIndex ("subSpeakers"))) >= 2;
    if (role == 19) return (int) std::lround (rt (parameterIndex ("scoopSpeakers"))) >= 2;
    if (role >= 20 && role <= 25) return true;
    return false;
}

bool EntropyCrossoverAudioProcessor::isRoleEnabledByLiveTopology (int role) const noexcept
{
    const int lrWays = native::jlimit (1, 5, (int) std::lround (liveRt (parameterIndex("lrWays"))));
    const int centerWays = native::jlimit (1, 2, (int) std::lround (liveRt (parameterIndex("centerWays"))));
    if (role >= 1 && role <= 5) return role <= lrWays;
    if (role >= 6 && role <= 10) return role - 5 <= lrWays;
    if (role >= 11 && role <= 15) return role - 10 <= centerWays;
    if (role == 16 || role == 17) return true;
    if (role == 18) return (int) std::lround (liveRt (parameterIndex("subSpeakers"))) >= 2;
    if (role == 19) return (int) std::lround (liveRt (parameterIndex("scoopSpeakers"))) >= 2;
    if (role >= 20 && role <= 25) return true;
    return false;
}

int EntropyCrossoverAudioProcessor::firGroupDelayForChannel (int channel) const noexcept
{
    const int role = roleForChannel (channel);
    if (! isRoleEnabledByTopology (role))
        return 0;
    if (rt (parameterIndex ("cinemaSource")) >= 0.5f && (role == 16 || role == 18)
        && rt (parameterIndex ("cinemaLfeFilterBypass")) >= 0.5f)
        return 0;
    if ((int) std::lround (rt (parameterIndex ("processMode1") + channel)) != 1)
        return 0;
    const int index = native::jlimit (0, 2, (int) std::lround (rt (parameterIndex ("firLength1") + channel)));
    return index == 0 ? 1024 : (index == 1 ? 2048 : 4096);
}

void EntropyCrossoverAudioProcessor::refreshReportedLatency()
{
    int latency = 0;
    for (int ch = 0; ch < maxOutputs; ++ch)
        latency = native::jmax (latency, firGroupDelayForChannel (ch));
    reportedFirLatency.store (latency, std::memory_order_relaxed);
}

int EntropyCrossoverAudioProcessor::stagesForFilterType (int filterType) noexcept
{
    switch (filterType)
    {
        case 0: return 1; // BW12
        case 1: return 2; // BW24
        case 2: return 2; // LR24 = BW12 x 2
        case 3: return 4; // LR48 = BW24 x 2
        case 4: return 3; // BW36
        case 5: return 4; // BW48
        default: return 2;
    }
}

double EntropyCrossoverAudioProcessor::processFilter (std::array<Biquad, 4>& stages, int filterType, double x) noexcept
{
    const int count = stagesForFilterType (filterType);
    for (int i = 0; i < count; ++i) x = stages[(size_t) i].process (x);
    return x;
}

void EntropyCrossoverAudioProcessor::compileCoefficients (int channel, double sampleRate)
{
    const int role = roleForChannel (channel);
    const bool bassLane = role >= 16 && role <= 19;
    const bool lrLane = role >= 1 && role <= 10;
    const bool cinemaLfeLane = rt (parameterIndex ("cinemaSource")) >= 0.5f && (role == 16 || role == 18);
    const float lrLimit = (int) std::lround (rt (parameterIndex ("lrBandLimit"))) == 1 ? 60000.0f : 40000.0f;
    const float maxForLane = cinemaLfeLane ? 240.0f : (bassLane ? 5000.0f : (lrLane ? lrLimit : maxFilterHz));
    const float hp = native::jlimit (2.0f, maxForLane, rt (parameterIndex ("hpHz1") + channel));
    const float lp = native::jlimit (2.0f, maxForLane, rt (parameterIndex ("lpHz1") + channel));
    building->effectiveHpHz[channel]=native::jlimit(2.0,sampleRate*0.45,(double)hp);
    building->effectiveLpHz[channel]=native::jlimit(2.0,sampleRate*0.45,(double)lp);
    const bool hpOn = rt (parameterIndex ("hpOn1") + channel) >= 0.5f;
    const bool lpOn = rt (parameterIndex ("lpOn1") + channel) >= 0.5f;
    const int type = native::jlimit (0, 5, (int) std::lround (rt (parameterIndex ("filterType1") + channel)));


    if (true)
    {
        auto& s = building->hp[(size_t) channel];
        if (type == 0) { setHighPass (s[0], sampleRate, hp, qButterworth2); }
        else if (type == 1) { setHighPass (s[0], sampleRate, hp, qButterworth4a); setHighPass (s[1], sampleRate, hp, qButterworth4b); }
        else if (type == 2) { setHighPass (s[0], sampleRate, hp, qButterworth2); setHighPass (s[1], sampleRate, hp, qButterworth2); }
        else if (type == 3) { setHighPass (s[0], sampleRate, hp, qButterworth4a); setHighPass (s[1], sampleRate, hp, qButterworth4b); setHighPass (s[2], sampleRate, hp, qButterworth4a); setHighPass (s[3], sampleRate, hp, qButterworth4b); }
        else if (type == 4) { setHighPass (s[0], sampleRate, hp, qButterworth6a); setHighPass (s[1], sampleRate, hp, qButterworth6b); setHighPass (s[2], sampleRate, hp, qButterworth6c); }
        else { setHighPass (s[0], sampleRate, hp, qButterworth8a); setHighPass (s[1], sampleRate, hp, qButterworth8b); setHighPass (s[2], sampleRate, hp, qButterworth8c); setHighPass (s[3], sampleRate, hp, qButterworth8d); }
    }
    if (true)
    {
        auto& s = building->lp[(size_t) channel];
        if (type == 0) { setLowPass (s[0], sampleRate, lp, qButterworth2); }
        else if (type == 1) { setLowPass (s[0], sampleRate, lp, qButterworth4a); setLowPass (s[1], sampleRate, lp, qButterworth4b); }
        else if (type == 2) { setLowPass (s[0], sampleRate, lp, qButterworth2); setLowPass (s[1], sampleRate, lp, qButterworth2); }
        else if (type == 3) { setLowPass (s[0], sampleRate, lp, qButterworth4a); setLowPass (s[1], sampleRate, lp, qButterworth4b); setLowPass (s[2], sampleRate, lp, qButterworth4a); setLowPass (s[3], sampleRate, lp, qButterworth4b); }
        else if (type == 4) { setLowPass (s[0], sampleRate, lp, qButterworth6a); setLowPass (s[1], sampleRate, lp, qButterworth6b); setLowPass (s[2], sampleRate, lp, qButterworth6c); }
        else { setLowPass (s[0], sampleRate, lp, qButterworth8a); setLowPass (s[1], sampleRate, lp, qButterworth8b); setLowPass (s[2], sampleRate, lp, qButterworth8c); setLowPass (s[3], sampleRate, lp, qButterworth8d); }
    }

    setAllPass(building->ap[channel],sampleRate,native::jlimit(2.0f,maxForLane,rt (parameterIndex ("apFreq1")+channel)),rt (parameterIndex ("apQ1")+channel));
}

double EntropyCrossoverAudioProcessor::readDelayedSample (ChannelDSP& c, double input, double delaySamples, uint64_t epoch) noexcept
{
    auto& ring = c.delayBuffer;
    if (ring.empty()) return input;
    ring[(size_t) c.writePosition] = input;
    if(traceOutputEpoch || c.delayValidSamples<ring.size())
        c.delayEpochs[(size_t) c.writePosition] = traceOutputEpoch?epoch:0;
    c.delayValidSamples=std::min(ring.size(),c.delayValidSamples+1);
    delaySamples = native::jlimit (0.0, (double) ring.size() - 3.0, delaySamples);
    double read = (double) c.writePosition - delaySamples;
    while (read < 0.0) read += (double) ring.size();
    const int i0 = (int) std::floor (read) % (int) ring.size();
    const int i1 = (i0 + 1) % (int) ring.size();
    const double frac = read - std::floor (read);
    // Both indices are in [0,size): wrapped age needs at most one correction.
    // A full ring stays valid until reset, so its steady path needs no age math.
    const bool full=c.delayValidSamples==ring.size();
    const auto valid=[&](int index){return full || size_t(index<=c.writePosition ? c.writePosition-index : c.writePosition+int(ring.size())-index)<c.delayValidSamples;};
    const double a=valid(i0)?ring[(size_t)i0]:0.0,b=valid(i1)?ring[(size_t)i1]:0.0;
    const double output = a + frac * (b - a);
    if(traceOutputEpoch) {
        const auto produced = std::max(valid(i0)?c.delayEpochs[(size_t)i0]:0, frac>0.0&&valid(i1)?c.delayEpochs[(size_t)i1]:0);
        c.delayedEpoch=produced;
    }
    const auto tap=[&](int samples) noexcept -> DelaySample {
        int index=c.writePosition-samples;
        if(index<0) index+=int(ring.size());
        const bool present=valid(index);
        return {present?ring[size_t(index)]:0.0,
                traceOutputEpoch && present?c.delayEpochs[size_t(index)]:0};
    };
    const auto result=c.fractionalDelay.process(tap,{output,traceOutputEpoch?c.delayedEpoch:0});
    if(traceOutputEpoch) c.delayedEpoch=result.epoch;
    if (++c.writePosition >= (int) ring.size()) c.writePosition = 0;
    return result.value;
}

void EntropyCrossoverAudioProcessor::captureLiveSpectrumBlock (
    int channel, const double* samples, int sampleCount) noexcept
{
    const uint64_t monitor=visualization.monitorIdentity.load(std::memory_order_acquire);
    const uint64_t generation=active?active->viewGeneration:0;
    const int requested=int(monitor&31u)-1;
    viewCaptureRevision.fetch_add(1,std::memory_order_acq_rel);
    if (requested != liveSpectrumActiveChannel || monitor!=viewActiveMonitor || generation!=viewActiveGeneration)
    {
        viewActiveMonitor=monitor;viewActiveGeneration=generation;viewTransitionUntil=0;
        liveSpectrumActiveChannel = requested;
        liveSpectrumProducerSamples = 0;
        liveSpectrumPublishedSamples.store (0, std::memory_order_release);
        liveSpectrumPeakHold.store (0.0f, std::memory_order_relaxed);
        liveSpectrumDeltaHold.store (0.0f, std::memory_order_relaxed);
        liveSpectrumStatSamples.store (0, std::memory_order_relaxed);
        liveSpectrumPreviousSample = 0.0f;
        liveSpectrumHasPreviousSample = false;
    }
    if (channel != liveSpectrumActiveChannel || samples == nullptr || sampleCount <= 0){viewCaptureRevision.fetch_add(1,std::memory_order_release);return;}

    constexpr uint64_t mask = (uint64_t) liveSpectrumSamples - 1u;
    static_assert ((liveSpectrumSamples & (liveSpectrumSamples - 1)) == 0,
                   "Live spectrum ring size must be a power of two");
    float blockPeak = 0.0f;
    float blockDelta = 0.0f;
    float previous = liveSpectrumPreviousSample;
    bool hasPrevious = liveSpectrumHasPreviousSample;
    for (int i = 0; i < sampleCount; ++i)
    {
        const float sample = static_cast<float> (samples[i]);
        liveSpectrumRing[(size_t) ((liveSpectrumProducerSamples + (uint64_t) i) & mask)]
            .store (sample, std::memory_order_relaxed);
        blockPeak = native::jmax (blockPeak, std::abs (sample));
        if (hasPrevious)
            blockDelta = native::jmax (blockDelta, std::abs (sample - previous));
        previous = sample;
        hasPrevious = true;
    }
    liveSpectrumPreviousSample = previous;
    liveSpectrumHasPreviousSample = hasPrevious;

    const auto atomicMaximum = [] (std::atomic<float>& destination, float value) noexcept
    {
        float current = destination.load (std::memory_order_relaxed);
        while (current < value
               && ! destination.compare_exchange_weak (current, value,
                    std::memory_order_release, std::memory_order_relaxed)) {}
    };
    atomicMaximum (liveSpectrumPeakHold, blockPeak);
    atomicMaximum (liveSpectrumDeltaHold, blockDelta);
    liveSpectrumStatSamples.fetch_add ((uint64_t) sampleCount, std::memory_order_release);
    liveSpectrumProducerSamples += (uint64_t) sampleCount;
    liveSpectrumPublishedSamples.store (liveSpectrumProducerSamples, std::memory_order_release);
    if(visualizationTransition())viewTransitionUntil=liveSpectrumProducerSamples+visual::fftSize;
    LARGE_INTEGER captured{};QueryPerformanceCounter(&captured);
    viewCaptureMonitor.store(monitor,std::memory_order_relaxed);viewCaptureGeneration.store(generation,std::memory_order_relaxed);
    viewCaptureDescriptor.store((uint64_t(active && active->values[parameterIndex("cinemaSource")]>=.5f))|(liveSpectrumProducerSamples<viewTransitionUntil?2u:0u),std::memory_order_relaxed);
    viewCaptureQpc.store(uint64_t(captured.QuadPart),std::memory_order_relaxed);viewFrameSequence.fetch_add(1,std::memory_order_relaxed);
    viewCaptureRevision.fetch_add(1,std::memory_order_release);
}

bool EntropyCrossoverAudioProcessor::copyLiveSpectrumSamples (
    float* destination, int sampleCount) const noexcept
{
    if (destination == nullptr || sampleCount <= 0 || sampleCount > liveSpectrumSamples)
        return false;
    const auto end = liveSpectrumPublishedSamples.load (std::memory_order_acquire);
    if (end < (uint64_t) sampleCount)
        return false;
    constexpr uint64_t mask = (uint64_t) liveSpectrumSamples - 1u;
    for (int i = 0; i < sampleCount; ++i)
        destination[i] = liveSpectrumRing[(size_t) ((end - (uint64_t) sampleCount + (uint64_t) i) & mask)]
            .load (std::memory_order_relaxed);
    return liveSpectrumPublishedSamples.load (std::memory_order_acquire) == end;
}

EntropyCrossoverAudioProcessor::LiveSpectrumStats
EntropyCrossoverAudioProcessor::consumeLiveSpectrumStats() noexcept
{
    LiveSpectrumStats result;
    result.peak = liveSpectrumPeakHold.exchange (0.0f, std::memory_order_acq_rel);
    result.maximumDelta = liveSpectrumDeltaHold.exchange (0.0f, std::memory_order_acq_rel);
    result.sampleCount = liveSpectrumStatSamples.exchange (0, std::memory_order_acq_rel);
    return result;
}

void EntropyCrossoverAudioProcessor::buildBassLane (
    const native::AudioBuffer<double>& source, native::AudioBuffer<double>& destination,
    int samples, int feed, int extractorMode,
    int& rightPolarity, int& centerPolarity, ComplexPcaLane& pcaState)
{
    auto* dst = destination.getWritePointer (0);
    const auto* left = source.getReadPointer (0);
    const auto* right = source.getReadPointer (1);
    const auto* center = source.getReadPointer (2);
    feed = native::jlimit (0, 4, feed);
    extractorMode = native::jlimit (0, 3, extractorMode);
    if (extractorMode == 3)
    {
        buildComplexPcaLane (source, destination, samples, feed, pcaState);
        return;
    }
    pcaState.running = false;

    const bool useL = feed == 0 || feed == 1 || feed == 2;
    const bool useR = feed == 0 || feed == 1 || feed == 3;
    const bool useC = feed == 1 || feed == 4;
    const int channelCount = native::jmax (1, (int) useL + (int) useR + (int) useC);

    double energyL = 0.0, energyR = 0.0, energyC = 0.0;
    double correlationLR = 0.0, correlationLC = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        if (useL) energyL += (double) left[i] * left[i];
        if (useR) energyR += (double) right[i] * right[i];
        if (useC) energyC += (double) center[i] * center[i];
        if (useL && useR) correlationLR += (double) left[i] * right[i];
        if (useL && useC) correlationLC += (double) left[i] * center[i];
    }

    // Hysteresis prevents polarity chatter around zero correlation.  PHASE SAFE
    // only changes the extracted mono lane; the original L/R/C paths are untouched.
    if (extractorMode == 2)
    {
        const double lrThreshold = 0.20 * std::sqrt (native::jmax (1.0e-24, energyL * energyR));
        const double lcThreshold = 0.20 * std::sqrt (native::jmax (1.0e-24, energyL * energyC));
        if (useL && useR)
        {
            if (correlationLR < -lrThreshold) rightPolarity = -1;
            else if (correlationLR > lrThreshold) rightPolarity = 1;
        }
        if (useL && useC)
        {
            if (correlationLC < -lcThreshold) centerPolarity = -1;
            else if (correlationLC > lcThreshold) centerPolarity = 1;
        }
    }
    else
    {
        rightPolarity = centerPolarity = 1;
    }

    for (int i = 0; i < samples; ++i)
    {
        double sum = 0.0;
        if (useL) sum += left[i];
        if (useR) sum += right[i] * (double) rightPolarity;
        if (useC) sum += center[i] * (double) centerPolarity;
        dst[i] = sum / (double) channelCount;
    }
}

void EntropyCrossoverAudioProcessor::resetComplexPca (ComplexPcaLane& lane)
{
    lane = {};
    static constexpr std::array<double, 5> boundaries { 40.0, 63.0, 98.0, 152.0, 240.0 };
    static constexpr std::array<double, 5> centres { 32.0, 50.0, 78.0, 122.0, 190.0 };
    const double sampleRate = native::jmax (1.0, runtimeSampleRate.load (std::memory_order_relaxed));
    for (size_t k = 0; k < boundaries.size(); ++k)
    {
        lane.lowPassAlpha[k] = -std::expm1 (-native::MathConstants<double>::twoPi * boundaries[k] / sampleRate);
        lane.envelopeAlpha[k] = -std::expm1 (-native::MathConstants<double>::twoPi * centres[k] * 0.55 / sampleRate);
        lane.phaseIncrement[k] = native::MathConstants<double>::twoPi * centres[k] / sampleRate;
    }
    lane.covarianceAlpha = -std::expm1 (-1.0 / (sampleRate * 0.35));
    lane.weightAlpha = -std::expm1 (-1.0 / (sampleRate * 0.03));
}

void EntropyCrossoverAudioProcessor::buildComplexPcaLane (
    const native::AudioBuffer<double>& source, native::AudioBuffer<double>& destination,
    int samples, int feed, ComplexPcaLane& lane)
{
    auto* dst = destination.getWritePointer (0);
    const std::array<const double*, 3> inputs {
        source.getReadPointer (0), source.getReadPointer (1), source.getReadPointer (2)
    };
    feed = native::jlimit (0, 4, feed);
    const std::array<bool, 3> enabled {
        feed == 0 || feed == 1 || feed == 2,
        feed == 0 || feed == 1 || feed == 3,
        feed == 1 || feed == 4
    };
    const int enabledCount = native::jmax (1, (int) enabled[0] + (int) enabled[1] + (int) enabled[2]);
    const double fixedWeight = 1.0 / (double) enabledCount;
    const bool cinemaLane = &lane == &cinemaScoopPca;
    const uint64_t sourceSession = cinemaLane ? 0 : inputMusicSession;
    const uint64_t sourceGeneration = cinemaLane ? inputCinemaGeneration : inputMusicGeneration;
    const bool newSource = ! lane.running || lane.sourceSession != sourceSession || lane.sourceGeneration != sourceGeneration;
    const bool newFeed = lane.feed != feed;
    if (newSource || newFeed)
    {
        // Bounded estimator invalidation only. Coefficients are prepared off the
        // callback; a feed edit keeps the continuously updated LP histories.
        for (auto& band : lane.bands)
        {
            band.envelope = {}; band.covariance = {}; band.reference = -1;
            for (int ch = 0; ch < 3; ++ch)
            {
                band.eigenvector[(size_t) ch] = enabled[(size_t) ch] ? std::complex<double> (1.0 / std::sqrt ((double) enabledCount), 0.0) : std::complex<double>();
                if (newSource) band.weights[(size_t) ch] = enabled[(size_t) ch] ? fixedWeight : 0.0;
            }
            if (newSource) band.phase = 0.0;
        }
        if (newSource) lane.lowPass = {};
        lane.feed = feed; lane.sourceSession = sourceSession; lane.sourceGeneration = sourceGeneration;
        lane.running = true;
    }

    // Power iteration on the smoothed complex covariance finds the dominant
    // coherent L/R/C component in each band. Aligning the new eigenvector to
    // the previous one prevents arbitrary sign/phase flips between blocks.
    for (auto& band : lane.bands)
    {
        auto old = band.eigenvector;
        auto v = old;
        double trace = 0.0, maximumEnergy = 0.0;
        for (int ch = 0; ch < 3; ++ch)
            if (enabled[(size_t) ch])
            {
                const double energy = native::jmax (0.0, band.covariance[(size_t) ch][(size_t) ch].real());
                trace += energy;
                maximumEnergy = native::jmax (maximumEnergy, energy);
            }
        // Retaining only the old seed can lock on a nonprincipal eigenvector
        // after coherent polarity changes. Compare bounded four-step iterations
        // from it and each selected coordinate; retain the best Rayleigh value.
        double bestEnergy = -1.0;
        for (int seed = -1; seed < 3; ++seed)
        {
            if (seed >= 0 && ! enabled[(size_t) seed]) continue;
            auto candidate = seed < 0 ? old : std::array<std::complex<double>, 3> {};
            if (seed >= 0) candidate[(size_t) seed] = 1.0;
            for (size_t ch = 0; ch < 3; ++ch)
                if (! enabled[ch]) candidate[ch] = {};
            for (int iteration = 0; iteration < 4; ++iteration)
            {
                std::array<std::complex<double>, 3> next {};
                for (size_t row = 0; row < 3; ++row)
                    if (enabled[row])
                        for (size_t column = 0; column < 3; ++column)
                            if (enabled[column]) next[row] += band.covariance[row][column] * candidate[column];
                double norm = 0.0;
                for (const auto& value : next) norm += std::norm (value);
                norm = std::sqrt (norm);
                if (norm < 1.0e-24) break;
                for (auto& value : next) value /= norm;
                candidate = next;
            }
            double energy = 0.0;
            for (size_t row = 0; row < 3; ++row)
                if (enabled[row])
                    for (size_t column = 0; column < 3; ++column)
                        if (enabled[column]) energy += (std::conj (candidate[row]) * band.covariance[row][column] * candidate[column]).real();
            if (energy > bestEnergy + native::jmax (1.0e-24, trace * 1.0e-12))
            { bestEnergy = energy; v = candidate; }
        }
        std::complex<double> continuity {};
        for (int ch = 0; ch < 3; ++ch) continuity += std::conj (old[(size_t) ch]) * v[(size_t) ch];
        if (std::abs (continuity) > 1.0e-12)
        {
            const auto rotation = std::polar (1.0, -std::arg (continuity));
            for (auto& value : v) value *= rotation;
        }
        band.eigenvector = v;
        // Keep the first usable reference until it is below 1% of the strongest
        // channel energy. Targets are computed once per block, never per sample.
        const double referenceFloor = native::jmax (1.0e-24, maximumEnergy * 0.01);
        if (band.reference < 0 || ! enabled[(size_t) band.reference]
            || band.covariance[(size_t) band.reference][(size_t) band.reference].real() < referenceFloor)
        {
            band.reference = -1;
            for (int ch = 0; ch < 3; ++ch)
                if (enabled[(size_t) ch] && band.covariance[(size_t) ch][(size_t) ch].real() >= referenceFloor)
                { band.reference = ch; break; }
        }
        std::array<double, 3> q {};
        double denominator = 0.0, lambda = 0.0;
        for (int row = 0; row < 3; ++row)
            if (enabled[(size_t) row])
            {
                std::complex<double> rv {};
                for (int col = 0; col < 3; ++col)
                    if (enabled[(size_t) col]) rv += band.covariance[(size_t) row][(size_t) col] * v[(size_t) col];
                lambda += (std::conj (v[(size_t) row]) * rv).real();
                if (band.reference >= 0)
                {
                    const auto ref = v[(size_t) band.reference];
                    const double amplitude = std::abs (v[(size_t) row]);
                    q[(size_t) row] = amplitude * (v[(size_t) row] * std::conj (ref)).real()
                        / native::jmax (1.0e-24, amplitude * std::abs (ref));
                    denominator += std::abs (q[(size_t) row]);
                }
            }
        const double coherence = enabledCount == 1 ? 1.0 : native::jlimit (0.0, 1.0,
            (lambda / native::jmax (1.0e-24, trace) - fixedWeight) / (1.0 - fixedWeight));
        const double confidence = coherence * trace / (trace + 1.0e-16);
        for (int ch = 0; ch < 3; ++ch)
            band.targetWeights[(size_t) ch] = enabled[(size_t) ch]
                ? (1.0 - confidence) * fixedWeight + confidence * (denominator > 1.0e-12 ? q[(size_t) ch] / denominator : fixedWeight)
                : 0.0;
    }

    for (int i = 0; i < samples; ++i)
    {
        std::array<double, 3> previousLowPass {};
        double output = 0.0;
        for (size_t k = 0; k < lane.bands.size(); ++k)
        {
            auto& band = lane.bands[k];
            // Existing complex analysis is an estimator only. All three inputs
            // stay current, even when excluded from the active spatial feed.
            const std::complex<double> oscillator (std::cos (band.phase), -std::sin (band.phase));
            for (size_t ch = 0; ch < 3; ++ch)
            {
                const auto target = inputs[ch][i] * oscillator;
                band.envelope[ch] += lane.envelopeAlpha[k] * (target - band.envelope[ch]);
            }
            for (size_t row = 0; row < 3; ++row)
                for (size_t column = 0; column < 3; ++column)
                {
                    const auto observation = band.envelope[row] * std::conj (band.envelope[column]);
                    band.covariance[row][column] += lane.covarianceAlpha * (observation - band.covariance[row][column]);
                }
            band.phase += lane.phaseIncrement[k];
            if (band.phase >= native::MathConstants<double>::twoPi) band.phase -= native::MathConstants<double>::twoPi;

            std::array<double, 3> weights {};
            double denominator = 0.0;
            for (size_t ch = 0; ch < 3; ++ch)
            {
                band.weights[ch] += lane.weightAlpha * (band.targetWeights[ch] - band.weights[ch]);
                weights[ch] = enabled[ch] ? band.weights[ch] : 0.0;
                denominator += std::abs (weights[ch]);
            }
            for (size_t ch = 0; ch < 3; ++ch)
            {
                auto& lp = lane.lowPass[k][ch];
                lp[0] += lane.lowPassAlpha[k] * (inputs[ch][i] - lp[0]);
                lp[1] += lane.lowPassAlpha[k] * (lp[0] - lp[1]);
                const double subband = lp[1] - previousLowPass[ch];
                previousLowPass[ch] = lp[1];
                if (enabled[ch])
                {
                    const double w = enabledCount == 1 || denominator < 1.0e-12
                        ? fixedWeight : weights[ch] / denominator;
                    output += w * subband;
                }
            }
        }
        for (size_t ch = 0; ch < 3; ++ch)
            if (enabled[ch]) output += fixedWeight * (inputs[ch][i] - previousLowPass[ch]);
        dst[i] = output;
    }

}

void EntropyCrossoverAudioProcessor::processBlock (native::AudioBuffer<double>& buffer, native::MidiBuffer&)
{
    // IMPORTANT: prepareToPlay(sampleRate, ...) is the authoritative host DSP rate.
    // Do not overwrite it here with AudioProcessor::getSampleRate(), because some
    // hosts/wrapper states can expose a stale/default value to the editor.
    runtimeBlockSize.store (buffer.getNumSamples(), std::memory_order_relaxed);
    runtimeInputs.store (getTotalNumInputChannels(), std::memory_order_relaxed);
    runtimeOutputs.store (getMainBusNumOutputChannels(), std::memory_order_relaxed);
    native::ScopedNoDenormals noDenormals;
    beginRt();
    struct Finish { EntropyCrossoverAudioProcessor& p; int n; ~Finish(){p.endRt(n);} } finish{*this, buffer.getNumSamples()};
    if(!active){buffer.clear();return;}
    const int samples = buffer.getNumSamples();
    auto input = buffer.slice (0, mainInputs);
    auto cinema = buffer.slice (mainInputs, cinemaInputs);
    auto output = buffer.slice (0, maxOutputs);

    if (samples > inputScratch.getNumSamples() || samples > (int)blockOutputEpochs.size())
    {
        assert(false);
        output.clear();
        return;
    }
    for (int ch = 0; ch < mainInputs; ++ch)
    {
        if (ch < input.getNumChannels()) inputScratch.copyFrom (ch, 0, input, ch, 0, samples);
        else inputScratch.clear (ch, 0, samples);
        auto& trim = musicTrimGains[(size_t) ch];
        trim.setTargetValue (native::Decibels::decibelsToGain (
            (double) rt (musicTrimBase + ch)));
        auto* musicData = inputScratch.getWritePointer (ch);
        for (int i = 0; i < samples; ++i)
            musicData[i] *= trim.getNextValue();
        mainInputPeaks[(size_t) ch].store ((float) inputScratch.getMagnitude (ch, 0, samples), std::memory_order_relaxed);
    }
    for (int ch = 0; ch < cinemaInputs; ++ch)
    {
        if (ch < cinema.getNumChannels()) cinemaScratch.copyFrom (ch, 0, cinema, ch, 0, samples);
        else cinemaScratch.clear (ch, 0, samples);
        auto& trim = cinemaTrimGains[(size_t) ch];
        trim.setTargetValue (native::Decibels::decibelsToGain (
            (double) rt (cinemaTrimBase + ch)));
        auto* cinemaData = cinemaScratch.getWritePointer (ch);
        if (ch == 3)
            cinemaLfeReferenceGain.setTargetValue (
                rt (parameterIndex ("cinemaLfeReferenceGain")) >= 0.5f ? native::Decibels::decibelsToGain (10.0) : 1.0);
        for (int i = 0; i < samples; ++i)
            cinemaData[i] *= trim.getNextValue() * (ch == 3 ? cinemaLfeReferenceGain.getNextValue() : 1.0);
        const double peak = cinemaScratch.getMagnitude (ch, 0, samples);
        cinemaInputPeaks[(size_t) ch].store ((float) peak, std::memory_order_relaxed);
    }
    output.clear();

    // Program selection is a DSP-level interlock. Keep a per-sample ramp so
    // even completely different MUSIC/CINEMA output maps switch without clicks.
    cinemaSourceMix.setTargetValue (rt (parameterIndex ("cinemaSource")) >= 0.5f ? 1.0f : 0.0f);
    auto* cinemaMixValues = cinemaMixScratch.getWritePointer (0);
    for (int i = 0; i < samples; ++i)
        cinemaMixValues[i] = cinemaSourceMix.getNextValue();
    // Observe the adopted RT snapshot; APVTS may already request another one.
    std::fill(blockOutputEpochs.begin(),blockOutputEpochs.end(),0);
    programBlock.firstTransitionFrame=UINT32_MAX; programBlock.transitions=0;
    const uint32_t appliedProgram = rt (parameterIndex ("cinemaSource")) >= 0.5f ? 1u : 0u;
    const auto sourceSession=appliedProgram?0:inputMusicSession;
    const auto sourceGeneration=appliedProgram?inputCinemaGeneration:inputMusicGeneration;
    if(inputEpochToken==0 || (inputEpochToken&1u)!=appliedProgram ||
       epochMusicSession!=sourceSession || epochGeneration!=sourceGeneration) {
        previousInputEpochToken=inputEpochToken;
        inputEpochToken=(((inputEpochToken>>1)+1)<<1)|appliedProgram;
        epochMusicSession=sourceSession; epochGeneration=sourceGeneration;
    }
    // Only unseen epochs need tracing. Stale tag histories are <= the emitted
    // high-water mark and cannot trigger another event; new tags follow current
    // audio write positions. No bulk history refill on the audio thread.
    traceOutputEpoch=inputEpochToken>programBlock.outputToken;
    if (programBlock.sequence == 0 || programBlock.program != appliedProgram)
        ++programBlock.revision;
    programBlock.program = appliedProgram;
    ++programBlock.sequence;
    programBlock.requiredInputs = (cinemaSourceMix.getCurrentValue() < 1.0 ? 1u : 0u)
                                | (cinemaSourceMix.getCurrentValue() > 0.0 ? 2u : 0u)
                                | (appliedProgram == 0 ? 1u : 2u);

    const int linkMode = native::jlimit (0, 3, (int) std::lround (rt (parameterIndex ("bassLinkMode"))));
    const int bassFeed = native::jlimit (0, 4, (int) std::lround (rt (parameterIndex ("bassFeed"))));
    const int scoopFeed = linkMode == 0 ? native::jlimit (0, 4, (int) std::lround (rt (parameterIndex ("scoopFeed")))) : bassFeed;
    const int bassExtractor = native::jlimit (0, 3, (int) std::lround (rt (parameterIndex ("bassExtractor"))));
    const int scoopExtractor = linkMode >= 2 ? bassExtractor : native::jlimit (0, 3, (int) std::lround (rt (parameterIndex ("scoopExtractor"))));
    buildBassLane (inputScratch, bassLaneScratch, samples, bassFeed, bassExtractor,
                   bassRightPolarity, bassCenterPolarity, bassPca);
    if (linkMode >= 2)
    {
        scoopPca.running = false; // Invalidate the independent estimator while unused.
        scoopLaneScratch.copyFrom (0, 0, bassLaneScratch, 0, 0, samples);
    }
    else
        buildBassLane (inputScratch, scoopLaneScratch, samples, scoopFeed, scoopExtractor,
                       scoopRightPolarity, scoopCenterPolarity, scoopPca);

    // CINEMA SUB and SCOOP are different sources. SUB roles read the dedicated
    // SW/LFE input directly. SCOOP is bass-managed from decoded CINEMA L/R/C.
    buildBassLane (cinemaScratch, cinemaScoopLaneScratch, samples,
                   native::jlimit (0, 4, (int) std::lround (rt (parameterIndex ("scoopFeed")))),
                   native::jlimit (0, 3, (int) std::lround (rt (parameterIndex ("scoopExtractor")))),
                   cinemaScoopRightPolarity, cinemaScoopCenterPolarity, cinemaScoopPca);

    // Cinema SCOOP can retain its L/R/C extractor, use the discrete LFE lane,
    // or sum both.  Smooth both contributors so changing the source is click-free.
    const int cinemaScoopSource = native::jlimit (0, 2, (int) std::lround (rt (parameterIndex ("cinemaScoopSource"))));
    cinemaScoopLrcGain.setTargetValue (cinemaScoopSource == 1 ? 0.0f : 1.0f);
    cinemaScoopLfeGain.setTargetValue (cinemaScoopSource == 0 ? 0.0f : 1.0f);
    auto* cinemaScoopData = cinemaScoopLaneScratch.getWritePointer (0);
    const auto* cinemaLfeData = cinemaScratch.getReadPointer (3);
    for (int i = 0; i < samples; ++i)
        cinemaScoopData[i] = cinemaScoopData[i] * cinemaScoopLrcGain.getNextValue()
                           + cinemaLfeData[i] * cinemaScoopLfeGain.getNextValue();

    const bool anySolo = [&]
    {
        for (int ch = 0; ch < maxOutputs; ++ch)
            if (rt (parameterIndex ("solo1") + ch) >= 0.5f) return true;
        return false;
    }();
    const double sr = native::jmax (1.0, runtimeSampleRate.load (std::memory_order_relaxed));
    const int commonLatency = active->latency;
    for (int ch = 0; ch < native::jmin (maxOutputs, output.getNumChannels()); ++ch)
    {

        auto& c = channels[(size_t) ch];
        const int role = roleForChannel (ch);
        const int musicRole = (int) std::lround (rt (parameterIndex ("source1") + ch));
        const int cinemaRole = (int) std::lround (rt (parameterIndex ("cinemaRole1") + ch));
        const bool muted = rt (parameterIndex ("mute1") + ch) >= 0.5f;
        const bool emergencyMute = liveRt (parameterIndex("emergencyMute1") + ch) >= 0.5f;
        const bool solo = rt (parameterIndex ("solo1") + ch) >= 0.5f;
        const bool protectorOn = liveRt (protectorOnBase + ch) >= 0.5f;
        if (protectionResetRequested[(size_t) ch].exchange (false))
        {
            c.protectionTripped = false;
            c.tripFrames = 0;
        }
        // Read safety gates directly from APVTS. Coefficient snapshots can
        // crossfade for 50 ms, but a topology reduction or Emergency Mute
        // must take effect in the very next callback even during that fade.
        const bool topologyDisabled = ! isRoleEnabledByLiveTopology (role);
        const bool silence = topologyDisabled || muted || emergencyMute || (anySolo && ! solo);
        auto* dst = output.getWritePointer (ch);
        if (topologyDisabled || emergencyMute)
        {
            c.gain.setCurrentAndTargetValue (0.0f);
            output.clear (ch, 0, samples);
            if (! c.safetySilenced)
            {
                for (auto& stage : c.hp) stage.reset();
                for (auto& stage : c.lp) stage.reset();
                c.allPass.reset();
                for (auto& stage : c.previousHp) stage.reset();
                for (auto& stage : c.previousLp) stage.reset();
                c.previousAllPass.reset();
                clearFirAllPassHistory(c);
                c.fir.reset();
                c.delayValidSamples = 0;
                c.fractionalDelay.reset();
                fractionalDelayStatus[ch].store(0,std::memory_order_relaxed);
                c.firEpoch.reset();
                c.writePosition = 0;
                c.safetySilenced = true;
            }
            outputPeaks[(size_t) ch].store (0.0f, std::memory_order_relaxed);
            protectionState[(size_t) ch].store (protectorOn ? 0 : 1,
                                                std::memory_order_relaxed);
            captureLiveSpectrumBlock (ch, output.getReadPointer (ch), samples);
            continue;
        }
        c.safetySilenced = false;
        const bool cinemaScene = rt (parameterIndex ("cinemaSource")) >= 0.5f;
        const double sceneGainDb = rt ((cinemaScene ? cinemaGainBase : musicGainBase) + ch);
        double targetGain = silence ? 0.0 : native::Decibels::decibelsToGain (sceneGainDb, -100.0);
        if (rt (parameterIndex ("polarity1") + ch) >= 0.5f) targetGain = -targetGain;
        // Topology changes and Emergency Mute are safety interlocks. They must
        // not leave audio on a way that is no longer valid while the normal
        // click-free gain ramp completes. The ordinary Mute/Solo controls keep
        // their smooth transition.
        c.gain.setTargetValue (targetGain);
        const bool hpOn = liveRt (parameterIndex("hpOn1") + ch) >= 0.5f;
        const bool lpOn = liveRt (parameterIndex("lpOn1") + ch) >= 0.5f;
        const int type = native::jlimit (0, 5, (int) std::lround (rt (parameterIndex ("filterType1") + ch)));
        const bool firMode = (int) std::lround (rt (parameterIndex ("processMode1") + ch)) == 1;
        const bool cinemaLfeFilterBypass = rt (parameterIndex ("cinemaSource")) >= 0.5f
            && (cinemaRole == 16 || cinemaRole == 18)
            && rt (parameterIndex ("cinemaLfeFilterBypass")) >= 0.5f;
        const bool allPassOn = rt (parameterIndex ("allPass1") + ch) >= 0.5f;
        const float safeMin = native::jlimit (2.0f, (float) (sr * 0.44), liveRt (safetyMinBase + ch));
        const float safeMax = native::jlimit (safeMin, (float) (sr * 0.5), liveRt (safetyMaxBase + ch));
        const uint64_t tripHold = (uint64_t) (sr * liveRt (tripHoldBase + ch) * 0.001f);
        // Validate the band compiled for each audible path, not requested UI
        // cutoffs. FIR orders crossed edges; IIR retains the assigned HP/LP.
        const auto bandConfiguredSafe = [&](const Config* config,bool liveEnables) noexcept {
            const int configRole=(int)std::lround(config->values[parameterIndex ("cinemaRole1")+ch]);
            const bool bypass=config->values[parameterIndex ("cinemaSource")]>=0.5f && (configRole==16 || configRole==18) && config->values[parameterIndex ("cinemaLfeFilterBypass")]>=0.5f;
            const bool highPass=(liveEnables ? hpOn : config->values[parameterIndex ("hpOn1")+ch]>=0.5f) && !bypass;
            const bool lowPass=(liveEnables ? lpOn : config->values[parameterIndex ("lpOn1")+ch]>=0.5f) && !bypass;
            double lower=config->effectiveHpHz[ch],upper=config->effectiveLpHz[ch];
            if((int)std::lround(config->values[parameterIndex ("processMode1")+ch])==1 && highPass && lowPass && lower>upper)
                std::swap(lower,upper);
            return (safeMin<=2.01f || (highPass && lower>=safeMin))
                && (safeMax>=sr*0.449f || (lowPass && upper<=safeMax));
        };
        const bool departingBandAudible=fading && (c.iirTransition || c.modeTransition
            || (firMode && !cinemaLfeFilterBypass && fading->kernelKey[ch]!=active->kernelKey[ch]));
        const bool bandConfigurationFault=!bandConfiguredSafe(active,!firMode && !c.modeTransition)
            || (departingBandAudible && !bandConfiguredSafe(fading,false));
        const auto protectSample = [&] (double x, int) noexcept
        {
            // Preserve numerical faults for the ABI's explicit processing-error
            // result. Never silently replace an overflow with zero or saturation.
            if (c.protectionTripped && protectorOn) return 0.0;
            // Bandwidth protection never analyses, limits or reshapes programme
            // content. Valid finite samples remain bit-transparent here.
            return x;
        };
        // Align every physical output to the longest active FIR, then add the
        // user's acoustic delay. FIR already contributes its own group delay.
        const double compensation = native::jmax (0, commonLatency - firGroupDelayForChannel (ch));
        const int sceneDelayBase = cinemaScene ? cinemaDelayBase : musicDelayBase;
        const double relativeDelayMs = (double) rt (sceneDelayBase + ch);
        const double effectiveDelayMs = native::jlimit (0.0, maxDelaySeconds * 1000.0,
            relativeDelayMs);
        c.delaySamples.setTargetValue (effectiveDelayMs * 0.001 * sr + compensation);
        c.fractionalDelay.request(active->delays[ch],int(sr*0.04));
        c.peak = 0.0;

        for (int i = 0; i < samples; ++i)
        {
            const auto sourceForRole = [this, i] (int selectedRole, bool cinemaScene) noexcept
            {
                if (selectedRole >= 1 && selectedRole <= 5)  return cinemaScene ? cinemaScratch.getSample (0, i) : inputScratch.getSample (0, i);
                if (selectedRole >= 6 && selectedRole <= 10) return cinemaScene ? cinemaScratch.getSample (1, i) : inputScratch.getSample (1, i);
                if (selectedRole >= 11 && selectedRole <= 15) return cinemaScene ? cinemaScratch.getSample (2, i) : inputScratch.getSample (2, i);
                if (selectedRole == 16 || selectedRole == 18) return cinemaScene ? cinemaScratch.getSample (3, i) : bassLaneScratch.getSample (0, i);
                if (selectedRole == 17 || selectedRole == 19) return cinemaScene ? cinemaScoopLaneScratch.getSample (0, i) : scoopLaneScratch.getSample (0, i);
                if (cinemaScene && selectedRole >= 20 && selectedRole <= 25) return cinemaScratch.getSample (selectedRole - 16, i);
                return 0.0;
            };
            const double cinemaMix = cinemaMixValues[i];
            auto epoch=(appliedProgram ? cinemaMix>0.0 : cinemaMix<1.0) ? inputEpochToken : previousInputEpochToken;
            double x = sourceForRole (musicRole, false) * (1.0 - cinemaMix)
                    + sourceForRole (cinemaRole, true) * cinemaMix;

            if (c.modeTransition && fading != nullptr)
            {
                // One FIR engine is needed: either the departing FIR or the
                // newly reset FIR. Keep the IIR output separately until mixing.
                const bool enteringFir = firMode && ! cinemaLfeFilterBypass;
                const auto* iirConfig = enteringFir ? fading : active;
                auto& hp = enteringFir ? c.previousHp : c.hp;
                auto& lp = enteringFir ? c.previousLp : c.lp;
                auto& ap = enteringFir ? c.previousAllPass : c.allPass;
                const int iirType = native::jlimit(0,5,(int)std::lround(iirConfig->values[parameterIndex ("filterType1")+ch]));
                const int iirRole = (int)std::lround(iirConfig->values[parameterIndex ("cinemaRole1")+ch]);
                const bool bypass = iirConfig->values[parameterIndex ("cinemaSource")]>=0.5f && (iirRole==16 || iirRole==18) && iirConfig->values[parameterIndex ("cinemaLfeFilterBypass")]>=0.5f;
                double iir = x;
                auto iirEpoch = epoch;
                if (!bypass && iirConfig->values[parameterIndex ("hpOn1")+ch]>=0.5f) {
                    iir=processFilter(hp,iirType,iir);
                    if(traceOutputEpoch) iirEpoch=traceFilterEpoch(hp,iirType,iirEpoch);
                }
                if (!bypass && iirConfig->values[parameterIndex ("lpOn1")+ch]>=0.5f) {
                    iir=processFilter(lp,iirType,iir);
                    if(traceOutputEpoch) iirEpoch=traceFilterEpoch(lp,iirType,iirEpoch);
                }
                if (!bypass && iirConfig->values[parameterIndex ("allPass1")+ch]>=0.5f) {
                    iir=ap.process(iir);
                    if(traceOutputEpoch) iirEpoch=ap.trace(iirEpoch);
                }
                c.modeIirBuffer[(size_t)i]=iir;
                if(traceOutputEpoch) c.modeIirEpochs[(size_t)i]=iirEpoch;
                dst[i]=x;
                continue;
            }

            if (firMode && ! cinemaLfeFilterBypass)
            {
                dst[i] = x;
                continue;
            }

            if (c.iirTransition && fading != nullptr)
            {
                double previous = x;
                const bool oldHpOn = fading->values[(size_t) (parameterIndex ("hpOn1") + ch)] >= 0.5f;
                const bool oldLpOn = fading->values[(size_t) (parameterIndex ("lpOn1") + ch)] >= 0.5f;
                const bool oldAllPassOn = fading->values[(size_t) (parameterIndex ("allPass1") + ch)] >= 0.5f;
                const int oldType = native::jlimit (0, 5, (int) std::lround (fading->values[(size_t) (parameterIndex ("filterType1") + ch)]));
                const bool oldBypass = fading->values[parameterIndex ("cinemaSource")] >= 0.5f
                    && ((int) std::lround (fading->values[(size_t) (parameterIndex ("cinemaRole1") + ch)]) == 16
                        || (int) std::lround (fading->values[(size_t) (parameterIndex ("cinemaRole1") + ch)]) == 18)
                    && fading->values[parameterIndex ("cinemaLfeFilterBypass")] >= 0.5f;
                if (! oldBypass && oldHpOn) previous = processFilter (c.previousHp, oldType, previous);
                if (! oldBypass && oldLpOn) previous = processFilter (c.previousLp, oldType, previous);
                if (! oldBypass && oldAllPassOn) previous = c.previousAllPass.process (previous);

                double current = x;
                if (! cinemaLfeFilterBypass && hpOn) current = processFilter (c.hp, type, current);
                if (! cinemaLfeFilterBypass && lpOn) current = processFilter (c.lp, type, current);
                if (! cinemaLfeFilterBypass && allPassOn) current = c.allPass.process (current);
                const double mix = native::jlimit (0.0, 1.0, (double) (fadePosition + i) / 9600.0);
                x = previous + (current - previous) * mix;
                if(traceOutputEpoch) {
                auto oldEpoch=epoch, newEpoch=epoch;
                if (!oldBypass && oldHpOn) oldEpoch=traceFilterEpoch(c.previousHp,oldType,oldEpoch);
                if (!oldBypass && oldLpOn) oldEpoch=traceFilterEpoch(c.previousLp,oldType,oldEpoch);
                if (!oldBypass && oldAllPassOn) oldEpoch=c.previousAllPass.trace(oldEpoch);
                if (!cinemaLfeFilterBypass && hpOn) newEpoch=traceFilterEpoch(c.hp,type,newEpoch);
                if (!cinemaLfeFilterBypass && lpOn) newEpoch=traceFilterEpoch(c.lp,type,newEpoch);
                if (!cinemaLfeFilterBypass && allPassOn) newEpoch=c.allPass.trace(newEpoch);
                epoch=std::max(mix>0.0?newEpoch:0,mix<1.0?oldEpoch:0);
                }
            }
            else
            {
                if (! cinemaLfeFilterBypass && hpOn) x = processFilter (c.hp, type, x);
                if (! cinemaLfeFilterBypass && lpOn) x = processFilter (c.lp, type, x);
                if (! cinemaLfeFilterBypass && allPassOn) x = c.allPass.process (x);
                if(traceOutputEpoch) {
                if (!cinemaLfeFilterBypass && hpOn) epoch=traceFilterEpoch(c.hp,type,epoch);
                if (!cinemaLfeFilterBypass && lpOn) epoch=traceFilterEpoch(c.lp,type,epoch);
                if (!cinemaLfeFilterBypass && allPassOn) epoch=c.allPass.trace(epoch);
                }
            }
            x = readDelayedSample (c, x, c.delaySamples.getNextValue(), epoch) * c.gain.getNextValue();
            x = protectSample (x, i);
            if(traceOutputEpoch && c.gain.getCurrentValue()!=0.0 &&
               !(c.protectionTripped && protectorOn) && (outputEpochSourceMask&(1u<<ch))!=0)
                blockOutputEpochs[(size_t)i]=std::max(blockOutputEpochs[(size_t)i],c.delayedEpoch);
            dst[i] = x;
            c.peak = native::jmax (c.peak, std::abs (x));
        }

        if (c.modeTransition && fading != nullptr)
        {
            const bool enteringFir = firMode && ! cinemaLfeFilterBypass;
            const auto* firConfig = enteringFir ? active : fading;
            auto& firAllPass = enteringFir ? c.allPass : c.previousAllPass;
            const bool firAllPassOn = firConfig->values[parameterIndex ("allPass1")+ch]>=0.5f;
            c.fir.process(dst,samples,&firConfig->kernels[ch],nullptr,0.0,0.0);
            // Fill the destination FIR history before making its cold-start
            // response audible; retain the existing 9600-frame total window.
            const int lengthIndex=native::jlimit(0,2,(int)std::lround(firConfig->values[parameterIndex ("firLength1")+ch]));
            const int warmup=enteringFir ? (lengthIndex==0 ? 2048 : (lengthIndex==1 ? 4096 : 8192)) : 0;
            c.peak=0.0;
            for(int i=0;i<samples;++i)
            {
                double fir=dst[i];
                const double firInput=fir;uint64_t firInputEpoch=0;
                if(firAllPassOn) fir=firAllPass.process(fir);
                const double mix=native::jlimit(0.0,1.0,(double)(fadePosition+i-warmup)/(9600.0-warmup));
                const double iir=c.modeIirBuffer[(size_t)i];
                double x=enteringFir ? iir+(fir-iir)*mix : fir+(iir-fir)*mix;
                uint64_t tag=0;
                if(traceOutputEpoch) {
                    auto firEpoch=c.firEpoch.process(
                        (appliedProgram ? cinemaMixValues[i]>0.0 : cinemaMixValues[i]<1.0) ? inputEpochToken : previousInputEpochToken,
                        firConfig->kernels[ch].firstNonzeroTap,-1,false,1.0);
                    firInputEpoch=firEpoch;
                    if(firAllPassOn) firEpoch=firAllPass.trace(firEpoch);
                    const auto iirEpoch=c.modeIirEpochs[(size_t)i];
                    const auto oldEpoch=enteringFir ? iirEpoch : firEpoch;
                    const auto newEpoch=enteringFir ? firEpoch : iirEpoch;
                    tag=std::max(mix<1.0 ? oldEpoch : 0,mix>0.0 ? newEpoch : 0);
                }
                if(enteringFir && i>=samples-2)recordFirAllPass(c,firInput,firInputEpoch);
                x=readDelayedSample(c,x,c.delaySamples.getNextValue(),tag)*c.gain.getNextValue();
                x=protectSample(x,i);
                if(traceOutputEpoch && c.gain.getCurrentValue()!=0.0 &&
                   !(c.protectionTripped && protectorOn) && (outputEpochSourceMask&(1u<<ch))!=0)
                    blockOutputEpochs[(size_t)i]=std::max(blockOutputEpochs[(size_t)i],c.delayedEpoch);
                dst[i]=x;
                c.peak=native::jmax(c.peak,std::abs(x));
            }
        }
        else if (firMode && ! cinemaLfeFilterBypass)
        {
            c.fir.process (dst,samples,&active->kernels[ch], fading && fading->kernelKey[ch]!=active->kernelKey[ch] ? &fading->kernels[ch] : nullptr, fadePosition / 9600.0, 1.0/9600.0);
            c.peak = 0.0;
            for (int i = 0; i < samples; ++i)
            {
                double x = dst[i];
                const double firInput=x;
                const bool apTransition=c.firAllPassTransition && fading;
                const double apMix=apTransition ? native::jlimit(0.0,1.0,(double)(fadePosition+i)/9600.0) : 1.0;
                const double current=allPassOn ? c.allPass.process(x) : x;
                if(apTransition){
                    const double previous=fading->values[parameterIndex ("allPass1")+ch]>=0.5f ? c.previousAllPass.process(x) : x;
                    x=previous+(current-previous)*apMix;
                }else x=current;
                const auto* oldKernel=fading && fading->kernelKey[ch]!=active->kernelKey[ch] ? &fading->kernels[ch] : nullptr;
                uint64_t tag=0;
                if(traceOutputEpoch) {
                tag=c.firEpoch.process(
                    (appliedProgram ? cinemaMixValues[i]>0.0 : cinemaMixValues[i]<1.0) ? inputEpochToken : previousInputEpochToken,
                    active->kernels[ch].firstNonzeroTap,oldKernel?oldKernel->firstNonzeroTap:-1,oldKernel!=nullptr,
                    std::min(1.0,fadePosition/9600.0+i*(1.0/9600.0)));
                }
                const uint64_t firInputEpoch=tag;
                const uint64_t currentEpoch=traceOutputEpoch && allPassOn ? c.allPass.trace(tag) : tag;
                if(traceOutputEpoch && apTransition){
                    const uint64_t previousEpoch=fading->values[parameterIndex ("allPass1")+ch]>=0.5f ? c.previousAllPass.trace(tag) : tag;
                    tag=std::max(apMix<1.0?previousEpoch:0,apMix>0.0?currentEpoch:0);
                }else tag=currentEpoch;
                if(i>=samples-2)recordFirAllPass(c,firInput,firInputEpoch);
                x = readDelayedSample (c, x, c.delaySamples.getNextValue(), tag) * c.gain.getNextValue();
                x = protectSample (x, i);
            if(traceOutputEpoch && c.gain.getCurrentValue()!=0.0 &&
               !(c.protectionTripped && protectorOn) && (outputEpochSourceMask&(1u<<ch))!=0)
                blockOutputEpochs[(size_t)i]=std::max(blockOutputEpochs[(size_t)i],c.delayedEpoch);
                dst[i] = x;
                c.peak = native::jmax (c.peak, std::abs (x));
            }
        }
        outputPeaks[(size_t) ch].store ((float) c.peak, std::memory_order_relaxed);
        fractionalDelayStatus[ch].store(uint32_t(c.fractionalDelay.effectiveOrder())
            | (c.fractionalDelay.transitioning()?0x100u:0u),std::memory_order_relaxed);
        // Band Guard is deliberately configuration-only. It verifies that
        // the crossover HP/LP assigned to this physical way remains inside
        // the loudspeaker's SAFE MIN/MAX. Programme content is irrelevant.
        if (bandConfigurationFault) c.tripFrames += (uint64_t) samples; else c.tripFrames = 0;
        if (protectorOn && c.tripFrames >= tripHold) c.protectionTripped = true;
        const bool warning = bandConfigurationFault;
        protectionState[(size_t) ch].store (c.protectionTripped && protectorOn ? 3 : (!protectorOn ? (warning ? 4 : 1) : (warning ? 2 : 0)), std::memory_order_relaxed);
        captureLiveSpectrumBlock (ch, output.getReadPointer (ch), samples);
    }

    if(traceOutputEpoch) for(int i=0;i<samples;++i) if(blockOutputEpochs[(size_t)i]>programBlock.outputToken) {
        programBlock.outputToken=blockOutputEpochs[(size_t)i];
        if(programBlock.transitions++==0) programBlock.firstTransitionFrame=(uint32_t)i;
    }
    runtimeInputs = getTotalNumInputChannels();
    runtimeOutputs = getMainBusNumOutputChannels();
}

bool EntropyCrossoverAudioProcessor::isCinemaBusEnabled() const noexcept
{
    return true;
}

void EntropyCrossoverAudioProcessor::saveChannelSnapshot (int channel)
{
    channel = native::jlimit (0, maxOutputs - 1, channel);
    auto root = apvts.state.getChildWithName ("MANUAL_SNAPSHOTS");
    if (! root.isValid())
    {
        root = native::XmlNode ("MANUAL_SNAPSHOTS");
        apvts.state.addChild (root, -1, nullptr);
    }

    const native::Identifier type ("CHANNEL_" + native::String (channel + 1));
    auto old = root.getChildWithName (type);
    if (old.isValid()) root.removeChild (old, nullptr);
    native::XmlNode snap (type);
    const char* bases[] = { "source", "cinemaRole", "filterType", "processMode", "firLength", "allPass", "apFreq", "apQ", "gainDb", "musicGainDb", "cinemaGainDb", "mute", "solo", "polarity", "delayMs", "musicDelayMs", "cinemaDelayMs", "fractionalDelayMode", "hpOn", "hpHz", "lpOn", "lpHz",
                            "protectorOn", "emergencyMute", "safetyMinHz", "safetyMaxHz", "tripHoldMs" };
    for (auto* base : bases)
    {
        const auto id = channelParam (base, channel);
        snap.setProperty (id, parameter (id), nullptr);
    }
    snap.setProperty ("savedAtMs", (native::int64) native::Time::currentTimeMillis(), nullptr);
    root.addChild (snap, -1, nullptr);
}

void EntropyCrossoverAudioProcessor::saveAllSnapshots()
{
    auto root = apvts.state.getChildWithName ("MANUAL_SAVE_ALL");
    if (root.isValid()) apvts.state.removeChild (root, nullptr);
    root = native::XmlNode ("MANUAL_SAVE_ALL");
    const char* globals[] = { "cinemaSource", "lrWays", "centerWays", "subSpeakers", "scoopSpeakers", "lrBandLimit", "fineDelayRatio",
                              "bassFeed", "scoopFeed", "bassLinkMode", "cinemaScoopSource", "cinemaLfeFilterBypass", "cinemaLfeReferenceGain", "bassExtractor", "scoopExtractor",
                              "bassProcess", "scoopProcess", "bassFirLength", "scoopFirLength",
                              "bassAllPass", "scoopAllPass", "bassAPFreq", "scoopAPFreq", "bassAPQ", "scoopAPQ" };
    for (auto* id : globals) root.setProperty (id, parameter (id), nullptr);
    for (int ch = 0; ch < cinemaInputs; ++ch)
    {
        const auto id = "cinemaTrimDb" + native::String (ch + 1);
        root.setProperty (id, parameter (id), nullptr);
    }
    for (int ch = 0; ch < mainInputs; ++ch)
    {
        const auto id = "musicTrimDb" + native::String (ch + 1);
        root.setProperty (id, parameter (id), nullptr);
    }
    const char* bases[] = { "source", "cinemaRole", "filterType", "processMode", "firLength", "allPass", "apFreq", "apQ", "gainDb", "musicGainDb", "cinemaGainDb", "mute", "solo", "polarity", "delayMs", "musicDelayMs", "cinemaDelayMs", "fractionalDelayMode", "hpOn", "hpHz", "lpOn", "lpHz",
                            "protectorOn", "emergencyMute", "safetyMinHz", "safetyMaxHz", "tripHoldMs" };
    for (int ch = 0; ch < maxOutputs; ++ch)
        for (auto* base : bases)
        {
            const auto id = channelParam (base, ch);
            root.setProperty (id, parameter (id), nullptr);
        }
    root.setProperty ("savedAtMs", (native::int64) native::Time::currentTimeMillis(), nullptr);
    apvts.state.addChild (root, -1, nullptr);
}

std::vector<native::String> EntropyCrossoverAudioProcessor::sceneParameterIds()
{
    std::vector<native::String> ids;
    for (const auto* id : { "lrWays", "centerWays", "subSpeakers", "scoopSpeakers", "lrBandLimit",
                            "bassLinkMode", "bassFeed", "scoopFeed", "bassExtractor", "scoopExtractor",
                            "bassProcess", "scoopProcess", "bassFirLength", "scoopFirLength",
                            "bassAllPass", "scoopAllPass", "bassAPFreq", "scoopAPFreq", "bassAPQ", "scoopAPQ" })
        ids.emplace_back (id);
    for (int ch = 0; ch < maxOutputs; ++ch)
        for (const auto* base : { "filterType", "processMode", "firLength", "allPass", "apFreq", "apQ",
                                  "polarity", "hpOn", "hpHz", "lpOn", "lpHz", "mute", "fractionalDelayMode" })
            ids.push_back (channelParam (base, ch));
    return ids;
}

void EntropyCrossoverAudioProcessor::captureSceneState (bool cinema)
{
    auto& destination = sceneStates[(size_t) (cinema ? 1 : 0)];
    destination.clear();
    for (const auto& id : sceneParameterIds())
        if (const auto* value = apvts.getRawParameterValue (id))
            destination[id.toStdString()] = value->load (std::memory_order_relaxed);
}

void EntropyCrossoverAudioProcessor::restoreSceneState (bool cinema)
{
    const auto& source = sceneStates[(size_t) (cinema ? 1 : 0)];
    for (const auto& id : sceneParameterIds())
        if (const auto found = source.find (id.toStdString()); found != source.end())
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (found->second));
}

void EntropyCrossoverAudioProcessor::writeSceneStatesToTree()
{
    for (int scene = 0; scene < 2; ++scene)
    {
        const native::Identifier type (scene == 0 ? "MUSIC_CROSSOVER_SCENE" : "CINEMA_CROSSOVER_SCENE");
        auto old = apvts.state.getChildWithName (type);
        if (old.isValid()) apvts.state.removeChild (old, nullptr);
        native::XmlNode node (type);
        for (const auto& [id, value] : sceneStates[(size_t) scene])
            node.setProperty (native::Identifier (id), value, nullptr);
        apvts.state.addChild (node, -1, nullptr);
    }
}

void EntropyCrossoverAudioProcessor::readSceneStatesFromTree()
{
    const bool currentCinema = apvts.getRawParameterValue ("cinemaSource")->load() >= 0.5f;
    bool complete = true;
    for (int scene = 0; scene < 2; ++scene)
    {
        const native::Identifier type (scene == 0 ? "MUSIC_CROSSOVER_SCENE" : "CINEMA_CROSSOVER_SCENE");
        const auto node = apvts.state.getChildWithName (type);
        if (! node.isValid()) { complete = false; break; }
        auto& destination = sceneStates[(size_t) scene]; destination.clear();
        for (const auto& id : sceneParameterIds())
            if (node.hasProperty (native::Identifier (id)))
                destination[id.toStdString()] = (float) node.getProperty (native::Identifier (id));
    }
    if (! complete)
    {
        // Legacy 1.9.13 state: preserve its exact sound in both scenes.
        captureSceneState (false); captureSceneState (true);
    }
    activeSceneCinema = currentCinema;
    sceneStatesInitialised = true;
    restoreSceneState (currentCinema);
}

void EntropyCrossoverAudioProcessor::getStateInformation (native::MemoryBlock& data)
{
    if (sceneStatesInitialised) captureSceneState (activeSceneCinema);
    writeSceneStatesToTree();
    if (auto xml = apvts.copyState().createXml()) native::copyXmlToBinary (*xml, data);
}

bool EntropyCrossoverAudioProcessor::restoreStateInformation (const void* data, int size)
{
    if (auto xml = native::getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto restored = native::XmlNode::fromXml (*xml);
            // Validate all incoming scalar controls before mutating any live parameter.
            // State parsing, migration and serialization run on the serialized control thread.
            const auto finiteNumber = [] (const native::Property& property) {
                const auto text = property.toString().toStdString();
                float value = 0.0f;
                const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
                return !text.empty() && result.ec == std::errc{}
                    && result.ptr == text.data() + text.size() && std::isfinite(value);
            };
            std::map<native::String, bool> seen;
            for (int index = 0; index < restored.getNumChildren(); ++index)
            {
                const auto child = restored.getChild(index);
                if (child.hasType("PARAM"))
                {
                    const auto id = child.getProperty("id").toString();
                    if (id.isEmpty() || !child.hasProperty("value")
                        || !finiteNumber(child.getProperty("value")) || seen.count(id))
                        return false;
                    seen[id] = true;
                }
                if (child.hasType("MUSIC_CROSSOVER_SCENE") || child.hasType("CINEMA_CROSSOVER_SCENE"))
                    for (const auto& id : sceneParameterIds())
                        if (child.hasProperty(id) && !finiteNumber(child.getProperty(id)))
                            return false;
            }
            for (const auto type : { native::Identifier ("CAL_ALIGNMENT"), native::Identifier ("MIC_GENERATOR") })
            {
                const auto obsolete = restored.getChildWithName (type);
                if (obsolete.isValid()) restored.removeChild (obsolete, nullptr);
            }
            // 1.9.10 and older stored common gain and per-channel timing values for both program
            // scenes. APVTS serialises parameters as PARAM children, so add the
            // two new children before replaceState. Channel values remain unchanged; retired common base delay is discarded.
            // MUSIC and CINEMA retain their independent channel controls.
            const auto findParameterNode = [&restored] (const native::String& id)
            {
                for (int index = 0; index < restored.getNumChildren(); ++index)
                {
                    auto child = restored.getChild (index);
                    if (child.hasType ("PARAM") && child.getProperty ("id").toString() == id)
                        return child;
                }
                return native::XmlNode {};
            };
            for (int ch = 0; ch < maxOutputs; ++ch)
            {
                const auto n = native::String (ch + 1);
                const auto legacy = findParameterNode ("gainDb" + n);
                const auto legacyValue = legacy.isValid() ? legacy.getProperty ("value") : native::Property (0.0f);
                for (const auto& id : { "musicGainDb" + n, "cinemaGainDb" + n })
                    if (! findParameterNode (id).isValid())
                    {
                        native::XmlNode parameterNode ("PARAM");
                        parameterNode.setProperty ("id", id, nullptr);
                        parameterNode.setProperty ("value", legacyValue, nullptr);
                        restored.addChild (parameterNode, -1, nullptr);
                    }
            }
            const auto appendMigratedParameter = [&restored, &findParameterNode]
                (const native::String& id, const native::Property& value)
            {
                if (findParameterNode (id).isValid())
                    return;
                native::XmlNode parameterNode ("PARAM");
                parameterNode.setProperty ("id", id, nullptr);
                parameterNode.setProperty ("value", value, nullptr);
                restored.addChild (parameterNode, -1, nullptr);
            };
            for (int ch = 0; ch < maxOutputs; ++ch)
            {
                const auto n = native::String (ch + 1);
                const auto legacyDelay = findParameterNode ("delayMs" + n);
                const auto legacyDelayValue = legacyDelay.isValid()
                    ? legacyDelay.getProperty ("value") : native::Property (0.0f);
                appendMigratedParameter ("musicDelayMs" + n, legacyDelayValue);
                appendMigratedParameter ("cinemaDelayMs" + n, legacyDelayValue);
                appendMigratedParameter ("fractionalDelayMode" + n, native::Property(0.0f));
            }
            const auto discardRetired = [&](auto&& self, native::XmlNode node) -> void
            {
                for (auto* id : { "globalDelayMs", "musicGlobalDelayMs", "cinemaGlobalDelayMs" })
                    node.removeProperty (id, nullptr);
                for (int index = node.getNumChildren(); --index >= 0;)
                {
                    auto child = node.getChild (index);
                    const auto id = child.getProperty ("id").toString();
                    if (child.hasType ("PARAM") && (id == "globalDelayMs" || id == "musicGlobalDelayMs" || id == "cinemaGlobalDelayMs"))
                        node.removeChild (child, nullptr);
                    else self (self, child);
                }
            };
            discardRetired (discardRetired, restored);
            apvts.replaceState (restored);
            readSceneStatesFromTree();
            return true;
        }
    return false;
}

void EntropyCrossoverAudioProcessor::setStateInformation (const void* data, int size)
{
    (void) restoreStateInformation (data, size);
}

void EntropyCrossoverAudioProcessor::CommandEventMailbox::publish(
    ControlCommandTag tag, uint32_t eventReason) noexcept
{
    const auto writing = revision.fetch_add(1, std::memory_order_acq_rel) + 1;
    session.store(tag.session, std::memory_order_relaxed);
    sequence.store(tag.sequence, std::memory_order_relaxed);
    payloadHash.store(tag.payloadHash, std::memory_order_relaxed);
    kind.store(tag.kind, std::memory_order_relaxed);
    reason.store(eventReason, std::memory_order_relaxed);
    revision.store(writing + 1, std::memory_order_release);
}

EntropyCrossoverAudioProcessor::ControlCommandTag
EntropyCrossoverAudioProcessor::CommandEventMailbox::read(uint32_t* eventReason) const noexcept
{
    ControlCommandTag tag;
    uint64_t firstRevision = 0;
    uint64_t secondRevision = 0;
    uint32_t reasonValue = 0;
    do
    {
        firstRevision = revision.load(std::memory_order_acquire);
        if ((firstRevision & 1u) != 0) continue;
        tag.session = session.load(std::memory_order_relaxed);
        tag.sequence = sequence.load(std::memory_order_relaxed);
        tag.kind = kind.load(std::memory_order_relaxed);
        tag.payloadHash = payloadHash.load(std::memory_order_relaxed);
        reasonValue = reason.load(std::memory_order_relaxed);
        secondRevision = revision.load(std::memory_order_acquire);
    }
    while (firstRevision != secondRevision || (secondRevision & 1u) != 0);
    if (eventReason != nullptr) *eventReason = reasonValue;
    return tag;
}

EntropyCrossoverAudioProcessor::ControlCommandStatus
EntropyCrossoverAudioProcessor::controlCommandStatus() const noexcept
{
    ControlCommandStatus status;
    status.accepted = acceptedCommand.read();
    status.applied = appliedCommand.read();
    status.failed = failedCommand.read(&status.reason);
    return status;
}

void EntropyCrossoverAudioProcessor::publishControlFailure(
    ControlCommandTag tag, uint32_t reason) noexcept
{
    if (tag.sequence != 0) failedCommand.publish(tag, reason);
}

void EntropyCrossoverAudioProcessor::publishControlAccepted(
    ControlCommandTag tag) noexcept
{
    if (tag.sequence != 0) acceptedCommand.publish(tag);
}

void EntropyCrossoverAudioProcessor::serviceUpdates()
{
    (void) publishControlSnapshot({});
}

bool EntropyCrossoverAudioProcessor::finiteConfig(const Config& c) noexcept {
    auto finiteBiquad=[](const Biquad& b){return std::isfinite(b.b0)&&std::isfinite(b.b1)&&std::isfinite(b.b2)&&std::isfinite(b.a1)&&std::isfinite(b.a2);};
    for(int ch=0;ch<16;++ch){
        for(const auto& b:c.hp[ch])if(!finiteBiquad(b))return false;
        for(const auto& b:c.lp[ch])if(!finiteBiquad(b))return false;
        if(!finiteBiquad(c.ap[ch]))return false;
        for(double a:c.delays[ch].a)if(!std::isfinite(a))return false;
        const auto& k=c.kernels[ch];for(double tap:k.head)if(!std::isfinite(tap))return false;
        for(int p=0;p<k.partitions;++p)for(auto bin:k.tail[p])if(!std::isfinite(bin.re)||!std::isfinite(bin.im))return false;
    }
    return true;
}
bool EntropyCrossoverAudioProcessor::preparedCoefficientsFinite() const noexcept {return hasCompiled&&lastPreparationFinite;}

bool EntropyCrossoverAudioProcessor::publishControlSnapshot(ControlCommandTag tag) {
    const bool requestedCinema = liveRt (parameterIndex("cinemaSource")) >= 0.5f;
    if (! sceneStatesInitialised)
    {
        activeSceneCinema = requestedCinema;
        captureSceneState (false);
        captureSceneState (true);
        sceneStatesInitialised = true;
    }
    else if (requestedCinema != activeSceneCinema)
    {
        captureSceneState (activeSceneCinema);
        restoreSceneState (requestedCinema);
        activeSceneCinema = requestedCinema;
    }
    // Control owner only. Detect no-op before taking ownership of any slot:
    // reclaiming a pending slot here would discard an accepted command.
    std::array<float,parameterCount> requestedValues{};
    for(int i=0;i<parameterCount;++i)
        requestedValues[i]=raw[i]?raw[i]->load(std::memory_order_relaxed):0.f;
    if(hasCompiled && requestedValues==lastCompiled && tag.sequence==0)return true;
    int slot=-1;for(int i=0;i<3;++i){int free=0;if(slotState[i].compare_exchange_strong(free,1,std::memory_order_acquire)){slot=i;break;}}
    bool reusedPending=false;ControlCommandTag reusedTag{};
    if(slot<0){int candidate=pending.load(std::memory_order_acquire);while(candidate>=0){
        if(pending.compare_exchange_weak(candidate,-2,std::memory_order_acq_rel,std::memory_order_acquire)){
            slot=candidate;reusedPending=true;reusedTag=configs[(size_t)slot].controlTag;break;
        }
    }}
    if(slot<0)return false;
    building=&configs[slot];
    building->values=requestedValues;
    building->controlTag=tag;
    // Compare against the last control snapshot (never an audio-owned object).
    static_assert(std::atomic<int>::is_always_lock_free);
    tlsConfig=building;
    building->latency=0;
    const double compiledRate=getRuntimeSampleRate();uint64_t rateBits=0;std::memcpy(&rateBits,&compiledRate,sizeof(rateBits));
    for(int ch=0;ch<16;++ch){uint64_t key=(1469598103934665603ULL^rateBits)*1099511628211ULL;for(int idx:{parameterIndex("cinemaSource"),parameterIndex("lrBandLimit"),parameterIndex("processMode1")+ch,parameterIndex("firLength1")+ch,parameterIndex("hpOn1")+ch,parameterIndex("lpOn1")+ch,parameterIndex("hpHz1")+ch,parameterIndex("lpHz1")+ch,parameterIndex("source1")+ch,parameterIndex("cinemaRole1")+ch}){uint32_t bits;std::memcpy(&bits,&building->values[idx],4);key=(key^bits)*1099511628211ULL;}building->kernelKey[ch]=key;}

    auto key=requestedValues;
    // UI selection/fine-gesture resolution, legacy migration fields, and the
    // other scene's per-lane settings do not affect this scene's preparation.
    // selectedChannel/fineDelayRatio/FFT monitoring are not canonical DSP
    // parameters at all. Compare only the effective scene's canonical values.
    for(int ch=0;ch<16;++ch){
        key[parameterIndex("gainDb1")+ch]=0;key[parameterIndex("delayMs1")+ch]=0;
        if(requestedCinema){key[parameterIndex("musicGainDb1")+ch]=0;key[parameterIndex("musicDelayMs1")+ch]=0;key[parameterIndex("source1")+ch]=0;}
        else{key[parameterIndex("cinemaGainDb1")+ch]=0;key[parameterIndex("cinemaDelayMs1")+ch]=0;key[parameterIndex("cinemaRole1")+ch]=0;}
    }
    for(int ch=0;ch<(requestedCinema?3:10);++ch){
        if(requestedCinema)key[parameterIndex("musicTrimDb1")+ch]=0;
        else key[parameterIndex("cinemaTrimDb1")+ch]=0;
    }
    auto& cached=preparedScenes[requestedCinema?1:0];
    const bool cacheHit=cached.config&&cached.key==key&&cached.rate==compiledRate
        &&cached.frames==getRuntimeBlockSize(); // fixed layout 13 -> 16, this implementation only
    if(cacheHit){
        *building=*cached.config;building->values=requestedValues;building->controlTag=tag;++preparationHits;
    }else{
    for(int ch=0;ch<16;++ch){
        compileCoefficients(ch,getRuntimeSampleRate()); updateFir(ch,getRuntimeSampleRate());
        building->latency=std::max(building->latency,firGroupDelayForChannel(ch));
    }
    // Immutable coefficients are compiled beside the existing kernels on the
    // control owner. Audio only adopts this snapshot at a block boundary.
    const bool cinema=rt (parameterIndex ("cinemaSource"))>=0.5f;
    for(int ch=0;ch<16;++ch) {
        const double acousticMs=native::jlimit(0.0,maxDelaySeconds*1000.0,
            double(rt((cinema?cinemaDelayBase:musicDelayBase)+ch)));
        const double compensation=native::jmax(0,building->latency-firGroupDelayForChannel(ch));
        building->delays[ch]=FractionalDelayDesign::compile(int(std::lround(rt(fractionalDelayModeBase+ch))),
            acousticMs*0.001*compiledRate+compensation);
    }
        if(!finiteConfig(*building)){
            tlsConfig=nullptr;building=nullptr;slotState[slot].store(0,std::memory_order_release);
            if(reusedPending&&reusedTag.sequence)failedCommand.publish(reusedTag,4u);
            lastPreparationFinite=false;return false;
        }
        if(!cached.config)cached.config=std::make_unique<Config>();
        building->viewGeneration=++viewGenerationCounter;
        *cached.config=*building;cached.key=key;cached.rate=compiledRate;cached.frames=getRuntimeBlockSize();++preparationBuilds;
    }
    lastPreparationFinite=finiteConfig(*building);
    prepareVisualization(unsigned(slot));
    tlsConfig=nullptr;
    lastCompiled=building->values;hasCompiled=true;building=nullptr;
    const int replaced = pending.exchange(slot, std::memory_order_acq_rel);
    if (reusedPending)
    {
        if (reusedTag.sequence != 0)
            failedCommand.publish(reusedTag, 4u); // ABI reason: superseded.
    }
    else if (replaced >= 0)
    {
        const auto replacedTag = configs[(size_t) replaced].controlTag;
        if (replacedTag.sequence != 0)
            failedCommand.publish(replacedTag, 4u); // ABI reason: superseded.
        slotState[(size_t) replaced].store(0, std::memory_order_release);
    }
    if (tag.sequence != 0) acceptedCommand.publish(tag);
    return true;
}
void EntropyCrossoverAudioProcessor::beginRt() noexcept {
    if(!fading){const int next=pending.exchange(-1,std::memory_order_acquire);if(next>=0){
        fading=active;fadingIndex=activeIndex;active=&configs[next];activeIndex=next;fadePosition=fading?0:9600;
        if(!fading||fading->viewGeneration!=active->viewGeneration)visualization.monitorIdentity.fetch_add(32,std::memory_order_acq_rel);
        // Publish only the adopted graph. Avoid AudioProcessor listeners/GUI
        // dispatch in the realtime callback; ABI readers use this atomic.
        reportedFirLatency.store(active->latency, std::memory_order_relaxed);
        if(active->controlTag.sequence!=0)appliedCommand.publish(active->controlTag);
        for(int ch=0;ch<16;++ch){
            auto copy=[](Biquad& d,const Biquad& s){d.b0=s.b0;d.b1=s.b1;d.b2=s.b2;d.a1=s.a1;d.a2=s.a2;};
            auto& channel=channels[ch];
            const bool oldIir=fading && (int)std::lround(fading->values[parameterIndex ("processMode1")+ch])==0;
            const bool newIir=(int)std::lround(active->values[parameterIndex ("processMode1")+ch])==0;
            bool changed=fading && (fading->values[parameterIndex ("hpOn1")+ch]!=active->values[parameterIndex ("hpOn1")+ch]
                || fading->values[parameterIndex ("lpOn1")+ch]!=active->values[parameterIndex ("lpOn1")+ch]
                || fading->values[parameterIndex ("hpHz1")+ch]!=active->values[parameterIndex ("hpHz1")+ch]
                || fading->values[parameterIndex ("lpHz1")+ch]!=active->values[parameterIndex ("lpHz1")+ch]
                || fading->values[parameterIndex ("filterType1")+ch]!=active->values[parameterIndex ("filterType1")+ch]
                || fading->values[parameterIndex ("allPass1")+ch]!=active->values[parameterIndex ("allPass1")+ch]
                || fading->values[parameterIndex ("apFreq1")+ch]!=active->values[parameterIndex ("apFreq1")+ch]
                || fading->values[parameterIndex ("apQ1")+ch]!=active->values[parameterIndex ("apQ1")+ch]
                || fading->values[parameterIndex ("cinemaLfeFilterBypass")]!=active->values[parameterIndex ("cinemaLfeFilterBypass")]);
            // Lane/program limits can change coefficients without changing the
            // requested filter parameters. Reuse the existing IIR crossfade.
            if(oldIir && newIir && !changed){
                auto differs=[](const Biquad& a,const Biquad& b){
                    return a.b0!=b.b0 || a.b1!=b.b1 || a.b2!=b.b2 || a.a1!=b.a1 || a.a2!=b.a2;
                };
                const int stages=stagesForFilterType(native::jlimit(0,5,(int)std::lround(active->values[parameterIndex ("filterType1")+ch])));
                for(int k=0;k<stages;++k)
                    changed=changed || (active->values[parameterIndex ("hpOn1")+ch]>=0.5f && differs(fading->hp[ch][k],active->hp[ch][k]))
                        || (active->values[parameterIndex ("lpOn1")+ch]>=0.5f && differs(fading->lp[ch][k],active->lp[ch][k]));
                changed=changed || (active->values[parameterIndex ("allPass1")+ch]>=0.5f && differs(fading->ap[ch],active->ap[ch]));
            }
            auto isFir=[ch](const Config* config){
                if(!config || (int)std::lround(config->values[parameterIndex ("processMode1")+ch])!=1)return false;
                const int role=(int)std::lround(config->values[parameterIndex ("cinemaRole1")+ch]);
                return !(config->values[parameterIndex ("cinemaSource")]>=0.5f && (role==16 || role==18) && config->values[parameterIndex ("cinemaLfeFilterBypass")]>=0.5f);
            };
            const bool oldFir=isFir(fading),newFir=isFir(active);
            channel.modeTransition=fading && oldFir!=newFir;
            const bool oldApOn=fading && fading->values[parameterIndex ("allPass1")+ch]>=0.5f,newApOn=active->values[parameterIndex ("allPass1")+ch]>=0.5f;
            const auto& oldAp=fading ? fading->ap[ch] : active->ap[ch];const auto& newAp=active->ap[ch];
            const bool apChanged=oldAp.b0!=newAp.b0 || oldAp.b1!=newAp.b1 || oldAp.b2!=newAp.b2 || oldAp.a1!=newAp.a1 || oldAp.a2!=newAp.a2;
            channel.firAllPassTransition=oldFir && newFir && (oldApOn!=newApOn || (newApOn && apChanged));
            if(channel.firAllPassTransition)channel.previousAllPass=channel.allPass;
            if(channel.modeTransition)clearFirAllPassHistory(channel);
            channel.iirTransition=oldIir && newIir && changed;
            if(channel.iirTransition || channel.modeTransition){channel.previousHp=channel.hp;channel.previousLp=channel.lp;channel.previousAllPass=channel.allPass;}
            for(int k=0;k<4;++k){copy(channel.hp[k],active->hp[ch][k]);copy(channel.lp[k],active->lp[ch][k]);}
            copy(channel.allPass,active->ap[ch]);
            if(!fading){channel.tripFrames=0;}
            if(channel.iirTransition || channel.modeTransition){for(auto& q:channel.hp)q.reset();for(auto& q:channel.lp)q.reset();channel.allPass.reset();}
            if(channel.firAllPassTransition){
                channel.allPass.reset();
                if(newApOn && channel.firAllPassHistoryValid){
                    // Seed destination AP from dry FIR history; old AP phase
                    // belongs only to the departing path. This preserves DC
                    // without injecting old-output resonance into new poles.
                    const auto& x=channel.firAllPassInputs;const auto& in=channel.firAllPassInputEpochs;
                    const double direct=newAp.b1-newAp.a1,delayed=newAp.b2-newAp.a2;
                    channel.allPass.z1=direct*x[1]+delayed*x[0];
                    channel.allPass.z2=delayed*x[1];
                    channel.allPass.epoch.z1=std::max(direct!=0.0?in[1]:0,delayed!=0.0?in[0]:0);
                    channel.allPass.epoch.z2=delayed!=0.0?in[1]:0;
                }
            }
            if(channel.modeTransition && newFir){channel.fir.reset();channel.firEpoch.reset();}
            else if(!fading || (!(oldFir && newFir) && (fading->values[parameterIndex ("allPass1")+ch]!=active->values[parameterIndex ("allPass1")+ch] || fading->values[parameterIndex ("apFreq1")+ch]!=active->values[parameterIndex ("apFreq1")+ch] || fading->values[parameterIndex ("apQ1")+ch]!=active->values[parameterIndex ("apQ1")+ch])))channel.allPass.reset();
        }
    }}
    tlsConfig=active;
    if(active)visualization.publish(active->viewGeneration,active->values[parameterIndex("cinemaSource")]>=.5f,visualizationTransition());
}
void EntropyCrossoverAudioProcessor::endRt(int samples) noexcept {
    tlsConfig=nullptr;
    if(fading && (fadePosition+=samples)>=9600){slotState[fadingIndex].store(0,std::memory_order_release);fading=nullptr;fadingIndex=-1;}
    if(active)visualization.publish(active->viewGeneration,active->values[parameterIndex("cinemaSource")]>=.5f,visualizationTransition());
}

}

namespace entropy::standalone {
void EntropyCrossoverAudioProcessor::prepareVisualization(unsigned slot){
 visualization.prepare(slot,[&](visual::Configuration& model){
  model.generation=building->viewGeneration;model.sampleRate=getRuntimeSampleRate();model.program=rt(parameterIndex("cinemaSource"))>=.5f;
  bool anySolo=false;for(int ch=0;ch<16;++ch)anySolo|=rt(parameterIndex("solo1")+ch)>=.5f;
  auto section=[](const Biquad& b){return visual::Section{b.b0,b.b1,b.b2,b.a1,b.a2};};
  for(int ch=0;ch<16;++ch){auto& c=model.channels[ch];const int role=int(std::lround(rt((model.program?parameterIndex("cinemaRole1"):parameterIndex("source1"))+ch)));
   c.enabled=isRoleEnabledByTopology(role)&&rt(parameterIndex("mute1")+ch)<.5f&&rt(parameterIndex("emergencyMute1")+ch)<.5f&&(!anySolo||rt(parameterIndex("solo1")+ch)>=.5f);
   c.gain=std::pow(10.0,rt((model.program?parameterIndex("cinemaGainDb1"):parameterIndex("musicGainDb1"))+ch)/20.0)*(rt(parameterIndex("polarity1")+ch)>=.5f?-1:1);
   c.stages=stagesForFilterType(int(std::lround(rt(parameterIndex("filterType1")+ch))));
   c.hpOn=rt(parameterIndex("hpOn1")+ch)>=.5f;c.lpOn=rt(parameterIndex("lpOn1")+ch)>=.5f;c.apOn=rt(parameterIndex("allPass1")+ch)>=.5f;c.fir=rt(parameterIndex("processMode1")+ch)>=.5f;
   c.bypass=model.program&&(role==16||role==18)&&rt(parameterIndex("cinemaLfeFilterBypass"))>=.5f;
   for(int i=0;i<4;++i){c.hp[i]=section(building->hp[ch][i]);c.lp[i]=section(building->lp[ch][i]);}c.ap=section(building->ap[ch]);
   if(c.fir)c.kernel=building->kernels[ch];c.delay=building->delays[ch];
  }
 });
}
bool EntropyCrossoverAudioProcessor::visualizationTransition()const noexcept{
 if(fading||pending.load(std::memory_order_acquire)>=0||cinemaSourceMix.isSmoothing())return true;
 for(const auto& c:channels)if(!c.safetySilenced&&(c.gain.isSmoothing()||c.delaySamples.isSmoothing()||c.fractionalDelay.transitioning()))return true;
 return false;
}
bool EntropyCrossoverAudioProcessor::copyLiveSpectrumFrame(float* destination,int count,visual::Capture& capture)const noexcept{
 if(!destination||count!=int(visual::fftSize))return false;
 const auto revision=viewCaptureRevision.load(std::memory_order_acquire);if(revision&1u)return false;
 const auto end=liveSpectrumPublishedSamples.load(std::memory_order_acquire);if(end<uint64_t(count))return false;
 capture.monitorIdentity=viewCaptureMonitor.load();capture.generation=viewCaptureGeneration.load();capture.captureQpc=viewCaptureQpc.load();capture.frameSequence=viewFrameSequence.load();capture.samples=end;
 capture.sampleRate=runtimeSampleRate.load();capture.channel=int(capture.monitorIdentity&31u)-1;const auto descriptor=viewCaptureDescriptor.load();capture.program=descriptor&1u;capture.transition=bool(descriptor&2u);
 for(int i=0;i<count;++i)destination[i]=liveSpectrumRing[(end-count+i)&(liveSpectrumSamples-1)].load(std::memory_order_relaxed);
 return revision==viewCaptureRevision.load(std::memory_order_acquire);
}
}
