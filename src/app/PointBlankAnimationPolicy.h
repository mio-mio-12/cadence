#pragma once
#include "app/CodmAnimationPolicy.h"
namespace cadence::pointblank_actions {
using codm_actions::lower;
inline std::string suffix(const scene::CastScene& s,const scene::Animation& a){
 auto name=lower(std::filesystem::path(a.sourceName).stem().string());auto prefix=lower(s.pointBlankWeaponStem)+"_";
 return name.starts_with(prefix)?name.substr(prefix.size()):std::string{};
}
inline void prepare(scene::CastScene& s){
 if(s.pointBlankWeaponStem.empty())return;
 for(auto& a:s.animations){auto n=suffix(s,a);a.domain=scene::AnimationDomain::ViewModel;a.action=scene::ActionRole::None;a.motion=scene::MotionRole::Unknown;a.looping=false;
  if(n=="attackidle"||n=="attackidle_zoom"||n=="attackldle_zoom_sopmod"){a.motion=scene::MotionRole::Idle;a.looping=true;}
  else if(n=="move_attackidle"){a.motion=scene::MotionRole::Walk;a.looping=true;}
  else if(n=="jumpstart_attackidle")a.motion=scene::MotionRole::Jump;
  else if(n=="jumpend_attackidle")a.motion=scene::MotionRole::Land;
  else if(n=="change")a.action=scene::ActionRole::Equip;
  else if(n.starts_with("reload"))a.action=scene::ActionRole::Reload;
  else if(n=="damage_attackidle")a.action=scene::ActionRole::Flinch;
  else if(n=="attack"||n=="attack_"||n=="attack_semi"||n=="attack_burst"||n=="attack_a"||n=="attack_b"||n=="attack_stab")a.action=lower(s.pointBlankWeaponStem).starts_with("viewmodel_knife_")?scene::ActionRole::Melee:scene::ActionRole::Fire;
 }
}
inline void populate(const scene::CastScene& s,weapon::Profile& p,const std::string& stem){
 const auto prefix=lower(stem)+"_";p.animationPrefix=prefix;
 auto set=[&](const char* slot,std::initializer_list<const char*> names){
  if(auto it=p.animations.find(slot);it!=p.animations.end())for(auto&a:s.animations)if(lower(std::filesystem::path(a.sourceName).filename().string())==lower(std::filesystem::path(it->second).filename().string())&&!a.tracks.empty())return;
  p.animations.erase(slot);for(auto n:names)if(auto*a=codm_actions::find(s,prefix,n)){p.animations[slot]=a->sourceName;return;}
 };
 set("idle",{"attackidle"});set("fire",{"attack","attack_semi","attack_a"});set("ads_fire",{"attack","attack_semi","attack_a"});
 set("pullout",{"change"});set("first_raise",{"change"});set("reload",{"reload","reload_a","reloada"});set("reload_empty",{"reload","reload_a","reloada"});
 if(weapon::isSniper(p.archetype)){set("rechamber",{"reloadc"});set("ads_rechamber",{"reloadc"});}
 set("jump_takeoff",{"jumpstart_attackidle"});set("jump_land",{"jumpend_attackidle"});set("walk",{"move_attackidle"});
 if(p.meleeWeapon){set("melee",{"attack_stab","attack_a","attack"});if(!p.animationVariants.contains("melee"))for(auto n:{"attack_a","attack_b","attack_stab"})if(auto*a=codm_actions::find(s,prefix,n))p.animationVariants["melee"].push_back(a->sourceName);}
 // Weapon / ... files are mechanism/3PV diagnostics, never automatic actions.
 // No fabricated ADS transitions, inspect or sprint clips.
}
inline void defaults(const scene::CastScene& s,weapon::Profile& p){
 codm_actions::defaultTimings(s,p);
 p.stats.adsIn=0;p.stats.adsOut=0;p.stats.dropTime=p.stats.quickDropTime=0;
 p.stats.hideWeaponOnAds=weapon::isSniper(p.archetype);
 p.stats.fireTime=weapon::isSniper(p.archetype)?.5f:(p.meleeWeapon?.45f:(p.archetype==weapon::Archetype::Pistol?.3f:.1f));
 // A/B/stab often have different lengths. Zero selects the actual clip's
 // duration, not the generic CoD melee time shared by all three variants.
 if(p.meleeWeapon)p.stats.meleeTime=0;
 p.source="Point Blank native clips; Source-style instant scope defaults";
}
}
