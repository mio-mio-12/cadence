#pragma once
#include "scene/Math3D.h"
#include <istream>
#include <ostream>

namespace gameplay::view {
struct RecoilSettings {
    float intensity{1.f};
    float duration{.40f},smoothness{1.f},maxPitch{12.f},maxYaw{8.f},maxRoll{4.f};
    float rollMin{-.12f},rollMax{.12f},authoredCameraScale{1.f};
    int count{4};
    std::array<scene::Vec2,16> points{{{0,0},{.12f,1},{.4f,.4f},{1,0}}};
    void sanitize(){
        const auto safe=[](float x,float lo,float hi,float fallback){return std::isfinite(x)?std::clamp(x,lo,hi):fallback;};
        duration=safe(duration,.04f,3,.4f);smoothness=safe(smoothness,0,1,1);
        intensity=safe(intensity,0,5,1);
        maxPitch=safe(maxPitch,0,35,12);maxYaw=safe(maxYaw,0,35,8);maxRoll=safe(maxRoll,0,20,4);
        rollMin=safe(rollMin,-10,10,-.12f);rollMax=safe(rollMax,-10,10,.12f);authoredCameraScale=safe(authoredCameraScale,0,2,1);
        count=std::clamp(count,2,16);points[0]={0,0};points[count-1]={1,0};
        for(int i=1;i<count-1;++i){points[i].x=safe(points[i].x,points[i-1].x+.001f,1.f-(count-1-i)*.001f,float(i)/(count-1));points[i].y=safe(points[i].y,-1,2,0);}
    }
    float sample(float t)const{
        if(!std::isfinite(t)||t<=0||t>=1)return 0;
        for(int i=1;i<count;++i)if(t<=points[i].x){float u=std::clamp((t-points[i-1].x)/std::max(.001f,points[i].x-points[i-1].x),0.f,1.f);u+=(u*u*(3-2*u)-u)*smoothness;return points[i-1].y+(points[i].y-points[i-1].y)*u;}
        return 0;
    }
};
inline std::ostream& operator<<(std::ostream& o,const RecoilSettings& s){o<<s.duration<<' '<<s.smoothness<<' '<<s.maxPitch<<' '<<s.maxYaw<<' '<<s.maxRoll<<' '<<s.rollMin<<' '<<s.rollMax<<' '<<s.authoredCameraScale<<' '<<s.count;for(int i=0;i<s.count;++i)o<<' '<<s.points[i].x<<' '<<s.points[i].y;return o;}
inline std::istream& operator>>(std::istream& in,RecoilSettings& s){RecoilSettings v;in>>v.duration>>v.smoothness>>v.maxPitch>>v.maxYaw>>v.maxRoll>>v.rollMin>>v.rollMax>>v.authoredCameraScale>>v.count;if(v.count<2||v.count>16){in.setstate(std::ios::failbit);return in;}for(int i=0;i<v.count;++i)in>>v.points[i].x>>v.points[i].y;if(in){v.sanitize();s=v;}return in;}

// Fixed capacity, no runtime allocations. Each shot starts at zero and finishes
// at zero; overlapping shots add, with bounded pitch/yaw/roll before rotation.
struct RecoilState {
    struct Pulse {float age{},duration{};scene::Vec3 kick{};RecoilSettings shape{};};
    std::array<Pulse,64> pulses{};std::size_t next{};
    void kick(scene::Vec3 amplitude,float centerSpeed,RecoilSettings shape){shape.sanitize();auto& p=pulses[next++%pulses.size()];p={0,shape.duration*std::clamp(15.f/std::max(1.f,centerSpeed),.25f,4.f),amplitude,shape};}
    scene::Vec3 advance(float dt,const RecoilSettings& limits){scene::Vec3 result{};dt=std::isfinite(dt)?std::max(0.f,dt):0;
        for(auto& p:pulses)if(p.duration>0){p.age+=dt;if(p.age>=p.duration){p.duration=0;continue;}result+=p.kick*p.shape.sample(p.age/p.duration);}
        return scene::Vec3{std::clamp(result.x,-limits.maxPitch,limits.maxPitch),std::clamp(result.y,-limits.maxYaw,limits.maxYaw),std::clamp(result.z,-limits.maxRoll,limits.maxRoll)}*std::clamp(limits.intensity,0.f,5.f);
    }
};
// Bounded quaternion rotation: avoids the old tan() singularity and Euler wraps.
inline void additiveCamera(scene::Vec3& forward,scene::Vec3& up,scene::Vec3 degrees){
    auto right=scene::normalize(scene::cross(up,forward));up=scene::normalize(scene::cross(forward,right));
    const auto rotate=[](scene::Vec3 v,scene::Quat q){const scene::Vec3 xyz{q.x,q.y,q.z};return v+scene::cross(xyz,scene::cross(xyz,v)+v*q.w)*2.f;};
    const auto pitch=scene::fromAxisAngle(right,-std::clamp(degrees.x,-45.f,45.f)*scene::kPi/180);
    forward=rotate(forward,pitch);up=rotate(up,pitch);
    const auto yaw=scene::fromAxisAngle(up,std::clamp(degrees.y,-45.f,45.f)*scene::kPi/180);forward=rotate(forward,yaw);
    up=rotate(up,scene::fromAxisAngle(forward,std::clamp(degrees.z,-30.f,30.f)*scene::kPi/180));
    forward=scene::normalize(forward);right=scene::normalize(scene::cross(up,forward));up=scene::normalize(scene::cross(forward,right));
}
inline float rechamberDelay(float overrideSeconds,float fireDuration){return std::isfinite(overrideSeconds)&&overrideSeconds>=0?std::clamp(overrideSeconds,0.f,5.f):std::max(.03f,fireDuration);}
}
