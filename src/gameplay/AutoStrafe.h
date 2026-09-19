#pragma once
#include "gameplay/SourceMovement.h"

namespace gameplay::source_v2 {
// In-engine assist: adjusts air wish direction only, never camera or velocity.
// Uses the same 30-unit air projection cap as accelerate().
inline Vec3 autoStrafeWish(Vec3 velocity,Vec3 intent,float yaw,float playerSpeed,float& turnSign,float airAcceleration=kCssAirAccelerate,float viewYawDelta=0){
    const float speed=std::hypot(velocity.x,velocity.y);
    if(speed<units(1))return scene::length(intent)>0.01f?intent:wishVelocity(1,0,yaw,playerSpeed);
    const float heading=std::atan2(velocity.y,velocity.x);
    const float desired=scene::length(intent)>0.01f?std::atan2(intent.y,intent.x):heading;
    const float delta=std::remainder(desired-heading,2*scene::kPi);
    if(std::isfinite(viewYawDelta)&&std::abs(viewYawDelta)>.00001f)turnSign=viewYawDelta>0?1.f:-1.f;
    else if(std::abs(delta)>0.015f)turnSign=delta>0?1.0f:-1.0f;
    else turnSign=-turnSign;
    const float gain=std::max(0.0f,airAcceleration)*playerSpeed*static_cast<float>(tickSeconds);
    const float optimal=std::acos(std::clamp((units(30)-gain)/speed,0.0f,1.0f));
    const float angle=heading+turnSign*optimal;
    return Vec3{std::cos(angle),std::sin(angle),0}*playerSpeed;
}
}
