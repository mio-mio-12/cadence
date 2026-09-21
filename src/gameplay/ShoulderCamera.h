#pragma once
#include "scene/CastScene.h"
#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>

namespace gameplay::view {
struct ShoulderCameraSettings {
    scene::Vec3 position{-90,18,5},adsPosition{-60,14,5}; // forward/right/up, IW inches
    scene::Vec3 rotation{}; // orbit pitch/yaw, view roll, degrees
    float fov{75},adsFov{65};
    void sanitize(){
        auto finite=[](float v,float fallback,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
        for(auto* p:{&position,&adsPosition}){p->x=finite(p->x,-90,-300,0);p->y=finite(p->y,18,-100,100);p->z=finite(p->z,5,-100,100);}
        rotation.x=finite(rotation.x,0,-80,80);rotation.y=finite(rotation.y,0,-180,180);rotation.z=finite(rotation.z,0,-90,90);
        fov=finite(fov,75,5,150);adsFov=finite(adsFov,65,5,150);
    }
};
struct CameraControls {
    ShoulderCameraSettings shoulder;
    float zoomInDuration{},zoomOutDuration{},zoomIntensity{}; // zero duration follows weapon timing
    void sanitize(){shoulder.sanitize();for(auto* v:{&zoomInDuration,&zoomOutDuration})*v=std::isfinite(*v)?std::clamp(*v,0.f,5.f):0.f;zoomIntensity=std::isfinite(zoomIntensity)?std::clamp(zoomIntensity,0.f,2.f):0.f;}
};
inline std::ostream& operator<<(std::ostream& o,const CameraControls& c){const auto& s=c.shoulder;return o<<s.position.x<<' '<<s.position.y<<' '<<s.position.z<<' '<<s.adsPosition.x<<' '<<s.adsPosition.y<<' '<<s.adsPosition.z<<' '<<s.rotation.x<<' '<<s.rotation.y<<' '<<s.rotation.z<<' '<<s.fov<<' '<<s.adsFov<<' '<<c.zoomInDuration<<' '<<c.zoomOutDuration<<' '<<c.zoomIntensity;}
inline std::istream& operator>>(std::istream& i,CameraControls& c){CameraControls v;auto& s=v.shoulder;if(i>>s.position.x>>s.position.y>>s.position.z>>s.adsPosition.x>>s.adsPosition.y>>s.adsPosition.z>>s.rotation.x>>s.rotation.y>>s.rotation.z>>s.fov>>s.adsFov>>v.zoomInDuration>>v.zoomOutDuration>>v.zoomIntensity){v.sanitize();c=v;}return i;}
inline scene::Vec3 shoulderBoom(const ShoulderCameraSettings& s,float yaw,float pitch,float ads){
    const float y=yaw+s.rotation.y*scene::kPi/180,p=std::clamp(pitch+s.rotation.x*scene::kPi/180,-1.5f,1.5f);
    const scene::Vec3 forward{std::cos(p)*std::cos(y),std::cos(p)*std::sin(y),std::sin(p)},right{std::sin(y),-std::cos(y),0};
    const auto offset=s.position*(1-std::clamp(ads,0.f,1.f))+s.adsPosition*std::clamp(ads,0.f,1.f);
    return (forward*offset.x+right*offset.y+scene::Vec3{0,0,offset.z})*2.54f;
}
inline scene::Vec3 shoulderUp(scene::Vec3 forward,float rollDegrees){
    auto lateral=scene::normalize(scene::cross(scene::Vec3{0,0,1},forward));
    if(scene::length(lateral)<.001f)lateral={0,1,0};
    const auto up=scene::normalize(scene::cross(forward,lateral));const float r=rollDegrees*scene::kPi/180;
    return scene::normalize(up*std::cos(r)+lateral*std::sin(r));
}
inline float zoomFov(float hip,float ads,float amount,float intensity){return std::clamp(hip+(ads-hip)*std::clamp(amount,0.f,1.f)*std::clamp(intensity,0.f,2.f),1.f,179.f);}
inline float scopeFov(float transitionFov,float weaponAdsFov,bool scopeVisible){
    return scopeVisible?std::clamp(std::isfinite(weaponAdsFov)?weaponAdsFov:30.f,1.f,179.f):transitionFov;
}
}
