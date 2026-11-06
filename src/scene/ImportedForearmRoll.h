#pragma once
#include "scene/CastScene.h"
#include <numeric>
#include <cctype>

namespace scene::imported {
// A distal twist helper is usable only when the native skin actually uses it.
// Refuse multiple candidates instead of choosing by bone order or game name.
inline int nativeDistalForearmRoll(const CastScene& driver,int elbow,int wrist){
    const auto& rig=driver.skeleton;
    if(elbow<0||wrist<0||std::size_t(elbow)>=rig.bones.size()||std::size_t(wrist)>=rig.bones.size())return -1;
    const auto origin=transformPoint(rig.bones[elbow].restGlobal,{});
    const auto axis=transformPoint(rig.bones[wrist].restGlobal,{})-origin;
    const float length2=dot(axis,axis);
    if(!std::isfinite(length2)||length2<1e-8f)return -1;
    std::vector<bool> weighted(rig.bones.size());
    for(const auto& mesh:driver.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices)for(int k=0;k<4;++k){
        if(!std::isfinite(v.weights[k])||v.weights[k]<=0||v.bones[k]>=weighted.size())continue;
        auto bone=v.bones[k];
        for(const auto& follower:driver.nativePoseFollowers)if(follower.first==bone){bone=follower.second;break;}
        if(bone<weighted.size())weighted[bone]=true;
    }
    int selected=-1;
    for(std::size_t i=0;i<rig.bones.size();++i){
        const auto& bone=rig.bones[i];
        if(i==std::size_t(wrist)||bone.parent!=elbow||!weighted[i])continue;
        auto name=bone.name;for(auto& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if(name.find("twist")==std::string::npos&&name.find("roll")==std::string::npos)continue;
        const auto offset=transformPoint(bone.restGlobal,{})-origin;
        const float along=dot(offset,axis)/length2;
        if(!std::isfinite(along)||along<.5f||along>1.f||length(offset-axis*along)>.01f*std::sqrt(length2))continue;
        if(selected>=0)return -1;
        selected=static_cast<int>(i);
    }
    return selected;
}
// Transfer the authored longitudinal roll distribution once at load time.
// The native skeleton and its animation remain unchanged.
inline void transferNativeForearmRoll(const CastScene& driver,std::vector<Mesh>& fitted,int elbow,int wrist,int explicitRoll=-1){
    const auto& rig=driver.skeleton;
    if(elbow<0||wrist<0||std::size_t(elbow)>=rig.bones.size()||std::size_t(wrist)>=rig.bones.size())return;
    const auto found=rig.boneByName.find(rig.bones[elbow].name+"Roll");
    if(explicitRoll<0&&(found==rig.boneByName.end()||found->second>=rig.bones.size())){
        explicitRoll=nativeDistalForearmRoll(driver,elbow,wrist);
        if(explicitRoll<0)return;
    }
    const auto roll=explicitRoll>=0?static_cast<std::size_t>(explicitRoll):found->second;
    if(roll>=rig.bones.size()||roll==std::size_t(elbow)||roll==std::size_t(wrist))return;
    const auto origin=transformPoint(rig.bones[elbow].restGlobal,{}),axis=transformPoint(rig.bones[wrist].restGlobal,{})-origin;
    const auto length2=dot(axis,axis);if(!std::isfinite(length2)||length2<1e-8f)return;
    const auto offset=transformPoint(rig.bones[roll].restGlobal,{})-origin;
    if(explicitRoll<0){if(length(offset)>.01f*std::sqrt(length2))return;}
    else{
        const float along=dot(offset,axis)/length2;
        if(!std::isfinite(along)||along<0||along>1||length(offset-axis*along)>.01f*std::sqrt(length2))return;
        if(rig.bones[roll].parent!=elbow)return;
    }
    std::vector<std::size_t> source(rig.bones.size());for(std::size_t i=0;i<source.size();++i)source[i]=i;
    for(const auto& [follower,bone]:driver.nativePoseFollowers)if(follower<source.size()&&bone<source.size())source[follower]=bone;
    constexpr int count=32;std::array<float,count> total{},rolled{};
    const auto bin=[&](Vec3 p){return std::clamp(dot(p-origin,axis)/length2,0.f,1.f)*(count-1);};
    for(const auto& mesh:driver.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices){
        float armWeight{},rollWeight{};Vec3 p{};const auto point=transformPoint(mesh.modelTransform,v.position);
        for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<source.size()){
            const auto b=source[v.bones[k]];const auto w=v.weights[k];
            p+=transformPoint(rig.bones[b].restGlobal*rig.bones[v.bones[k]].inverseBind,point)*w;
            if(b==std::size_t(elbow)||b==roll)armWeight+=w;if(b==roll)rollWeight+=w;
        }
        if(armWeight<=0)continue;
        const float x=bin(p);const int lo=int(x),hi=std::min(lo+1,count-1);const float t=x-lo;
        total[lo]+=armWeight*(1-t);rolled[lo]+=rollWeight*(1-t);total[hi]+=armWeight*t;rolled[hi]+=rollWeight*t;
    }
    if(std::accumulate(rolled.begin(),rolled.end(),0.f)<=0)return;
    const auto fraction=[&](float x){int lo=int(x),hi=std::min(lo+1,count-1);while(lo>=0&&total[lo]<=0)--lo;while(hi<count&&total[hi]<=0)++hi;
        if(lo<0)return hi<count?rolled[hi]/total[hi]:0.f;if(hi>=count)return rolled[lo]/total[lo];
        const float t=hi==lo?0.f:std::clamp((x-lo)/(hi-lo),0.f,1.f);return (1-t)*rolled[lo]/total[lo]+t*rolled[hi]/total[hi];};
    for(auto& mesh:fitted)for(auto& v:mesh.vertices){
        float armWeight{};for(int k=0;k<4;++k)if(v.bones[k]==std::size_t(elbow))armWeight+=v.weights[k];if(armWeight<=0)continue;
        const float w=armWeight*std::clamp(fraction(bin(v.position)),0.f,1.f);if(w<=0)continue;
        std::vector<std::pair<uint32_t,float>> weights;float remaining=armWeight-w;
        for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]!=std::size_t(elbow))weights.emplace_back(v.bones[k],v.weights[k]);
        if(remaining>0)weights.emplace_back(elbow,remaining);weights.emplace_back(static_cast<uint32_t>(roll),w);
        std::sort(weights.begin(),weights.end(),[](auto a,auto b){return a.second>b.second;});v.bones={};v.weights={};float sum{};
        for(std::size_t k=0;k<std::min<std::size_t>(4,weights.size());++k){v.bones[k]=weights[k].first;v.weights[k]=weights[k].second;sum+=weights[k].second;}for(auto& value:v.weights)value/=sum;
    }
}
}
