#include "scene/CodmWorldWeapon.h"
#include "scene/PointBlankNative.h"
#include "app/WorldWeaponAssembly.h"
#include "assets/AssetCatalog.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>
#include <fstream>
int main(){
 const std::filesystem::path root="D:/Editing/COD Resource/3D Rip/saluki/exported_files",out="diagnostics/v145";
 std::filesystem::create_directories(out);std::ofstream log(out/"world-models.txt");
 auto bo=scene::buildScene(cast::Document::load(root/"bo2/models/playermodels/isa/c_usa_mp_isa_assault_fb/c_usa_mp_isa_assault_fb_LOD0.cast"));
 auto pb=scene::buildScene(cast::Document::load(root/"pointblank/models/playermodels/SWAT/playermode_SWAT_Male_fb/playermode_SWAT_Male_fb.cast"));
 assets::Catalog catalog;std::string error;if(!assets::appendScan(root/"codm","codm",catalog,error))return 1;
 int worlds=0,missing=0,passed=0;
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,720,"CODM world test",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 render::StageRenderer renderer;if(!renderer.initialize(error))return 4;renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.08f,.08f,.1f,1});
 for(auto& asset:catalog.entries){if(asset.role!=assets::Role::WorldWeapon)continue;++worlds;const auto stem="viewmodel_"+asset.name.substr(11);const auto view=root/"codm/models"/(stem+".cast");std::filesystem::path idle;
  for(auto suffix:scene::codm::worldReferenceSuffixes){auto p=root/"codm/animations"/(stem+suffix);if(std::filesystem::exists(p)){idle=p;break;}}
  log<<"reference="<<idle.filename()<<"\n";
  if(!std::filesystem::is_regular_file(view)||!std::filesystem::is_regular_file(idle)){log<<asset.name<<" missing exact view/idle\n";++missing;continue;}
  const auto worldDoc=cast::Document::load(asset.path),viewDoc=cast::Document::load(view),idleDoc=cast::Document::load(idle);
  for(int family=0;family<2;++family){auto actor=family?pb:bo;const auto mount=scene::codm::worldMount(worldDoc,viewDoc,idleDoc,actor.skeleton,error);if(!mount){log<<asset.name<<" "<<family<<" FAIL "<<error<<"\n";++missing;continue;}
   scene::Vec3 p,s;scene::Quat q;scene::decomposeAffine(mount->transform,p,q,s);if(std::abs(s.x-100)>0.02f||std::abs(s.y-100)>0.02f||std::abs(s.z-100)>0.02f)return 5;
   scene::appendAttachment(worldDoc,actor,mount->bone,asset.name);cadence::world_weapon::setTransform(actor.attachments.back(),mount->transform);
   const auto rebuilt=actor.attachments.back().localMatrix();float e=0;for(int k=0;k<16;++k)e=std::max(e,std::abs(rebuilt.v[k]-mount->transform.v[k]));if(e>.001f)return 6;
   log<<asset.name<<" "<<(family?"PB":"BO2")<<" mount="<<actor.skeleton.bones[mount->bone].name<<" scale="<<s.x<<" roundtrip="<<e<<"\n";++passed;
   const bool detailed=asset.name=="worldmodel_smg_pdw57_Default"||asset.name=="worldmodel_pistol_50gs_Default"||asset.name=="worldmodel_sniper_locus";
   auto source=std::make_shared<scene::CastScene>(bo);
   const bool pistol=asset.name.find("pistol")!=std::string::npos;
   auto clip=root/(pistol?"bo2/animations/pb/stand/pistol/pb_stand_alert_pistol.cast":"bo2/animations/pb/stand/generic/pb_stand_alert.cast");
   if(!std::filesystem::exists(clip)){log<<"MISSING pose "<<clip<<"\n";return 7;}
   scene::appendAnimations(cast::Document::load(clip),*source);if(source->animations.empty())return 8;
   actor.animations=source->animations;if(family)scene::pointblank::bridgeWorld(actor,0,source,0);
   if(!renderer.loadScene(actor,error))return 9;
   for(int sample=0;sample<(detailed?3:1);++sample){auto pose=actor.samplePose(0,actor.animations[0].durationFrames*sample*.45f);const auto center=scene::transformPoint(pose[mount->bone],{});
    for(int angle=0;angle<2;++angle){auto vp=scene::perspective(45*scene::kPi/180,960.f/720,1.f,3000.f)*scene::lookAt(center+(angle?scene::Vec3{90,110,25}:scene::Vec3{90,-110,25}),center,{0,0,1});renderer.render(actor,pose,vp,960,720,false,false,false);if(!renderer.saveColorPng(out/(asset.name+"_"+(family?"PB":"BO2")+"_"+std::to_string(sample)+"_"+std::to_string(angle)+".png"),error))return 10;}
   }
  }
 }
 log<<"worlds="<<worlds<<" mounts="<<passed<<" unavailable="<<missing<<"\n";std::cout<<"worlds="<<worlds<<" mounts="<<passed<<" unavailable="<<missing<<"\n";
 renderer.shutdown();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();return missing?11:0;
}
