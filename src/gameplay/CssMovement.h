#pragma once
#include "gameplay/MovementTrace.h"

// Adapted from Valve's CGameMovement (Source SDK 2013), revision
// b8cfb12c0e083a2ef5b2f9f9b50f3902fa034474, game/shared/gamemovement.cpp.
// Copyright (c) Valve Corporation. All rights reserved.
// Source 1 SDK license and notices: tools/licenses/source-sdk/.
// Engine/entity access is replaced by native-unit State and a hull trace callback.
// See docs/CSS_MOVEMENT_V265.md for CSS policies and unsupported engine services.
namespace gameplay::css {
using scene::Vec3;
using movement::Trace;
constexpr double interval=.015;
constexpr float dt=.015f;
struct State {
    Vec3 position{},velocity{};
    bool grounded{},jumpHeld{},initialized{};
    float surfaceFriction{1};
    int clockMs{};
};
struct Command {
    float forward{},side{},yaw{};
    bool jump{},duck{},walk{};
};
struct Settings {
    float maxSpeed{250},airAccelerate{10};
    bool autoJump{true},unlimitedBunnyhop{};
};
inline Vec3 wish(const Command& cmd,const Settings& cfg) {
    const float c=std::cos(cmd.yaw),s=std::sin(cmd.yaw);
    Vec3 v{cmd.forward*c+cmd.side*s,cmd.forward*s-cmd.side*c,0};
    const float length=scene::length(v);
    const float speed=cmd.walk?std::min(cfg.maxSpeed,135.f):cfg.maxSpeed;
    if(length>0)v=v*(speed/std::max(1.f,length));
    return v;
}
// CGameMovement::AirAccelerate / Accelerate. Intentionally uses the UNcapped
// wish speed in the acceleration term; only the air projection is capped.
inline void accelerate(Vec3& velocity,Vec3 wishVelocity,float acceleration,float friction,bool air) {
    const float wishspeed=scene::length(wishVelocity);
    if(wishspeed<=0)return;
    const Vec3 wishdir=wishVelocity/wishspeed;
    const float wishspd=air?std::min(wishspeed,30.f):wishspeed;
    const float currentspeed=scene::dot(velocity,wishdir);
    const float addspeed=wishspd-currentspeed;
    if(addspeed<=0)return;
    float accelspeed=acceleration*wishspeed*dt*friction;
    if(accelspeed>addspeed)accelspeed=addspeed;
    velocity+=wishdir*accelspeed;
}
inline void friction(State& s) {
    const float speed=scene::length(s.velocity);
    if(speed<.1f)return;
    const float control=std::max(speed,75.f);
    const float newspeed=std::max(0.f,speed-control*4.f*s.surfaceFriction*dt);
    s.velocity=s.velocity*(newspeed/speed);
}
// CGameMovement::ClipVelocity, including its second inward-error correction.
inline Vec3 clip(Vec3 in,Vec3 normal) {
    Vec3 out=in-normal*scene::dot(in,normal);
    const float adjust=scene::dot(out,normal);
    if(adjust<0)out-=normal*adjust;
    return out;
}
inline void checkVelocity(State& s) {
    const auto bound=[](float v){return std::isfinite(v)?std::clamp(v,-3500.f,3500.f):0.f;};
    s.velocity={bound(s.velocity.x),bound(s.velocity.y),bound(s.velocity.z)};
}
// CGameMovement::TryPlayerMove; stock four bumps/five planes, normalized
// crease, first airborne plane update, no facing-dependent stopping logic.
template<class Sweep> void tryPlayerMove(State& s,Sweep&& trace) {
    Vec3 planes[5]{},original=s.velocity;
    const Vec3 primal=s.velocity;
    int numplanes=0;
    float timeLeft=dt,allFraction=0;
    for(int bump=0;bump<4;++bump){
        if(scene::length(s.velocity)==0)break;
        const auto pm=trace(s.position,s.position+s.velocity*timeLeft);
        allFraction+=pm.fraction;
        // Cadence's triangle tracer reports startSolid, not Source allsolid.
        if(pm.startSolid){s.velocity={};return;}
        if(pm.fraction>0){
            if(pm.fraction==1){const auto stuck=trace(pm.end,pm.end);if(stuck.startSolid||stuck.fraction!=1){s.velocity={};break;}}
            s.position=pm.end;original=s.velocity;numplanes=0;
        }
        if(pm.fraction==1)break;
        timeLeft-=timeLeft*pm.fraction;
        if(numplanes>=5){s.velocity={};break;}
        planes[numplanes++]=pm.normal;
        if(numplanes==1&&!s.grounded){
            // sv_bounce=0: floor and wall branches use the same overbounce.
            s.velocity=clip(original,planes[0]);original=s.velocity;
        }else{
            int i=0;
            for(;i<numplanes;++i){
                s.velocity=clip(original,planes[i]);
                int j=0;for(;j<numplanes;++j)if(j!=i&&scene::dot(s.velocity,planes[j])<0)break;
                if(j==numplanes)break;
            }
            if(i==numplanes){
                if(numplanes!=2){s.velocity={};break;}
                const auto dir=scene::normalize(scene::cross(planes[0],planes[1]));
                s.velocity=dir*scene::dot(dir,s.velocity);
            }
            if(scene::dot(s.velocity,primal)<=0){s.velocity={};break;}
        }
    }
    if(allFraction==0)s.velocity={};
}
template<class Sweep> void stepMove(State& s,Sweep&& trace) {
    const State start=s;
    tryPlayerMove(s,trace);const State down=s;
    s=start;
    constexpr float step=18.f+1.f/32.f;
    const auto up=trace(s.position,s.position+Vec3{0,0,step});
    if(!up.startSolid)s.position=up.end;
    tryPlayerMove(s,trace);
    const auto floor=trace(s.position,s.position-Vec3{0,0,step});
    if(floor.normal.z<.7f){s=down;return;}
    if(!floor.startSolid)s.position=floor.end;
    const auto distance=[&](Vec3 p){p-=start.position;return p.x*p.x+p.y*p.y;};
    if(distance(down.position)>distance(s.position))s=down;
    else s.velocity.z=down.velocity.z;
}
template<class Sweep> void stayOnGround(State& s,Sweep&& trace) {
    const auto up=trace(s.position,s.position+Vec3{0,0,2});
    const auto down=trace(up.end,s.position-Vec3{0,0,18});
    if(down.fraction>0&&down.fraction<1&&!down.startSolid&&down.normal.z>=.7f&&std::abs(s.position.z-down.end.z)>.5f/32.f)s.position=down.end;
}
template<class Sweep> void walkMove(State& s,Vec3 wishVelocity,Sweep&& trace) {
    s.velocity.z=0;accelerate(s.velocity,wishVelocity,5,s.surfaceFriction,false);s.velocity.z=0;
    if(scene::length(s.velocity)<1){s.velocity={};return;}
    const auto pm=trace(s.position,s.position+s.velocity*dt);
    if(pm.fraction==1&&!pm.startSolid)s.position=pm.end;
    else stepMove(s,trace);
    stayOnGround(s,trace);
}
template<class Sweep> void categorize(State& s,Sweep&& trace) {
    s.surfaceFriction=1;
    if(s.velocity.z>140){s.grounded=false;return;}
    const Vec3 end=s.position-Vec3{0,0,2};
    auto pm=trace(s.position,end);
    const auto standable=[](const Trace& t){return !t.startSolid&&t.fraction<1&&t.normal.z>=.7f;};
    bool ground=standable(pm);
    if(!ground){
        // Source TryTouchGroundInQuadrants; narrowed ground tests never move
        // the origin, so a leaning wall cannot pull the hull into geometry.
        for(auto offset:std::array<Vec3,4>{{{-8,-8,0},{8,8,0},{-8,8,0},{8,-8,0}}})
            if(standable(trace(s.position,end,8.f,offset))){ground=true;break;}
    }
    s.grounded=ground;
    if(ground)s.velocity.z=0;
    else if(s.velocity.z>0)s.surfaceFriction=.25f;
}
// Source FullWalkMove ordering. CSS jump impulse is sqrt(2*800*57), not
// GoldSrc/HL2MP's sqrt(2*800*45). Autojump and unlimited speed are explicit
// Cadence conveniences; neither detects or boosts backward travel.
template<class Sweep> void tick(State& s,const Command& cmd,const Settings& cfg,Sweep&& trace) {
    if(!s.initialized){categorize(s,trace);s.initialized=true;}
    else if(s.velocity.z>250)s.grounded=false;
    s.clockMs+=15;
    Vec3 wishVelocity=wish(cmd,cfg);
    if(cmd.duck&&s.grounded)wishVelocity=wishVelocity*.34f;
    s.velocity.z-=800.f*dt*.5f;checkVelocity(s);
    if(cmd.jump&&s.grounded&&(cfg.autoJump||!s.jumpHeld)){
        if(!cfg.unlimitedBunnyhop){
            const float speed=scene::length(s.velocity),cap=cfg.maxSpeed*1.2f;
            if(cap>0&&speed>cap)s.velocity=s.velocity*(cap/speed);
        }
        s.grounded=false;
        s.velocity.z+=301.99337741083f;
        s.velocity.z-=800.f*dt*.5f;checkVelocity(s);
    }
    s.jumpHeld=cmd.jump;
    if(s.grounded){s.velocity.z=0;friction(s);}
    checkVelocity(s);
    if(s.grounded)walkMove(s,wishVelocity,trace);
    else{accelerate(s.velocity,wishVelocity,cfg.airAccelerate,s.surfaceFriction,true);tryPlayerMove(s,trace);}
    categorize(s,trace);checkVelocity(s);
    s.velocity.z-=800.f*dt*.5f;checkVelocity(s);
    if(s.grounded)s.velocity.z=0;
}
} // namespace gameplay::css
