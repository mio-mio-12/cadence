#pragma once
#include "scene/Math3D.h"
#include <algorithm>

namespace gameplay::presentation {
inline float falloffAlpha(double now,double start,float duration){
    if(start<0||duration<=0)return 1;
    const float t=static_cast<float>(std::clamp((now-start)/duration,0.0,1.0));
    return t*t*(3-2*t);
}
// Consume the controller's existing interpolation, never smooth its output a
// second time. Frame-stepped mantle/noclip publish their current position here.
inline scene::Mat4 worldActorTransform(scene::Vec3 authoritative,scene::Vec3 interpolated,
                                      bool renderSample,float yaw,float groundLift){
    const auto position=renderSample?interpolated:authoritative;
    return scene::translation(position+scene::Vec3{0,0,groundLift})*
           scene::rotation(scene::fromEulerRadians({0,0,yaw}));
}
inline float presentationElapsed(double now,double& previous){
    const float elapsed=previous<0?0.0f:static_cast<float>(std::clamp(now-previous,0.0,0.1));
    previous=now;
    return elapsed;
}
}
