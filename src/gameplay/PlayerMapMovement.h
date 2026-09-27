#pragma once
#include "gameplay/MantleAcquisition.h"

namespace gameplay::movement {
struct MoveResult {scene::Vec3 position{},velocity{},normal{};bool blocked{},startSolid{};};
// Swept rounded hull, with bounded plane clipping instead of positional
// depenetration. Floors and ceilings participate even on single-sided meshes.
inline MoveResult slidePlayer(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 velocity,float dt,float radius,float height){
    MoveResult out;out.position=start;out.velocity=velocity;
    std::array<scene::Vec3,5> planes{};int count=0;
    for(int bump=0;bump<5&&dt>1e-6f;++bump){
        const auto hit=mantle::sweepRounded(map,out.position,out.position+out.velocity*dt,radius,height);
        if(hit.startSolid){out.startSolid=true;out.blocked=true;out.velocity={};break;}
        out.position=hit.end;if(hit.fraction>=1)break;
        out.blocked=true;out.normal=hit.normal;dt*=1-hit.fraction;
        planes[count++]=hit.normal;
        auto clipped=clip(out.velocity,hit.normal);
        for(int i=0;i<count;++i)if(scene::dot(clipped,planes[i])<-.001f){
            const auto crease=scene::cross(planes[i],hit.normal);const float length=scene::length(crease);
            clipped=length>1e-5f?crease*(scene::dot(out.velocity,crease)/(length*length)):scene::Vec3{};
            for(int j=0;j<count;++j)if(scene::dot(clipped,planes[j])<-.001f){clipped={};break;}
        }
        out.velocity=clipped;
        if(scene::length(clipped)<.001f)break;
    }
    return out;
}
inline MoveResult movePlayer(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 velocity,float dt,float radius,float height,float stepHeight,bool grounded){
    auto direct=slidePlayer(map,start,velocity,dt,radius,height);
    // A jump/boost cannot turn into an automatic step through its ceiling.
    if(!grounded||velocity.z>0||!direct.blocked||direct.startSolid||stepHeight<=0)return direct;
    const auto up=mantle::sweepRounded(map,start,start+scene::Vec3{0,0,stepHeight},radius,height);
    if(up.startSolid||up.fraction<.9999f)return direct;
    auto step=slidePlayer(map,up.end,{velocity.x,velocity.y,0},dt,radius,height);
    if(step.startSolid)return direct;
    const auto down=mantle::sweepRounded(map,step.position,step.position-scene::Vec3{0,0,stepHeight+.1f},radius,height);
    if(down.startSolid||down.fraction>=1||down.normal.z<.7f||down.end.z>start.z+stepHeight+.01f)return direct;
    const auto distance=[&](scene::Vec3 p){return (p.x-start.x)*(p.x-start.x)+(p.y-start.y)*(p.y-start.y);};
    if(distance(step.position)<=distance(direct.position)+.01f)return direct;
    step.position=down.end;step.velocity.z=0;return step;
}
inline std::optional<float> playerSupport(const scene::glb::Map& map,scene::Vec3 position,float radius,float height,float distance){
    const auto down=mantle::sweepRounded(map,position,position-scene::Vec3{0,0,std::max(.1f,distance)},radius,height);
    if(down.startSolid||down.fraction>=1||down.normal.z<.7f)return {};
    return down.end.z;
}
inline bool shouldCheckSupport(bool grounded,float incomingZ,float resolvedZ){
    // Uphill collision clipping can create upward velocity without a jump.
    // Ground takeoff clears grounded before movement; never snap a jump down.
    return resolvedZ<=0 || (grounded&&incomingZ<=0);
}
}
