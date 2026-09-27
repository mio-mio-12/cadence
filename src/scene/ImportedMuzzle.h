#pragma once
#include "scene/ImportedNative.h"
#include <map>

namespace scene::imported {
inline void prepareMuzzleAnchors(CastScene& scene,const std::filesystem::path& path){
    const auto game=gameForPath(path);
    if(!assets::imported::supported(game))return;
    // Keep the model socket fixed within its moving parent: fire clips may
    // carry generic or zeroed socket tracks that do not describe this barrel.
    for(std::size_t i=0;i<scene.skeleton.bones.size();++i){
        const auto& b=scene.skeleton.bones[i];const auto n=assets::imported::lower(b.name);
        const bool halo=n.find("dew2cast_socket__")!=std::string::npos&&(n.ends_with("muzzle_flash__0")||n.ends_with("primary_trigger__0"));
        const bool cs=n=="muzzle"||n.ends_with("__muzzle")||n.ends_with("_muzzle_flash")||n.ends_with("_tag_flash");
        if(!halo&&!cs)continue;
        const auto parent=b.parent>=0?static_cast<std::size_t>(b.parent):i;
        const auto local=transformPoint(scene.skeleton.bones[parent].inverseBind,transformPoint(b.restGlobal,{}));
        const int side=n.find("__left__")!=std::string::npos?1:0;
        // Halo weapons without a muzzle-flash marker export the firing
        // origin as primary_trigger. Prefer a flash marker when both exist.
        const bool flash=n.ends_with("muzzle_flash__0");
        const bool hasFlash=std::any_of(scene.skeleton.bones.begin(),scene.skeleton.bones.end(),[&](const auto& other){const auto key=assets::imported::lower(other.name);return key.ends_with("muzzle_flash__0")&&(key.find("__left__")!=std::string::npos?1:0)==side;});
        if(halo&&!flash&&hasFlash)continue;
        scene.muzzleAnchors.push_back({parent,local,side});
    }
    if(!scene.muzzleAnchors.empty()||!assets::imported::sourceFamily(game)||
       (assets::imported::role(game,path.stem().string())!=assets::Role::ViewWeapon&&assets::imported::role(game,path.stem().string())!=assets::Role::WorldWeapon))return;
    const auto name=assets::imported::lower(path.stem().string());
    if(name.find("knife")!=std::string::npos||name.find("grenade")!=std::string::npos||name.find("bomb")!=std::string::npos)return;
    // The original Source weapon bind points down -Y (animations place it
    // in the +X camera frame). Only gun vertices participate, never viewhands.
    float front=std::numeric_limits<float>::max(),rear=-front;
    for(const auto& m:scene.meshes)if(!explicitHandMesh(m.name))for(const auto& v:m.vertices){
        const auto p=transformPoint(m.modelTransform,v.position);front=std::min(front,p.y);rear=std::max(rear,p.y);
    }
    if(!std::isfinite(front)||rear-front<.01f)return;
    const float slice=std::max(.025f,(rear-front)*.003f);
    std::map<std::size_t,std::pair<Vec3,float>> groups;
    for(const auto& m:scene.meshes)if(!explicitHandMesh(m.name))for(const auto& v:m.vertices){
        const auto p=transformPoint(m.modelTransform,v.position);if(p.y>front+slice)continue;
        const auto influence=std::max_element(v.weights.begin(),v.weights.end())-v.weights.begin();
        if(v.weights[influence]<=0||v.bones[influence]>=scene.skeleton.bones.size())continue;
        auto& group=groups[v.bones[influence]];group.first=group.first+p;group.second+=1.f;
    }
    if(groups.empty())return;
    const auto chosen=std::max_element(groups.begin(),groups.end(),[](const auto& a,const auto& b){return a.second.second<b.second.second;});
    auto tip=chosen->second.first/chosen->second.second;tip.y=front;
    scene.muzzleAnchors.push_back({chosen->first,transformPoint(scene.skeleton.bones[chosen->first].inverseBind,tip),0});
}
}
