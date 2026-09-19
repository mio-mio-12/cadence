#pragma once
#include "gameplay/BotActor.h"

namespace gameplay::bot {
// The simulation remains authoritative. Only the draw position is delayed by
// one fixed tick (8 ms of game time), just like the player presentation path.
inline scene::Vec3 presentationPosition(Actor& actor){
    if(!actor.fixedPresentationThisUpdate||!actor.physicsPresentationValid||
       scene::length(actor.position-actor.previousPhysicsPosition)>iw::worldUnits(128)){
        actor.previousPhysicsPosition=actor.position;
        actor.physicsPresentationValid=true;
        // Mantle/death movement is integrated every frame, not in fixed ticks.
        if(!actor.fixedPresentationThisUpdate)actor.fixedAccumulator=0;
        return actor.position;
    }
    return scene::lerp(actor.previousPhysicsPosition,actor.position,
        std::clamp(actor.fixedAccumulator/(1.0f/125.0f),0.0f,1.0f));
}
}
