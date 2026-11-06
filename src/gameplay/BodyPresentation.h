#pragma once
#include "gameplay/WorldActorPresentation.h"
#include "scene/ImportedBody.h"
#include "scene/ImportedBodyMotion.h"

namespace gameplay::presentation {
// The controller owns horizontal locomotion, including takeoff and landing.
// Keep the same ownership through blends so an outgoing running clip cannot
// reintroduce its accumulated travel for the first frame of a jump.
// Native pelvis Z and special-action trajectories retain their own policy.
inline bool suppressPlayerGaitTransport(const scene::Skeleton& skeleton,
    std::vector<scene::Mat4>& pose,scene::MotionRole motion,scene::ActionRole action){
    if(action!=scene::ActionRole::None||!scene::imported::bodyLayout(skeleton))return false;
    switch(motion){
    case scene::MotionRole::Idle:case scene::MotionRole::Walk:
    case scene::MotionRole::Run:case scene::MotionRole::Sprint:
    case scene::MotionRole::Crawl:case scene::MotionRole::Jump:
    case scene::MotionRole::Land:break;
    default:return false;
    }
    scene::imported::suppressBodyRootMotion(skeleton,pose,true);
    return true;
}
// Remove the model's static authored facing only at its world-placement root.
// Controller headings and authored turns within animation channels stay intact.
inline scene::Mat4 bodyWorldTransform(const scene::Skeleton& skeleton,
    scene::Vec3 authoritative,scene::Vec3 interpolated,bool renderSample,
    float heading,float groundLift) {
    return worldActorTransform(authoritative,interpolated,renderSample,
        heading+scene::imported::bodyPresentationYaw(skeleton),groundLift);
}
}
