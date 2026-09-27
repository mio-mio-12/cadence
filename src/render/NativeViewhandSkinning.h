#pragma once
#include "scene/CastScene.h"
#include <algorithm>

namespace render {
// Native Halo FP exports place forearm pronation on the wrist. Transfer its
// axial roll to the forearm's SKIN matrix only. Palm/finger/socket matrices,
// animation data and recorded poses remain untouched (including old takes).
inline void nativeViewhandSkinning(const scene::CastScene& scene,
                                  const std::vector<scene::Mat4>& pose,
                                  std::vector<scene::Mat4>& skin) {
    if(scene.importedViewGame!="eldewrito"||!scene.viewHandsDriverGame.empty())return;
    for(const auto* side:{"l_","r_"}){
        const auto hand=scene.skeleton.boneByName.find(std::string(side)+"hand");
        const auto forearm=scene.skeleton.boneByName.find(std::string(side)+"forearm");
        if(hand==scene.skeleton.boneByName.end()||forearm==scene.skeleton.boneByName.end())continue;
        const auto h=hand->second,f=forearm->second;
        if(h>=pose.size()||f>=pose.size()||f>=skin.size()||h>=scene.skeleton.bones.size()||f>=scene.skeleton.bones.size()||scene.skeleton.bones[h].parent!=static_cast<int>(f))continue;
        const auto& bind=scene.skeleton.bones[h].restLocal;
        if(scene::length(bind.position)<1e-5f)continue;
        const auto axis=scene::normalize(bind.position);
        scene::Vec3 position,scale;scene::Quat rotation;
        scene::decomposeAffine(scene::inverseAffine(pose[f])*pose[h],position,rotation,scale);
        const auto delta=scene::multiply(rotation,scene::Quat{-bind.rotation.x,-bind.rotation.y,-bind.rotation.z,bind.rotation.w});
        const float along=delta.x*axis.x+delta.y*axis.y+delta.z*axis.z;
        const float norm=std::sqrt(along*along+delta.w*delta.w);
        // A 180-degree swing perpendicular to the forearm has no unique twist.
        if(!std::isfinite(norm)||norm<1e-5f)continue;
        const scene::Quat twist{axis.x*along/norm,axis.y*along/norm,axis.z*along/norm,delta.w/norm};
        const auto corrected=pose[f]*scene::trs({},twist,{1,1,1})*scene.skeleton.bones[f].inverseBind;
        if(std::all_of(corrected.v.begin(),corrected.v.end(),[](float v){return std::isfinite(v);}))skin[f]=corrected;
    }
}
}
