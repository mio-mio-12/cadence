#pragma once
#include "gameplay/ScoreDisplay.h"
#include "gameplay/ScoreEffects.h"
namespace gameplay::score {
struct Point {float x{},y{};};
// Position anchors and transform pivots are deliberately independent.
inline Point anchor(const Settings& s,float width,float height){
    switch(s.anchor){
        case 1:return {};case 2:return {width,0};case 3:return {0,height};case 4:return {width,height};
        case 5:return {width*s.anchorX,height*s.anchorY};default:return {width*.5f,height*.5f};
    }
}
inline Point pivot(const Settings& s,Point size,Point centroid){
    switch(s.pivot){
        case 1:return {};case 2:return {size.x*.5f,size.y};case 3:return {size.x*s.pivotX,size.y*s.pivotY};
        case 4:return centroid;default:return {size.x*.5f,size.y*.5f};
    }
}
inline Point transform(Point vertex,Point origin,Point pivot,float scale,float degrees){
    const float radians=degrees*.0174532925199433f,c=std::cos(radians),s=std::sin(radians);
    const auto x=(vertex.x-pivot.x)*scale,y=(vertex.y-pivot.y)*scale;
    return {origin.x+x*c-y*s,origin.y+x*s+y*c};
}
// Independent UI clock: never changes live scoring, replay time, or simulation.
struct Preview {
    bool playing{true},loop{true},overPov{},povBackground{true},guides{true},useKillAmount{true};
    float time{},gap{.65f},speed{1};int amount{100};double lastClock{-1};
    void advance(const Settings& s,double now){
        if(!std::isfinite(now))return;
        const auto dt=lastClock<0?0.:std::clamp(now-lastClock,0.,.25);lastClock=now;
        const float end=std::max(.001f,duration(s));
        if(playing)time+=float(dt)*speed;
        if(loop)time=std::fmod(std::max(0.f,time),end+std::max(0.f,gap));
        else if(time>=end){time=end;playing=false;}
    }
    State state(const Settings& s)const {State value;value.last=0;value.count=1;value.amount=useKillAmount?s.perKill:amount;return value;}
    void restart(){time=0;playing=true;lastClock=-1;}
};
}
