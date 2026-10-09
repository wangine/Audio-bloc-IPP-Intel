#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace entropy::host::ui
{
// The sink queues BEGIN (0), SET (1), END (2) in this order.
class DelayEditModel
{
public:
    template<class Sink> void begin(std::string id, double initial, Sink&& sink)
    {
        if (active_) return;
        id_ = std::move(id);
        accumulator_ = std::clamp(initial, -800.0, 800.0);
        failed_ = !sink(id_, 0, value());
        active_ = !failed_;
    }
    template<class Sink> void set(double value, Sink&& sink)
    {
        if (!active_) return;
        accumulator_ = std::clamp(value, -800.0, 800.0);
        failed_ = !sink(id_, 1, this->value()) || failed_;
    }
    template<class Sink> void move(int delta, double ratio, Sink&& sink)
    { set(accumulator_ + delta * 1.6 * ratio, sink); }
    template<class Sink> void finish(Sink&& sink)
    {
        if (!active_) return;
        failed_ = !sink(id_, 1, value()) || failed_;
        failed_ = !sink(id_, 2, value()) || failed_;
        active_ = false;
    }
    bool active() const { return active_; }
    bool failed() const { return failed_; }
    const std::string& id() const { return id_; }
    double value() const { return std::round(accumulator_ * 1000.0) / 1000.0; }
    static double effective(double global, double relative)
    { return std::clamp(global + relative, 0.0, 1600.0); }
private:
    std::string id_;
    double accumulator_ = 0;
    bool active_ = false, failed_ = false;
};
}
