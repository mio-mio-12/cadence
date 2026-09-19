#pragma once
#include "gameplay/MantleTrajectory.h"
#include "scene/TestCourse.h"

namespace gameplay::mantle {
inline scene::Vec3 closestOnTriangle(scene::Vec3 p,scene::Vec3 a,scene::Vec3 b,scene::Vec3 c){
    const auto ab=b-a,ac=c-a,ap=p-a;const float d1=scene::dot(ab,ap),d2=scene::dot(ac,ap);if(d1<=0&&d2<=0)return a;
    const auto bp=p-b;const float d3=scene::dot(ab,bp),d4=scene::dot(ac,bp);if(d3>=0&&d4<=d3)return b;
    const float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
    const auto cp=p-c;const float d5=scene::dot(ab,cp),d6=scene::dot(ac,cp);if(d6>=0&&d5<=d6)return c;
    const float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
    const float va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0)return b+(c-b)*((d4-d3)/(d4-d3+d5-d6));
    const float divisor=va+vb+vc;if(std::abs(divisor)<1e-12f)return a;
    return a+ab*(vb/divisor)+ac*(vc/divisor);
}
// Continuous conservative advancement for the same three rounded samples
// used by constrainMove. Unlike that discrete solver, include floor/ceiling
// faces too. Source-v2 callers retain their native swept-box hull.
inline movement::Trace sweepRounded(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 end,float radius,float height){
    using namespace scene;movement::Trace result;result.end=end;const auto delta=end-start;
    const Vec3 lo{std::min(start.x,end.x)-radius,std::min(start.y,end.y)-radius,std::min(start.z,end.z)},hi{std::max(start.x,end.x)+radius,std::max(start.y,end.y)+radius,std::max(start.z,end.z)+height};
    std::vector<std::uint32_t> candidates=map.globalCollision;
    if(map.gridWidth>0&&!map.gridCollision.empty()){
        for(int y=std::max(0,map.gridCoordY(lo.y));y<=std::min(map.gridHeight-1,map.gridCoordY(hi.y));++y)for(int x=std::max(0,map.gridCoordX(lo.x));x<=std::min(map.gridWidth-1,map.gridCoordX(hi.x));++x){const auto& cell=map.gridCollision[map.cellIndex(x,y)];candidates.insert(candidates.end(),cell.begin(),cell.end());}
        std::sort(candidates.begin(),candidates.end());candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
    }else{candidates.resize(map.collision.size());for(std::size_t i=0;i<candidates.size();++i)candidates[i]=static_cast<std::uint32_t>(i);}
    const float epsilon=std::max(.025f,std::max({std::abs(start.x),std::abs(start.y),std::abs(end.x),std::abs(end.y)})*std::numeric_limits<float>::epsilon()*8);
    for(auto index:candidates){const auto& tri=map.collision[index];if(tri.maximum.x<lo.x||tri.minimum.x>hi.x||tri.maximum.y<lo.y||tri.minimum.y>hi.y||tri.maximum.z<lo.z||tri.minimum.z>hi.z)continue;
        for(float z:{radius,std::max(radius,height*.5f),std::max(radius,height-radius)}){
            if(tri.maximum.z<std::min(start.z,end.z)+z-radius||tri.minimum.z>std::max(start.z,end.z)+z+radius)continue;
            float t=0;for(int iteration=0;iteration<24;++iteration){const auto center=start+Vec3{0,0,z}+delta*t;const auto separation=center-closestOnTriangle(center,tri.a,tri.b,tri.c);const float distance=length(separation);const auto normal=distance>1e-6f?separation/distance:tri.normal;
                if(t==0&&distance<radius-epsilon){
                    // The ordinary stepper permits contact with ankle-high
                    // curbs. Permit an upward/tangential escape from that
                    // existing contact, never penetration of a taller wall.
                    if(delta.z>=0&&tri.maximum.z<=start.z+scene::course::kStepHeight)break;
                    result.startSolid=true;result.fraction=0;result.end=start;return result;
                }
                const float closing=-dot(delta,normal);if(closing<=epsilon)break;
                const float gap=distance-radius;if(gap<=epsilon||iteration==23){if(t<result.fraction){result.fraction=t;result.normal=normal;if(t==0){result.end=start;return result;}}break;}
                t+=gap/closing;if(t>result.fraction)break;
            }
        }
    }
    result.end=start+delta*result.fraction;return result;
}
// Conservative fallback for lips/sloped lids: lift above the entire hull
// footprint before crossing the face. Two straight sweeps validate this path.
inline std::optional<Trajectory> findClimb(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 forward,
    float radius,float height,float step,float maximum,float reach,float minimum,float rate=1.f,bool boxHull=false){
    forward.z=0;if(scene::length(forward)<.5f)return {};forward=scene::normalize(forward);
    const auto sweep=[&](scene::Vec3 a,scene::Vec3 b){return boxHull?movement::sweep(map,a,b,radius,height):sweepRounded(map,a,b,radius,height);};
    const auto obstruction=sweep(start,start+forward*(radius+reach));
    if(obstruction.startSolid||obstruction.fraction>=1)return {};
    // groundHeight's historical 45-unit ceiling tolerance is accounted for
    // here rather than accidentally querying above the allowed mantle height.
    const float ceiling=start.z+maximum-45.f;
    for(float distance=radius+8;distance<=radius+reach+.01f;distance+=8){
        auto target=start+forward*distance;
        const float center=map.groundHeight(target.x,target.y,ceiling,-1e9f);
        if(center<start.z+minimum||center>start.z+maximum)continue;
        float top=center;bool supported=true;
        for(int y=-1;y<=1&&supported;++y)for(int x=-1;x<=1;++x){
            const float z=map.groundHeight(target.x+x*radius,target.y+y*radius,ceiling,-1e9f);
            if(z>center+step){supported=false;break;}top=std::max(top,z);
        }
        if(!supported||top>start.z+maximum)continue;
        target.z=top+.04f;
        for(float clearance=0;clearance<=step+8;clearance+=8){
            const float apex=target.z+clearance;if(apex>start.z+maximum+.05f)break;
            const scene::Vec3 lifted{start.x,start.y,apex},over{target.x,target.y,apex};
            const auto up=sweep(start,lifted);if(up.startSolid||up.fraction<.9999f)continue;
            const auto across=sweep(lifted,over);if(across.startSolid||across.fraction<.9999f)continue;
            const auto down=sweep(over,target);if(down.startSolid||down.fraction<.9999f)continue;
            auto result=plan(start,target,{},forward,rate);result.liftFirst=true;result.clearanceZ=apex;return result;
        }
    }
    return {};
}
// Replay a navigation link to its exact supported endpoint. Reacquiring a
// nearby ledge at execution time can land beside, rather than on, that link.
inline std::optional<Trajectory> findClimbTo(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 target,
    float radius,float height,float step,float maximum,float reach,float minimum){
    auto direction=target-start;direction.z=0;
    const float distance=scene::length(direction),rise=target.z-start.z;
    if(distance<1.f||distance>radius+reach||rise<minimum||rise>maximum)return {};
    direction=direction/distance;target.z+=.04f;
    for(float clearance=0;clearance<=step+8;clearance+=8){
        const float apex=target.z+clearance;if(apex>start.z+maximum+.05f)break;
        const scene::Vec3 lifted{start.x,start.y,apex},over{target.x,target.y,apex};
        const auto up=sweepRounded(map,start,lifted,radius,height);if(up.startSolid||up.fraction<.9999f)continue;
        const auto across=sweepRounded(map,lifted,over,radius,height);if(across.startSolid||across.fraction<.9999f)continue;
        const auto down=sweepRounded(map,over,target,radius,height);if(down.startSolid||down.fraction<.9999f)continue;
        auto result=plan(start,target,{},direction,1.f);result.liftFirst=true;result.clearanceZ=apex;return result;
    }
    return {};
}
}
