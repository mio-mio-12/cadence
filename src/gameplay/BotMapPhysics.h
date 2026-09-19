#pragma once
#include "gameplay/BotActor.h"

namespace gameplay::bot {
// Map collision belongs to each fixed tick, including its presentation history.
// Separate entry point leaves Classic's movement behavior unchanged.
template<class Map> void stepOnMap(Actor& actor,float delta,float moveScale,const Map& map){
    constexpr float tick=1.0f/125.0f,radius=scene::course::kPlayerRadius;
    const float height=iw::worldUnits(actor.stance==scene::Stance::Stand?72:actor.stance==scene::Stance::Crouch?40:20);
    constexpr float stepHeight=scene::course::kStepHeight;
    constexpr float noGround=-1.e20f;
    actor.fixedPresentationThisUpdate=true;actor.physicsDeltaThisUpdate=0;
    if(!actor.physicsPresentationValid){actor.previousPhysicsPosition=actor.position;actor.physicsPresentationValid=true;}
    const float speedScale=(actor.input.sprint?iw::kSprintScale:iw::stanceSpeedScale(actor.stance))*std::max(0.f,moveScale);
    scene::Vec3 local{actor.input.forward*(actor.input.forward<0?iw::kBackSpeedScale:1)*speedScale,-actor.input.right*(actor.input.sprint?iw::kSprintStrafeSpeedScale:iw::kStrafeSpeedScale)*speedScale,0};
    if(scene::length(local)>speedScale)local=scene::normalize(local)*speedScale;
    const float cy=std::cos(actor.yaw),sy=std::sin(actor.yaw);
    scene::Vec3 wish=actor.spLedgeTime>0?actor.spLedgeTravel:scene::Vec3{(local.x*cy-local.y*sy)*iw::kRunSpeed,(local.x*sy+local.y*cy)*iw::kRunSpeed,0};
    if constexpr(requires { map.gameplay; })if(actor.grounded)wish=map.gameplay.assistedWish(actor.position,wish,radius,height);
    if(actor.input.jump&&!actor.previousJump&&actor.grounded){actor.velocity.z=std::sqrt(2*iw::kGravity*iw::kJumpHeight);actor.grounded=false;}
    actor.previousJump=actor.input.jump;
    actor.fixedAccumulator=std::min(actor.fixedAccumulator+std::clamp(delta,0.f,.05f),.1f);
    while(actor.fixedAccumulator>=tick){
        actor.fixedAccumulator-=tick;actor.physicsDeltaThisUpdate+=tick;
        const auto old=actor.position;const bool wasGrounded=actor.grounded;actor.previousPhysicsPosition=old;
        if(wasGrounded){const float speed=horizontalSpeed(actor);if(speed>0){const float next=std::max(0.f,speed-std::max(speed,iw::kStopSpeed)*iw::kFriction*tick);actor.velocity.x*=next/speed;actor.velocity.y*=next/speed;}}
        const float speed=scene::length(wish);
        if(speed>0){const auto dir=wish/speed;const float add=speed-scene::dot(scene::Vec3{actor.velocity.x,actor.velocity.y,0},dir);if(add>0){const float accel=wasGrounded?iw::groundAcceleration(actor.stance):iw::kAirAccelerate;const float amount=std::min(add,accel*tick*speed);actor.velocity.x+=amount*dir.x;actor.velocity.y+=amount*dir.y;}}
        // Authored reaction travel is a kinematic horizontal target, not a
        // tiny walking input that minimum stop friction can erase. Collision
        // and gravity still own the resulting displacement every fixed tick.
        if(wasGrounded&&actor.spMovingScenario&&!actor.spScenarioInterrupted){actor.velocity.x=wish.x;actor.velocity.y=wish.y;}
        actor.velocity.z-=iw::kGravity*tick;
        scene::Vec3 contact{};
        // The legacy map solver adds 25% step tolerance. Keep real ascent at
        // the configured step height here, and disallow airborne auto-stepping.
        const auto proposed=old+actor.velocity*tick;
        actor.position=map.constrainMove(old,proposed,radius,height,wasGrounded?stepHeight/1.25f:0.f,&contact);
        // Legacy constrainMove has an additional ground-query allowance and
        // can propose an ascent taller than the real step limit. Such a rise
        // must be an explicit mantle, never an invisible auto-step onto a sill.
        if(actor.position.z>old.z+(wasGrounded?stepHeight:std::max(0.f,proposed.z-old.z))+.1f){
            actor.position=map.constrainMove(old,proposed,radius,height,0.f,&contact);
            if(actor.position.z>old.z+(wasGrounded?stepHeight:std::max(0.f,proposed.z-old.z))+.1f)actor.position={old.x,old.y,proposed.z};
        }
        // Never let a step candidate teleport a falling body down to a ledge.
        if(actor.position.z<proposed.z)actor.position.z=proposed.z;
        // The map query adds a historical 45-unit ceiling allowance. Cancel
        // that allowance so an overlaid lip above a falling body's feet cannot
        // hide the lower floor that the body actually crosses this tick.
        const float groundCeiling=wasGrounded?old.z+stepHeight:std::max(old.z,actor.position.z)+.1f;
        const float ground=map.navigationGroundHeight(actor.position.x,actor.position.y,groundCeiling-45.f,noGround,radius*.55f);
        const bool hasGround=ground>noGround*.5f;
        // Steering arcs and reaction travel can deviate from a validated path.
        // Do not walk off unsupported ground; deliberate jumps remain airborne
        // and short, supported route drops are allowed.
        if(wasGrounded&&(!hasGround||ground<old.z-iw::worldUnits(128))){
            const float oldGround=map.navigationGroundHeight(old.x,old.y,old.z+.1f-45.f,noGround,radius*.55f);
            if(oldGround>noGround*.5f&&std::abs(oldGround-old.z)<=2.f){
                actor.position=old;actor.velocity={};actor.grounded=true;actor.groundGrace=.12f;continue;
            }
        }
        const bool landing=hasGround&&actor.velocity.z<=0&&old.z>=ground-.1f&&actor.position.z<=ground+.1f;
        const bool stepping=hasGround&&wasGrounded&&ground>=old.z&&ground-old.z<=stepHeight+.1f&&actor.position.z<=ground+.1f;
        const bool supported=hasGround&&wasGrounded&&actor.velocity.z<=0&&std::abs(old.z-ground)<=3.f&&actor.position.z<=ground+3.f;
        actor.grounded=landing||stepping||supported;
        if(actor.grounded){actor.position.z=ground;actor.velocity.z=0;actor.groundGrace=.12f;}else actor.groundGrace=std::max(0.f,actor.groundGrace-tick);
        if(scene::length(contact)>0.1f){contact.z=0;const float n=scene::length(contact);if(n>.1f){contact=contact/n;const float into=scene::dot(actor.velocity,contact);if(into<0)actor.velocity=actor.velocity-contact*into;}}
    }
}
}
