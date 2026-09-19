#pragma once
#include "scene/CastScene.h"
#include "scene/NativeBoneNames.h"

namespace scene {
// Immutable native animation and bind-frame mapping. No CAST rewriting,
// mutable shared sampling state, or fitting/file access during playback.
struct ColdWarWorldPose {
    std::shared_ptr<const CastScene> source;
    std::size_t animation{};
    std::vector<int> sourceBones;
    std::vector<Quat> rotationOffsets;
    std::vector<Quat> translationFrames;
    std::vector<Transform> targetReference;
    // Collapsed hierarchy joints need the complete source-parent displacement.
    // Empty for existing adapters, so their behavior is unchanged.
    std::vector<bool> globalTranslation;
    std::vector<Mat4> sourceActionReference,targetActionReference;
    Quat actionBasis{};
    // Immutable import-time data; fallback supports older manually built adapters.
    std::vector<unsigned char> boneFlags;
    std::vector<Mat4> translationMatrices,sourceActionInverse;
    Mat4 cachedActionBasis{},cachedActionBasisInverse{};
    void prepareSampling(const Skeleton& target) {
        boneFlags.clear();translationMatrices.clear();sourceActionInverse.clear();
        for(const auto& b:target.bones){
            const auto name=canonicalName(b.name);
            boneFlags.push_back(((name=="tag_weapon_right"||name=="tag_weapon_left")?1:0) |
                ((b.parent<0||name=="j_mainroot"||name=="j_hips"||name=="pelvis"||name.starts_with("tag_"))?2:0));
        }
        for(const auto& q:translationFrames)translationMatrices.push_back(trs({},q,{1,1,1}));
        cachedActionBasis=rotation(actionBasis);cachedActionBasisInverse=inverseAffine(cachedActionBasis);
        for(const auto& m:sourceActionReference)sourceActionInverse.push_back(inverseAffine(m));
    }
    float deathGroundLift{}; // Precomputed PB corpse contact, never fitted per frame.
    std::vector<Transform> sample(const Skeleton& target,float frame) const {
        auto local=targetReference;
        const auto finish=[&](){
            if(deathGroundLift!=0){const float duration=std::max(1.f,float(source->animations[animation].durationFrames));const float t=std::clamp((frame/duration-.35f)/.65f,0.f,1.f);const float lift=deathGroundLift*t*t*(3-2*t);for(size_t b=0;b<local.size();++b)if(target.bones[b].parent<0)local[b].position.z+=lift;}
            return local;
        };
        if(target.bones.size()<local.size()){local.clear();for(const auto&b:target.bones)local.push_back(b.restLocal);return local;}
        for(std::size_t i=local.size();i<target.bones.size();++i)local.push_back(target.bones[i].restLocal);
        const auto sourceLocal=source->sampleLocalPose(animation,frame);
        const auto sourceGlobal=source->globalPose(sourceLocal);
        if(!sourceActionReference.empty()){
            std::vector<Mat4> globals(local.size());
            for(size_t i=0;i<local.size();++i){const auto parent=target.bones[i].parent;const auto pg=parent>=0?globals[parent]:Mat4::identity();const int s=i<sourceBones.size()?sourceBones[i]:-1;
                if(s>=0&&i<targetActionReference.size()){
                    Vec3 p,scale;Quat q;decomposeAffine(sourceGlobal[s],p,q,scale);
                    const auto desired=multiply(multiply(actionBasis,q),rotationOffsets[i]);Vec3 pp,ps;Quat pq;decomposeAffine(pg,pp,pq,ps);
                    local[i].rotation=normalize(multiply(Quat{-pq.x,-pq.y,-pq.z,pq.w},desired));
                    if(i<globalTranslation.size()&&globalTranslation[i]){
                        // Rotate the fitted offset too. A translation-only delta
                        // breaks weapon/wrist contact when the donor rotates.
                        const auto basis=sourceActionInverse.empty()?rotation(actionBasis):cachedActionBasis;
                        const auto fitted=basis*sourceGlobal[s]*(sourceActionInverse.empty()?inverseAffine(sourceActionReference[s]):sourceActionInverse[s])*(sourceActionInverse.empty()?inverseAffine(basis):cachedActionBasisInverse)*targetActionReference[i];
                        local[i].position=transformPoint(inverseAffine(pg),transformPoint(fitted,{}));
                    }
                }
                globals[i]=pg*trs(local[i].position,local[i].rotation,local[i].scale);
            }return finish();
        }
        std::vector<Mat4> globals(local.size());
        for(std::size_t i=0;i<local.size();++i){
            const auto parent=target.bones[i].parent;
            const auto parentGlobal=parent>=0?globals[parent]:Mat4::identity();
            const auto mapped=i<sourceBones.size()?sourceBones[i]:-1;
            if(mapped>=0){
                const auto name=i<boneFlags.size()?std::string{}:canonicalName(target.bones[i].name);
                const bool socket=i<boneFlags.size()?(boneFlags[i]&1)!=0:(name=="tag_weapon_right"||name=="tag_weapon_left");
                const bool translate=i<boneFlags.size()?(boneFlags[i]&2)!=0:(parent<0||name=="j_mainroot"||name=="j_hips"||name=="pelvis"||name.starts_with("tag_"));
                // Weapon sockets are authored mount frames, not anatomical
                // joints. A legacy bind socket can be far from the actual
                // grip. Anchor the native animated socket at the fitted wrist,
                // keeping its native model axes instead of a bind-pose delta.
                const auto sourceParent=source->skeleton.bones[mapped].parent;
                if(socket&&parent>=0&&
                   sourceParent>=0&&static_cast<std::size_t>(parent)<sourceBones.size()&&sourceBones[parent]==sourceParent){
                    auto desired=sourceGlobal[mapped];
                    for(int axis=12;axis<=14;++axis)desired.v[axis]+=parentGlobal.v[axis]-sourceGlobal[sourceParent].v[axis];
                    decomposeAffine(inverseAffine(parentGlobal)*desired,local[i].position,local[i].rotation,local[i].scale);
                    globals[i]=parentGlobal*trs(local[i].position,local[i].rotation,local[i].scale);
                    continue;
                }
                Vec3 p,s;Quat q;decomposeAffine(sourceGlobal[mapped],p,q,s);
                const auto desired=multiply(q,rotationOffsets[i]);
                Vec3 pp,ps;Quat pq;decomposeAffine(parentGlobal,pp,pq,ps);
                local[i].rotation=normalize(multiply(Quat{-pq.x,-pq.y,-pq.z,pq.w},desired));
                if(i<globalTranslation.size()&&globalTranslation[i]){
                    const auto reference=transformPoint(target.bones[i].restGlobal,{});
                    const auto delta=p-transformPoint(source->skeleton.bones[mapped].restGlobal,{});
                    local[i].position=transformPoint(inverseAffine(parentGlobal),reference+delta);
                    globals[i]=parentGlobal*trs(local[i].position,local[i].rotation,local[i].scale);
                    continue;
                }
                // Root/pelvis retain authored translation. Limb lengths stay
                // target-authored; source global orientations include all helpers.
                const bool duplicateParent=parent>=0&&static_cast<std::size_t>(parent)<sourceBones.size()&&sourceBones[parent]==mapped;
                if(!duplicateParent&&translate){
                    const auto& bind=source->skeleton.bones[mapped].restLocal;
                    const auto delta=sourceLocal[mapped].position-bind.position;
                    local[i].position=local[i].position+transformPoint(i<translationMatrices.size()?translationMatrices[i]:trs({},translationFrames[i],{1,1,1}),delta);
                }
            }
            globals[i]=parentGlobal*trs(local[i].position,local[i].rotation,local[i].scale);
        }
        return finish();
    }
};

inline void addColdWarWorldAliases(Skeleton& skeleton){
    const auto add=[&](const std::string& name){auto found=skeleton.boneByName.find(nativeBoneHash(name));if(found!=skeleton.boneByName.end()){skeleton.boneByCanonicalName[name]=found->second;if(name.starts_with("j_"))skeleton.boneByCanonicalName[name.substr(2)]=found->second;}};
    for(const auto* side:{"le","ri"}){
        for(const auto* joint:{"hip","knee","ankle","ball","hiptwist","knee_bulge","shoulderraise","shouldertwist","elbow_bulge","wristtwist"})add("j_"+std::string(joint)+"_"+side);
        for(const auto* finger:{"index","mid","pinky","ring","thumb"})for(int n=0;n<=3;++n)add("j_"+std::string(finger)+"_"+side+"_"+std::to_string(n));
    }
}
inline std::shared_ptr<const ColdWarWorldPose> makeColdWarWorldPose(const Skeleton& target,std::shared_ptr<const CastScene> source,std::size_t animation){
    if(!source||animation>=source->animations.size())return {};
    auto adapter=std::make_shared<ColdWarWorldPose>();adapter->source=std::move(source);adapter->animation=animation;
    const auto& native=adapter->source->skeleton;
    for(const auto& bone:target.bones){
        const auto name=canonicalName(bone.name);int mapped=-1;
        if(auto found=native.boneByCanonicalName.find(name);found!=native.boneByCanonicalName.end())mapped=static_cast<int>(found->second);
        else if(auto hashed=native.boneByName.find(nativeBoneHash(name));hashed!=native.boneByName.end())mapped=static_cast<int>(hashed->second);
        adapter->sourceBones.push_back(mapped);adapter->targetReference.push_back(bone.restLocal);
        Quat offset,translationFrame;
        if(mapped>=0){Vec3 p,s;Quat from,to;decomposeAffine(native.bones[mapped].restGlobal,p,from,s);decomposeAffine(bone.restGlobal,p,to,s);offset=normalize(multiply(Quat{-from.x,-from.y,-from.z,from.w},to));
            const auto sp=native.bones[mapped].parent;decomposeAffine(sp>=0?native.bones[sp].restGlobal:Mat4::identity(),p,from,s);decomposeAffine(bone.parent>=0?target.bones[bone.parent].restGlobal:Mat4::identity(),p,to,s);translationFrame=normalize(multiply(Quat{-to.x,-to.y,-to.z,to.w},from));
        }
        adapter->rotationOffsets.push_back(offset);
        adapter->translationFrames.push_back(translationFrame);
    }
    adapter->prepareSampling(target);
    return adapter;
}
}
