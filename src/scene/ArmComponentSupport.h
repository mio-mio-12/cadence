#pragma once
#include "scene/ImportedViewRetarget.h"
#include <map>
#include <numeric>
#include <tuple>

namespace scene::imported {
// Opt-in pre-fit diagnostic. No game, mesh, bone-name or weapon special cases.
// Retain ancestor-weighted cloth only when the entire welded component has
// substantial support from exactly one anatomical arm. Never map torso globally.
inline std::size_t retainArmComponentSupport(CastScene& skin){
    const auto layout=viewLayout(skin);
    const auto& bones=skin.skeleton.bones;
    const auto below=[&](int bone,int ancestor){
        for(std::size_t n=0;bone>=0&&static_cast<std::size_t>(bone)<bones.size()&&n<bones.size();++n){
            if(bone==ancestor)return true;
            bone=bones[bone].parent;
        }
        return false;
    };
    std::array<int,2> shoulders{layout.arms[0].shoulder,layout.arms[1].shoulder},roots=shoulders;
    for(int side=0;side<2;++side){
        if(shoulders[side]<0||static_cast<std::size_t>(shoulders[side])>=bones.size())return 0;
        const int parent=bones[shoulders[side]].parent;
        if(parent>=0&&static_cast<std::size_t>(parent)<bones.size()&&!below(shoulders[1-side],parent))roots[side]=parent;
    }
    if(below(roots[0],roots[1])||below(roots[1],roots[0]))return 0;
    std::size_t changed{};
    for(auto& mesh:skin.meshes){
        if(!mesh.skinned||mesh.viewmodelWeapon||mesh.indices.empty()||mesh.indices.size()%3)continue;
        const auto count=mesh.vertices.size();bool valid=true;
        for(auto index:mesh.indices)if(index>=count)valid=false;
        for(const auto& vertex:mesh.vertices){
            const auto p=vertex.position;
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
               std::abs(p.x)>1e9f||std::abs(p.y)>1e9f||std::abs(p.z)>1e9f)valid=false;
            float sum{};
            for(int k=0;k<4;++k){const auto w=vertex.weights[k];
                if(!std::isfinite(w)||w<0||(w>0&&vertex.bones[k]>=bones.size()))valid=false;
                sum+=w;
            }
            if(!std::isfinite(sum)||std::abs(sum-1)>1e-3f)valid=false;
        }
        if(!valid)continue;
        std::vector<std::size_t> parent(count);std::iota(parent.begin(),parent.end(),0);
        const auto root=[&](std::size_t i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
        const auto join=[&](std::size_t a,std::size_t b){parent[root(a)]=root(b);};
        std::map<std::tuple<long long,long long,long long>,std::size_t> positions;
        std::vector<bool> referenced(count);
        for(auto index:mesh.indices)referenced[index]=true;
        for(std::size_t i=0;i<count;++i)if(referenced[i]){
            const auto p=mesh.vertices[i].position;
            const auto key=std::make_tuple(std::llround(double(p.x)*1e5),std::llround(double(p.y)*1e5),std::llround(double(p.z)*1e5));
            auto [it,inserted]=positions.emplace(key,i);if(!inserted)join(i,it->second);
        }
        for(std::size_t i=0;i<mesh.indices.size();i+=3){join(mesh.indices[i],mesh.indices[i+1]);join(mesh.indices[i],mesh.indices[i+2]);}
        std::map<std::size_t,std::vector<std::size_t>> components;
        for(std::size_t i=0;i<count;++i)if(referenced[i])components[root(i)].push_back(i);
        for(const auto& [key,vertices]:components){
            unsigned sides{};bool supported=true;
            for(auto i:vertices){float armWeight{};const auto& v=mesh.vertices[i];
                for(int k=0;k<4;++k)if(v.weights[k]>0)for(int side=0;side<2;++side)
                    if(below(static_cast<int>(v.bones[k]),roots[side])){sides|=1u<<side;armWeight+=v.weights[k];}
                if(armWeight<.4f)supported=false;
            }
            if(!supported||(sides!=1&&sides!=2))continue;
            const int side=sides==1?0:1;
            for(auto i:vertices)for(int k=0;k<4;++k){auto& v=mesh.vertices[i];
                if(v.weights[k]>0&&below(roots[side],static_cast<int>(v.bones[k]))&&v.bones[k]!=static_cast<unsigned>(shoulders[side])){
                    v.bones[k]=static_cast<unsigned>(shoulders[side]);++changed;
                }
            }
        }
    }
    return changed;
}
}
