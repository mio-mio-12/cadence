#pragma once
#include <array>
#include <chrono>
#include <algorithm>
struct TakeRecordingCost {
    std::array<double,4> sum{}; // view pose, world pose, bot snapshots, append
    double total{},peak{};std::size_t samples{};
};
struct TakeRecordingProbe {
    using Clock=std::chrono::steady_clock;
    TakeRecordingCost& cost;Clock::time_point start=Clock::now(),mark=start;
    void stage(std::size_t index){const auto now=Clock::now();cost.sum[index]+=std::chrono::duration<double,std::milli>(now-mark).count();mark=now;}
    ~TakeRecordingProbe(){const double ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();cost.total+=ms;cost.peak=std::max(cost.peak,ms);++cost.samples;}
};
