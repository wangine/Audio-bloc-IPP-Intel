#pragma once
#include <atomic>
#include <algorithm>
#include <cstring>
#include <string_view>
namespace entropy::host {
class FirstOutputFault final {
public:
    void capture(unsigned domain,std::string_view detail) noexcept {
        if(claimed_.test_and_set(std::memory_order_acquire))return;
        domain_=domain;
        const auto count=(std::min)(detail.size(),sizeof(detail_)-1);
        std::memcpy(detail_,detail.data(),count);detail_[count]=0;
        ready_.store(true,std::memory_order_release);
    }
    bool read(unsigned& domain,char* text,std::size_t capacity) const noexcept {
        if(!ready_.load(std::memory_order_acquire)||!text||!capacity)return false;
        domain=domain_;
        const auto count=(std::min)(std::strlen(detail_),capacity-1);
        std::memcpy(text,detail_,count);text[count]=0;return true;
    }
private:
    std::atomic_flag claimed_=ATOMIC_FLAG_INIT;
    std::atomic<bool> ready_{false};
    unsigned domain_=0;
    char detail_[512]{};
};
}
