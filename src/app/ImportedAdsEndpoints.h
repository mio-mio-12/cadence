#pragma once
#include "scene/CastScene.h"

namespace cadence::imported_actions {
// Convert evaluated endpoints back into rest-relative channels. Absolute
// channels would apply rigMountReferences a second time during sampling.
inline scene::Animation adsEndpointClip(const scene::CastScene& s,std::size_t idle,std::size_t aim,bool down,std::string source){
    const auto hip=s.sampleLocalPose(idle,0),ads=s.sampleLocalPose(aim,0);
    scene::Animation result;result.name=down?"ADS down (pose transition)":"ADS up (pose transition)";
    result.sourceName=std::move(source);result.sourceGame=s.animations[aim].sourceGame;
    result.domain=scene::AnimationDomain::ViewModel;result.action=scene::ActionRole::Aim;
    result.framerate=30;result.durationFrames=30;
    result.viewmodelCameraReference=s.animations[idle].viewmodelCameraReference;
    for(std::size_t b=0;b<s.skeleton.bones.size();++b){
        const auto& from=down?ads[b]:hip[b];const auto& to=down?hip[b]:ads[b];const auto& rest=s.skeleton.bones[b].restLocal;
        for(int channel=0;channel<7;++channel){
            scene::Track t;t.boneIndex=b;t.property=static_cast<scene::TrackProperty>(channel);t.mode=scene::TrackMode::Relative;t.frames={0,30};t.ownsLayer=false;
            for(const auto index:{idle,aim})for(const auto& original:s.animations[index].tracks)
                if(original.boneIndex==b&&original.property==t.property)t.ownsLayer|=original.ownsLayer;
            if(t.property==scene::TrackProperty::Rotation){
                const scene::Quat inverse{-rest.rotation.x,-rest.rotation.y,-rest.rotation.z,rest.rotation.w};
                t.rotationValues={scene::normalize(scene::multiply(inverse,from.rotation)),scene::normalize(scene::multiply(inverse,to.rotation))};
            }else{
                const bool scale=channel>3;const int axis=scale?channel-4:channel;
                const auto value=[&](const scene::Transform& p){const auto v=scale?p.scale:p.position;return axis==0?v.x:axis==1?v.y:v.z;};
                t.scalarValues={value(from)-value(rest),value(to)-value(rest)};
            }
            result.tracks.push_back(std::move(t));
        }
    }
    return result;
}
}
