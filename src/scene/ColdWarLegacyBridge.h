#pragma once
#include "scene/CastScene.h"
#include "scene/NativeBoneNames.h"
#include "scene/T9LegacyConstraints.h"
#include <array>
#include <algorithm>
#include <numeric>

namespace scene {
// Legacy arm clips have no independent IW torso deformation. Preserve sleeve
// support using the calibrated clavicles, without changing body/weapon meshes,
// vertex positions, triangles, or the source files. Repeated calls are inert.
inline void supportIwLegacySleeves(CastScene& target){
    const auto& names=target.skeleton.boneByName;
    if(!names.contains("j_spineupper")||!names.contains("j_clavicle_le")||!names.contains("j_clavicle_ri"))return;
    const auto spine=names.at("j_spineupper"),left=names.at("j_clavicle_le"),right=names.at("j_clavicle_ri");
    const auto lp=transformPoint(target.skeleton.bones[left].restGlobal,{}),rp=transformPoint(target.skeleton.bones[right].restGlobal,{});
    for(auto& mesh:target.meshes){
        if(mesh.viewmodelWeapon)continue;
        bool shoulder=false,body=false,hasSpine=false;
        for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<target.skeleton.bones.size()){
            const auto& name=target.skeleton.bones[v.bones[k]].name;
            shoulder=shoulder||name.starts_with("j_shouldertwist");
            body=body||name.starts_with("j_hip")||name.starts_with("j_knee")||name.starts_with("j_neck")||name.starts_with("j_head");
            hasSpine=hasSpine||v.bones[k]==spine;
        }
        if(!shoulder||body||!hasSpine)continue;
        for(auto& v:mesh.vertices){
            const auto side=length(v.position-lp)<length(v.position-rp)?left:right;
            for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]==spine)v.bones[k]=side;
        }
    }
}
inline bool coldWarLegacyTarget(const Skeleton& skeleton){
    for(const char* name:{"tag_view","j_shoulder_le","j_shoulder_ri","j_wrist_le","j_wrist_ri","j_index_le_0","j_index_ri_0"}){
        const auto it=skeleton.boneByCanonicalName.find(name);
        if(it==skeleton.boneByCanonicalName.end()||it->second>=skeleton.bones.size())return false;
    }
    return true;
}
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
    const bool iwTarget=target.skeleton.boneByName.contains("j_clavicle_le")&&target.skeleton.boneByName.contains("j_wristfronttwist1_le");
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
    // The T9 constraints end in legacy IW4 frames, not IW7 deform frames.
    // Compose the existing inverse IW calibration, including finger indexing
    // and wrist twist offsets, rather than applying legacy offsets to IW7 skin.
    if(iwTarget)for(std::size_t i=0;i<bindings.size();++i){
        const auto& name=target.skeleton.bones[i].name;
        // T9 exports also carry body-only joints with IW names. They are not
        // view-arm drivers: importing their bind pose creates an independent
        // spine branch, forcing sparse ADS to replace the calibrated idle arms.
        // Uncalibrated IW deform helpers must retain their native parent chain.
        bool weaponBone=false;
        for(int b=static_cast<int>(i);b>=0;b=target.skeleton.bones[b].parent){
            const auto& ancestor=target.skeleton.bones[b].name;
            if(ancestor=="j_gun"||ancestor=="tag_weapon"){weaponBone=true;break;}
        }
        if(name.starts_with("j_")&&!weaponBone&&!legacyToIwCalibration(name))bindings[i]={};
        if(const auto calibration=legacyToIwCalibration(target.skeleton.bones[i].name))
            for(const auto& c:kT9LegacyConstraints)if(c.target==calibration->legacyBone){
                if(auto s=t9SourceBone(source.skeleton,c.source)){
                    if(c.target=="tag_weapon")if(auto gun=t9SourceBone(source.skeleton,"tag_weapon"))s=gun;
                    bindings[i]={s,trs(c.position,fromEulerRadians(c.rotationDegrees*(kPi/180.f)),{1,1,1})*calibration->offset};
                }
                break;
            }
    }
    if(iwTarget)for(const auto side:{"le","ri"}){
        const auto clavicle=target.skeleton.boneByName.find(std::string("j_clavicle_")+side);
        const auto shoulder=target.skeleton.boneByName.find(std::string("j_shoulder_")+side);
        if(clavicle==target.skeleton.boneByName.end()||shoulder==target.skeleton.boneByName.end())continue;
        const auto& child=target.skeleton.bones[shoulder->second];
        if(child.parent!=static_cast<std::int32_t>(clavicle->second)||!bindings[shoulder->second].source)continue;
        // Recover the missing legacy parent from the calibrated shoulder.
        // It must share ADS ownership too; leaving it in bind pose stretches
        // weighted collar vertices across the camera as the arms aim.
        bindings[clavicle->second]=bindings[shoulder->second];
        bindings[clavicle->second].offset=bindings[clavicle->second].offset*inverseAffine(trs(child.restLocal.position,child.restLocal.rotation,child.restLocal.scale));
    }
    for(const char* name:{"j_shoulder_le","j_shoulder_ri","j_wrist_le","j_wrist_ri","j_index_le_0","j_index_ri_0"}){
        if(iwTarget&&std::string_view(name).starts_with("j_index_")){
            const auto actual=target.skeleton.boneByName.find(std::string(name).substr(0,std::string(name).size()-1)+"1");
            if(actual==target.skeleton.boneByName.end()||!bindings[actual->second].source)return false;
            continue;
        }
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
    if(changed&&iwTarget)supportIwLegacySleeves(target);
    return changed;
}
}
