#pragma once
#include <entropy/r04_diagnostics.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <string>
#include <windows.h>
namespace entropy::host {
struct R04PushEvent {
 std::uint64_t qpc=0,sequence=0,input_offset=0;
 std::uint32_t flags=0;std::int32_t result=0;
 entropy::r04::Metrics input;
};
struct R04RenderEvent {
 std::uint64_t render_offset=0;
 std::uint32_t stages_valid=0;
 std::uint32_t requested_frames=0;
 std::int32_t clock_result=INT32_MIN,guardian_result=INT32_MIN,pcm_result=INT32_MIN,submit_result=INT32_MIN;
 std::uint64_t callback_qpc=0,end_qpc=0;
 std::uint64_t b_qpc=0,c_qpc=0,d_qpc=0;
 std::uint64_t used_sink_frames=0,used_host100ns=0;
 EntropyR04ClockQuality quality{};
 EntropyR04ClockSnapshot clock{};
 entropy::r04::Metrics output,guardian,pcm;
};
// Bit 0 clock snapshot, bit 1 sink quality, bit 2 B, bit 3 C, bit 4 D, bit 5 submit succeeded.
class R04DiagnosticCapture {
 entropy::r04::Queue<R04PushEvent,256> pushes_;
 entropy::r04::Queue<R04RenderEvent,256> renders_;
 std::atomic<bool> stop_{false};
 std::thread worker_;
 std::filesystem::path root_;
 std::uint64_t frequency_=0;
 std::uint32_t sinkRate_=0;
 double estimatedPpm(double hz) const noexcept {return hz>0&&sinkRate_?(hz/sinkRate_-1.0)*1e6:0.0;}
 std::ofstream pushFile_,renderFile_,continuityFile_,statusFile_;
 void run() noexcept;
 void write(const R04PushEvent&);
 void write(const R04RenderEvent&);
 void metric(char boundary,std::uint64_t sequence,std::uint64_t qpc,std::uint64_t offset,const entropy::r04::Metrics&);
public:
 const std::filesystem::path& path()const noexcept{return root_;}
 ~R04DiagnosticCapture(){stop();}
 bool start(const std::filesystem::path&,std::uint32_t domain,std::uint32_t sinkRate,std::uint32_t channels,std::uint32_t target,std::uint32_t capacity,std::uint64_t seed,std::string& error);
 void stop() noexcept;
 void push(const R04PushEvent& e) noexcept {pushes_.push(e);}
 void render(const R04RenderEvent& e) noexcept {renders_.push(e);}
};
}
