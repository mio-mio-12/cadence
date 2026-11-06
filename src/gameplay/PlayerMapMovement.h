#pragma once
#include "gameplay/MantleAcquisition.h"
#include <type_traits>

namespace gameplay::movement {
struct MoveResult {scene::Vec3 position{},velocity{},normal{};bool blocked{},startSolid{};};
inline scene::Vec3 contactSafeEnd(scene::Vec3 start,scene::Vec3 delta,const std::array<scene::Vec3,5>& planes,int count){
    auto end=start+delta;
    // At large coordinates, adding a tangent displacement can round back into
    // a plane we just clipped. Correct endpoint rounding, not collision size
    // or the sweep's tolerance for genuine slow inward movement.
    for(int pass=0;pass<count;++pass)for(int i=0;i<count;++i){
        const auto n=planes[i];
        // Keep floor/slope support integration untouched. This correction is
        // solely for near-vertical side contacts, not grounded travel.
        if(std::abs(n.z)>.01f)continue;
        const float intended=scene::dot(delta,n);
        const float tolerance=8.f*std::numeric_limits<float>::epsilon()*scene::length(delta);
        if(intended < -tolerance || scene::dot(end-start,n)>=0)continue;
        float* component=&end.x;float normal=n.x;
        if(std::abs(n.y)>std::abs(normal)){component=&end.y;normal=n.y;}
        if(std::abs(n.z)>std::abs(normal)){component=&end.z;normal=n.z;}
        if(std::abs(normal)<1e-6f)continue;
        *component-=scene::dot(end-start,n)/normal;
        *component=std::nextafter(*component,normal>0?std::numeric_limits<float>::infinity():-std::numeric_limits<float>::infinity());
    }
    return end;
}
// Swept rounded hull, with bounded plane clipping instead of positional
// depenetration. Floors and ceilings participate even on single-sided meshes.
template<class Hull> inline MoveResult slidePlayer(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 velocity,float dt,Hull radius,float height){
    MoveResult out;out.position=start;out.velocity=velocity;
    std::array<scene::Vec3,5> planes{};int count=0;
    for(int bump=0;bump<5&&dt>1e-6f;++bump){
        auto end=out.position+out.velocity*dt;
        if constexpr(std::is_same_v<Hull,scene::Vec2>)end=contactSafeEnd(out.position,out.velocity*dt,planes,count);
        const auto hit=mantle::sweepRounded(map,out.position,end,radius,height);
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
template<class Hull> inline MoveResult movePlayer(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 velocity,float dt,Hull radius,float height,float stepHeight,bool grounded){
    auto direct=slidePlayer(map,start,velocity,dt,radius,height);
    // A jump/boost cannot turn into an automatic step through its ceiling.
    if(!grounded||velocity.z>0||!direct.blocked||direct.startSolid||stepHeight<=0)return direct;
    const auto up=mantle::sweepRounded(map,start,start+scene::Vec3{0,0,stepHeight},radius,height);
    // The ceiling may permit a small step even when the full step allowance
    // does not fit. Use the actually swept rise; never move through the cap.
    const float rise=up.end.z-start.z;
    if(up.startSolid||rise<=.01f)return direct;
    auto step=slidePlayer(map,up.end,{velocity.x,velocity.y,0},dt,radius,height);
    if(step.startSolid)return direct;
    const auto down=mantle::sweepRounded(map,step.position,step.position-scene::Vec3{0,0,rise+.1f},radius,height);
    if(down.startSolid||down.fraction>=1||down.normal.z<.7f||down.end.z>start.z+stepHeight+.01f)return direct;
    const auto distance=[&](scene::Vec3 p){return (p.x-start.x)*(p.x-start.x)+(p.y-start.y)*(p.y-start.y);};
    if(distance(step.position)<=distance(direct.position)+.01f)return direct;
    step.position=down.end;step.velocity.z=0;return step;
}
template<class Hull> inline std::optional<float> playerSupport(const scene::glb::Map& map,scene::Vec3 position,Hull radius,float height,float distance){
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
