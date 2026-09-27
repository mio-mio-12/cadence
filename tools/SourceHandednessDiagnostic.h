#pragma once
#include "scene/ImportedWorldGrip.h"

namespace diagnostic {
// Explicit construction experiment, never called by production loaders.
// The plane is the normalized native view's sagittal Y=0 plane.
inline float determinant3(const scene::Mat4& m) {
    return scene::dot(scene::Vec3{m.v[0],m.v[1],m.v[2]},scene::cross(
        scene::Vec3{m.v[4],m.v[5],m.v[6]},scene::Vec3{m.v[8],m.v[9],m.v[10]}));
}
inline bool reflectNativeConstruction(scene::CastScene& model,std::vector<scene::Mat4>& pose) {
    if(pose.size()!=model.skeleton.bones.size()||model.dualWield||!model.attachments.empty()||model.codmRigAdapter||!model.runtimePoseAdapters.empty())return false;
    auto copy=model;auto sampled=pose;auto h=scene::Mat4::identity();h.v[5]=-1;
    for(auto& mesh:copy.meshes){
        if(mesh.indices.size()%3)return false;
        for(auto& v:mesh.vertices){v.position.y=-v.position.y;v.normal.y=-v.normal.y;}
        for(std::size_t i=0;i<mesh.indices.size();i+=3)std::swap(mesh.indices[i+1],mesh.indices[i+2]);
        mesh.modelTransform=h*mesh.modelTransform*h;
        if(mesh.bounds.valid){const float lo=mesh.bounds.minimum.y;mesh.bounds.minimum.y=-mesh.bounds.maximum.y;mesh.bounds.maximum.y=-lo;}
    }
    copy.skeleton.boneByName.clear();copy.skeleton.boneByCanonicalName.clear();
    for(std::size_t i=0;i<copy.skeleton.bones.size();++i){auto& b=copy.skeleton.bones[i];
        b.restGlobal=h*b.restGlobal*h;b.inverseBind=h*b.inverseBind*h;sampled[i]=h*sampled[i]*h;
        if(!scene::imported::world_grip::finite(sampled[i])||!scene::imported::world_grip::finite(b.restGlobal)||determinant3(sampled[i])<=0||determinant3(b.restGlobal)<=0||b.parent>=static_cast<int>(model.skeleton.bones.size()))return false;
        const auto local=b.parent<0?b.restGlobal:scene::inverseAffine(h*model.skeleton.bones[b.parent].restGlobal*h)*b.restGlobal;
        scene::decomposeAffine(local,b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);
        b.absoluteTranslationOffset.y=-b.absoluteTranslationOffset.y;
        if(b.name.starts_with("cs_hand_L."))b.name[8]='R';
        else if(b.name.starts_with("cs_hand_R."))b.name[8]='L';
        copy.skeleton.boneByName[b.name]=i;copy.skeleton.boneByCanonicalName[scene::canonicalName(b.name)]=i;
    }
    for(auto& a:copy.muzzleAnchors)a.local.y=-a.local.y;
    for(auto& part:copy.rigParts)part.muzzleOffset=h*part.muzzleOffset*h;
    for(auto& [index,t]:copy.rigMountReferences){const auto m=h*scene::trs(t.position,t.rotation,t.scale)*h;scene::decomposeAffine(m,t.position,t.rotation,t.scale);}
    if(copy.bounds.valid){const float lo=copy.bounds.minimum.y;copy.bounds.minimum.y=-copy.bounds.maximum.y;copy.bounds.maximum.y=-lo;}
    // Only sampled frame-zero data is valid; stale unreflected curve caches
    // must not accidentally be evaluated by this standalone experiment.
    copy.animations.clear();
    model=std::move(copy);pose=std::move(sampled);return true;
}
}
