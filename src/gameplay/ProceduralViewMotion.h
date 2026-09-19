#pragma once
#include "gameplay/ViewRecoil.h"
namespace gameplay::view {
inline RecoilSettings mantleResponse(){RecoilSettings s;s.count=5;s.points={{{0,0},{.12f,1},{.286f,-.4f},{.4f,0},{1,0}}};return s;}
struct MotionProfile {
    bool enabled{true};float intensity{.19f},duration{1.05f};
    scene::Vec3 position{-2,0,-10.8f},rotation{0,-19.3f,0};
    RecoilSettings curve{mantleResponse()};
};
inline MotionProfile boostDefaults(){return {true,.19f,1.05f,{2.7f,0,.4f},{0,21.7f,0}};}
inline MotionProfile boostCameraDefaults(){MotionProfile s{true,.70f,.74f,{0,0,0},{-3.3f,6.1f,0}};s.curve.points={{{0,0},{.113f,.47f},{.286f,-.4f},{.4f,0},{1,0}}};return s;}
inline MotionProfile slideDefaults(){MotionProfile s{true,.39f,3.f,{0,0,-3.1f},{90,0,0}};s.curve.points={{{0,0},{.055f,.75f},{.098f,.52f},{.5f,0},{1,0}}};return s;}
struct DirectionalTilt {
    bool enabled{true},camera{true},viewmodel{false};
    float intensity{.07f},rollDegrees{4.7f},pitchDegrees{6},speed{300},attack{.09f},release{.20f},adsScale{0};
};
inline scene::Vec3 tiltTarget(const DirectionalTilt& s,scene::Vec3 velocity,float yaw,float ads){
    if(!s.enabled)return {};
    const float scale=s.intensity*(1+(s.adsScale-1)*std::clamp(ads,0.f,1.f));
    const float forward=velocity.x*std::cos(yaw)+velocity.y*std::sin(yaw);
    const float side=-velocity.x*std::sin(yaw)+velocity.y*std::cos(yaw);
    return {-std::clamp(side/std::max(1.f,s.speed),-1.f,1.f)*s.rollDegrees*scale,
        std::clamp(forward/std::max(1.f,s.speed),-1.f,1.f)*s.pitchDegrees*scale,0};
}
inline std::ostream& operator<<(std::ostream& o,const DirectionalTilt& s){return o<<s.enabled<<' '<<s.camera<<' '<<s.viewmodel<<' '<<s.intensity<<' '<<s.rollDegrees<<' '<<s.pitchDegrees<<' '<<s.speed<<' '<<s.attack<<' '<<s.release<<' '<<s.adsScale;}
inline std::istream& operator>>(std::istream& in,DirectionalTilt& s){DirectionalTilt v;if(in>>v.enabled>>v.camera>>v.viewmodel>>v.intensity>>v.rollDegrees>>v.pitchDegrees>>v.speed>>v.attack>>v.release>>v.adsScale){const auto safe=[](float x,float lo,float hi,float d){return std::isfinite(x)?std::clamp(x,lo,hi):d;};v.intensity=safe(v.intensity,0,3,1);v.rollDegrees=safe(v.rollDegrees,-15,15,2);v.pitchDegrees=safe(v.pitchDegrees,-15,15,.5f);v.speed=safe(v.speed,20,1000,300);v.attack=safe(v.attack,.01f,1,.12f);v.release=safe(v.release,.01f,2,.22f);v.adsScale=safe(v.adsScale,0,1,.35f);s=v;}return in;}
inline scene::Vec3 advanceTilt(scene::Vec3 current,scene::Vec3 target,float dt,const DirectionalTilt& s){
    const float seconds=scene::length(target)>scene::length(current)?s.attack:s.release;
    return scene::lerp(current,target,1-std::exp(-std::max(0.f,dt)/std::max(.01f,seconds)));
}
inline scene::Mat4 motionTransform(const MotionProfile& s,float elapsed,float units,scene::Vec2 direction={1,0},bool directional=false){
    const float value=s.enabled?s.curve.sample(elapsed/std::max(.05f,s.duration))*s.intensity:0;
    auto p=s.position,r=s.rotation;
    if(directional){p.x*=direction.x;p.y*=direction.y;r.x*=direction.y;r.y*=direction.x;r.z*=direction.y;}
    return scene::trs(p*(units*value),scene::fromEulerRadians(r*(scene::kPi/180.f*value)),{1,1,1});
}
inline std::ostream& operator<<(std::ostream& o,const MotionProfile& s){return o<<s.enabled<<' '<<s.intensity<<' '<<s.duration<<' '<<s.position.x<<' '<<s.position.y<<' '<<s.position.z<<' '<<s.rotation.x<<' '<<s.rotation.y<<' '<<s.rotation.z<<' '<<s.curve;}
inline std::istream& operator>>(std::istream& in,MotionProfile& s){MotionProfile v;if(in>>v.enabled>>v.intensity>>v.duration>>v.position.x>>v.position.y>>v.position.z>>v.rotation.x>>v.rotation.y>>v.rotation.z>>v.curve){const auto safe=[](float x,float a,float b,float fallback){return std::isfinite(x)?std::clamp(x,a,b):fallback;};v.intensity=safe(v.intensity,0,2,.19f);v.duration=safe(v.duration,.05f,5,1.05f);v.position={safe(v.position.x,-20,20,0),safe(v.position.y,-20,20,0),safe(v.position.z,-20,20,0)};v.rotation={safe(v.rotation.x,-90,90,0),safe(v.rotation.y,-90,90,0),safe(v.rotation.z,-90,90,0)};s=v;}return in;}
}
