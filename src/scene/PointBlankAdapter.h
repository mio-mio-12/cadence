#pragma once
#include "scene/PointBlankNative.h"
#include "scene/CodmLegacyAdapter.h"
#include "scene/ImportedViewRetarget.h"
#include <map>
namespace scene::pointblank {
inline bool fitHands(CastScene& native,const cast::Document& referenceDocument,const cast::Document& target,std::string& error){
 auto reference=buildScene(referenceDocument,false);
 if(exportedByPb2cast(target)){
  // Shared bone names do not imply matching proportions or forearm rolls.
  // Use the same segment-length and volume-preserving fit as foreign skins;
  // retain the native weapon, driver skeleton and authored animation channels.
  return imported::fitPoseOnlyViewSkin(native,buildScene(target,false),error);
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
