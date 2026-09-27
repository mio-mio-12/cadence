#pragma once
#include "scene/CastScene.h"

namespace cadence::body_donor {
inline std::vector<bool> requiredBones(const scene::CastScene& body){
    const auto& bones=body.skeleton.bones;std::vector<bool> required(bones.size());
    for(const auto& mesh:body.meshes)if(mesh.skinned)for(const auto& vertex:mesh.vertices)
        for(std::size_t influence=0;influence<vertex.weights.size();++influence)if(vertex.weights[influence]>0){
            auto index=static_cast<int>(vertex.bones[influence]);
            for(std::size_t depth=0;depth<bones.size()&&index>=0&&static_cast<std::size_t>(index)<bones.size();++depth){
                if(required[index])break;required[index]=true;index=bones[index].parent;
            }
        }
    return required;
}
// Only donor selection may use this equivalence. Direct track reuse still
// requires identical indices; differing export orders must go through retargeting.
inline bool equivalent(const scene::Skeleton& a,const std::vector<bool>& aRequired,
                       const scene::Skeleton& b,const std::vector<bool>& bRequired){
    if(aRequired.size()!=a.bones.size()||bRequired.size()!=b.bones.size())return false;
    const auto direction=[](const scene::Skeleton& from,const std::vector<bool>& required,const scene::Skeleton& to){
        for(std::size_t i=0;i<from.bones.size();++i){const auto& x=from.bones[i];const auto found=to.boneByName.find(x.name);
            if(found==to.boneByName.end()){if(required[i])return false;continue;}
            if(found->second>=to.bones.size())return false;const auto& y=to.bones[found->second];
            if((x.parent<0)!=(y.parent<0))return false;
            if(x.parent>=0&&(static_cast<std::size_t>(x.parent)>=from.bones.size()||static_cast<std::size_t>(y.parent)>=to.bones.size()||from.bones[x.parent].name!=to.bones[y.parent].name))return false;
            for(std::size_t k=0;k<16;++k)if(!std::isfinite(x.restGlobal.v[k])||!std::isfinite(y.restGlobal.v[k])||!std::isfinite(x.inverseBind.v[k])||!std::isfinite(y.inverseBind.v[k])||std::abs(x.restGlobal.v[k]-y.restGlobal.v[k])>1e-4f||std::abs(x.inverseBind.v[k]-y.inverseBind.v[k])>1e-4f)return false;
        }return true;
    };
    return direction(a,aRequired,b)&&direction(b,bRequired,a);
}
}
