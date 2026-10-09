#pragma once

#include <cstdint>

namespace entropy::host
{
class GraphMetadataSyncGate final
{
public:
    bool changed(std::uint64_t parameterGeneration,
                 std::uint64_t matrixGeneration,
                 std::uint64_t bulkGeneration,
                 std::uint32_t muteMask) noexcept
    {
        const bool result = !initialized_ ||
            parameterGeneration_ != parameterGeneration ||
            matrixGeneration_ != matrixGeneration ||
            bulkGeneration_ != bulkGeneration ||
            muteMask_ != muteMask;
        initialized_ = true;
        parameterGeneration_ = parameterGeneration;
        matrixGeneration_ = matrixGeneration;
        bulkGeneration_ = bulkGeneration;
        muteMask_ = muteMask;
        return result;
    }

private:
    bool initialized_ = false;
    std::uint64_t parameterGeneration_ = 0;
    std::uint64_t matrixGeneration_ = 0;
    std::uint64_t bulkGeneration_ = 0;
    std::uint32_t muteMask_ = 0;
};
}
