#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace cadence {
// UI-thread-only cache. Readers own immutable snapshots so a forced refresh
// inside a package-row button cannot invalidate the surrounding iteration.
template<class T>
class TimedSnapshotCache {
public:
    using Clock=std::chrono::steady_clock;
    explicit TimedSnapshotCache(Clock::duration interval=std::chrono::seconds(1)):interval_(interval){}
    template<class Loader>
    std::shared_ptr<const T> get(const std::string& key,Clock::time_point now,bool force,Loader&& loader){
        if(value_&&key_==key&&!force&&now<nextRefresh_)return value_;
        // Publish only after a successful complete load; exceptions leave the
        // old snapshot intact and do not postpone a subsequent retry.
        auto next=std::make_shared<const T>(std::forward<Loader>(loader)());
        key_=key;value_=std::move(next);nextRefresh_=now+interval_;
        return value_;
    }
private:
    Clock::duration interval_;
    Clock::time_point nextRefresh_{};
    std::string key_;
    std::shared_ptr<const T> value_;
};
}
