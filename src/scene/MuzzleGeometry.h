#pragma once
#include "scene/CastScene.h"
#include <algorithm>
#include <limits>
#include <map>

namespace scene::imported {
// For exports with no firing socket: estimate the bore exit from the front
// slice of weapon geometry, in a verified native forward axis. Arm influences
// are excluded by the caller. This runs at import, never once per shot.
inline std::optional<MuzzleAnchor> geometryMuzzle(const CastScene& scene,Vec3 forward,const std::vector<bool>& excluded,const std::vector<Mat4>* pose=nullptr){
    forward=normalize(forward);
    const auto position=[&](const Mesh& mesh,const Vertex& v){
        if(!pose)return transformPoint(mesh.modelTransform,v.position);
        Vec3 p{};float weight{};
        for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0&&v.bones[k]<pose->size()&&v.bones[k]<scene.skeleton.bones.size()){
            p+=transformPoint((*pose)[v.bones[k]]*scene.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];weight+=v.weights[k];
        }
        return transformPoint(mesh.modelTransform,weight>0?p/weight:v.position);
    };
    const auto owner=[&](const Vertex& v)->std::optional<std::size_t>{
        const auto k=std::max_element(v.weights.begin(),v.weights.end())-v.weights.begin();
        if(v.weights[k]<=0||v.bones[k]>=scene.skeleton.bones.size())return {};
        for(std::size_t j=0;j<v.weights.size();++j)if(v.weights[j]>0&&v.bones[j]<excluded.size()&&excluded[v.bones[j]])return {};
        return v.bones[k];
    };
    float front=-std::numeric_limits<float>::max(),rear=-front;
    for(const auto& m:scene.meshes)for(const auto& v:m.vertices)if(owner(v)){
        const float distance=dot(position(m,v),forward);
        if(std::isfinite(distance)){front=std::max(front,distance);rear=std::min(rear,distance);}
    }
    if(!std::isfinite(front)||front-rear<.01f)return {};
    const float slice=std::max(.025f,(front-rear)*.003f);
    std::map<std::size_t,std::pair<Vec3,std::size_t>> groups;
    for(const auto& m:scene.meshes)for(const auto& v:m.vertices)if(const auto bone=owner(v)){
        const auto p=position(m,v);if(dot(p,forward)<front-slice)continue;
        auto& group=groups[*bone];group.first+=p;++group.second;
    }
    if(groups.empty())return {};
    const auto chosen=std::max_element(groups.begin(),groups.end(),[](const auto& a,const auto& b){return a.second.second<b.second.second;});
    auto tip=chosen->second.first/float(chosen->second.second);tip+=forward*(front-dot(tip,forward));
    const auto inverse=pose&&chosen->first<pose->size()?inverseAffine((*pose)[chosen->first]):scene.skeleton.bones[chosen->first].inverseBind;
    return MuzzleAnchor{chosen->first,transformPoint(inverse,tip),0};
}
}
