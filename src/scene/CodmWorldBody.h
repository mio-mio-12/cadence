#pragma once
#include "scene/CodmNative.h"
#include <map>
namespace scene::codm {
inline bool worldBody(const Skeleton& s){return s.boneByName.contains("Bip01")&&s.boneByName.contains("b_Hips")&&s.boneByName.contains("b_RightLegUpper")&&s.boneByName.contains("b_LeftHand");}
inline std::string worldSemantic(const std::string& name){
 static const auto names=[](){std::map<std::string,std::string> m{{"b_Root","tag_origin"},{"Bip01","j_mainroot"},{"b_Hips","pelvis"},{"b_Spine2","j_spinelower"},{"b_Spine1","j_spineupper"},{"b_Spine","j_spine4"},{"b_Neck","j_neck"},{"b_Head","j_head"}};
  for(auto side:{std::pair{"Left","le"},std::pair{"Right","ri"}}){const auto prefix="b_"+std::string(side.first),suffix=std::string(side.second);
   for(auto joint:{std::pair{"Clav","clavicle"},std::pair{"Arm","shoulder"},std::pair{"ForeArm","elbow"},std::pair{"Hand","wrist"},std::pair{"LegUpper","hip"},std::pair{"Leg","knee"},std::pair{"Ankle","ankle"},std::pair{"Toe","ball"}})m[prefix+joint.first]="j_"+std::string(joint.second)+"_"+suffix;
   m[prefix+"Weapon"]="tag_weapon_"+std::string(side.first==std::string("Left")?"left":"right");
   const char* fingers[]={"thumb","index","mid","ring","pinky"};for(int f=0;f<5;++f)for(int n=0;n<3;++n)m[prefix+"Finger"+std::to_string(f)+(n?std::to_string(n):std::string{})]="j_"+std::string(fingers[f])+"_"+suffix+"_"+std::to_string(n+1);
  }return m;}();
 if(auto i=names.find(name);i!=names.end())return i->second;return name;
}
inline void prepareWorldBody(CastScene& s,const std::filesystem::path& path){
 if(!path.stem().string().starts_with("c_codm_player_")||!worldBody(s.skeleton))return;
 std::string error;if(!nativeMetres(path,error)){s.warnings.push_back(error);return;}
 // Full-body art is physical metre-sized geometry: never use the 1P fit factor.
 if(!s.codmNativeCentimetres){
  normalizeToCentimetres(s);
  // Native characters face -Y; Cadence world actors face +X. This is a
  // rigid world-character basis change, not the first-person fitting scale.
  const auto q=fromEulerRadians({0,0,kPi*.5f});const auto basis=rotation(q);
  for(auto& m:s.meshes){for(auto& v:m.vertices){v.position=transformPoint(basis,v.position);v.normal=transformPoint(basis,v.normal);}m.modelTransform=basis*m.modelTransform*inverseAffine(basis);}
  for(auto& b:s.skeleton.bones){b.restGlobal=basis*b.restGlobal;b.inverseBind=b.inverseBind*inverseAffine(basis);if(b.parent<0){b.restLocal.position=transformPoint(basis,b.restLocal.position);b.restLocal.rotation=multiply(q,b.restLocal.rotation);}}
 }
 for(size_t i=0;i<s.skeleton.bones.size();++i){const auto& name=s.skeleton.bones[i].name;const auto semantic=worldSemantic(name);if(semantic!=name)s.skeleton.boneByCanonicalName[semantic]=i;}
 s.warnings.push_back("CODM world body: metres -> centimetres exactly once; native bind helpers preserved");
}
}
