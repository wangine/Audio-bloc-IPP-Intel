#include "ParametricEqModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <limits>
#include <string>

namespace entropy::host
{
namespace
{
constexpr std::array<const char*, 15> suffixes{{
    "enabled", "filterType", "detectorMode", "detectorSource",
    "dynamicsMode", "frequencyHz", "q", "staticGainDb", "thresholdDb",
    "ratio", "kneeDb", "attackMs", "releaseMs", "maxCutDb",
    "maxBoostDb"}};

bool integerField(std::uint32_t field) noexcept
{
    return field <= ENTROPY_PARAMETRIC_EQ_FIELD_DYNAMICS_MODE;
}
}

const EntropyParametricEqParameterV1* ParametricEqModel::descriptor(
    std::string_view identifier) const noexcept
{
    const auto found = descriptorIndex_.find(std::string(identifier));
    return found == descriptorIndex_.end() ? nullptr : &descriptors_[found->second];
}

bool ParametricEqModel::updateFromTelemetry(
    const EngineTelemetrySnapshot& telemetry)
{
    schemaValid_ = false;
    descriptorIndex_.clear();
    if (telemetry.parametricEqParameterCount != descriptors_.size()) return false;
    for (std::size_t index = 0; index < descriptors_.size(); ++index)
    {
        descriptors_[index] = telemetry.parametricEqParameters[index];
        const auto& item = descriptors_[index];
        if (item.struct_size < sizeof(item) || item.identifier[0] == '\0' ||
            !descriptorIndex_.emplace(item.identifier, index).second)
            return false;
    }
    if (descriptor("mode") == nullptr) return false;
    for (std::uint32_t band = 0; band < 8; ++band)
        for (const auto* suffix : suffixes)
            if (descriptor("band" + std::to_string(band) + "." + suffix) == nullptr)
                return false;

    if (telemetry.parametricEqSettings.struct_size < sizeof(working_) ||
        telemetry.parametricEqSettings.band_count != 8) return false;
    if (!pending()) working_ = telemetry.parametricEqSettings;
    schemaValid_ = true;
    return true;
}

bool ParametricEqModel::assign(EntropyParametricEqSettingsV1& settings,
                               const EntropyParametricEqParameterV1& item,
                               double value) noexcept
{
    if (!std::isfinite(value) || value < item.minimum || value > item.maximum)
        return false;
    if (integerField(item.field) && std::abs(value - std::round(value)) > 1e-9)
        return false;
    if (item.band_index == (std::numeric_limits<std::uint32_t>::max)())
    {
        if (item.field != ENTROPY_PARAMETRIC_EQ_FIELD_MODE) return false;
        settings.mode = static_cast<std::uint32_t>(std::llround(value));
        return true;
    }
    if (item.band_index >= 8) return false;
    auto& band = settings.bands[item.band_index];
    switch (item.field)
    {
        case ENTROPY_PARAMETRIC_EQ_FIELD_ENABLED: band.enabled=static_cast<std::uint32_t>(std::llround(value)); break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_FILTER_TYPE: band.filter_type=static_cast<std::uint32_t>(std::llround(value)); break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_DETECTOR_MODE: band.detector_mode=static_cast<std::uint32_t>(std::llround(value)); break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_DETECTOR_SOURCE: band.detector_source=static_cast<std::uint32_t>(std::llround(value)); break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_DYNAMICS_MODE: band.dynamics_mode=static_cast<std::uint32_t>(std::llround(value)); break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_FREQUENCY_HZ: band.frequency_hz=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_Q: band.q=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_STATIC_GAIN_DB: band.static_gain_db=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_THRESHOLD_DB: band.threshold_db=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_RATIO: band.ratio=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_KNEE_DB: band.knee_db=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_ATTACK_MS: band.attack_ms=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_RELEASE_MS: band.release_ms=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_MAX_CUT_DB: band.max_cut_db=value; break;
        case ENTROPY_PARAMETRIC_EQ_FIELD_MAX_BOOST_DB: band.max_boost_db=value; break;
        default: return false;
    }
    return true;
}

EntropyParametricEqSettingsV1 ParametricEqModel::defaultSettings() const noexcept
{
    EntropyParametricEqSettingsV1 result{};
    result.struct_size = sizeof(result);
    result.band_count = 8;
    for (auto& band : result.bands) band.struct_size = sizeof(band);
    if (!schemaValid_) return result;
    for (const auto& item : descriptors_)
        assign(result, item, item.default_value);
    return result;
}

void ParametricEqModel::setWorkingSettings(
    const EntropyParametricEqSettingsV1& settings) noexcept
{
    if (settings.struct_size >= sizeof(settings) && settings.band_count == 8)
        working_ = settings;
}

bool ParametricEqModel::edit(std::string_view identifier, double value) noexcept
{
    const auto* item = descriptor(identifier);
    return item != nullptr && assign(working_, *item, value);
}

void ParametricEqModel::selectBand(int band) noexcept
{
    selectedBand_ = std::clamp(band, 0, 7);
}

void ParametricEqModel::markSubmitted(std::uint64_t sequence) noexcept
{
    if (sequence != 0) pendingSequence_ = sequence;
}

void ParametricEqModel::acknowledge(std::uint64_t appliedSequence) noexcept
{
    if (pendingSequence_ != 0 && appliedSequence >= pendingSequence_)
        pendingSequence_ = 0;
}

void ParametricEqModel::reject(std::uint64_t requestSequence) noexcept
{
    if (pendingSequence_ != 0 && requestSequence == pendingSequence_)
        pendingSequence_ = 0;
}

double parametricEqMagnitudeDb(const EntropyParametricEqSettingsV1& settings,
                               double frequencyHz,
                               double sampleRate) noexcept
{
    if (!std::isfinite(frequencyHz) || !std::isfinite(sampleRate) ||
        frequencyHz <= 0.0 || sampleRate <= 0.0 || settings.band_count != 8 ||
        settings.mode == ENTROPY_PARAMETRIC_EQ_MODE_BYPASS ||
        settings.mode == ENTROPY_PARAMETRIC_EQ_MODE_DYNAMIC)
        return 0.0;
    constexpr double pi = 3.1415926535897932384626433832795;
    std::complex<double> response{1.0, 0.0};
    for (const auto& band : settings.bands)
    {
        if (!band.enabled || band.static_gain_db == 0.0 || band.q <= 0.0)
            continue;
        const double centre = std::clamp(band.frequency_hz, 20.0, 0.475*sampleRate);
        const double g = std::tan(pi * centre / sampleRate);
        const double mapped = std::tan(pi * std::min(frequencyHz, 0.499*sampleRate) /
                                       sampleRate) / g;
        const std::complex<double> s{0.0, mapped};
        const auto denominator = s*s + (1.0/band.q)*s + 1.0;
        std::complex<double> filtered{};
        if (band.filter_type == ENTROPY_PARAMETRIC_EQ_FILTER_LOW_SHELF)
            filtered = 1.0 / denominator;
        else if (band.filter_type == ENTROPY_PARAMETRIC_EQ_FILTER_HIGH_SHELF)
            filtered = (s*s) / denominator;
        else
            filtered = ((1.0/band.q)*s) / denominator;
        const double multiplier = std::pow(10.0, band.static_gain_db/20.0) - 1.0;
        response += multiplier * filtered;
    }
    return 20.0 * std::log10(std::max(std::abs(response), 1e-12));
}
} // namespace entropy::host
