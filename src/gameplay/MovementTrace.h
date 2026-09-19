#pragma once
#include "scene/GlbMap.h"
#include <array>
#include <limits>

namespace gameplay::movement {
struct Trace {
    float fraction{1.0f};
    scene::Vec3 end{}, normal{};
    bool startSolid{};
};

// Continuous swept AABB/triangle SAT. Unlike constrainMove this checks the
// entire path, including ceilings and walkable faces. Feet are the origin.
inline Trace sweep(const scene::glb::Map& map, scene::Vec3 start,
                   scene::Vec3 end, float radius, float height) {
    using namespace scene;
    Trace result; result.end=end;
    const Vec3 ext{radius,radius,height*0.5f};
    const Vec3 center=start+Vec3{0,0,ext.z}, delta=end-start;
    const Vec3 lo{std::min(start.x,end.x)-radius,std::min(start.y,end.y)-radius,std::min(start.z,end.z)};
    const Vec3 hi{std::max(start.x,end.x)+radius,std::max(start.y,end.y)+radius,std::max(start.z,end.z)+height};
    std::vector<std::uint32_t> candidates;
    if(map.gridWidth>0 && !map.gridCollision.empty()) {
        // Dense canopy cells contain thousands of triangles far above the
        // hull. Apply the unchanged narrow-phase AABB rejection BEFORE the
        // sort/dedup; survivors still have the exact same index/tie order.
        const auto append=[&](const auto& list){for(auto index:list){const auto& t=map.collision[index];
            if(t.maximum.x<lo.x||t.minimum.x>hi.x||t.maximum.y<lo.y||t.minimum.y>hi.y||t.maximum.z<lo.z||t.minimum.z>hi.z)continue;
            candidates.push_back(index);
        }};
        append(map.globalCollision);
        const int x0=std::max(0,map.gridCoordX(lo.x)), x1=std::min(map.gridWidth-1,map.gridCoordX(hi.x));
        const int y0=std::max(0,map.gridCoordY(lo.y)), y1=std::min(map.gridHeight-1,map.gridCoordY(hi.y));
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            const auto& cell=map.gridCollision[map.cellIndex(x,y)];
            append(cell);
        }
        std::sort(candidates.begin(),candidates.end());
        candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
    } else {
        candidates.resize(map.collision.size());
        for(std::size_t i=0;i<candidates.size();++i)candidates[i]=static_cast<std::uint32_t>(i);
    }
    // Mesh maps often use large absolute coordinates. An exact contact point
    // can round just inside the next triangle and be reported as startSolid.
    // Keep a tiny, coordinate-aware separation skin (in world units).
    const float coordinate=std::max({std::abs(center.x),std::abs(center.y),std::abs(center.z),
                                    std::abs(end.x),std::abs(end.y),std::abs(end.z),1.0f});
    const float epsilon=std::max(0.001f,coordinate*std::numeric_limits<float>::epsilon()*4.0f);
    for(auto index:candidates) {
        const auto& t=map.collision[index];
        if(t.maximum.x<lo.x||t.minimum.x>hi.x||t.maximum.y<lo.y||t.minimum.y>hi.y||t.maximum.z<lo.z||t.minimum.z>hi.z)continue;
        const Vec3 edges[]{t.b-t.a,t.c-t.b,t.a-t.c};
        std::array<Vec3,13> axes{{{1,0,0},{0,1,0},{0,0,1},cross(edges[0],edges[1])}};
        int n=4; for(auto edge:edges)for(auto axis:std::array<Vec3,3>{{{1,0,0},{0,1,0},{0,0,1}}})axes[n++]=cross(edge,axis);
        float enter=-std::numeric_limits<float>::infinity(),leave=1.0f;
        Vec3 normal{}; bool miss=false, strictInside=true;
        for(auto axis:axes) {
            const float size=length(axis); if(size<1e-7f)continue;
            axis=axis/size;
            const float reach=std::abs(axis.x)*ext.x+std::abs(axis.y)*ext.y+std::abs(axis.z)*ext.z;
            const float a=dot(t.a,axis),b=dot(t.b,axis),c=dot(t.c,axis);
            const float low=std::min({a,b,c})-reach,high=std::max({a,b,c})+reach;
            const float p=dot(center,axis),v=dot(delta,axis);
            if(p<=low+epsilon||p>=high-epsilon)strictInside=false;
            if(std::abs(v)<1e-8f) {
                // A touching face with tangential travel must not glue the hull.
                if(p<=low+epsilon||p>=high-epsilon){miss=true;break;}
                continue;
            }
            float t0=(low-p)/v,t1=(high-p)/v;
            Vec3 contact=v>0?-axis:axis;
            if(t0>t1)std::swap(t0,t1);
            if(t0>enter){enter=t0;normal=contact;}
            leave=std::min(leave,t1);
            if(enter>leave){miss=true;break;}
        }
        if(miss||leave<0||enter>result.fraction)continue;
        if(strictInside){result.startSolid=true;result.fraction=0;result.end=start;return result;}
        if(enter<-epsilon||dot(delta,normal)>=-epsilon)continue;
        result.fraction=std::max(0.0f,enter-epsilon/-dot(delta,normal));
        result.normal=normal;
    }
    result.end=start+delta*result.fraction;
    return result;
}

inline scene::Vec3 clip(scene::Vec3 v,scene::Vec3 normal) {
    const float into=scene::dot(v,normal);
    return into<0?v-normal*into:v;
}
} // namespace gameplay::movement
