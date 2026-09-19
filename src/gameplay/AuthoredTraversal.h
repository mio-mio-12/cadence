#pragma once
#include "gameplay/MantleAcquisition.h"

namespace gameplay::authored {
inline std::optional<mantle::Trajectory> climb(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 looking,float radius,float height,float step,float maximum,float reach,float minimum,float rate,bool boxHull){
    if(!map.gameplay.enabled||!map.gameplay.mantles)return {};
    looking.z=0;if(scene::length(looking)<.5f)return {};looking=scene::normalize(looking);
    for(const auto& link:map.gameplay.climbs){
        if(scene::length(link.start-start)>radius+reach)continue;
        auto dir=link.end-start;dir.z=0;if(scene::length(dir)<1||scene::dot(scene::normalize(dir),looking)<.7071f)continue;
        auto target=link.end;const float ground=map.navigationGroundHeight(target.x,target.y,target.z+step-45.f,-1e9f,radius*.7f);
        if(std::abs(ground-target.z)>step)continue;target.z=ground;
        if(auto path=mantle::findClimbTo(map,start,target,radius,height,step,maximum,reach,minimum)){
            path->duration/=std::clamp(rate,.25f,4.f);path->playbackSpeed=std::clamp(rate,.25f,4.f);
            const auto sweep=[&](scene::Vec3 a,scene::Vec3 b){return boxHull?movement::sweep(map,a,b,radius,height):mantle::sweepRounded(map,a,b,radius,height);};
            if(mantle::clear(*path,sweep))return path;
        }
    }
    for(auto dir:map.gameplay.mantleDirections(start,looking,radius,height,reach))
        if(auto path=mantle::findClimb(map,start,dir,radius,height,step,maximum,reach,minimum,rate,boxHull))return path;
    return {};
}
struct LadderState {
    bool active{},exiting{};
    std::size_t volume{};
    float cooldown{},elapsed{},exitProbeDelay{};
    scene::Vec3 normal{};
    mantle::Trajectory exitPath;
};
// Returns true only while this controller owns movement. Weapon/aim processing
// continues normally; no visual configuration is added to recordings.
inline bool ladder(LadderState& state,const scene::glb::Map& map,scene::Vec3& position,scene::Vec3& velocity,bool& grounded,
                   scene::Vec3 facing,float input,bool jumpPressed,float dt,float radius,float height){
    state.cooldown=std::max(0.f,state.cooldown-dt);
    state.exitProbeDelay=std::max(0.f,state.exitProbeDelay-dt);
    if(!map.gameplay.enabled||!map.gameplay.ladders){state.active=state.exiting=false;return false;}
    const auto sweep=[&](scene::Vec3 a,scene::Vec3 b){return mantle::sweepRounded(map,a,b,radius,height);};
    if(!state.active&&!state.exiting&&state.cooldown<=0&&std::abs(input)>.1f){
        if(const auto* v=map.gameplay.ladder(position,radius,height)){
            auto normal=v->facing;if(scene::length(normal)<.5f)return false;
            if(scene::dot(position-v->center,normal)<0)normal=-normal;
            if(scene::dot(facing,-normal)<.25f||input<0&&v->forbidDown)return false;
            state.volume=static_cast<std::size_t>(v-map.gameplay.volumes.data());state.normal=normal;state.active=true;velocity={};
        }
    }
    if(!state.active&&!state.exiting)return false;
    if(state.volume>=map.gameplay.volumes.size()){state={};return false;}
    if(jumpPressed){state.active=state.exiting=false;state.cooldown=.35f;velocity=state.normal*250.f+scene::Vec3{0,0,240};grounded=false;return true;}
    const auto& volume=map.gameplay.volumes[state.volume];
    if(!state.exiting&&!volume.overlaps(position,radius+20,height)){state.active=false;state.cooldown=.25f;return false;}
    grounded=false;velocity={};
    float remaining=std::clamp(dt,0.f,.1f);
    while(remaining>0){const float tick=std::min(remaining,.008f);remaining-=tick;
        if(state.exiting){
            state.elapsed=std::min(state.elapsed+tick,state.exitPath.duration);
            const auto sample=state.exitPath.sample(state.elapsed);const auto hit=sweep(position,sample.position);position=hit.end;
            if(hit.startSolid||hit.fraction<.9999f||state.elapsed>=state.exitPath.duration){state.active=state.exiting=false;state.cooldown=.35f;grounded=!hit.startSolid&&hit.fraction>=.9999f;return true;}
            continue;
        }
        // Trigger boxes can include the handrail above the actual landing.
        // Search earlier, but only leave through a fully swept, supported path.
        if(input>.1f&&position.z+height*1.25f>=volume.maximum.z&&state.exitProbeDelay<=0){
            state.exitProbeDelay=.08f;
            for(float distance:{radius*2+12,radius*3+12}){
                auto target=position-state.normal*distance;
                target.z=map.navigationGroundHeight(target.x,target.y,volume.maximum.z+40-45.f,-1e9f,radius*.7f);
                const float settle=scene::course::kStepHeight+8.f;
                if(target.z<position.z-settle||target.z>position.z+height*.8f)continue;
                if(auto path=mantle::findClimbTo(map,position,target,radius,height,scene::course::kStepHeight,height*.8f,radius*4,-settle)){
                    if(mantle::clear(*path,sweep)){state.exitPath=*path;state.elapsed=0;state.exiting=true;break;}
                }
                for(float clearance=0;clearance<=settle;clearance+=8){
                    const float apex=std::max(position.z,target.z)+clearance;
                    const scene::Vec3 high{position.x,position.y,apex},over{target.x,target.y,apex};
                    const auto up=sweep(position,high),across=sweep(high,over),down=sweep(over,target+scene::Vec3{0,0,.04f});
                    if(up.startSolid||across.startSolid||down.startSolid||up.fraction<.9999f||across.fraction<.9999f||down.fraction<.9999f)continue;
                    auto path=mantle::plan(position,target+scene::Vec3{0,0,.04f},{},-state.normal);path.liftFirst=true;path.clearanceZ=apex;
                    if(mantle::clear(path,sweep)){state.exitPath=path;state.elapsed=0;state.exiting=true;break;}
                }
                if(state.exiting)break;
            }
            if(state.exiting)continue;
        }
        auto target=position+scene::Vec3{0,0,input*160.f*tick};
        target.z=std::min(target.z,volume.maximum.z+8);
        const auto hit=sweep(position,target);position=hit.end;
        if(input<-.1f){const float ground=map.groundHeight(position.x,position.y,position.z-43.f,-1e9f);
            if(ground>=position.z-3&&ground<=position.z+2){position.z=ground;grounded=true;state.active=false;state.cooldown=.35f;return true;}
        }
    }velocity={0,0,input*160.f};return true;
}
}
