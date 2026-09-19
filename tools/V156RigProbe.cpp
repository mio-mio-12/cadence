#include "scene/CastScene.h"
#include "cast/CastDocument.h"
#include "scene/PointBlankWorld.h"
#include <iostream>
#include <filesystem>
int main(){
 auto source=std::make_shared<scene::CastScene>(scene::buildScene(cast::Document::load("D:/Editing/COD Resource/3D Rip/saluki/exported_files/bo2/models/playermodels/isa/c_usa_mp_isa_smg_fb/c_usa_mp_isa_smg_fb_LOD0.cast"),false));
 scene::appendAnimations(cast::Document::load("D:/Editing/COD Resource/3D Rip/saluki/exported_files/bo2/animations/pb/stand/pistol/pb_stand_alert_pistol.cast"),*source);
 for(const auto& b:source->skeleton.bones)if(b.name.find("_ri")!=std::string::npos){auto p=scene::transformPoint(b.restGlobal,{});std::cout<<"SOURCE "<<b.name<<" bind="<<p.x<<","<<p.y<<","<<p.z<<"\n";}
 const std::filesystem::path root="D:/Editing/COD Resource/3D Rip/saluki/exported_files/codm/models";
 for(auto&f:std::filesystem::directory_iterator(root))if(f.path().extension()==".cast"&&f.path().filename().string().starts_with("c_codm_player_")){
  auto s=scene::buildScene(cast::Document::load(f.path()),false);
  std::cout<<f.path().filename()<<" bounds height="<<s.bounds.maximum.z-s.bounds.minimum.z<<" bones="<<s.skeleton.bones.size()<<"\n";
  for(auto&b:s.skeleton.bones){auto p=scene::transformPoint(b.restGlobal,{});std::cout<<b.name<<" parent="<<b.parent<<" xyz="<<p.x<<","<<p.y<<","<<p.z<<" scale="<<b.restLocal.scale.x<<","<<b.restLocal.scale.y<<","<<b.restLocal.scale.z<<"\n";}
 }
}
