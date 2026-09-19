#pragma once
#include "scene/CastScene.h"
#include "scene/NativeBoneNames.h"
#include "scene/T9LegacyConstraints.h"
#include <array>
#include <algorithm>
#include <numeric>

namespace scene {
inline std::optional<std::size_t> t9SourceBone(const Skeleton& s,std::string_view name){
    if(auto it=s.boneByName.find(std::string(name));it!=s.boneByName.end())return it->second;
    if(auto it=s.boneByCanonicalName.find(std::string(name));it!=s.boneByCanonicalName.end())return it->second;
    if(auto it=s.boneByName.find(nativeBoneHash(name));it!=s.boneByName.end())return it->second;
    return std::nullopt;
}

// Evaluate the supplied T9 -> legacy constraint rig, then express its output
// in the loaded legacy skeleton's local channels. Native weapon mechanisms
// remain in the same source coordinate space as the converted hands.
inline bool retargetColdWarLegacyRange(CastScene& target,std::size_t firstTarget,const CastScene& source,std::size_t firstSource){
    const auto tv=t9SourceBone(target.skeleton,"tag_view"),sv=t9SourceBone(source.skeleton,"tag_view");
    if(!tv||!sv||firstTarget>=target.animations.size()||firstSource>=source.animations.size())return false;
    struct Binding { std::optional<std::size_t> source; Mat4 offset=Mat4::identity(); };
    std::vector<Binding> bindings(target.skeleton.bones.size());
    for(const auto& c:kT9LegacyConstraints){
        const auto t=target.skeleton.boneByCanonicalName.find(std::string(c.target));const auto s=t9SourceBone(source.skeleton,c.source);
        if(t==target.skeleton.boneByCanonicalName.end()||!s)continue;
        bindings[t->second]={s,trs(c.position,fromEulerRadians(c.rotationDegrees*(kPi/180.f)),{1,1,1})};
        // A T9 weapon root may merge into the legacy tag_weapon socket. Keep
        // its extra authored rotation too, not just tag_weapon_right motion.
        if(c.target=="tag_weapon")if(auto gun=t9SourceBone(source.skeleton,"tag_weapon"))bindings[t->second].source=gun;
    }
    for(std::size_t i=0;i<bindings.size();++i)if(!bindings[i].source){
        const auto& name=target.skeleton.bones[i].name;
        // Do not substitute legacy wrist helpers for the free T9 weapon root.
        if(name=="tag_weapon_right"||name=="tag_weapon_left")continue;
        if(auto s=t9SourceBone(source.skeleton,name))bindings[i].source=s;
    }
    for(const char* name:{"j_shoulder_le","j_shoulder_ri","j_wrist_le","j_wrist_ri","j_index_le_0","j_index_ri_0"}){
        const auto it=target.skeleton.boneByCanonicalName.find(name);if(it==target.skeleton.boneByCanonicalName.end()||!bindings[it->second].source)return false;
    }
    const auto alignment=target.skeleton.bones[*tv].restGlobal*inverseAffine(source.skeleton.bones[*sv].restGlobal);
    bool changed=false;
    for(std::size_t t=firstTarget,s=firstSource;t<target.animations.size()&&s<source.animations.size();++t,++s){
        const auto& input=source.animations[s];auto& output=target.animations[t];
        if(!input.sourceName.starts_with("vm_")||input.durationFrames>10000)continue;
        std::vector<bool> authored(source.skeleton.bones.size());for(const auto& track:input.tracks)if(track.ownsLayer&&track.boneIndex<authored.size())authored[track.boneIndex]=true;
        std::vector<std::array<Track,7>> tracks(bindings.size());
        for(std::size_t i=0;i<bindings.size();++i){
            bool owns=false;
            if(bindings[i].source){
                int parent=target.skeleton.bones[i].parent;
                while(parent>=0&&!bindings[parent].source)parent=target.skeleton.bones[parent].parent;
                const auto parentSource=parent>=0?bindings[parent].source:std::nullopt;
                // Local channels depend on both source branches below their
                // common ancestor. Skip unchanged legacy helper nodes when
                // finding that parent: a wrist-mounted gun must not claim ADS
                // motion shared by both the wrist and free native weapon root.
                std::vector<bool> ancestors(authored.size());
                for(int b=parentSource?static_cast<int>(*parentSource):-1;b>=0;b=source.skeleton.bones[b].parent)ancestors[b]=true;
                int common=static_cast<int>(*bindings[i].source);
                while(common>=0&&!ancestors[common])common=source.skeleton.bones[common].parent;
                for(int b=static_cast<int>(*bindings[i].source);b>=0&&b!=common;b=source.skeleton.bones[b].parent)owns=owns||authored[b];
                for(int b=parentSource?static_cast<int>(*parentSource):-1;b>=0&&b!=common;b=source.skeleton.bones[b].parent)owns=owns||authored[b];
            }
            for(int k=0;k<7;++k){auto& track=tracks[i][k];track.boneIndex=i;track.property=static_cast<TrackProperty>(k);track.mode=TrackMode::Absolute;track.ownsLayer=owns;track.frames.resize(input.durationFrames+1);std::iota(track.frames.begin(),track.frames.end(),0);if(k==3)track.rotationValues.reserve(track.frames.size());else track.scalarValues.reserve(track.frames.size());}
        }
        for(std::uint32_t frame=0;frame<=input.durationFrames;++frame){
            const auto pose=source.samplePose(s,static_cast<float>(frame));std::vector<Mat4> globals(bindings.size());
            for(std::size_t i=0;i<bindings.size();++i){const auto& b=target.skeleton.bones[i];const auto parent=b.parent>=0?globals[b.parent]:Mat4::identity();
                globals[i]=bindings[i].source?alignment*pose[*bindings[i].source]*bindings[i].offset:parent*trs(b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);
                Vec3 p,scale;Quat q;decomposeAffine(inverseAffine(parent)*globals[i],p,q,scale);
                auto& out=tracks[i];out[0].scalarValues.push_back(p.x);out[1].scalarValues.push_back(p.y);out[2].scalarValues.push_back(p.z);out[3].rotationValues.push_back(q);out[4].scalarValues.push_back(scale.x);out[5].scalarValues.push_back(scale.y);out[6].scalarValues.push_back(scale.z);
            }
        }
        output.tracks.clear();for(auto& bone:tracks)for(auto& track:bone){
            // Constant support channels need a single key, not hundreds.
            if(track.property!=TrackProperty::Rotation&&std::all_of(track.scalarValues.begin(),track.scalarValues.end(),[&](float v){return v==track.scalarValues.front();})){track.frames.resize(1);track.scalarValues.resize(1);}
            output.tracks.push_back(std::move(track));
        }
        output.unmappedCurveCount=input.unmappedCurveCount;
        changed=true;
    }
    return changed;
}
}
