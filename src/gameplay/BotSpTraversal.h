#pragma once
#include "gameplay/BotActor.h"
#include "gameplay/MantleAcquisition.h"
#include "gameplay/BotMapPhysics.h"

namespace gameplay::bot {
struct SpLedgeExit {scene::Vec3 travel{},landing{};float duration{};};
// Validate with the same fixed-step controller used at runtime. No destination
// snapping: the committed input only maintains travel while aiming independently.
template<class Map> std::optional<SpLedgeExit> findSpLedgeExit(const Map& map,const Actor& a,scene::Vec3 goal){
    if(!a.alive||!a.grounded||a.mantling||a.spMovingScenario||a.spScenarioWeight>.01f||a.reloadTime>0||a.equipmentPoseTime>0||a.spJumpCooldown>0||a.position.z-goal.z<=scene::course::kStepHeight)return {};
    auto direction=goal-a.position;direction.z=0;
    const float distance=scene::length(direction);
    if(!std::isfinite(distance)||distance<80.f)return {};
    direction=direction/distance;
    const auto edge=a.position+direction*(scene::course::kPlayerRadius+50.f);
    const float ground=map.navigationGroundHeight(edge.x,edge.y,a.position.z+.1f-45.f,-1e20f,0.f);
    if(!std::isfinite(ground)||ground<-1e19f||a.position.z-ground<=scene::course::kStepHeight||a.position.z-ground>iw::worldUnits(128))return {};
    for(float speed:{iw::kRunSpeed*.65f,iw::kRunSpeed}){
        Actor test;test.position=a.position;test.velocity=a.velocity;test.yaw=std::atan2(direction.y,direction.x);
        test.input.jump=true;test.input.forward=1;test.spLedgeTime=3;test.spLedgeTravel=direction*speed;
        bool departed=false;
        for(int step=0;step<75;++step){
            stepOnMap(test,.032f,1.f,map);test.input.jump=false;
            departed|=!test.grounded;
            const auto moved=test.position-a.position;
            const float forward=scene::dot(moved,direction);
            const auto lateral=scene::Vec3{moved.x,moved.y,0}-direction*forward;
            if(!std::isfinite(test.position.z)||test.position.z<a.position.z-iw::worldUnits(128)-5||scene::length(lateral)>scene::course::kPlayerRadius)break;
            if(departed&&test.grounded){
                if(a.position.z-test.position.z<=scene::course::kStepHeight||forward<70||horizontalDistance(test.position,goal)>=distance-50)break;
                bool supported=true;
                for(int i=0;i<8;++i){const float angle=i*scene::kPi/4;const auto p=test.position+scene::Vec3{std::cos(angle),std::sin(angle),0}*(scene::course::kPlayerRadius*.8f);
                    const float floor=map.navigationGroundHeight(p.x,p.y,test.position.z+2-45.f,-1e20f,0.f);
                    if(!std::isfinite(floor)||std::abs(floor-test.position.z)>4){supported=false;break;}}
                if(supported)return SpLedgeExit{direction*speed,test.position,(step+1)*.032f};
                break;
            }
        }
    }
    return {};
}
inline bool spFinitePosition(scene::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline scene::Vec3 spMantlePosition(scene::Vec3 start,scene::Vec3 end,float fraction,float clearance=-std::numeric_limits<float>::max()){
    if(!spFinitePosition(start))return {};
    if(!spFinitePosition(end)||!std::isfinite(fraction))return start;
    gameplay::mantle::Trajectory path;path.start=start;path.target=end;path.duration=1;path.liftFirst=true;path.clearanceZ=clearance;return path.sample(fraction).position;
}
template<class Map> std::optional<scene::Vec3> obviousSpMantle(const Map& map,const Actor& a){
    if(!a.alive||!a.grounded||a.mantling||a.spMovingScenario||!a.spWantsMove||a.reloadTime>0||a.input.fire||a.firePoseTime>0||!spFinitePosition(a.position)||!spFinitePosition(a.spMoveGoal)||!std::isfinite(a.yaw))return {};
    auto direction=a.spMoveGoal-a.position;direction.z=0;direction=scene::normalize(direction);
    const scene::Vec3 facing{std::cos(a.yaw),std::sin(a.yaw),0};
    if(scene::dot(direction,facing)<.85f)return {};
    // A local, forward, chest-height step, never an off-route climb or chain.
    const auto candidate=map.mantleTarget(a.position,direction,scene::course::kPlayerRadius,iw::worldUnits(72),scene::course::kStepHeight,iw::worldUnits(57),iw::worldUnits(28),iw::kJumpHeight+8.f);
    if(!candidate||!spFinitePosition(*candidate))return {};
    // Broad footprint ground queries may find a lip while the actor's centre
    // is still outside it. Require actual support under the landing footprint;
    // only advance a short distance along the already intended route.
    std::optional<scene::Vec3> target;
    const float radius=scene::course::kPlayerRadius;
    for(float advance=0;advance<=2*radius+8&&!target;advance+=8){
        const auto landing=*candidate+direction*advance;
        if(horizontalDistance(a.position,landing)>145.f||horizontalDistance(landing,a.spMoveGoal)>=horizontalDistance(a.position,a.spMoveGoal)-15.f)continue;
        bool supported=true;
        for(int i=-1;i<8;++i){
            const float angle=2*scene::kPi*static_cast<float>(i)/8;
            const auto p=landing+(i<0?scene::Vec3{}:scene::Vec3{std::cos(angle)*radius,std::sin(angle)*radius,0});
            const float floor=map.navigationGroundHeight(p.x,p.y,landing.z+2,-1e9f,0.f);
            if(!std::isfinite(floor)||std::abs(floor-landing.z)>3.f){supported=false;break;}
        }
        if(supported)target=landing;
    }
    if(!target)return {};
    auto previous=a.position;
    for(int i=1;i<=24;++i){
        const auto next=spMantlePosition(a.position,*target,static_cast<float>(i)/24);
        const auto actual=map.constrainMove(previous,next,scene::course::kPlayerRadius,iw::worldUnits(72),0.f);
        if(!spFinitePosition(actual)||scene::length(actual-next)>1.f)return {};
        previous=next;
    }
    return target;
}
template<class Map> bool clearSpJumpArc(const Map& map,const Actor& a){
    const float speed=horizontalSpeed(a);
    if(!a.alive||!a.grounded||a.mantling||a.spMovingScenario||!std::isfinite(speed)||speed<80.f||!a.spWantsMove||!std::isfinite(a.spMoveThrottle)||a.spMoveThrottle<.2f||a.spJumpCooldown>0||!spFinitePosition(a.position)||!spFinitePosition(a.velocity))return false;
    scene::Vec3 velocity=a.velocity;velocity.z=std::sqrt(2*iw::kGravity*iw::kJumpHeight);
    const float flight=2*velocity.z/iw::kGravity;
    auto previous=a.position;
    // Require a clear launch/flight and a nearby same-level landing; no
    // speculative shortcuts over gaps, railings or multi-level spaces.
    for(int i=1;i<=20;++i){
        const float t=flight*static_cast<float>(i)/20;
        const auto next=a.position+velocity*t-scene::Vec3{0,0,.5f*iw::kGravity*t*t};
        const auto actual=map.constrainMove(previous,next,scene::course::kPlayerRadius,iw::worldUnits(72),0.f);
        if(!spFinitePosition(actual)||scene::length(actual-next)>1.f)return false;
        previous=next;
    }
    const float ground=map.navigationGroundHeight(previous.x,previous.y,previous.z+scene::course::kStepHeight,-1e9f,scene::course::kPlayerRadius*.7f);
    return std::isfinite(ground)&&std::abs(ground-a.position.z)<scene::course::kStepHeight;
}
}
