#pragma once
#include "scene/CastScene.h"
#include <string>
#include <unordered_map>
#include <optional>
#include <cstdlib>
#include <cctype>

namespace cadence::world_weapon {
inline std::string modelIdentity(std::string name){
    for(auto& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(name.ends_with("_lod0"))name.resize(name.size()-5);
    for(const auto prefix:{"viewmodel_","worldmodel_","weapon_","vm_","npc_","wm_"})
        if(name.starts_with(prefix)){name.erase(0,std::char_traits<char>::length(prefix));break;}
    for(const auto suffix:{"_view","_world","_vm"})if(name.ends_with(suffix)){name.resize(name.size()-std::char_traits<char>::length(suffix));break;}
    return name;
}
using Fields=std::unordered_map<std::string,std::string>;
template<class Available>
inline std::vector<std::size_t> preferAvailableWorldModels(std::vector<std::size_t> candidates,Available available){
    std::vector<std::size_t> complete;for(const auto index:candidates)if(available(index))complete.push_back(index);
    // With no native world exports (e.g. this MW3 rip), retain the requested
    // pool and its missing-world diagnostics rather than substitute another game.
    return complete.empty()?std::move(candidates):std::move(complete);
}
inline bool detachedAwBotModule(std::string name){
    name=modelIdentity(std::move(name));
    // Inspected native BAL27 damage/mobility/range exports contain only a
    // one-mesh module, unlike the complete base_standard receiver. Keep manual picking
    // available, but never silently distribute this fragment as a full gun.
    return name=="bal27_damage_base"||name=="bal27_damage_base_atlas"||
           name=="bal27_mobility_base"||name=="bal27_mobility_base_atlas"||
           name=="bal27_range_base"||name=="bal27_range_base_atlas";
}
inline Fields weaponFields(std::string_view data){
    Fields result;if(!data.starts_with("WEAPONFILE\\"))return result;
    data.remove_prefix(11);
    while(!data.empty()){
        const auto split=data.find('\\');if(split==std::string_view::npos)return {};
        const std::string key(data.substr(0,split));data.remove_prefix(split+1);
        const auto end=data.find('\\');const std::string value(data.substr(0,end));
        if(result.contains(key)&&(key=="worldModel"||key=="worldClipModel"||key.starts_with("attachWorldModel")))return {};
        result[key]=value; // Real exports repeat unrelated timing fields.
        if(end==std::string_view::npos)break;data.remove_prefix(end+1);
    }return result;
}
inline std::optional<float> number(const Fields& fields,const std::string& key){
    const auto it=fields.find(key);if(it==fields.end()||it->second.empty())return {};
    char* end{};const float value=std::strtof(it->second.c_str(),&end);
    if(end!=it->second.c_str()+it->second.size()||!std::isfinite(value))return {};return value;
}
// T6 weaponfile attachment stations: base optic=1, magazine=7. CAST exports
// are centimetres; weaponfile offsets are IW inches. Convert exactly once.
inline std::optional<scene::Mat4> t6Station(const Fields& fields,std::string_view root){
    const int slot=root=="tag_scope"?1:root=="tag_clip"?7:0;if(!slot)return {};
    const auto prefix="attachWorldModelOffset"+std::to_string(slot);
    const auto rotation="attachWorldModelRotation"+std::to_string(slot);
    const auto x=number(fields,prefix+"X"),y=number(fields,prefix+"Y"),z=number(fields,prefix+"Z");
    const auto pitch=number(fields,rotation+"Pitch"),yaw=number(fields,rotation+"Yaw"),roll=number(fields,rotation+"Roll");
    if(!x||!y||!z||!pitch||!yaw||!roll)return {};
    // Nonzero engine Euler conventions need a separately validated conversion.
    if(*pitch!=0||*yaw!=0||*roll!=0)return {};
    return scene::translation(scene::Vec3{*x,*y,*z}*2.54f);
}
inline std::optional<scene::Mat4> socketTransform(const scene::CastScene& base,const scene::CastScene& part){
    std::optional<scene::Mat4> result;
    for(const auto& bone:part.skeleton.bones)if(bone.parent<0){
        const auto name=scene::canonicalName(bone.name);
        // A generic weapon root is not an accessory mounting socket.
        if(name=="tag_weapon"||name=="j_gun"||name=="root")return {};
        const auto found=base.skeleton.boneByCanonicalName.find(name);
        if(found==base.skeleton.boneByCanonicalName.end()||result)return {};
        result=base.skeleton.bones[found->second].restGlobal*scene::inverseAffine(bone.restGlobal);
    }return result;
}
inline void setTransform(scene::Attachment& attachment,const scene::Mat4& transform){
    scene::Quat q;scene::decomposeAffine(transform,attachment.position,q,attachment.scale);
    attachment.rotationDegrees=scene::Vec3{
        std::atan2(2*(q.w*q.x+q.y*q.z),1-2*(q.x*q.x+q.y*q.y)),
        std::asin(std::clamp(2*(q.w*q.y-q.z*q.x),-1.0f,1.0f)),
        std::atan2(2*(q.w*q.z+q.x*q.y),1-2*(q.y*q.y+q.z*q.z))}*(180.0f/scene::kPi);
}
}
