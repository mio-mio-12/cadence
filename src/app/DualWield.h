#pragma once
#include "scene/CastScene.h"
#include "weapon/WeaponProfile.h"
#include <unordered_set>

namespace cadence::dual {
inline std::string lower(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline constexpr std::string_view leftPrefix="dual_left_";
// MW3 exports one physical model. Duplicate its bind/mesh, not a reflected
// screen-space image: the left clip supplies the authored orientation.
inline bool duplicateMw3(scene::CastScene& s){
 if(s.skeleton.boneByName.contains("dual_left_j_gun"))return true;
 auto gun=s.skeleton.boneByCanonicalName.find("j_gun");if(gun==s.skeleton.boneByCanonicalName.end())return false;
 const auto root=gun->second;auto mount=s.skeleton.bones[root].parent;
 if(mount<0)return false;
 std::unordered_map<size_t,size_t> remap;
 const auto originalBones=s.skeleton.bones.size();
 auto clone=[&](size_t i){auto b=s.skeleton.bones[i];b.name=std::string(leftPrefix)+b.name;if(b.parent>=0&&remap.contains(b.parent))b.parent=static_cast<int>(remap.at(b.parent));auto index=s.skeleton.bones.size();remap[i]=index;s.skeleton.boneByName[b.name]=index;s.skeleton.boneByCanonicalName[scene::canonicalName(b.name)]=index;s.skeleton.bones.push_back(b);};
 clone(static_cast<size_t>(mount));
 for(size_t i=root;i<originalBones;++i){auto parent=s.skeleton.bones[i].parent;if(i==root||(parent>=0&&remap.contains(parent)))clone(i);}
 const auto originalMeshes=s.meshes.size();size_t added=0;
 for(size_t i=0;i<originalMeshes;++i){auto m=s.meshes[i];if(!m.viewmodelWeapon)continue;bool belongs=false,all=true;
  for(auto& v:m.vertices)for(size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0){belongs|=remap.contains(v.bones[k]);all&=remap.contains(v.bones[k]);}
  if(!belongs||!all)continue;for(auto& v:m.vertices)for(size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0)v.bones[k]=static_cast<uint32_t>(remap.at(v.bones[k]));m.name="Left / "+m.name;s.meshes.push_back(std::move(m));++added;
 }
 return added>0;
}
inline std::optional<size_t> clip(const scene::CastScene& s,const std::string& prefix,const std::string& suffix){
 for(size_t i=0;i<s.animations.size();++i)if(lower(std::filesystem::path(s.animations[i].sourceName).stem().string())==prefix+suffix)return i;return {};
}
inline std::optional<size_t> sideClip(const scene::CastScene&s,const std::string&p,const std::string&key,int side,bool pb){
 const std::string hand=side?"left":"right",shortHand=side?"l":"r";
 if(pb)return clip(s,p,key+"_"+hand);
 for(const auto& suffix:{"dw_"+hand+"_"+key,key+"_"+shortHand,"akimbo_"+shortHand+"_"+key,"akimbo_"+key+"_"+shortHand,key+"_akimbo_"+shortHand})if(auto i=clip(s,p,suffix))return i;
 if(p.starts_with("viewmodel_"))return clip(s,"viewmodel_akimbo_"+p.substr(10),key+"_"+shortHand);return {};
}
inline bool isLeftArm(std::string n){n=lower(n);return n.starts_with("l ")||n.find("_le")!=std::string::npos;}
inline bool isRightArm(std::string n){n=lower(n);return n.starts_with("r ")||n.find("_ri")!=std::string::npos;}
inline bool under(const scene::CastScene&s,size_t b,const std::unordered_set<size_t>& roots){for(int i=static_cast<int>(b);i>=0;i=s.skeleton.bones[i].parent)if(roots.contains(i))return true;return false;}
inline bool prepare(scene::CastScene& s,weapon::Profile& p,std::string prefix,bool mw3,std::string& error){
 if(s.dualWield)return true;
 const bool pb=!s.pointBlankWeaponStem.empty();prefix=lower(prefix);if(!prefix.ends_with('_'))prefix+='_';
 auto ri=sideClip(s,prefix,pb?"attackidle":"idle",0,pb),li=sideClip(s,prefix,pb?"attackidle":"idle",1,pb);
 auto rf=sideClip(s,prefix,pb?"attack":"fire",0,pb),lf=sideClip(s,prefix,pb?"attack":"fire",1,pb);
 if(!ri||!li||!rf||!lf){error="Dual wield requires matching left/right idle and fire clips";return false;}
 std::unordered_map<size_t,size_t> leftMap;std::unordered_set<size_t> rightRoots,leftRoots;
 for(size_t i=0;i<s.skeleton.bones.size();++i){const auto n=lower(s.skeleton.bones[i].name);
  if(n.starts_with(leftPrefix)){auto source=s.skeleton.boneByName.find(s.skeleton.bones[i].name.substr(leftPrefix.size()));if(source!=s.skeleton.boneByName.end())leftMap[source->second]=i;}
  if(n=="j_gun"||n=="tag_weapon"||n=="pb2cast_weapon__gundummy")rightRoots.insert(i);
  if(n=="j_gun1"||n=="tag_weapon1"||n=="pb2cast_weapon__left__gundummy"||n.starts_with(leftPrefix))leftRoots.insert(i);
 }
 auto part=[&](size_t index,int side,bool shared){auto a=s.animations[index];a.notifications.clear();a.tracks.clear();
  for(auto t:s.animations[index].tracks){const auto& name=s.skeleton.bones[t.boneIndex].name;bool arm=side?isLeftArm(name):isRightArm(name);
   bool mechanism=under(s,t.boneIndex,side?leftRoots:rightRoots);
   if(mw3&&side&&leftMap.contains(t.boneIndex)){t.boneIndex=leftMap.at(t.boneIndex);mechanism=true;}
   if(arm||mechanism||(!side&&shared&&!isLeftArm(name)&&!under(s,t.boneIndex,leftRoots))){t.ownsLayer=true;a.tracks.push_back(std::move(t));}
  }return a;
 };
 auto add=[&](std::string slot,scene::Animation a){a.name=a.sourceName=prefix+"cadence_dual_"+slot+".cast";a.domain=scene::AnimationDomain::ViewModel;a.weapon=scene::WeaponClass::DualWield;if(slot.starts_with("fire"))a.action=scene::ActionRole::Fire;if(slot=="idle"){a.action=scene::ActionRole::None;a.motion=scene::MotionRole::Idle;a.looping=true;}auto index=s.animations.size();s.animations.push_back(std::move(a));p.animations[slot]=s.animations.back().sourceName;p.animationVariants.erase(slot);return index;};
 auto pair=[&](std::string slot,size_t right,size_t left){auto a=part(right,0,true),b=part(left,1,false);const auto fps=a.framerate;
  for(auto& t:b.tracks){for(auto& frame:t.frames)frame=static_cast<uint32_t>(std::lround(frame*fps/std::max(1.f,b.framerate)));a.tracks.push_back(std::move(t));}
  a.durationFrames=std::max(a.durationFrames,static_cast<uint32_t>(std::lround(b.durationFrames*fps/std::max(1.f,b.framerate))));return add(slot,std::move(a));};
 pair("idle",*ri,*li);s.dualFireClips[0]=add("fire_right",part(*rf,0,false));s.dualFireClips[1]=add("fire_left",part(*lf,1,false));p.animations["fire"]=p.animations["fire_right"];
 if(pb)for(const auto& mapping:std::array<std::pair<const char*,const char*>,3>{{{"jump_takeoff","jumpstart_attackidle"},{"jump_land","jumpend_attackidle"},{"walk","move_attackidle"}}}){
  auto r=sideClip(s,prefix,mapping.second,0,true),l=sideClip(s,prefix,mapping.second,1,true);
  if(r&&l)pair(mapping.first,*r,*l);else p.animations.erase(mapping.first);
 }
 for(auto key:{"reload","reload_empty","pullout","putaway","sprint_in","sprint_loop","sprint_out"}){
  auto r=sideClip(s,prefix,key,0,pb),l=sideClip(s,prefix,key,1,pb);
  if(r&&l)pair(key,*r,*l);
  else if(auto shared=clip(s,prefix,pb?(std::string(key)=="pullout"?"change":key):"dw_"+std::string(key)))p.animations[key]=s.animations[*shared].sourceName;
  else p.animations.erase(key);
 }
 if(!p.animations.contains("reload_empty")&&p.animations.contains("reload"))p.animations["reload_empty"]=p.animations["reload"];
 if(p.animations.contains("pullout"))p.animations["first_raise"]=p.animations["pullout"];else p.animations.erase("first_raise");
 p.animations.erase("ads_up");p.animations.erase("ads_down");p.stats.hideWeaponOnAds=false;p.stats.boltAction=false;p.archetype=weapon::Archetype::DualWield;s.dualWield=true;return true;
}
inline std::optional<scene::Vec3> muzzle(const scene::CastScene&s,const std::vector<scene::Mat4>&pose,int side){
 const std::array<std::string,3> names=side?std::array<std::string,3>{"dual_left_tag_flash","tag_flash1","pb2cast_weapon__left__FXDummy"}:std::array<std::string,3>{"tag_flash","tag_flash_1","pb2cast_weapon__FXDummy"};
 for(auto& name:names)if(auto it=s.skeleton.boneByName.find(name);it!=s.skeleton.boneByName.end()&&it->second<pose.size())return scene::transformPoint(pose[it->second],{});return {};
}
}
