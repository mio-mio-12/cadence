#pragma once
#include "scene/CastScene.h"
#include <cmath>
#include <stdexcept>

namespace scene::imported {
// Diagnostic only. The caller must establish POST delta provenance externally.
// A bind-baked absolute CAST does not itself contain that provenance.
inline Animation composeBindBakedPostDelta(const CastScene& native,std::size_t holding,
    std::size_t delta,std::size_t torsoRoot,float holdingFrame=0.f){
    if(holding>=native.animations.size()||delta>=native.animations.size()||torsoRoot>=native.skeleton.bones.size())
        throw std::invalid_argument("Missing native holding/delta/spine");
    const auto& input=native.animations[delta];
    if(input.durationFrames>10000||!(input.framerate>0))throw std::invalid_argument("Invalid delta duration");
    for(const auto& t:input.tracks)if(t.mode!=TrackMode::Absolute)
        throw std::invalid_argument("Expected bind-baked absolute delta tracks");
    auto output=input;output.tracks.clear();output.coldWarWorldPose.reset();
    output.domain=AnimationDomain::PlayerTorso;output.motion=MotionRole::Unknown;output.action=ActionRole::Fire;
    output.name="diagnostic_post_delta_"+input.name;
    const auto base=native.sampleLocalPose(holding,holdingFrame);
    std::vector<bool> owned(native.skeleton.bones.size());
    for(std::size_t i=0;i<owned.size();++i){int p=static_cast<int>(i);for(std::size_t n=0;n<owned.size()&&p>=0;++n){
        if(static_cast<std::size_t>(p)==torsoRoot){owned[i]=true;break;}
        if(static_cast<std::size_t>(p)>=owned.size())throw std::invalid_argument("Invalid native hierarchy");
        p=native.skeleton.bones[p].parent;
    }}
    const TrackProperty properties[]={TrackProperty::TranslationX,TrackProperty::TranslationY,TrackProperty::TranslationZ,TrackProperty::Rotation,TrackProperty::ScaleX,TrackProperty::ScaleY,TrackProperty::ScaleZ};
    for(std::size_t i=0;i<owned.size();++i)for(const auto property:properties){Track track;track.boneIndex=i;track.property=property;track.ownsLayer=owned[i];output.tracks.push_back(std::move(track));}
    for(std::uint32_t frame=0;frame<=input.durationFrames;++frame){
        const auto sampled=native.sampleLocalPose(delta,static_cast<float>(frame));
        for(std::size_t i=0;i<owned.size();++i){auto value=base[i];const auto& rest=native.skeleton.bones[i].restLocal;
            if(owned[i]){
                const auto bind=normalize(rest.rotation);const Quat inverseBind{-bind.x,-bind.y,-bind.z,bind.w};
                value.rotation=normalize(multiply(base[i].rotation,multiply(inverseBind,sampled[i].rotation)));
                value.position=base[i].position+(sampled[i].position-rest.position);
                // Exporter proof covers p/q POST deltas, not scale deltas.
                if(length(sampled[i].scale-rest.scale)>1e-4f)throw std::invalid_argument("Unsupported delta scale");
            }
            const float scalar[]={value.position.x,value.position.y,value.position.z,0,value.scale.x,value.scale.y,value.scale.z};
            for(int k=0;k<7;++k){auto& track=output.tracks[i*7+k];track.frames.push_back(frame);if(k==3)track.rotationValues.push_back(value.rotation);else track.scalarValues.push_back(scalar[k]);}
        }
    }
    output.sourceCurveCount=output.tracks.size();output.unmappedCurveCount=0;
    return output;
}
}
