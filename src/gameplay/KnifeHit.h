#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <optional>

namespace gameplay {
inline bool inKnifeCone(scene::Vec3 origin,scene::Vec3 forward,scene::Vec3 point,float radius,float fullAngle){
    const auto offset=point-origin;const float distance=scene::length(offset),directionLength=scene::length(forward);
    if(!std::isfinite(distance)||!std::isfinite(directionLength)||!std::isfinite(radius)||!std::isfinite(fullAngle)||radius<=0||distance<.001f||distance>radius||directionLength<.001f)return false;
    const float cosine=scene::dot(offset,forward)/(distance*directionLength);
    return cosine>=std::cos(std::clamp(fullAngle,1.f,80.f)*.5f*scene::kPi/180.f);
}
template<class Visible>
std::optional<scene::Vec3> knifeTarget(scene::Vec3 origin,scene::Vec3 forward,scene::Vec3 feet,float height,float radius,float angle,Visible&& visible){
    if(!std::isfinite(height)||height<=0)return {};
    const float low=feet.z+height*.35f,high=feet.z+height;
    const float xy=forward.x*forward.x+forward.y*forward.y;
    const float along=xy>.00001f?((feet.x-origin.x)*forward.x+(feet.y-origin.y)*forward.y)/xy:0;
    const scene::Vec3 points[]={{feet.x,feet.y,std::clamp(origin.z+forward.z*along,low,high)},
        {feet.x,feet.y,std::clamp(origin.z,low,high)},{feet.x,feet.y,feet.z+height*.58f},{feet.x,feet.y,high}};
    for(const auto& point:points)if(inKnifeCone(origin,forward,point,radius,angle)&&visible(point))return point;
    return {};
}
}
