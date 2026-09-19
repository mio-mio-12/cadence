#pragma once
#include "scene/GlbMap.h"
#include <algorithm>
#include <cmath>

namespace scene::glb {
// Import-time geometry heuristic, not authored spawn metadata. Never changes
// collision approval. A result may be used as a preview camera location only.
inline std::optional<Vec3> findFallbackSpawn(const Map& map){
    constexpr float radius=15.f*2.54f,height=72.f*2.54f,step=18.f*2.54f,absent=-1e20f;
    std::vector<Vec3> candidates;
    std::vector<float> xs,ys,zs;
    for(const auto& t:map.collision)if(t.walkable){
        const auto p=(t.a+t.b+t.c)/3.f;
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))continue;
        candidates.push_back(p);xs.push_back(p.x);ys.push_back(p.y);zs.push_back(p.z);
    }
    if(candidates.empty())return {};
    // Robust to isolated sky/background geometry and maps offset from (0,0).
    std::sort(xs.begin(),xs.end());std::sort(ys.begin(),ys.end());std::sort(zs.begin(),zs.end());
    const auto median=[](const std::vector<float>& values){return values[values.size()/2]*.5f+values[(values.size()-1)/2]*.5f;};
    const Vec3 center{median(xs),median(ys),zs[zs.size()/4]};
    // Triangle centroids alone miss safe space at a shared diagonal (a small
    // two-triangle floor is the simplest example). Test the central point too.
    candidates.push_back(center);
    const auto score=[&](Vec3 p){const auto d=p-center;return double(d.x)*d.x+double(d.y)*d.y+double(d.z)*d.z;};
    std::stable_sort(candidates.begin(),candidates.end(),[&](Vec3 a,Vec3 b){return score(a)<score(b);});
    for(auto p:candidates){
        p.z=map.groundHeight(p.x,p.y,p.z+1,absent);if(!std::isfinite(p.z)||p.z<absent*.5f)continue;
        bool valid=true;
        for(int i=0;i<9&&valid;++i){
            const float angle=2*kPi*i/8;
            const auto foot=p+(i==8?Vec3{}:Vec3{radius*std::cos(angle),radius*std::sin(angle),0});
            const auto floor=map.groundHeight(foot.x,foot.y,p.z+step,absent);
            if(!std::isfinite(floor)||floor<absent*.5f||std::abs(floor-p.z)>step){valid=false;break;}
            const auto bottom=foot+Vec3{0,0,std::max(2.f,floor-p.z+2.f)};
            if(!map.lineOfSight(bottom,foot+Vec3{0,0,height})||!map.lineOfSight(p+Vec3{0,0,height*.5f},foot+Vec3{0,0,height*.5f}))valid=false;
        }
        if(!valid)continue;
        const auto moved=map.constrainMove(p,p+Vec3{.1f,0,0},radius,height,step/1.25f);
        if(length(Vec3{moved.x-p.x,moved.y-p.y,0})>1.f)continue;
        return p+Vec3{0,0,5};
    }
    return {};
}
}
