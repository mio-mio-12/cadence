#pragma once
#include "scene/ColdWarWorld.h"
#include "scene/CodmWorldBody.h"
#include <map>
namespace scene::pointblank {
inline bool body(const Skeleton& s){return s.boneByName.contains("Pelvis")&&s.boneByName.contains("L Thigh")&&s.boneByName.contains("R Thigh");}
inline std::string worldSemantic(std::string name){
    // World-hand phalanges are 1-based in the COD donor rigs (not viewhand 0-based chains).
    if(const auto codmName=codm::worldSemantic(name);codmName!=name)return codmName;
    static const auto names=[](){std::map<std::string,std::string> m{{"Root","tag_origin"},{"Pelvis","j_mainroot"},{"Spine1","j_spinelower"},{"Spine2","j_spineupper"},{"Spine3","j_spine4"},{"Neck","j_neck"},{"Head","j_head"}};
        for(auto side:{std::pair{"L ","le"},std::pair{"R ","ri"}}){for(auto part:{std::pair{"Clavicle","clavicle"},std::pair{"UpperArm","shoulder"},std::pair{"Forearm","elbow"},std::pair{"Hand","wrist"},std::pair{"Thigh","hip"},std::pair{"Calf","knee"},std::pair{"Foot","ankle"},std::pair{"Toe","ball"}})m[std::string(side.first)+part.first]="j_"+std::string(part.second)+"_"+side.second;
            for(auto finger:{std::pair{"Thumb","thumb"},std::pair{"Index","index"},std::pair{"Middle","mid"},std::pair{"Ring","ring"},std::pair{"Little","pinky"}})for(int n=1;n<=3;++n)m[std::string(side.first)+finger.first+std::to_string(n)]="j_"+std::string(finger.second)+"_"+side.second+"_"+std::to_string(n);
        }return m;}();
    if(auto i=names.find(name);i!=names.end())return i->second;return canonicalName(name);
}
inline void worldAliases(Skeleton& s){if(!body(s))return;for(size_t i=0;i<s.bones.size();++i)s.boneByCanonicalName[worldSemantic(s.bones[i].name)]=i;}
inline void overlayTorso(const CastScene& actor,size_t clip,float frame,std::vector<Transform>& base){
    if(clip>=actor.animations.size())return;const auto pose=actor.sampleLocalPose(clip,frame);
    for(const auto& t:actor.animations[clip].tracks){const auto b=t.boneIndex;if(!t.ownsLayer||b>=base.size()||b>=pose.size())continue;switch(t.property){case TrackProperty::Rotation:base[b].rotation=pose[b].rotation;break;case TrackProperty::TranslationX:base[b].position.x=pose[b].position.x;break;case TrackProperty::TranslationY:base[b].position.y=pose[b].position.y;break;case TrackProperty::TranslationZ:base[b].position.z=pose[b].position.z;break;case TrackProperty::ScaleX:base[b].scale.x=pose[b].scale.x;break;case TrackProperty::ScaleY:base[b].scale.y=pose[b].scale.y;break;case TrackProperty::ScaleZ:base[b].scale.z=pose[b].scale.z;break;default:break;}}
}
inline void bridgeWorld(CastScene& target,size_t first,std::shared_ptr<const CastScene> source,size_t sourceFirst){
    auto semanticTarget=target.skeleton;for(auto& b:semanticTarget.bones){const auto original=b.name;b.name=worldSemantic(original);
        // BO2 separates mainroot translation from pelvis rotation; PB collapses
        // those into its pelvis chain. Use the complete pelvis pose, not just mainroot.
        if(original=="Pelvis"&&source->skeleton.boneByCanonicalName.contains("pelvis"))b.name="pelvis";
        if(original=="L Twist1")b.name="j_elbow_le";if(original=="R Twist1")b.name="j_elbow_ri";
    }
    for(size_t i=first;i<target.animations.size()&&sourceFirst+i-first<source->animations.size();++i){
        const auto si=sourceFirst+i-first;auto adapter=std::make_shared<ColdWarWorldPose>(*makeColdWarWorldPose(semanticTarget,source,si));adapter->globalTranslation.resize(target.skeleton.bones.size());
        if(codm::worldBody(target.skeleton)){
            // Match anatomical segment directions, not A-pose local angles.
            // Mesh bind data stays native; the runtime pose supplies the fit.
            const auto arc=[](Vec3 from,Vec3 to){from=normalize(from);to=normalize(to);const float d=std::clamp(dot(from,to),-1.f,1.f);if(d<-.9999f){auto axis=cross(from,Vec3{0,0,1});if(length(axis)<.001f)axis=cross(from,Vec3{0,1,0});axis=normalize(axis);return Quat{axis.x,axis.y,axis.z,0};}const auto c=cross(from,to);return normalize(Quat{c.x,c.y,c.z,1+d});};
            for(size_t b=0;b<target.skeleton.bones.size();++b){const auto driver=adapter->sourceBones[b];if(driver<0)continue;const auto& name=semanticTarget.bones[b].name;std::string child;
                for(const auto* side:{"le","ri"})for(auto pair:{std::pair{"shoulder","elbow"},std::pair{"elbow","wrist"},std::pair{"hip","knee"},std::pair{"knee","ankle"},std::pair{"wrist","mid"}}){if(name=="j_"+std::string(pair.first)+"_"+side)child="j_"+std::string(pair.second)+"_"+side+(std::string(pair.first)=="wrist"?"_1":"");}
                if(child.empty())continue;const auto tb=target.skeleton.boneByCanonicalName.find(child);const auto sb=source->skeleton.boneByCanonicalName.find(child);if(tb==target.skeleton.boneByCanonicalName.end()||sb==source->skeleton.boneByCanonicalName.end())continue;
                const auto tdir=transformPoint(target.skeleton.bones[tb->second].restGlobal,{})-transformPoint(target.skeleton.bones[b].restGlobal,{}),sdir=transformPoint(source->skeleton.bones[sb->second].restGlobal,{})-transformPoint(source->skeleton.bones[driver].restGlobal,{});if(length(tdir)<.001f||length(sdir)<.001f)continue;
                Vec3 p,scale;Quat from,to;decomposeAffine(source->skeleton.bones[driver].restGlobal,p,from,scale);decomposeAffine(target.skeleton.bones[b].restGlobal,p,to,scale);
                adapter->rotationOffsets[b]=normalize(multiply(Quat{-from.x,-from.y,-from.z,from.w},multiply(arc(tdir,sdir),to)));
            }
        }
        for(size_t b=0;b<target.skeleton.bones.size();++b)adapter->globalTranslation[b]=target.skeleton.bones[b].name=="Pelvis";
        auto& a=target.animations[i];a=source->animations[si];a.coldWarWorldPose=adapter;a.tracks.clear();
        std::vector<bool> upper(target.skeleton.bones.size());
        for(size_t b=0;b<upper.size();++b){const auto name=worldSemantic(target.skeleton.bones[b].name);const auto p=target.skeleton.bones[b].parent;upper[b]=name.starts_with("j_spine")||name=="j_neck"||name.starts_with("j_clavicle")||(p>=0&&upper[p]);}
        // The adapter evaluates global source frames, so local source tracks
        // are NOT the target's channel mask (notably T6 mainroot -> PB pelvis).
        // Publish the actual evaluated channels for blending and SP overlays.
        std::vector<bool> driven(source->skeleton.bones.size());
        for(const auto& t:source->animations[si].tracks)if(t.ownsLayer&&t.boneIndex<driven.size())driven[t.boneIndex]=true;
        for(size_t b=0;b<driven.size();++b){const int p=source->skeleton.bones[b].parent;if(p>=0)driven[b]=driven[b]||driven[p];}
        for(size_t b=0;b<adapter->sourceBones.size();++b){const auto driver=adapter->sourceBones[b];if(driver<0)continue;
            const bool owns=driven[driver]&&(a.domain!=AnimationDomain::PlayerTorso||upper[b]);
            auto mask=[&](TrackProperty property){Track t;t.boneIndex=b;t.property=property;t.ownsLayer=owns;t.frames={0};if(property==TrackProperty::Rotation)t.rotationValues={target.skeleton.bones[b].restLocal.rotation};else t.scalarValues={0};a.tracks.push_back(std::move(t));};
            mask(TrackProperty::Rotation);
            const auto name=semanticTarget.bones[b].name;
            if(adapter->globalTranslation[b]||target.skeleton.bones[b].parent<0||name=="j_mainroot"||name=="j_hips"||name=="pelvis"||name.starts_with("tag_"))for(auto p:{TrackProperty::TranslationX,TrackProperty::TranslationY,TrackProperty::TranslationZ})mask(p);
        }
        adapter->prepareSampling(semanticTarget);
        if(body(source->skeleton)&&a.action==ActionRole::Death){
            // Source corpses can penetrate their authored floor, especially
            // across male/female proportions. Measure once at import, then
            // ease the small contact correction into the end of the fall.
            const auto finalPose=target.samplePose(i,float(a.durationFrames));
            std::vector<Mat4> skin(finalPose.size());for(size_t b=0;b<skin.size();++b)skin[b]=finalPose[b]*target.skeleton.bones[b].inverseBind;
            float minimum=1e9f,bindMinimum=1e9f;
            for(const auto& mesh:target.meshes)if(mesh.skinned&&mesh.attachmentIndex<0)for(const auto& v:mesh.vertices){
                Vec3 p{};float weight=0;
                for(size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0&&v.bones[k]<finalPose.size()){p+=transformPoint(skin[v.bones[k]],v.position)*v.weights[k];weight+=v.weights[k];}
                if(weight>.99f){minimum=std::min(minimum,p.z);bindMinimum=std::min(bindMinimum,v.position.z);}
            }
            if(minimum<1e8f&&bindMinimum<1e8f)adapter->deathGroundLift=std::clamp(bindMinimum-minimum,-30.f,30.f);
        }
    }
}
}
