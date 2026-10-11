#include "MultiChannelSrcEngine.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace { thread_local bool tracking = false; thread_local std::size_t allocations = 0, deallocations = 0; }
void* operator new(std::size_t size) { if (tracking) ++allocations; if (auto* p = std::malloc(size == 0 ? 1 : size)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { if (tracking && p != nullptr) ++deallocations; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

int main()
{
    try
    {
        constexpr std::uint32_t channels = 8, frames = 257;
        for (std::uint32_t engine = 0; engine < 2; ++engine)
        {
            entropy::src::MultiChannelSrcEngine converter;
            EntropySrcConfig config {};
            config.struct_size = sizeof(config); config.engine = engine; config.source_rate = 48000;
            config.destination_rate = 192000; config.channels = channels; config.maximum_input_frames = frames;
            if (!converter.configure(config)) throw std::runtime_error("configure failed");
            config = converter.config();
            std::array<std::vector<double>, channels> input, output;
            std::array<const double*, channels> inputs {}; std::array<double*, channels> outputs {};
            for (std::uint32_t channel = 0; channel < channels; ++channel)
            {
                input[channel].resize(frames); output[channel].resize(config.required_output_capacity);
                inputs[channel] = input[channel].data(); outputs[channel] = output[channel].data();
            }
            EntropySrcBlockF64 block {};
            block.struct_size = sizeof(block); block.channels = channels; block.input_frames = frames;
            block.output_capacity = config.required_output_capacity; block.inputs = inputs.data(); block.outputs = outputs.data();
            for (std::uint32_t iteration = 0; iteration < 64; ++iteration)
                if (converter.process(block) != ENTROPY_OK) throw std::runtime_error("warmup failed");
            allocations = deallocations = 0;
            for (std::uint32_t iteration = 0; iteration < 256; ++iteration)
            {
                for (std::uint32_t channel = 0; channel < channels; ++channel)
                    for (std::uint32_t frame = 0; frame < frames; ++frame)
                        input[channel][frame] = 0.1 * std::sin(0.017 * (iteration * frames + frame + channel));
                tracking = true;
                const auto result = converter.process(block);
                tracking = false;
                if (result != ENTROPY_OK) throw std::runtime_error("realtime process failed");
            }
            std::cout << "engine " << engine << " allocations=" << allocations << " deallocations=" << deallocations << '\n';
            if (allocations != 0 || deallocations != 0) throw std::runtime_error("audio callback allocated memory");
        }
        return 0;
    }
    catch (const std::exception& error) { tracking = false; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
