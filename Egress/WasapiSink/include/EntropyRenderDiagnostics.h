#pragma once
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <share.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <memory>

namespace entropy::diagnostics {
inline std::uint64_t ticks() noexcept {
    LARGE_INTEGER value {}; QueryPerformanceCounter(&value); return value.QuadPart;
}
struct Reader {
    std::int64_t starved = 0, resyncs = 0;
    float fill = 0, target = 0, ppm = 0;
    int reason = 0; bool ready = false;
};
struct Record {
    std::uint64_t qpc = 0, generation = 0, dropped = 0;
    int kind = 0, slot = -1, stage = 0;
    HRESULT hr = S_OK, clockHr = E_NOINTERFACE;
    unsigned rate = 0, channels = 0, frames = 0, bits = 0, validBits = 0;
    int encoding = 0;
    DWORD waitResult = 0;
    std::uint64_t waitTicks = 0, eventGapTicks = 0, getTicks = 0;
    std::uint64_t gigTicks = 0, directTicks = 0, postTicks = 0;
    std::uint64_t releaseTicks = 0, totalTicks = 0, clockTicks = 0;
    UINT64 clockPosition = 0, clockQpc = 0, clockFrequency = 0;
    Reader gig {}, direct {};
};
// Single producer (render), single consumer (disk). No locks, allocation or
// file operations on the render path; overload drops records, never audio.
template<class T, std::size_t Capacity> class Queue {
    static_assert(Capacity > 1);
    std::array<T, Capacity> entries_ {};
    alignas(64) std::atomic<std::size_t> write_ {0};
    alignas(64) std::atomic<std::size_t> read_ {0};
public:
    bool push(const T& item) noexcept {
        const auto w = write_.load(std::memory_order_relaxed);
        const auto next = (w + 1) % Capacity;
        if (next == read_.load(std::memory_order_acquire)) return false;
        entries_[w] = item;
        write_.store(next, std::memory_order_release); return true;
    }
    bool pop(T& item) noexcept {
        const auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        item = entries_[r];
        read_.store((r + 1) % Capacity, std::memory_order_release); return true;
    }
};
class Logger {
    Queue<Record, 2048> queue_;
    std::atomic<bool> running_ {true};
    std::thread writer_;
    std::uint64_t dropped_ = 0;
    static inline std::atomic<unsigned> nextId_ {0};
    void run(unsigned id) noexcept {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        FILE* file = nullptr;
        try {
            wchar_t appdata[32768] {};
            const auto length = GetModuleFileNameW(nullptr, appdata, 32768);
            if (!length || length >= 32768) return;
            auto folder = std::filesystem::path(appdata).parent_path() / L"Diagnostics";
            std::filesystem::create_directories(folder);
            LARGE_INTEGER frequency {}; QueryPerformanceFrequency(&frequency);
            FILETIME utc {}; GetSystemTimePreciseAsFileTime(&utc);
            const auto anchor = ticks();
            const auto utc100ns = (std::uint64_t(utc.dwHighDateTime) << 32) | utc.dwLowDateTime;
            unsigned part = 0;
            const auto base = L"PhysicalIO-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(id);
            auto open = [&] {
                const auto path = folder / (base + L"-" + std::to_wstring(part % 4) + L".csv");
                file = _wfsopen(path.c_str(), L"wb", _SH_DENYWR);
                if (!file) return false;
                std::fprintf(file, "# MatrixCrossoverLab=0.1.0 PhysicalIO=0.14-derived qpc_frequency=%lld utc_filetime_100ns=%llu anchor_qpc=%llu part=%u\n",
                    frequency.QuadPart, utc100ns, anchor, part);
                std::fputs("qpc,kind,slot,generation,stage,hr,rate,channels,frames,bits,valid_bits,encoding,wait_result,wait_ticks,event_gap_ticks,get_ticks,gig_ticks,direct_and_mix_ticks,post_ticks,release_ticks,total_before_clock_ticks,clock_call_ticks,clock_hr,clock_position,clock_qpc_100ns,clock_frequency,gig_starved,gig_resync,gig_fill,gig_target,gig_ppm,gig_reason,gig_ready,direct_starved,direct_resync,direct_fill,direct_target,direct_ppm,direct_reason,direct_ready,dropped\n",file);
                std::fflush(file);
                return true;
            };
            if (!open()) return;
            for (;;) {
                Record r;
                bool any = false;
                while (queue_.pop(r)) {
                    any = true;
                    const auto written = std::fprintf(file,
                        "%llu,%d,%d,%llu,%d,0x%08lx,%u,%u,%u,%u,%u,%d,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,0x%08lx,%llu,%llu,%llu,%lld,%lld,%.3f,%.3f,%.3f,%d,%d,%lld,%lld,%.3f,%.3f,%.3f,%d,%d,%llu\n",
                        r.qpc,r.kind,r.slot,r.generation,r.stage,(unsigned long)r.hr,r.rate,r.channels,r.frames,r.bits,r.validBits,r.encoding,
                        r.waitResult,r.waitTicks,r.eventGapTicks,r.getTicks,r.gigTicks,r.directTicks,r.postTicks,r.releaseTicks,r.totalTicks,r.clockTicks,
                        (unsigned long)r.clockHr,r.clockPosition,r.clockQpc,r.clockFrequency,
                        r.gig.starved,r.gig.resyncs,r.gig.fill,r.gig.target,r.gig.ppm,r.gig.reason,int(r.gig.ready),
                        r.direct.starved,r.direct.resyncs,r.direct.fill,r.direct.target,r.direct.ppm,r.direct.reason,int(r.direct.ready),r.dropped);
                    if (written < 0) { std::fclose(file); file = nullptr; return; }
                    if (std::ftell(file) >= 16 * 1024 * 1024) {
                        std::fclose(file); file = nullptr; ++part;
                        if (!open()) return;
                    }
                }
                if (any) std::fflush(file);
                if (!running_.load(std::memory_order_acquire)) {
                    // Producer has stopped before destructor sets running_ to false.
                    if (!any) break;
                    continue;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        } catch (...) { /* Diagnostics must not terminate the audio process. */ }
        if (file) std::fclose(file);
    }
public:
    Logger() noexcept {
        try { writer_ = std::thread([this, id = nextId_++] { run(id); }); }
        catch (...) { running_.store(false); }
    }
    ~Logger() {
        running_.store(false, std::memory_order_release);
        if (writer_.joinable()) writer_.join();
    }
    void push(Record record) noexcept {
        record.qpc = ticks(); record.dropped = dropped_;
        if (!queue_.push(record)) ++dropped_;
    }
};
}
