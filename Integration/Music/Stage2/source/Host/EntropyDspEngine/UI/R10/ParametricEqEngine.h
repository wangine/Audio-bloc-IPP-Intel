#pragma once

#include <array>
#include <cstdint>

namespace entropy::parametric_eq
{
inline constexpr std::size_t bandCount = 8;

enum class Mode : std::uint8_t { bypass, staticEq, dynamicEq, staticAndDynamic };
enum class FilterType : std::uint8_t { bell, lowShelf, highShelf };
enum class DetectorMode : std::uint8_t { peak, rms, hybrid };
enum class DetectorSource : std::uint8_t { local, global, hybrid };
enum class DynamicsMode : std::uint8_t {
    downCompression, upCompression, downExpansion, upExpansion
};

struct BandParams
{
    bool enabled = true;
    FilterType type = FilterType::bell;
    double frequencyHz = 1000.0;
    double q = 1.0;
    double staticGainDb = 0.0;
    double thresholdDb = -24.0;
    double ratio = 2.0;
    double kneeDb = 6.0;
    double attackMs = 20.0;
    double releaseMs = 180.0;
    double maxCutDb = -12.0;
    double maxBoostDb = 6.0;
    DetectorMode detectorMode = DetectorMode::rms;
    DetectorSource detectorSource = DetectorSource::local;
    DynamicsMode dynamicsMode = DynamicsMode::downCompression;
};

struct StereoSnapshot
{
    Mode mode = Mode::bypass;
    std::array<BandParams, bandCount> bands{};
};

StereoSnapshot makeDefaultStereoSnapshot() noexcept;
bool validateStereoSnapshot(const StereoSnapshot& value) noexcept;

class StereoEngine final
{
public:
    static constexpr std::uint32_t channelCount = 2;
    static constexpr std::uint32_t latencySamples = 0;
    static constexpr std::uint32_t absoluteMaximumFrames = 16384;

    bool prepare(double sampleRate, std::uint32_t maximumFrames) noexcept;
    void reset() noexcept;
    void process(const double* leftIn, const double* rightIn,
                 double* leftOut, double* rightOut, std::uint32_t frames,
                 const StereoSnapshot& snapshot) noexcept;
    void readBandTelemetry(std::array<float, bandCount>& detectorDb,
                           std::array<float, bandCount>& dynamicGainDb) const noexcept;

private:
    struct SvfState
    {
        double ic1 = 0.0;
        double ic2 = 0.0;
        double g = 0.0;
        double k = 1.0;
        double process(double input, FilterType type) noexcept;
        void reset() noexcept;
    };

    struct DetectorState
    {
        double peak = 0.0;
        double rmsSquare = 0.0;
        double gainDb = 0.0;
        void reset() noexcept;
    };

    static double detectorValue(double sample, DetectorState& state,
                                const BandParams& band, double attack,
                                double release) noexcept;

    double sampleRate_ = 192000.0;
    double coefficientStep_ = 1.0;
    double bypassStep_ = 1.0;
    double wet_ = 0.0;
    std::uint32_t maximumFrames_ = 0;
    bool initialised_ = false;
    std::array<std::array<SvfState, bandCount>, channelCount> filters_{};
    std::array<DetectorState, bandCount> detectors_{};
    std::array<float, bandCount> detectorDb_{};
    std::array<float, bandCount> dynamicGainDb_{};
    std::array<std::array<double, absoluteMaximumFrames>, channelCount> dry_{};
};
} // namespace entropy::parametric_eq
