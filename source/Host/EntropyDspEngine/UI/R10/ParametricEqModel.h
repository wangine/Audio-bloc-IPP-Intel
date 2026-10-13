#pragma once

#include "Core/EngineTelemetry.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace entropy::host
{
class ParametricEqModel final
{
public:
    bool updateFromTelemetry(const EngineTelemetrySnapshot& telemetry);
    bool schemaValid() const noexcept { return schemaValid_; }
    std::uint32_t bandCount() const noexcept { return 8; }
    const EntropyParametricEqParameterV1* descriptor(
        std::string_view identifier) const noexcept;
    EntropyParametricEqSettingsV1 defaultSettings() const noexcept;
    const EntropyParametricEqSettingsV1& workingSettings() const noexcept
    {
        return working_;
    }
    void setWorkingSettings(const EntropyParametricEqSettingsV1& settings) noexcept;
    bool edit(std::string_view identifier, double value) noexcept;
    void selectBand(int band) noexcept;
    int selectedBand() const noexcept { return selectedBand_; }
    void markSubmitted(std::uint64_t sequence) noexcept;
    void acknowledge(std::uint64_t appliedSequence) noexcept;
    void reject(std::uint64_t requestSequence) noexcept;
    bool pending() const noexcept { return pendingSequence_ != 0; }
    std::uint64_t pendingSequence() const noexcept { return pendingSequence_; }

private:
    static bool assign(EntropyParametricEqSettingsV1& settings,
                       const EntropyParametricEqParameterV1& descriptor,
                       double value) noexcept;
    std::array<EntropyParametricEqParameterV1, 121> descriptors_{};
    std::unordered_map<std::string, std::size_t> descriptorIndex_;
    EntropyParametricEqSettingsV1 working_{};
    bool schemaValid_ = false;
    int selectedBand_ = 0;
    std::uint64_t pendingSequence_ = 0;
};

double parametricEqMagnitudeDb(const EntropyParametricEqSettingsV1& settings,
                               double frequencyHz,
                               double sampleRate) noexcept;
} // namespace entropy::host
