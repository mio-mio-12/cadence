#pragma once
#include "scene/CastScene.h"
#include <cmath>
#include <istream>
#include <ostream>
namespace gameplay::collision {
// Zero retains the model-derived default. Preview is intentionally not saved.
inline bool validPlayerHeight(float value){return std::isfinite(value)&&value>=0.f&&value<=1000000.f;}
inline float playerStandingHeight(float automatic,float overrideCm){return validPlayerHeight(overrideCm)&&overrideCm>0?overrideCm:automatic;}
inline bool readPlayerHeight(std::istream& in,float& value){float parsed{};if(!(in>>parsed)||!validPlayerHeight(parsed))return false;value=parsed;return true;}
struct FootprintProbe {scene::Vec2 halfExtents;scene::Vec3 offset;};
inline FootprintProbe scaleFootprintProbe(scene::Vec2 full,float nominalRadius,float radius,scene::Vec3 offset){
    const float scale=radius/nominalRadius;
    return {{full.x*scale,full.y*scale},{offset.x*full.x/nominalRadius,offset.y*full.y/nominalRadius,0}};
}
inline scene::Vec3 playerHeightPreviewCenter(scene::Vec3 feet,float yaw,float ground){return {feet.x+std::cos(yaw)*200.f,feet.y+std::sin(yaw)*200.f,ground};}
}
