#pragma once
#include "scene/PointBlankNative.h"
#include "scene/CodmLegacyAdapter.h"
#include <map>
namespace scene::pointblank {
inline bool fitHands(CastScene& native,const cast::Document& referenceDocument,const cast::Document& target,std::string& error){
 auto reference=buildScene(referenceDocument,false);
 if(exportedByPb2cast(target)){
  // PB male/female skins have different binds, despite identical identities.
  // Fit the selected skin into the exported clip driver once at assembly.
  // Unlike cross-game fitting, native PB chains need no helper reassignment.
  auto skin=buildScene(target,false);std::vector<size_t> map(skin.skeleton.bones.size());std::vector<Mat4> fit(map.size());
  for(size_t i=0;i<map.size();++i){const auto& b=skin.skeleton.bones[i];auto source=native.skeleton.boneByName.find(b.name);if(source==native.skeleton.boneByName.end()){map[i]=static_cast<size_t>(-1);continue;}map[i]=source->second;fit[i]=native.skeleton.bones[source->second].restGlobal*b.inverseBind;}
  for(auto& m:skin.meshes){m.viewmodelWeapon=false;for(auto& v:m.vertices){Vec3 position{},normal{};float total=0;
   for(size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0){const auto b=v.bones[k];if(b>=map.size()||map[b]==static_cast<size_t>(-1)){error="Unmapped weighted PB hand bone: "+(b<skin.skeleton.bones.size()?skin.skeleton.bones[b].name:std::to_string(b));return false;}const auto transform=fit[b]*m.modelTransform;position+=transformPoint(transform,v.position)*v.weights[k];normal+=volume_fit::vector(volume_fit::transpose3(inverseAffine(transform)),v.normal)*v.weights[k];total+=v.weights[k];v.bones[k]=static_cast<uint32_t>(map[b]);}
   if(total>0){v.position=position/total;v.normal=normalize(normal);}else v.position=transformPoint(m.modelTransform,v.position);
  }m.modelTransform=Mat4::identity();}
  std::erase_if(native.meshes,[](const auto& m){return !m.viewmodelWeapon;});native.meshes.insert(native.meshes.begin(),std::make_move_iterator(skin.meshes.begin()),std::make_move_iterator(skin.meshes.end()));
  native.warnings.push_back("PB native skin bind conversion: "+std::filesystem::path(target.sourceName()).filename().string());return true;
 }
 // Explicit anatomical identities. The PB Little chain is the pinky; twist
 // helpers are a parallel forearm chain, never additional finger segments.
 std::map<std::string,std::string> names{{"Spine1","b_Spine"}};
 for(auto side:{std::pair{"L ","Left"},std::pair{"R ","Right"}}){
  for(auto part:{std::pair{"UpperArm","Arm"},std::pair{"Forearm","ForeArm"},std::pair{"Hand","Hand"},std::pair{"Twist1","ForeArmRoll"}})names[std::string(side.first)+part.first]="b_"+std::string(side.second)+part.second;
  for(auto finger:{std::pair{"Thumb","Thumb"},std::pair{"Index","Index"},std::pair{"Middle","Middle"},std::pair{"Ring","Ring"},std::pair{"Little","Pinky"}})for(int j=1;j<=3;++j)names[std::string(side.first)+finger.first+std::to_string(j)]="b_"+std::string(side.second)+finger.second+std::to_string(j);
 }
 for(auto&[source,alias]:names){auto r=reference.skeleton.boneByName.find(source),n=native.skeleton.boneByName.find(source);if(r==reference.skeleton.boneByName.end()||n==native.skeleton.boneByName.end()){error="Point Blank anatomical joint missing: "+source;return false;}reference.skeleton.bones[r->second].name=alias;reference.skeleton.boneByName[alias]=r->second;native.skeleton.boneByName[alias]=n->second;}
 return codm::fitLegacyHands(native,reference,target,error);
}
}
