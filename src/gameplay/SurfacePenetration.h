#pragma once
#include "scene/GlbMap.h"
#include <algorithm>
#include <limits>

namespace gameplay::shots {
// Count distinct ray-crossed faces, not triangles or inferred solid volumes.
// A closed wall normally consumes two faces. Only runs once per fired shot.
inline std::optional<scene::glb::Map::RaycastHit> stoppingSurface(
    const scene::glb::Map& map,scene::Vec3 origin,scene::Vec3 direction,float range,int budget){
    budget=std::clamp(budget,0,16);
    if(!budget)return map.raycastShot(origin,direction,range);
    direction=scene::normalize(direction);auto cursor=origin;float travelled=0;
    for(int face=0;face<=budget;++face){
        auto hit=map.raycastShot(cursor,direction,range-travelled);
        if(!hit)return {};
        if(face==budget){hit->distance=scene::length(hit->position-origin);return hit;}
        const float coordinate=std::max({std::abs(hit->position.x),std::abs(hit->position.y),std::abs(hit->position.z),1.f});
        const float skin=std::max(.01f,coordinate*std::numeric_limits<float>::epsilon()*4);
        travelled=scene::length(hit->position-origin)+skin;
        if(travelled>=range)return {};
        cursor=origin+direction*travelled;
    }
    return {};
}
}
