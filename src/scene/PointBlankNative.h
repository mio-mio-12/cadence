#pragma once
#include "scene/CastScene.h"
#include "scene/PointBlankWorld.h"
#include <limits>

namespace scene::pointblank {
inline bool animationPath(const cast::Document& d){for(auto& p:std::filesystem::path(d.sourceName()))if(p=="pointblank")return true;return false;}
inline void classifyPlayer(Animation& a){
 auto n=std::filesystem::path(a.sourceName).stem().string();for(auto& c:n)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
 if(!n.starts_with("pb_")&&!n.starts_with("pt_"))return;
 a.domain=n.starts_with("pb_")?AnimationDomain::PlayerBody:AnimationDomain::PlayerTorso;
 for(const auto* prefix:{"pb_swat_female_swat-sniper_","pt_swat_female_swat-sniper_","pb_swat_male_","pt_swat_male_"})if(n.starts_with(prefix)){n.erase(0,std::char_traits<char>::length(prefix));break;}
 a.action=ActionRole::None;a.motion=MotionRole::Unknown;a.looping=false;a.contextual=false;a.weapon=WeaponClass::Any;a.direction=Direction::Any;
 if(n.find("_ui_")!=std::string::npos){a.motion=MotionRole::Idle;a.looping=true;a.contextual=true;}
 // Death-Scythe is a weapon, not a death action. Native deaths live in the
 // body common_death family, not in torso weapon action names.
 else if(a.domain==AnimationDomain::PlayerBody&&n.find("_death_")!=std::string::npos)a.action=ActionRole::Death;
 else if(n.find("reload")!=std::string::npos)a.action=ActionRole::Reload;
 else if(n.find("attackidle")!=std::string::npos){a.motion=MotionRole::Idle;a.looping=true;}
 else if(n.find("attack")!=std::string::npos)a.action=ActionRole::Fire;
 else if(n.find("damage")!=std::string::npos)a.action=ActionRole::Flinch;
 else if(n.find("_change")!=std::string::npos)a.action=ActionRole::Equip;
 if(n.find("_move_")!=std::string::npos){a.motion=MotionRole::Run;a.looping=a.action==ActionRole::None;}
 a.ads=n.find("_zoom")!=std::string::npos;
 if(n.find("_sit_")!=std::string::npos)a.stance=Stance::Crouch;
 else a.stance=Stance::Stand;
 if(n.find("dualhandgun")!=std::string::npos)a.weapon=WeaponClass::DualWield;
 else if(n.find("assultrifle")!=std::string::npos)a.weapon=WeaponClass::Rifle;
 else if(n.find("handgun")!=std::string::npos)a.weapon=WeaponClass::Pistol;
 else if(n.find("sniper")!=std::string::npos||n.find("sniffer")!=std::string::npos)a.weapon=WeaponClass::Sniper;
 else if(n.find("shotgun")!=std::string::npos)a.weapon=WeaponClass::Shotgun;
 else if(n.find("smg")!=std::string::npos||n.find("submachine")!=std::string::npos)a.weapon=WeaponClass::Automatic;
 else if(n.find("knife")!=std::string::npos||n.find("melee")!=std::string::npos||n.find("kukri")!=std::string::npos||n.find("knuckle")!=std::string::npos)a.weapon=WeaponClass::Knife;
 if(a.weapon==WeaponClass::Knife&&a.action==ActionRole::Fire)a.action=ActionRole::Melee;
}
inline bool exportedByPb2cast(const cast::Document& d){
 for(const auto& r:d.roots())for(const auto& n:r.children){
  const auto* s=n.findProperty("s");const auto* up=n.findProperty("up");
  if(cast::nodeTypeName(n.identifier)=="Metadata"&&s&&s->stringValue=="pb2cast"&&up&&up->stringValue=="z")return true;
 }return false;
}
inline bool detachedPresentationMesh(const Mesh& mesh,const Skeleton& skeleton,float detachedAbove=std::numeric_limits<float>::infinity()){
 // These exported accessories have no anatomical binding. Preserve properly
 // skinned clan patches (including both male outfits) and all body geometry.
 const bool knownAccessory=mesh.name.starts_with("Model_Clan_")||mesh.name.starts_with("A_R_Kopassus_")||
  mesh.name.starts_with("A_R_Kopassus01_")||mesh.name.starts_with("A_R_Recon_")||
  mesh.name.starts_with("A_B_Kopassus_")||mesh.name.starts_with("A_B_Recon_")||
  mesh.name.starts_with("Bella_Equip_")||mesh.name.starts_with("Equipments_007_")||
  mesh.name.starts_with("O_R_Tarantula_Ori_");
 if(!mesh.skinned||mesh.vertices.empty())return false;
 for(const auto& v:mesh.vertices){bool weighted=false;for(size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0){weighted=true;if(v.bones[k]>=skeleton.bones.size()||skeleton.bones[v.bones[k]].name!="Root")return false;}if(!weighted)return false;
  if(!knownAccessory&&!(transformPoint(mesh.modelTransform,v.position).z>detachedAbove))return false;
 }
 return true;
}
// pb2cast geometry is metre-scale, already Z-up. Do not rotate the export or
// reuse CODM's presentation multiplier. Keep the conversion on the scene.
inline void normalize(CastScene& s){
 if(s.pointBlankNativeCentimetres)return;
 // The exporter writes bottom-left UVs; its preview flips PNG upload rows.
 // Cadence keeps top-down image rows for CoD, so convert only pb2cast UVs.
 for(auto& m:s.meshes){for(auto& v:m.vertices){v.position=v.position*100.f;v.uv.y=1.f-v.uv.y;}for(int k:{12,13,14})m.modelTransform.v[k]*=100.f;}
 for(auto& b:s.skeleton.bones){b.restLocal.position=b.restLocal.position*100.f;b.absoluteTranslationOffset=b.absoluteTranslationOffset*100.f;for(int k:{12,13,14}){b.restGlobal.v[k]*=100.f;b.inverseBind.v[k]*=100.f;}}
 s.pointBlankNativeCentimetres=true;
 if(body(s.skeleton)){
  // Parked cosmetic alternatives can be exported a body-height above the
  // character, weighted only to Root. Do not hide heads by their filename.
  float bottom=std::numeric_limits<float>::infinity(),top=-std::numeric_limits<float>::infinity();
  for(const auto& m:s.meshes)if(m.skinned)for(const auto& v:m.vertices)for(size_t k=0;k<v.weights.size();++k)
   if(v.weights[k]>0&&v.bones[k]<s.skeleton.bones.size()&&s.skeleton.bones[v.bones[k]].name!="Root"){
    const float z=transformPoint(m.modelTransform,v.position).z;if(std::isfinite(z)){bottom=std::min(bottom,z);top=std::max(top,z);}break;
   }
  const float detachedAbove=std::isfinite(top)?top+std::max(10.f,(top-bottom)*.1f):std::numeric_limits<float>::infinity();
  std::erase_if(s.meshes,[&](const auto& m){if(!detachedPresentationMesh(m,s.skeleton,detachedAbove))return false;s.warnings.push_back("Point Blank detached root-only accessory hidden: "+m.name);return true;});
  const auto rotation=fromEulerRadians({0,0,-kPi*.5f});const auto basis=trs({},rotation,{1,1,1});
  for(auto& m:s.meshes)for(auto& v:m.vertices){v.position=transformPoint(basis,v.position);v.normal=transformPoint(basis,v.normal);}
  for(auto& b:s.skeleton.bones){b.restGlobal=basis*b.restGlobal;b.inverseBind=inverseAffine(b.restGlobal);if(b.parent<0){b.restLocal.position=transformPoint(basis,b.restLocal.position);b.restLocal.rotation=multiply(rotation,b.restLocal.rotation);}}
  worldAliases(s.skeleton);
 }
}
inline bool assemble(const cast::Document& weapon,const cast::Document& hands,CastScene& out,std::string& error){
 if(!exportedByPb2cast(weapon)||!exportedByPb2cast(hands)){error="Point Blank requires pb2cast Z-up exports";return false;}
 auto skin=buildScene(hands,false),gun=buildScene(weapon,false);
 if(skin.meshes.empty()||gun.meshes.empty()||!skin.skeleton.boneByName.contains("R Hand")||!skin.skeleton.boneByName.contains("L Hand")){error="Missing Point Blank native hands or weapon";return false;}
 const auto offset=skin.skeleton.bones.size();
 for(auto b:gun.skeleton.bones){if(skin.skeleton.boneByName.contains(b.name)){error="Point Blank skeleton identity collision: "+b.name;return false;}if(b.parent>=0)b.parent+=static_cast<int>(offset);skin.skeleton.boneByName[b.name]=skin.skeleton.bones.size();skin.skeleton.boneByCanonicalName[canonicalName(b.name)]=skin.skeleton.bones.size();skin.skeleton.bones.push_back(std::move(b));}
 for(auto& m:skin.meshes)m.viewmodelWeapon=false;
 for(auto m:gun.meshes){for(auto& v:m.vertices)for(auto& b:v.bones)b+=static_cast<uint32_t>(offset);m.viewmodelWeapon=true;skin.meshes.push_back(std::move(m));}
 skin.pointBlankWeaponStem=std::filesystem::path(weapon.sourceName()).stem().string();
 for(auto name:{"tag_view","tag_camera"}){Bone b;b.name=name;b.restLocal.position={0,0,165};if(std::string_view(name)=="tag_camera")b.restLocal.rotation=fromEulerRadians({0,0,kPi*.5f});b.restGlobal=trs(b.restLocal.position,b.restLocal.rotation,{1,1,1});b.inverseBind=inverseAffine(b.restGlobal);skin.skeleton.boneByName[name]=skin.skeleton.bones.size();skin.skeleton.boneByCanonicalName[name]=skin.skeleton.bones.size();skin.skeleton.bones.push_back(b);}
 // Weapon root channels are already in character space. Attaching GunDummy
// to the wrist a second time corrupts reloads and knife rotations.
 out=std::move(skin);return true;
}
}
