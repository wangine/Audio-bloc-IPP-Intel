#pragma once
#include "CrossoverControlLedger.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <thread>
#include <type_traits>
namespace entropy::host {
// Only fixed numerical/text payloads cross owners; never a closure referencing UI.
struct CrossoverWorkerCommand {
    CrossoverCommandId id{};
    std::uint64_t sourceSequence=0;
    std::uint32_t kind=0, source=0, action=0, side=0;
    bool processing=false, cinema=false, right=false;
    double value=0;
    std::array<char,128> identifier{};
};
struct CrossoverWorkerResult {
    CrossoverWorkerCommand command{};
    std::array<EntropyResult,4> results{};
    EntropyResult authorityResult=ENTROPY_ERROR_PROCESSING;
    EntropyCrossoverControlStatusV1 status{};
    bool statusAvailable=false;
    bool cinemaProgram=false, cinemaProgramValid=false;
    std::uint64_t preparationUs=0;
};
// One audio-owner producer / one control-owner consumer. Results reverse direction.
// Mutex protects only non-audio mutable control/view/export state. Audio processing
// never locks it. Main readbacks use try_lock; mutable project state operations
// are excluded from other mutable control access. Save snapshots use a separate
// non-audio owner; prepared audio snapshots continue. Project Load stays stopped.
class CrossoverControlWorker {
public:
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    static constexpr std::size_t capacity=32;
    using Execute=CrossoverWorkerResult(*)(void*,const CrossoverWorkerCommand&);
    CrossoverControlWorker(void* context,Execute execute):context_(context),execute_(execute),thread_([this]{run();}){}
    ~CrossoverControlWorker(){stop();}
    CrossoverControlWorker(const CrossoverControlWorker&)=delete;
    CrossoverControlWorker& operator=(const CrossoverControlWorker&)=delete;
    bool submit(const CrossoverWorkerCommand& command) noexcept {
        if(stopping_.load(std::memory_order_acquire))return false;
        if(outstanding_.load(std::memory_order_acquire)>=capacity){overflow_.fetch_add(1,std::memory_order_relaxed);return false;}
        // Count reservations including results not yet consumed. Thus worker can
        // always publish a result without blocking or dropping acknowledgements.
        outstanding_.fetch_add(1,std::memory_order_release);
        if(!commands_.push(command)){outstanding_.fetch_sub(1,std::memory_order_release);overflow_.fetch_add(1,std::memory_order_relaxed);return false;}
        return true;
    }
    bool poll(CrossoverWorkerResult& result) noexcept {
        if(!results_.pop(result))return false;
        outstanding_.fetch_sub(1,std::memory_order_release);return true;
    }
    bool idle() const noexcept{return outstanding_.load(std::memory_order_acquire)==0;}
    std::uint64_t overflowCount() const noexcept{return overflow_.load(std::memory_order_relaxed);}
    std::mutex& controlMutex() noexcept{return controlMutex_;}
    // Lifecycle owner only, outside running producer loop, before handle destruction.
    void stop() noexcept{stopping_.store(true,std::memory_order_release);if(thread_.joinable())thread_.join();}
private:
    template<class T> struct Queue {
        static_assert(std::is_trivially_copyable_v<T>);
        std::array<T,capacity+1> slots{};
        std::atomic<std::size_t> read{0},write{0};
        bool push(const T& value)noexcept{auto w=write.load(std::memory_order_relaxed);auto n=(w+1)%slots.size();if(n==read.load(std::memory_order_acquire))return false;slots[w]=value;write.store(n,std::memory_order_release);return true;}
        bool pop(T& value)noexcept{auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return false;value=slots[r];read.store((r+1)%slots.size(),std::memory_order_release);return true;}
    };
    void run() noexcept {
        for(;;){
            CrossoverWorkerCommand command{};
            if(!commands_.pop(command)){
                if(stopping_.load(std::memory_order_acquire))break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;
            }
            CrossoverWorkerResult result{};result.command=command;
            auto begin=std::chrono::steady_clock::now();
            {std::lock_guard guard(controlMutex_);try{result=execute_(context_,command);}catch(...){result.command=command;result.authorityResult=ENTROPY_ERROR_PROCESSING;}}
            result.preparationUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
            // Guaranteed space by outstanding reservation, including stop/drain.
            (void)results_.push(result);
        }
    }
    void* context_;Execute execute_;
    Queue<CrossoverWorkerCommand> commands_;
    Queue<CrossoverWorkerResult> results_;
    std::atomic<std::size_t> outstanding_{0};
    std::atomic<std::uint64_t> overflow_{0};
    std::atomic<bool> stopping_{false};
    std::mutex controlMutex_;
    std::thread thread_;
};
}
