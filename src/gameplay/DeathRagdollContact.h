#pragma once
#include "scene/GlbMap.h"
#include "scene/TestCourse.h"
namespace gameplay::ragdoll {
inline scene::Vec3 contact(const scene::glb::Map* loaded,scene::Vec3 from,scene::Vec3 to,float radius){
    if(!loaded){to.z=std::max(to.z,scene::course::groundHeight(to.x,to.y)+radius);return to;}
    const auto& map=loaded->shotGeometry?*loaded->shotGeometry:*loaded;
    const auto travel=to-from;const float distance=scene::length(travel);
    if(distance>.0001f)if(auto hit=map.raycastSurface(from,travel/distance,distance)){
        auto normal=hit->normal;if(scene::dot(normal,travel)>0)normal=-normal;to=hit->position+normal*radius;
    }
    for(const auto direction:std::array<scene::Vec3,6>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}})
        if(auto hit=map.raycastSurface(to,direction,radius)){auto normal=hit->normal;if(scene::dot(normal,direction)>0)normal=-normal;to+=normal*(radius-hit->distance);}
    return to;
}
}
