#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <cmath>
namespace gameplay::ladder {
// Ladder-plane decomposition and Counter-Strike incidence dampening:
// ValveSoftware/source-sdk-2013 src/game/shared/gamemovement.cpp LadderMove.
inline scene::Vec3 velocity(scene::Vec3 forward,scene::Vec3 right,float f,float r,scene::Vec3 normal,float speed,bool floor){
    normal=scene::normalize(normal);const auto across=scene::normalize(scene::cross(scene::Vec3{0,0,1},normal));
    const auto up=scene::cross(normal,across);const auto wish=(forward*f+right*r)*speed;
    const float inward=scene::dot(wish,normal);auto lateral=wish-normal*inward;
    const float side=scene::dot(lateral,across),vertical=scene::dot(lateral,up);
    const auto incidence=normal*inward+across*side;
    if(scene::length(incidence)>.0001f&&scene::dot(scene::normalize(incidence),normal)<-.707f)lateral=up*vertical+across*(side*.2f);
    auto result=lateral-up*inward;if(floor&&inward>0)result=result+normal*speed;return result;
}
}
