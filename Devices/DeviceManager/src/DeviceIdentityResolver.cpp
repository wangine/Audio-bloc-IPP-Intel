#include "DeviceIdentityResolver.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace entropy::device
{
namespace
{
template <std::size_t Size>
void copyText(char (&destination)[Size], const char* source) noexcept
{
    std::fill(std::begin(destination), std::end(destination), '\0');
    if (source == nullptr) return;
    const auto count = (std::min)(Size - 1, std::strlen(source));
    std::memcpy(destination, source, count);
}

bool equalText(const char* first, const char* second) noexcept
{
    return first != nullptr && second != nullptr && first[0] != 0 &&
           second[0] != 0 && std::strcmp(first, second) == 0;
}

const EntropyDeviceEvidence* evidence(const EntropyDeviceInfo& device,
                                     std::uint32_t kind) noexcept
{
    const auto count = (std::min)(device.evidence_count,
                                  ENTROPY_DEVICE_MAX_EVIDENCE);
    for (std::uint32_t index = 0; index < count; ++index)
        if (device.evidence[index].kind == kind &&
            device.evidence[index].value_utf8[0] != 0)
            return &device.evidence[index];
    return nullptr;
}

const EntropyDeviceEvidence* evidence(const EntropyDeviceBinding& binding,
                                     std::uint32_t kind) noexcept
{
    const auto count = (std::min)(binding.evidence_count,
                                  ENTROPY_DEVICE_MAX_EVIDENCE);
    for (std::uint32_t index = 0; index < count; ++index)
        if (binding.evidence[index].kind == kind &&
            binding.evidence[index].value_utf8[0] != 0)
            return &binding.evidence[index];
    return nullptr;
}

bool exactEndpoint(const EntropyDeviceBinding& binding,
                   const EntropyDeviceInfo& device) noexcept
{
    return equalText(binding.approved_endpoint_id_utf8,
                     device.endpoint_id_utf8);
}

bool strongUniqueMatch(const EntropyDeviceBinding& binding,
                       const EntropyDeviceInfo& device) noexcept
{
    constexpr std::array<std::uint32_t, 3> kinds{
        ENTROPY_DEVICE_EVIDENCE_SERIAL_NUMBER,
        ENTROPY_DEVICE_EVIDENCE_CONTAINER_ID,
        ENTROPY_DEVICE_EVIDENCE_INSTANCE_ID};
    for (const auto kind : kinds)
    {
        const auto* saved = evidence(binding, kind);
        const auto* current = evidence(device, kind);
        if (saved == nullptr || current == nullptr ||
            saved->stability < ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE ||
            current->stability < ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE)
            continue;
        if (equalText(saved->value_utf8, current->value_utf8)) return true;
    }
    return false;
}

bool resembles(const EntropyDeviceBinding& binding,
               const EntropyDeviceInfo& device) noexcept
{
    constexpr std::array<std::uint32_t, 7> kinds{
        ENTROPY_DEVICE_EVIDENCE_SERIAL_NUMBER,
        ENTROPY_DEVICE_EVIDENCE_CONTAINER_ID,
        ENTROPY_DEVICE_EVIDENCE_INSTANCE_ID,
        ENTROPY_DEVICE_EVIDENCE_VID_PID,
        ENTROPY_DEVICE_EVIDENCE_HARDWARE_IDS,
        ENTROPY_DEVICE_EVIDENCE_FRIENDLY_NAME,
        ENTROPY_DEVICE_EVIDENCE_DRIVER_IDENTITY};
    for (const auto kind : kinds)
    {
        const auto* saved = evidence(binding, kind);
        const auto* current = evidence(device, kind);
        if (saved != nullptr && current != nullptr &&
            equalText(saved->value_utf8, current->value_utf8))
            return true;
    }
    return false;
}

std::uint32_t materialMismatchCount(const EntropyDeviceBinding& binding,
                                    const EntropyDeviceInfo& device) noexcept
{
    std::uint32_t mismatches = 0;
    const auto count = (std::min)(binding.evidence_count,
                                  ENTROPY_DEVICE_MAX_EVIDENCE);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const auto& saved = binding.evidence[index];
        if ((saved.flags & ENTROPY_DEVICE_EVIDENCE_FLAG_MATERIAL) == 0 ||
            saved.value_utf8[0] == 0)
            continue;
        const auto* current = evidence(device, saved.kind);
        if (current == nullptr ||
            !equalText(saved.value_utf8, current->value_utf8))
            ++mismatches;
    }
    return mismatches;
}

void detail(EntropyDeviceBindingResolution& resolution,
            const char* text) noexcept
{
    copyText(resolution.detail_utf8, text);
}

void addCandidate(EntropyDeviceBindingResolution& resolution,
                  std::uint32_t index) noexcept
{
    if (resolution.candidate_count < ENTROPY_DEVICE_MAX_MATCHES)
        resolution.matching_device_indices[resolution.candidate_count] = index;
    ++resolution.candidate_count;
}

std::uint64_t fnv(std::uint64_t hash, const void* data,
                  std::size_t bytes) noexcept
{
    const auto* cursor = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < bytes; ++index)
    {
        hash ^= cursor[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}
}

void normalizeDevice(EntropyDeviceInfo& device) noexcept
{
    device.struct_size = sizeof(device);
    device.evidence_count = (std::min)(device.evidence_count,
                                       ENTROPY_DEVICE_MAX_EVIDENCE);
    device.format_count = (std::min)(device.format_count,
                                     ENTROPY_DEVICE_MAX_FORMATS);
    std::uint64_t first = 1469598103934665603ULL;
    std::uint64_t second = 1099511628211ULL;
    for (std::uint32_t index = 0; index < device.evidence_count; ++index)
    {
        auto& item = device.evidence[index];
        item.struct_size = sizeof(item);
        const auto bytes = std::strlen(item.value_utf8);
        first = fnv(first, &item.kind, sizeof(item.kind));
        first = fnv(first, &item.stability, sizeof(item.stability));
        first = fnv(first, item.value_utf8, bytes);
        second = fnv(second, &item.flags, sizeof(item.flags));
        second = fnv(second, item.value_utf8, bytes);
    }
    for (std::uint32_t index = 0; index < device.format_count; ++index)
    {
        device.formats[index].struct_size = sizeof(EntropyDeviceExactFormat);
        first = fnv(first, &device.formats[index],
                    sizeof(EntropyDeviceExactFormat));
    }
    std::snprintf(device.composite_fingerprint,
                  sizeof(device.composite_fingerprint),
                  "%016llx%016llx",
                  static_cast<unsigned long long>(first),
                  static_cast<unsigned long long>(second));
}

EntropyDeviceBinding captureBinding(const EntropyDeviceInfo& device,
                                    const char* logicalDeviceId,
                                    std::uint64_t approvalRevision) noexcept
{
    EntropyDeviceBinding binding{};
    binding.struct_size = sizeof(binding);
    binding.schema_version = ENTROPY_DEVICE_BINDING_SCHEMA_VERSION;
    binding.approval_revision = approvalRevision;
    copyText(binding.logical_device_id, logicalDeviceId);
    copyText(binding.approved_endpoint_id_utf8, device.endpoint_id_utf8);
    copyText(binding.approved_display_name_utf8, device.display_name_utf8);
    copyText(binding.composite_fingerprint, device.composite_fingerprint);
    binding.evidence_count = (std::min)(device.evidence_count,
                                        ENTROPY_DEVICE_MAX_EVIDENCE);
    binding.format_count = (std::min)(device.format_count,
                                      ENTROPY_DEVICE_MAX_FORMATS);
    std::copy_n(device.evidence, binding.evidence_count, binding.evidence);
    std::copy_n(device.formats, binding.format_count, binding.formats);
    return binding;
}

EntropyDeviceBindingResolution resolveBinding(
    const EntropyDeviceBinding& binding,
    const std::vector<EntropyDeviceInfo>& devices) noexcept
{
    EntropyDeviceBindingResolution resolution{};
    resolution.struct_size = sizeof(resolution);
    resolution.matched_device_index = UINT32_MAX;
    std::fill(std::begin(resolution.matching_device_indices),
              std::end(resolution.matching_device_indices), UINT32_MAX);

    if (binding.struct_size < sizeof(binding) ||
        binding.schema_version != ENTROPY_DEVICE_BINDING_SCHEMA_VERSION ||
        binding.approval_revision == 0 ||
        binding.logical_device_id[0] == 0 || binding.evidence_count == 0)
    {
        resolution.state = ENTROPY_DEVICE_UNVERIFIED;
        detail(resolution, "No explicit Host/User approved binding exists");
        return resolution;
    }

    std::vector<std::uint32_t> exact;
    std::vector<std::uint32_t> strong;
    std::vector<std::uint32_t> similar;
    for (std::uint32_t index = 0; index < devices.size(); ++index)
    {
        // Windows retains disconnected MMDevice endpoints.  They are useful
        // inventory records, but may never satisfy a live binding.
        if (devices[index].active == 0) continue;
        if (exactEndpoint(binding, devices[index])) exact.push_back(index);
        if (strongUniqueMatch(binding, devices[index])) strong.push_back(index);
        if (resembles(binding, devices[index])) similar.push_back(index);
    }
    resolution.exact_endpoint_matches = static_cast<std::uint32_t>(exact.size());
    resolution.strong_identity_matches = static_cast<std::uint32_t>(strong.size());

    const auto finalizeUnique = [&](std::uint32_t index,
                                    const char* reason) {
        resolution.material_mismatches =
            materialMismatchCount(binding, devices[index]);
        addCandidate(resolution, index);
        resolution.matched_device_index = index;
        if (resolution.material_mismatches != 0)
        {
            resolution.state = ENTROPY_DEVICE_CHANGED;
            detail(resolution, "Identity candidate found, but material capabilities changed");
            return;
        }
        resolution.state = ENTROPY_DEVICE_VERIFIED;
        copyText(resolution.resolved_endpoint_id_utf8,
                 devices[index].endpoint_id_utf8);
        detail(resolution, reason);
    };

    if (exact.size() == 1)
    {
        finalizeUnique(exact.front(),
                       "Verified by the exact previously approved endpoint and material evidence");
        return resolution;
    }
    if (exact.size() > 1)
    {
        resolution.state = ENTROPY_DEVICE_AMBIGUOUS;
        for (const auto index : exact) addCandidate(resolution, index);
        detail(resolution, "More than one endpoint claims the approved open address");
        return resolution;
    }
    if (strong.size() == 1)
    {
        finalizeUnique(strong.front(),
                       "Verified by unique hardware-stable identity evidence");
        return resolution;
    }
    if (strong.size() > 1)
    {
        resolution.state = ENTROPY_DEVICE_AMBIGUOUS;
        for (const auto index : strong) addCandidate(resolution, index);
        detail(resolution, "Strong identity evidence resolves to multiple endpoints");
        return resolution;
    }
    if (similar.empty())
    {
        resolution.state = ENTROPY_DEVICE_OFFLINE;
        detail(resolution, "No current endpoint resembles the approved DAC identity");
        return resolution;
    }
    for (const auto index : similar) addCandidate(resolution, index);
    if (similar.size() > 1)
    {
        resolution.state = ENTROPY_DEVICE_AMBIGUOUS;
        detail(resolution, "Only weak/composite evidence matches multiple endpoints; refusing to guess");
    }
    else
    {
        resolution.state = ENTROPY_DEVICE_CHANGED;
        resolution.matched_device_index = similar.front();
        resolution.material_mismatches =
            materialMismatchCount(binding, devices[similar.front()]);
        detail(resolution, "Endpoint address changed without unique hardware-stable proof; approval required");
    }
    return resolution;
}
}
