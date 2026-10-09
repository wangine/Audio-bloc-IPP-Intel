#pragma once
#include <atomic>
#include <cstdint>
namespace entropy::host {
// Last nonzero required-stage fault survives until the observer sees it.
// A good following block does not erase the evidence or change audio policy.
class AuthoritativeFaultStatus final {
public:
 void publish(std::uint32_t code) noexcept {if(code)last_.store(code,std::memory_order_release);}
 std::uint32_t last() const noexcept {return last_.load(std::memory_order_acquire);}
private:
 std::atomic<std::uint32_t> last_{0};
};
}
