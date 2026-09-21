#pragma once
#include "scene/CastScene.h"
#include <array>

namespace cadence::iw_reload {
// Present exported intro/insertion/out as one action; never alter source CASTs.
inline bool prepare(scene::CastScene& scene,const std::string& prefix) {
    const auto find=[&](const std::string& suffix){for(std::size_t i=0;i<scene.animations.size();++i)if(scene.animations[i].sourceName==prefix+suffix)return i;return SIZE_MAX;};
    if(find("reload.cast")!=SIZE_MAX)return false;
    const std::array ids{find("reload_intro.cast"),find("reload_loop.cast"),find("reload_out.cast")};
    for(auto id:ids)if(id==SIZE_MAX||scene.animations[id].tracks.empty()||!scene.animations[id].durationFrames)return false;
    const float fps=scene.animations[ids[0]].framerate;
    for(auto id:ids)if(scene.animations[id].framerate!=fps||scene.animations[id].durationFrames>1800)return false;
    scene::Animation joined;joined.name=joined.sourceName=prefix+"reload.cast";joined.sourceGame="mw";joined.framerate=fps;
    scene::classifyAnimationName(joined.sourceName,joined);
    // Sample already-bound local channels without applying editable mount offsets twice.
    auto mounts=std::move(scene.rigMountReferences);scene.rigMountReferences.clear();
    std::vector<scene::Transform> local;
    for(std::size_t bone=0;bone<scene.skeleton.bones.size();++bone)for(auto property:{scene::TrackProperty::TranslationX,scene::TrackProperty::TranslationY,scene::TrackProperty::TranslationZ,scene::TrackProperty::Rotation,scene::TrackProperty::ScaleX,scene::TrackProperty::ScaleY,scene::TrackProperty::ScaleZ}){scene::Track track;track.boneIndex=bone;track.property=property;joined.tracks.push_back(std::move(track));}
    std::uint32_t offset=0;
    for(auto id:ids){auto& clip=scene.animations[id];const bool looping=clip.looping;clip.looping=false;
        for(std::uint32_t f=0;f<=clip.durationFrames;++f){scene.sampleLocalPoseInto(id,float(f),local);for(auto& track:joined.tracks){if(!track.frames.empty()&&track.frames.back()==offset+f){track.frames.pop_back();if(track.property==scene::TrackProperty::Rotation)track.rotationValues.pop_back();else track.scalarValues.pop_back();}track.frames.push_back(offset+f);const auto& p=local[track.boneIndex];switch(track.property){case scene::TrackProperty::Rotation:track.rotationValues.push_back(p.rotation);break;case scene::TrackProperty::TranslationX:track.scalarValues.push_back(p.position.x);break;case scene::TrackProperty::TranslationY:track.scalarValues.push_back(p.position.y);break;case scene::TrackProperty::TranslationZ:track.scalarValues.push_back(p.position.z);break;case scene::TrackProperty::ScaleX:track.scalarValues.push_back(p.scale.x);break;case scene::TrackProperty::ScaleY:track.scalarValues.push_back(p.scale.y);break;case scene::TrackProperty::ScaleZ:track.scalarValues.push_back(p.scale.z);break;}}}
        for(auto notification:clip.notifications){for(auto& frame:notification.frames)frame+=offset;joined.notifications.push_back(std::move(notification));}
        offset+=clip.durationFrames;clip.looping=looping;
    }
    scene.rigMountReferences=std::move(mounts);joined.durationFrames=offset;joined.sourceCurveCount=joined.tracks.size();scene.animations.push_back(std::move(joined));return true;
}
}
