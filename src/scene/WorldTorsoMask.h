#pragma once
#include "scene/CastScene.h"

namespace scene {
inline std::vector<bool> worldTorsoMask(const Skeleton& skeleton){
    std::vector<bool> roots(skeleton.bones.size()),result(skeleton.bones.size());
    for(const auto name:{"j_spinelower","j_spineupper","j_spine4","j_spine1","tag_torso","spine1","spine2","spine3"}){
        const auto it=skeleton.boneByCanonicalName.find(name);
        if(it!=skeleton.boneByCanonicalName.end()&&it->second<roots.size())roots[it->second]=true;
    }
    for(std::size_t b=0;b<result.size();++b){
        auto p=static_cast<int>(b);
        for(std::size_t depth=0;depth<result.size()&&p>=0&&static_cast<std::size_t>(p)<result.size();++depth){
            if(roots[p]){result[b]=true;break;}p=skeleton.bones[p].parent;
        }
    }
    return result;
}
}
