#pragma once
#include "scene/CastScene.h"
#include "assets/ImportedGamePolicy.h"

namespace scene::imported {
inline void completeBlackSquadActions(CastScene& s){
    if(s.viewHandsDriverGame!="blacksquad"&&s.importedViewGame!="blacksquad")return;
    size_t idle=s.animations.size();
    for(size_t i=0;i<s.animations.size();++i){const auto& a=s.animations[i];
        if(assets::imported::additionalAction("blacksquad",a.sourceName)!="idle"||a.tracks.empty()||
           std::any_of(a.tracks.begin(),a.tracks.end(),[](const auto& t){return t.mode==TrackMode::Additive;}))continue;
        if(idle==s.animations.size()||a.sourceName.size()<s.animations[idle].sourceName.size())idle=i;
    }
    if(idle==s.animations.size())return;
    // The exporter preserves additive deltas, but not AdditiveRefName. Match
    // its native preview's Idle fallback, not the skeletal bind pose. Bake
    // once so playback, capture and foreign hands share one absolute action.
    for(size_t i=0;i<s.animations.size();++i){auto& a=s.animations[i];
        if(a.tracks.empty()||a.framerate<=0||a.durationFrames>10000||
           !std::all_of(a.tracks.begin(),a.tracks.end(),[](const auto& t){return t.mode==TrackMode::Additive;}))continue;
        std::vector<Track> baked(s.skeleton.bones.size()*7);
        for(size_t b=0;b<s.skeleton.bones.size();++b)for(int k=0;k<7;++k){auto& t=baked[b*7+k];t.boneIndex=b;t.property=static_cast<TrackProperty>(k);t.mode=TrackMode::Absolute;}
        for(unsigned frame=0;frame<=a.durationFrames;++frame){
            const auto base=s.sampleLocalPose(idle,std::min(float(s.animations[idle].durationFrames),frame*s.animations[idle].framerate/a.framerate));
            const auto deltaPose=s.sampleLocalPose(i,float(frame));
            for(size_t b=0;b<base.size();++b){const auto& rest=s.skeleton.bones[b].restLocal;
                const auto delta=multiply(Quat{-rest.rotation.x,-rest.rotation.y,-rest.rotation.z,rest.rotation.w},deltaPose[b].rotation);
                const auto rotation=normalize(multiply(delta,base[b].rotation));
                const auto position=base[b].position+deltaPose[b].position-rest.position;
                const auto scale=base[b].scale+deltaPose[b].scale-rest.scale;
                for(int k=0;k<7;++k){auto& t=baked[b*7+k];t.frames.push_back(frame);if(k==3)t.rotationValues.push_back(rotation);else t.scalarValues.push_back(k==0?position.x:k==1?position.y:k==2?position.z:k==4?scale.x:k==5?scale.y:scale.z);}
            }
        }
        for(auto& t:baked){
            const bool constant=t.property==TrackProperty::Rotation
                ?std::all_of(t.rotationValues.begin(),t.rotationValues.end(),[&](const auto& q){const auto& f=t.rotationValues.front();return q.x==f.x&&q.y==f.y&&q.z==f.z&&q.w==f.w;})
                :std::all_of(t.scalarValues.begin(),t.scalarValues.end(),[&](float v){return v==t.scalarValues.front();});
            if(constant){t.frames.resize(1);if(t.property==TrackProperty::Rotation)t.rotationValues.resize(1);else t.scalarValues.resize(1);}
        }
        a.tracks=std::move(baked);a.name+=" [idle base]";
    }
}
}
