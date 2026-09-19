#pragma once
#include "scene/CastScene.h"
#include <algorithm>
#include <cctype>

namespace scene {
// T9 campaign arm skins retain the character hierarchy. Native VM clips use
// the separate view/ADS/torso hierarchy shown by the T9 side of t9-iw3.ma.
// Preserve every skin bind matrix and limb length; only detach the VM anchors
// from the body chain. This is native presentation, not cross-game retargeting.
inline bool prepareColdWarViewmodel(CastScene& scene,std::string modelName){
    std::transform(modelName.begin(),modelName.end(),modelName.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(!modelName.starts_with("c_t9_")||modelName.find("_arms_")==std::string::npos)return false;
    auto& s=scene.skeleton;
    if(s.boneByCanonicalName.contains("tag_view")||!scene.animations.empty()||!scene.rigParts.empty())return false;
    for(const char* name:{"tag_camera","j_head","j_spine4","j_clavicle_le","j_clavicle_ri","j_wrist_le","j_wrist_ri","tag_weapon_left","tag_weapon_right"})
        if(!s.boneByCanonicalName.contains(name))return false;
    const auto parentIs=[&](const char* bone,const char* parent){return s.bones[s.boneByCanonicalName.at(bone)].parent==static_cast<int>(s.boneByCanonicalName.at(parent));};
    if(!parentIs("tag_camera","j_head")||!parentIs("j_clavicle_le","j_spine4")||!parentIs("j_clavicle_ri","j_spine4")||!parentIs("tag_weapon_right","j_wrist_ri")||!parentIs("tag_weapon_left","j_wrist_le"))return false;
    const auto camera=s.bones[s.boneByCanonicalName.at("tag_camera")].restGlobal;
    const Vec3 origin{camera.v[12],camera.v[13],camera.v[14]};
    const auto view=translation(origin),inverseView=inverseAffine(view);
    const auto originalCount=s.bones.size();
    s.bones.insert(s.bones.begin(),3,Bone{});
    for(std::size_t i=3;i<s.bones.size();++i)if(s.bones[i].parent>=0)s.bones[i].parent+=3;
    for(auto& [name,index]:s.boneByName)index+=3;
    for(auto& [name,index]:s.boneByCanonicalName)index+=3;
    for(int i=0;i<3;++i){auto& b=s.bones[i];b.name=i==0?"tag_view":i==1?"tag_ads":"tag_torso";b.parent=i-1;b.restLocal.position=i==0?origin:Vec3{};b.restGlobal=view;b.inverseBind=inverseView;s.boneByName[b.name]=s.boneByCanonicalName[b.name]=i;}
    for(const char* name:{"j_clavicle_le","j_clavicle_ri","tag_weapon_left","tag_weapon_right","tag_accessory_left","tag_accessory_right","tag_camera"}){
        const auto it=s.boneByCanonicalName.find(name);if(it==s.boneByCanonicalName.end())continue;
        auto& b=s.bones[it->second];b.parent=std::string_view(name)=="tag_camera"?0:2;
        decomposeAffine(inverseView*b.restGlobal,b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);
    }
    for(auto& mesh:scene.meshes)if(mesh.skinned)for(auto& vertex:mesh.vertices)for(auto& bone:vertex.bones)if(bone<originalCount)bone+=3;
    return true;
}
}
