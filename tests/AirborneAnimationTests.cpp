#include "gameplay/AirborneAnimation.h"
#include "scene/CastScene.h"
#include <cmath>
#include <iostream>

int main() {
    using namespace gameplay::presentation;
    int failures=0;
    const auto check=[&](bool passed,const char* label){if(!passed){std::cerr<<label<<'\n';++failures;}};
    AirborneAnimation state;
    state.update(false,1.0f);
    state.update(true,0.1f);
    check(state.active&&state.elapsed==0.0f,"Takeoff starts at frame zero independent of global clock");
    state.update(true,0.25f);
    check(airborneAnimationFrame(state,30,30,true)==7.5f,"Fractional airborne frames interpolate");
    for(int i=0;i<1000;++i)state.update(true,0.01f);
    const float held=airborneAnimationFrame(state,30,30,true);
    check(held<30&&held>29.999f,"Loop-tagged takeoff holds final pose");
    check(std::fmod(held,30.0f)>29.999f,"Scene loop sampler cannot wrap held frame to zero");
    check(airborneAnimationFrame(state,30,30,false)==30,"Nonlooping clips hold exact endpoint");
    state.update(false,0.1f);
    check(!state.active&&airborneAnimationFrame(state,30,30,true)==held,"Landing transition retains outgoing airborne pose");
    state.update(true,0.01f);
    check(state.elapsed==0,"A new grounded-to-airborne episode restarts takeoff");
    check(heldAnimationFrame(100,0,true)==0,"Zero-duration clips are safe");
    check(heldAnimationFrame(-1,30,true)==0,"Negative frame is clamped");
    const float before=state.elapsed;state.update(true,-1);
    check(state.elapsed==before,"Negative delta does not rewind");
    // Native/manual preview still loops: neither this policy nor its callers
    // changes Animation.looping or modifies imported curve metadata.
    check(std::fmod(60.0f,30.0f)==0,"Unrelated preview loop semantics unchanged");
    scene::CastScene rig;
    scene::Bone leg;leg.name="leg";
    scene::Bone torso;torso.name="torso";
    rig.skeleton.bones={leg,torso};
    scene::Animation jump;jump.looping=true;jump.durationFrames=30;jump.motion=scene::MotionRole::Jump;
    scene::Track legTrack;legTrack.boneIndex=0;legTrack.property=scene::TrackProperty::TranslationX;legTrack.frames={0,30};legTrack.scalarValues={0,10};
    jump.tracks.push_back(legTrack);
    scene::Animation action;action.durationFrames=30;action.action=scene::ActionRole::Fire;
    auto torsoTrack=legTrack;torsoTrack.boneIndex=1;torsoTrack.scalarValues={0,20};action.tracks.push_back(torsoTrack);
    rig.animations={jump,action};
    const auto raw=rig.sampleLocalPose(0,30);
    check(raw[0].position.x==0,"Fixture reproduces source looping endpoint wrap");
    const auto finalPose=rig.sampleLocalPose(0,held);
    check(finalPose[0].position.x>9.999f,"Actual scene sampler holds jump endpoint");
    scene::PoseSlot actionSlot;actionSlot.nodes.push_back({1,15,1,scene::LayerMode::Override,false});
    const auto layered=rig.sampleLocalPoseSlots(0,held,{actionSlot});
    check(layered[0].position.x>9.999f,"Held legs remain stable under torso action");
    check(std::abs(layered[1].position.x-10)<0.001f,"Torso fire still advances over held jump");
    return failures?1:0;
}
