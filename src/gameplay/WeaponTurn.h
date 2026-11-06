#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <cmath>

namespace gameplay::weapon_turn {
// Behavioral reference: iw4L a6fa0b9, weapon_iw4/src/sway.rs.
// AK47-class tuning from the user's IW4 raw weapon data. This is an
// experimental presentation adapter, not a claim of whole-engine parity.
struct Params { float maxAngle, speed, pitch, yaw, horizontal, vertical; };
constexpr Params hip{20,4,-1,-1,.3f,.1f}, ads{2,6,.1f,.1f,.14f,.25f};
inline float wrap(float value){return std::remainder(value,360.f);}
inline float track(float target,float current,float speed,float dt){
    const float difference=target-current;
    return std::abs(difference)<=.001f?target:current+difference*std::clamp(speed*dt,0.f,1.f);
}
struct State {
    bool initialized{};
    float previousPitch{},previousYaw{},pitch{},yaw{},horizontal{},vertical{};
    void update(float viewPitch,float viewYaw,float adsFraction,float dt){
        if(!std::isfinite(viewPitch)||!std::isfinite(viewYaw)||!std::isfinite(dt)||dt<0){*this={};return;}
        if(!initialized||dt>.25f){*this={};initialized=true;previousPitch=viewPitch;previousYaw=viewYaw;return;}
        if(dt==0)return;
        const float t=std::clamp(adsFraction,0.f,1.f);
        const auto mix=[&](float a,float b){return a+(b-a)*t;};
        const float limit=mix(hip.maxAngle,ads.maxAngle),speed=mix(hip.speed,ads.speed);
        const float dp=std::clamp(wrap(viewPitch-previousPitch)/(dt*60),-limit,limit);
        const float dy=std::clamp(wrap(viewYaw-previousYaw)/(dt*60),-limit,limit);
        pitch=wrap(track(pitch+wrap(mix(hip.pitch,ads.pitch)*dp-pitch),pitch,speed,dt));
        yaw=wrap(track(yaw+wrap(mix(hip.yaw,ads.yaw)*dy-yaw),yaw,speed,dt));
        horizontal=track(mix(hip.horizontal,ads.horizontal)*dy,horizontal,speed,dt);
        vertical=track(mix(hip.vertical,ads.vertical)*dp,vertical,speed,dt);
        previousPitch=viewPitch;previousYaw=viewYaw;
    }
    scene::Mat4 transform(float adsFraction=0,float hipMultiplier=1,float adsMultiplier=1)const{
        const float t=std::clamp(adsFraction,0.f,1.f);
        const float strength=hipMultiplier+(adsMultiplier-hipMultiplier)*t;
        if(!std::isfinite(strength))return scene::Mat4::identity();
        // Native camera basis is +X forward, +Y left, +Z up. IW pitch
        // (positive down) rotates about +Y; IW yaw about +Z.
        return scene::trs({0,-horizontal*2.54f*strength,vertical*2.54f*strength},
            scene::fromEulerRadians({0,-pitch*strength*scene::kPi/180.f,-yaw*strength*scene::kPi/180.f}),{1,1,1});
    }
};
}
