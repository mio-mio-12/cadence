#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include <iostream>
#include <functional>
int main(int argc,char** argv){
 if(argc==3&&std::string_view(argv[1])=="--verify"){
  const std::filesystem::path root=argv[2];int failures=0;
  for(const auto* knifePath:{"bo2/models/weapons/view/melee/t6_wpn_knife_base_view/t6_wpn_knife_base_view_LOD0.cast","ghosts/models/weapons/view/melee/viewmodel_knife_iw6/viewmodel_knife_iw6_LOD0.cast"}){
  const auto knife=cast::Document::load(root/knifePath);
  for(const auto* hands:{"bo2/models/viewhands/seal6/c_usa_mp_seal6_longsleeve_viewhands/c_usa_mp_seal6_longsleeve_viewhands_LOD0.cast","mw3/models/viewhands/viewhands_sas/viewhands_sas_LOD0.cast"}){
   auto rig=scene::buildScene(cast::Document::load(root/hands));
   const auto gun=cast::Document::load(root/"bo2/models/weapons/view/pistols/t6_wpn_pistol_fnp45_view/t6_wpn_pistol_fnp45_view_LOD0.cast");
   scene::appendRigModel(gun,rig,"t6_wpn_pistol_fnp45_view");
   // Existing left weapon must not capture the auxiliary knife root.
   const auto gunIndex=rig.skeleton.boneByCanonicalName.at("j_gun");
   rig.skeleton.boneByCanonicalName["j_gun1"]=gunIndex;
   const auto flash=rig.skeleton.boneByCanonicalName.at("tag_flash");
   scene::appendRigModel(knife,rig,std::filesystem::path(knifePath).stem().string(),std::nullopt,1.f,true);
   const auto k=rig.rigParts.back().rootBones.front(),socket=rig.skeleton.boneByCanonicalName.at("tag_knife_attach");
   if(!rig.hiddenBones.contains(k))++failures;
   if(k==gunIndex||rig.skeleton.bones[k].parent!=int(socket)||rig.skeleton.boneByCanonicalName.at("tag_flash")!=flash)++failures;
   for(const auto* file:{"viewmodel_m4m203_knife_melee_1.cast","viewmodel_m4m203_knife_melee_2.cast"}){
    const auto before=rig.animations.size();scene::appendAnimations(cast::Document::load(root/"bo2/animations/viewmodel/m4m203"/file),rig);
    if(rig.animations.size()==before){++failures;continue;}
    for(unsigned frame=0;frame<=rig.animations.back().durationFrames;++frame){const auto pose=rig.samplePose(before,float(frame));for(int c=0;c<16;++c)if(std::abs(pose[k].v[c]-pose[socket].v[c])>.001f)++failures;}
   }
   std::cout<<hands<<" knife parent="<<rig.skeleton.bones[socket].name<<"\n";
  }
  }
  std::cout<<"Melee mount failures: "<<failures<<"\n";return failures?1:0;
 }
 for(int f=1;f<argc;++f){
  const auto doc=cast::Document::load(argv[f]);auto rig=scene::buildScene(doc);
  std::cout<<argv[f]<<"\n";
  for(std::size_t i=0;i<rig.skeleton.bones.size();++i){const auto& b=rig.skeleton.bones[i];if(b.name.find("tag_")==0||b.name.find("gun")!=std::string::npos||b.name.find("wrist")!=std::string::npos)std::cout<<i<<" "<<b.name<<" parent="<<b.parent<<" pos="<<b.restLocal.position.x<<","<<b.restLocal.position.y<<","<<b.restLocal.position.z<<"\n";}
  std::function<void(const cast::Node&)> walk=[&](const cast::Node& n){if(n.identifier==0x76727563){const auto p=n.findProperty("nn");if(p&&p->stringValue&&(p->stringValue->find("tag_")==0||p->stringValue->find("gun")!=std::string::npos)) {std::cout<<"curve ";for(const auto& v:n.properties)std::cout<<v.name<<"="<<doc.propertyPreview(v,4)<<" ";std::cout<<"\n";}}for(const auto& c:n.children)walk(c);};for(const auto& n:doc.roots())walk(n);
 }
}
