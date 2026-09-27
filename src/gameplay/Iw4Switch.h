#pragma once
#include <cmath>
#include <cstddef>
// State decision adapted independently from iw4L weapon_change.rs (pinned in docs).
namespace gameplay::iw4switch {
enum class Action { None, BeginDrop, CancelDrop, RetargetDrop };
enum class Busy { Ready, Fire, Reload, Rechamber, Melee, Offhand };
// iw4L raise_time_for_cmd: missing quick duration uses normal raise, then
// a one-millisecond minimum. Other pipelines retain their authored policy.
inline float raiseDurationSeconds(bool referenceTiming,bool quick,float quickTime,float normalTime){
    if(!referenceTiming)return quick?quickTime:normalTime;
    if(quick&&quickTime>0)return quickTime;
    return normalTime>0?normalTime:.001f;
}
// Combat cadence is independent of presentation clip duration. The extra
// post-burst Ready cooldown must never extend this Firing interval.
struct FireCycle {
    double started{}, until{};
    int slot{-1};
    std::size_t weapon{static_cast<std::size_t>(-1)};
    bool active{};
    void clear(){*this={};}
    void shot(double now,double duration,int held,std::size_t asset){
        clear();
        if(!std::isfinite(now)||!std::isfinite(duration)||duration<=0)return;
        started=now;until=now+duration;slot=held;weapon=asset;
        active=std::isfinite(until);
    }
    bool pending(double now,int held,std::size_t asset,bool enabled=true){
        if(!enabled||!std::isfinite(now)||now<started||now>=until||held!=slot||asset!=weapon)clear();
        return active;
    }
};
inline bool admits(Busy busy,bool timeRemaining,bool delayRemaining=false){
    if(busy==Busy::Melee||busy==Busy::Offhand)return false;
    if(!timeRemaining||busy==Busy::Reload||busy==Busy::Rechamber)return true;
    return busy!=Busy::Fire&&!delayRemaining;
}
inline Action request(int held,int target,int stage,bool allowed){
    if(!allowed)return Action::None;
    if(stage==1)return target==held?Action::CancelDrop:Action::RetargetDrop;
    return target==held?Action::None:Action::BeginDrop;
}
}
