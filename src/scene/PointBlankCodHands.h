#pragma once
#include "scene/PointBlankAdapter.h"
namespace scene::pointblank {
// Keep the COD driver (including weapon, camera and all action timing) intact.
// Only the PB skin/binds are fitted once. Its native files stay untouched.
inline bool fitToCod(CastScene& driver,const cast::Document& pbHands,std::string& error){
 auto reference=driver,skin=buildScene(pbHands);const auto originalMap=driver.skeleton.boneByName;
 const bool codmHands=skin.skeleton.boneByName.contains("b_LeftHand");
 if(codmHands&&!codm::prepareLegacyNamedHands(skin,pbHands,error))return false;
 if(!codmHands&&!skin.skeleton.boneByName.contains("L Hand")){error="Not supported replacement viewhands";return false;}
 for(const auto* side:codmHands?std::initializer_list<const char*>{"j_shoulder_le","j_shoulder_ri"}:std::initializer_list<const char*>{"L UpperArm","R UpperArm"}){
  const auto arm=skin.skeleton.boneByName.find(side);float weight=0;
  if(arm!=skin.skeleton.boneByName.end())for(const auto& mesh:skin.meshes)for(const auto& vertex:mesh.vertices)for(std::size_t k=0;k<vertex.weights.size();++k)if(vertex.weights[k]>0){
   auto b=vertex.bones[k];std::size_t remaining=skin.skeleton.bones.size();while(b<skin.skeleton.bones.size()&&remaining--){if(b==arm->second){weight+=vertex.weights[k];break;}const auto parent=skin.skeleton.bones[b].parent;if(parent<0)break;b=static_cast<std::size_t>(parent);}
  }
  if(weight<10){error=std::string("Incomplete PB hand export: no weighted ")+side+" mesh; re-export the arm skin/bone palette";return false;}
 }
 const bool oneBased=reference.skeleton.boneByName.contains("j_metaindex_le_1")&&!reference.skeleton.boneByName.contains("j_pinky_le_0");
 const auto aliasSource=[&](std::string name,std::string alias){
  auto it=originalMap.find(name);if(it==originalMap.end())throw std::runtime_error("COD reference missing "+name);
  reference.skeleton.bones[it->second].name=alias;reference.skeleton.boneByName[alias]=it->second;driver.skeleton.boneByName[alias]=it->second;
 };
 try{
  const auto root=originalMap.contains("j_spinelower")?"j_spinelower":originalMap.contains("tag_origin")?"tag_origin":"tag_view";
  aliasSource(root,"b_Spine");
  for(auto side:{std::pair{"le","Left"},std::pair{"ri","Right"}}){
   const std::string d=side.first,s=side.second;
   aliasSource("j_shoulder_"+d,"b_"+s+"Arm");aliasSource("j_elbow_"+d,"b_"+s+"ForeArm");aliasSource("j_wrist_"+d,"b_"+s+"Hand");
   // The roll driver may be absent; the forearm's frame is then authoritative.
   const auto roll=originalMap.contains("j_elbow_bulge_"+d)?"j_elbow_bulge_"+d:"j_elbow_"+d;
   const auto rollIndex=originalMap.at(roll);reference.skeleton.boneByName["b_"+s+"ForeArmRoll"]=rollIndex;driver.skeleton.boneByName["b_"+s+"ForeArmRoll"]=rollIndex;
   for(auto finger:{std::pair{"thumb","Thumb"},std::pair{"index","Index"},std::pair{"mid","Middle"},std::pair{"ring","Ring"},std::pair{"pinky","Pinky"}})for(int j=0;j<3;++j)aliasSource("j_"+std::string(finger.first)+"_"+d+"_"+std::to_string(j+(oneBased?1:0)),"b_"+s+finger.second+std::to_string(j+1));
  }
  skin.skeleton.boneByName.clear();skin.skeleton.boneByCanonicalName.clear();
  for(std::size_t i=0;i<skin.skeleton.bones.size();++i){auto& b=skin.skeleton.bones[i];
   for(auto side:{std::pair{"L ","le"},std::pair{"R ","ri"}}){
    const std::string p=side.first,d=side.second;
    for(auto joint:{std::pair{"UpperArm","shoulder"},std::pair{"Forearm","elbow"},std::pair{"Hand","wrist"}})if(b.name==p+joint.first)b.name="j_"+std::string(joint.second)+"_"+d;
    for(auto finger:{std::pair{"Thumb","thumb"},std::pair{"Index","index"},std::pair{"Middle","mid"},std::pair{"Ring","ring"},std::pair{"Little","pinky"}})for(int j=1;j<=3;++j)if(b.name==p+finger.first+std::to_string(j))b.name="j_"+std::string(finger.second)+"_"+d+"_"+std::to_string(j-1);
   }skin.skeleton.boneByName[b.name]=i;skin.skeleton.boneByCanonicalName[canonicalName(b.name)]=i;
  }
  // Request volume-preserving finger fitting without marking COD playback PB.
  const bool flag=driver.pointBlankNativeCentimetres;driver.pointBlankNativeCentimetres=true;
  const bool ok=codm::fitPreparedHands(driver,reference,std::move(skin),pbHands.sourceName(),error);driver.pointBlankNativeCentimetres=flag;
  if(!ok){driver.skeleton.boneByName=originalMap;return false;}
  return true;
 }catch(const std::exception& e){driver.skeleton.boneByName=originalMap;error=e.what();return false;}
}
}
