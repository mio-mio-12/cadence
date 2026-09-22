#pragma once
#include "scene/CodmLegacyAdapter.h"
#include <optional>
namespace scene::codm {
struct WorldMount {size_t bone{};Mat4 transform;};
inline constexpr const char* worldReferenceSuffixes[]={"_idle.cast","_weapon_idle.cast","_pose.cast","_for_pose.cast","_ads_idle.cast","_reload.cast","_reload_empty.cast"};
// World exports are actual metre-sized third-person geometry, not the smaller
// first-person art. Do not apply the viewmodel presentation scale to them.
// Use the matching native idle's right palm and shared authored grip socket
// to register the world receiver, then express that frame in the target wrist.
// Keep this as attachment TRS so take manifests reproduce it without re-fitting.
inline std::optional<WorldMount> worldMount(const cast::Document& worldDocument,const cast::Document& viewDocument,const cast::Document& idleDocument,const Skeleton& target,std::string& error,const Skeleton* palmReference=nullptr){
    try{
        if(!nativeMetres(worldDocument.sourceName(),error)||!nativeMetres(viewDocument.sourceName(),error))return {};
        auto world=buildScene(worldDocument,false),view=buildScene(viewDocument,false);
        normalizeToCentimetres(view,presentationScale);
        view.codmNativeWeaponStem=std::filesystem::path(viewDocument.sourceName()).stem().string();
        appendAnimations(idleDocument,view);
        if(view.animations.empty())throw std::runtime_error("Matching native CODM idle unavailable");
        const auto pose=view.samplePose(0,0);
        const auto find=[](const Skeleton&s,std::initializer_list<const char*> names)->size_t{for(auto n:names){if(auto it=s.boneByName.find(n);it!=s.boneByName.end())return it->second;if(auto it=s.boneByCanonicalName.find(n);it!=s.boneByCanonicalName.end())return it->second;}throw std::runtime_error(std::string("Missing world-grip landmark: ")+*names.begin());};
        const auto sw=find(view.skeleton,{"b_RightHand"}),si=find(view.skeleton,{"b_RightIndex1"}),sp=find(view.skeleton,{"b_RightPinky1"}),sm=find(view.skeleton,{"b_RightMiddle1"});
        const auto tw=find(target,{"j_wrist_ri","R Hand"});
        const auto vg=find(view.skeleton,{"Grip_point"}),wg=find(world.skeleton,{"Grip_point"});
        const auto pos=[](const Mat4&m){return transformPoint(m,{});};
        const auto s0=pos(pose[sw]),t0=pos(target.bones[tw].restGlobal);
        // Reload-only exports may use their start pose, but reject a detached
        // hand instead of silently calibrating to an arbitrary action frame.
        if(length(pos(pose[vg])-s0)>35.f)throw std::runtime_error("Native reference hand is detached from grip socket");
        const auto sourceFrame=anatomicalFrame(s0,pos(pose[sm]),cross(pos(pose[si])-s0,pos(pose[sp])-s0));
        const auto palm=[&](const Skeleton&s){const auto w=find(s,{"j_wrist_ri","R Hand"}),i=find(s,{"j_index_ri_0","R Index1","j_index_ri_1"}),p=find(s,{"j_pinky_ri_0","R Little1","j_pinky_ri_1"}),m=find(s,{"j_mid_ri_0","R Middle1","j_mid_ri_1"});const auto origin=pos(s.bones[w].restGlobal);return anatomicalFrame(origin,pos(s.bones[m].restGlobal),cross(pos(s.bones[i].restGlobal)-origin,pos(s.bones[p].restGlobal)-origin));};
        Mat4 targetFrame;
        try{targetFrame=palm(target);}catch(const std::runtime_error&){
            // Low-detail bodies have a rigid wrist but no phalanges. Borrow
            // only the donor palm's wrist-local frame, never its world position.
            if(!palmReference)throw;const auto rw=find(*palmReference,{"j_wrist_ri","R Hand"});
            targetFrame=target.bones[tw].restGlobal*inverseAffine(palmReference->bones[rw].restGlobal)*palm(*palmReference);
        }
        auto worldGrip=world.skeleton.bones[wg].restGlobal;
        for(int k:{12,13,14})worldGrip.v[k]*=100.f;
        // Socket scales can differ between the 1P and 3P prefabs (Locus does).
        // They are registration frames, not an instruction to resize world art.
        const auto rigid=[](const Mat4&m){Vec3 p,s;Quat q;decomposeAffine(m,p,q,s);return trs(p,q,{1,1,1});};
        const auto transform=inverseAffine(target.bones[tw].restGlobal)*targetFrame*inverseAffine(sourceFrame)*rigid(pose[vg])*inverseAffine(rigid(worldGrip))*scale({100,100,100});
        for(float v:transform.v)if(!std::isfinite(v))throw std::runtime_error("Non-finite CODM world mount");
        return WorldMount{tw,transform};
    }catch(const std::exception&e){error=e.what();return {};}
}
}
