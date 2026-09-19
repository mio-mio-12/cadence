#pragma once
#include "gameplay/IwPresentation.h"
#include "gameplay/MovementTrace.h"

// Independent dry-land controller; see docs/SOURCE_MOVEMENT_V2.md for the
// reference and provisional CSS-specific policies. Not a binary-exact port.
namespace gameplay::source_v2 {
using scene::Vec3;
constexpr float scale(){return gameplay::iw::worldUnits(1.0f);}
constexpr float units(float v){return v*scale();}
constexpr float kCssMaxSpeed=320, kCssAccelerate=5, kCssAirAccelerate=10;
constexpr float kCssFriction=4, kCssStopSpeed=75, kCssGravity=800;
constexpr float kCssJumpVelocity=268.328157f;
constexpr float maxSpeed(){return units(kCssMaxSpeed);}
constexpr float gravity(){return units(kCssGravity);}
constexpr float jumpVelocity(){return units(kCssJumpVelocity);}
constexpr float stopSpeed(){return units(kCssStopSpeed);}
constexpr double tickSeconds=0.015;
struct Settings {
    float playerSpeed{gameplay::iw::kRunSpeed}, radius{units(16)}, height{units(72)};
    float stepHeight{units(18)}, surfaceFriction{1},airAccelerate{kCssAirAccelerate};
    // Source-v2 is the user-facing CSS controller. Holding jump therefore
    // repeats CheckJumpButton as soon as the player becomes grounded.
    bool autoJump{true}, unlimitedBunnyhop{false};
};
struct State { Vec3 position{},velocity{}; bool grounded{},jumpHeld{}; };
// Use the same hull as movement, rather than the legacy capsule helper. A
// close steep plane counts only while entering/sliding; leaving it restores
// normal air steering and airborne ledge acquisition immediately.
template<class Sweep> bool surfContact(Vec3 position,Vec3 velocity,Sweep&& trace) {
    const auto contact=trace(position,position-Vec3{0,0,units(2)});
    return !contact.startSolid&&contact.fraction<1&&contact.normal.z>=0.01f&&contact.normal.z<0.7f&&
           scene::dot(velocity,contact.normal)<=units(0.1f);
}
inline void applyFriction(Vec3& v,float dt,float surface=1) {
    const float speed=std::hypot(v.x,v.y);
    if(speed<0.0001f)return;
    const float factor=std::max(0.0f,speed-std::max(speed,stopSpeed())*kCssFriction*surface*dt)/speed;
    v.x*=factor;v.y*=factor;
}
inline void accelerate(Vec3& v,Vec3 direction,float wishSpeed,float dt,bool air,float surface=1,float airAcceleration=kCssAirAccelerate) {
    direction.z=0;direction=scene::normalize(direction);
    wishSpeed=std::clamp(wishSpeed,0.0f,maxSpeed());
    const float add=(air?std::min(wishSpeed,units(30)):wishSpeed)-scene::dot(v,direction);
    if(add<=0)return;
    // Legacy Source uses UNcapped wishSpeed in the acceleration term.
    v+=direction*std::min(add,(air?std::max(0.0f,airAcceleration):kCssAccelerate)*wishSpeed*dt*surface);
}
inline Vec3 wishVelocity(float forward,float side,float yaw,float speed) {
    Vec3 local{forward,-side,0}; if(scene::length(local)>1)local=scene::normalize(local);
    const float c=std::cos(yaw),s=std::sin(yaw);
    return Vec3{local.x*c-local.y*s,local.x*s+local.y*c,0}*std::min(speed,maxSpeed());
}
template<class Sweep> void slide(State& state,float dt,Sweep&& trace) {
    Vec3 planes[5]{}; int count=0;
    const Vec3 original=state.velocity;
    Vec3 segmentVelocity=state.velocity;
    float remaining=dt;
    for(int bump=0;bump<4;++bump) {
        if(scene::length(state.velocity)<0.0001f)break;
        const auto hit=trace(state.position,state.position+state.velocity*remaining);
        if(hit.startSolid){state.velocity={};return;}
        state.position=hit.end;
        // Constraints belong to the current contact, not every surface crossed
        // during this tick. Reset after progress, as in Source TryPlayerMove.
        if(hit.fraction>0){count=0;segmentVelocity=state.velocity;}
        if(hit.fraction>=1)break;
        remaining*=1-hit.fraction;
        if(count==5){state.velocity={};break;}
        planes[count++]=hit.normal;
        bool found=false;
        for(int i=0;i<count;++i) {
            auto candidate=movement::clip(segmentVelocity,planes[i]);
            bool valid=true;
            for(int j=0;j<count;++j)if(j!=i&&scene::dot(candidate,planes[j])<-0.001f){valid=false;break;}
            if(valid){state.velocity=candidate;found=true;break;}
        }
        if(!found) {
            if(count!=2){state.velocity={};break;}
            const auto crease=scene::normalize(scene::cross(planes[0],planes[1]));
            state.velocity=crease*scene::dot(state.velocity,crease);
        }
        if(scene::dot(state.velocity,original)<=0){state.velocity={};break;}
    }
}
template<class Sweep> void tick(State& state,Vec3 wish,bool jump,const Settings& cfg,Sweep&& trace) {
    constexpr float dt=static_cast<float>(tickSeconds);
    const auto ground=trace(state.position,state.position-Vec3{0,0,units(2)});
    state.grounded=state.velocity.z<=units(140)&&!ground.startSolid&&ground.fraction<1&&ground.normal.z>=0.7f;
    if(state.grounded)state.position=ground.end;
    state.velocity.z-=gravity()*dt*0.5f;
    if(jump&&state.grounded&&(cfg.autoJump||!state.jumpHeld)) {
        const float speed=std::hypot(state.velocity.x,state.velocity.y),cap=cfg.playerSpeed*1.2f;
        // Provisional CSS jump-speed policy; never a backward-specific boost.
        if(!cfg.unlimitedBunnyhop&&speed>cap){state.velocity.x*=cap/speed;state.velocity.y*=cap/speed;}
        // StartGravity and CheckJumpButton::FinishGravity both precede move.
        state.velocity.z=jumpVelocity()-gravity()*dt;
        state.grounded=false;
    }
    state.jumpHeld=jump;
    if(state.grounded){state.velocity.z=0;applyFriction(state.velocity,dt,cfg.surfaceFriction);}
    accelerate(state.velocity,wish,std::min(scene::length(wish),cfg.playerSpeed),dt,!state.grounded,cfg.surfaceFriction,cfg.airAccelerate);
    const State before=state;
    slide(state,dt,trace);
    if(before.grounded) {
        State stepped=before;
        const auto up=trace(before.position,before.position+Vec3{0,0,cfg.stepHeight});
        if(!up.startSolid&&up.fraction>0) {
            stepped.position=up.end;slide(stepped,dt,trace);
            const auto down=trace(stepped.position,stepped.position-Vec3{0,0,cfg.stepHeight+units(2)});
            const auto dist=[](Vec3 a,Vec3 b){return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y);};
            if(!down.startSolid&&down.fraction<1&&down.normal.z>=0.7f&&dist(stepped.position,before.position)>dist(state.position,before.position)+0.0001f) {
                stepped.position=down.end;stepped.velocity.z=state.velocity.z;state=stepped;
            }
        }
    }
    const float groundDistance=before.grounded?cfg.stepHeight+units(2):units(2);
    const auto down=trace(state.position,state.position-Vec3{0,0,groundDistance});
    state.grounded=state.velocity.z<=(before.grounded?units(140):0.0f)&&!down.startSolid&&down.fraction<1&&down.normal.z>=0.7f;
    if(state.grounded){state.position=down.end;state.velocity.z=0;}
    else state.velocity.z-=gravity()*dt*0.5f;
}
} // namespace gameplay::source_v2
