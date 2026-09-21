#include "assets/LocalAssetPaths.h"
#include "scene/CastScene.h"
#include "scene/PointBlankWorld.h"
#include "app/CodmAnimationPolicy.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <set>
#include <fstream>
using Clock=std::chrono::steady_clock;
int main(int argc,char**argv){
 const std::filesystem::path root=cadence::local_assets::exportPath("");
 std::string error;
 if(argc>1&&std::string(argv[1])=="motion"){
  for(const char* stem:{"viewmodel_pistol_50gs","viewmodel_pistol_50gs_GirlsFrontline","viewmodel_pistol_mw11","viewmodel_special_baseversionmp7"}){
   scene::CastScene s;if(!scene::codm::assemble(cast::Document::load(root/"codm/models"/(std::string(stem)+".cast")),cast::Document::load(root/"codm/models/codm_viewhands_C_M_Ghost_1P.cast"),s,error)){std::cout<<stem<<" ASSEMBLY FAILED "<<error<<'\n';continue;}
   for(const char* slot:{"idle","fire","ads_fire","aiming_fire_s","reload"}){
    const auto path=root/"codm/animations"/(std::string(stem)+"_"+slot+".cast");if(!std::filesystem::exists(path)){std::cout<<stem<<" "<<slot<<" MISSING\n";continue;}
    const auto index=s.animations.size();scene::appendAnimations(cast::Document::load(path),s);if(index==s.animations.size()){std::cout<<stem<<" "<<slot<<" REJECTED\n";continue;}
    const auto& a=s.animations[index];float scalarDelta=0,quatDelta=0,poseDelta=0;size_t varying=0;
    for(const auto& t:a.tracks){float delta=0;for(float v:t.scalarValues)delta=std::max(delta,std::abs(v-t.scalarValues.front()));scalarDelta=std::max(scalarDelta,delta);for(auto q:t.rotationValues){const auto r=t.rotationValues.front();const float d=std::min(std::abs(q.x-r.x)+std::abs(q.y-r.y)+std::abs(q.z-r.z)+std::abs(q.w-r.w),std::abs(q.x+r.x)+std::abs(q.y+r.y)+std::abs(q.z+r.z)+std::abs(q.w+r.w));delta=std::max(delta,d);quatDelta=std::max(quatDelta,d);}varying+=delta>1e-6f;}
    float alignedDelta=0;const auto first=s.samplePose(index,0);for(float frame=0;frame<=a.durationFrames;frame+=.25f){const auto p=s.samplePose(index,frame),aligned=cadence::codm_actions::alignedHipBolt(s,first,index,frame,1);for(size_t b=0;b<p.size();++b)for(int k=0;k<16;++k){poseDelta=std::max(poseDelta,std::abs(p[b].v[k]-first[b].v[k]));alignedDelta=std::max(alignedDelta,std::abs(aligned[b].v[k]-first[b].v[k]));}}
    std::cout<<"carrierMotion="<<alignedDelta<<" muzzleExact="<<s.skeleton.boneByName.contains("Muzzle_point")<<" ";
    std::cout<<stem<<" "<<slot<<" frames="<<a.durationFrames<<" tracks="<<a.tracks.size()<<" unmapped="<<a.unmappedCurveCount<<" varying="<<varying<<" scalarDelta="<<scalarDelta<<" quatDelta="<<quatDelta<<" poseDelta="<<poseDelta<<'\n';
   }
  }
  return 0;
 }
 if(argc>1&&std::string(argv[1])=="materials"){
  if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* w=glfwCreateWindow(1280,720,"CODM materials",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
  render::StageRenderer renderer;if(!renderer.initialize(error)){std::cerr<<error;return 2;}
  if(!renderer.setT6SkyboxIwi(cadence::portable::executableDirectory()/"skyboxes/city/sky.iwi",error)){std::cerr<<error;return 7;}
  renderer.setMaterialParameters(.5f,{.8f,.9f,1},1,.85f,209,.14f,6,1,true,.97f,.08f,1,true,1,0,0,1,0,0,.18f,1);
  std::filesystem::create_directories("diagnostics/v167");
  for(const char* name:{"viewmodel_special_raygun","viewmodel_ar_ak47","viewmodel_sniper_locus"}){
   scene::CastScene s;if(!scene::codm::assemble(cast::Document::load(root/"codm/models"/(std::string(name)+".cast")),cast::Document::load(root/"codm/models/codm_viewhands_C_M_Ghost_1P.cast"),s,error)){std::cerr<<error;return 3;}
   auto path=root/"codm/animations"/(std::string(name)+"_idle.cast");if(!std::filesystem::exists(path))path=root/"codm/animations"/(std::string(name)+"_weapon_idle.cast");
   scene::appendAnimations(cast::Document::load(path),s);if(s.animations.empty())return 4;
   const auto pose=s.samplePose(0,s.animations[0].durationFrames*.5f);const auto c=pose[s.skeleton.boneByCanonicalName.at("tag_camera")];
   scene::Vec3 eye{c.v[12],c.v[13],c.v[14]},forward{c.v[0],c.v[1],c.v[2]},up{c.v[8],c.v[9],c.v[10]};
   auto vp=scene::perspective(65*scene::kPi/180,16.f/9,.1f,2000)*scene::lookAtDirection(eye,forward,up);
   renderer.setCameraPosition(eye);renderer.setSun(true,false,{.3f,.4f,-1},1,.5f,{1,1,1},{1,1,1},1024,800,600,{},false,512,600,1600,100);
   for(int workflow:{0,1}){for(auto& mesh:s.meshes)mesh.specularGlossiness=workflow!=0;
    if(!renderer.loadScene(s,error))return 5;renderer.render(s,pose,vp,1280,720,false,false,false);
    if(!renderer.saveColorPng(std::filesystem::path("diagnostics/v167")/(std::string(name)+"_workflow_"+std::to_string(workflow)+".png"),error))return 6;
   }
   std::cout<<name<<" material comparison captured\n";
  }
  renderer.shutdown();glfwDestroyWindow(w);glfwTerminate();return 0;
 }
 if(argc>1&&std::string(argv[1])=="render"){
  if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
  auto* w=glfwCreateWindow(1280,720,"CODM diagnostic",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);glfwSwapInterval(0);
  render::StageRenderer renderer;if(!renderer.initialize(error)){std::cerr<<error;return 2;}
  for(const char* file:{"bo2/models/playermodels/isa/c_usa_mp_isa_smg_fb/c_usa_mp_isa_smg_fb_LOD0.cast","codm/models/c_codm_player_C_M_Ghost_BR_fb.cast","codm/models/c_codm_player_C_F_Scylla_AN94_New_BR_UI_fb.cast","codm/models/c_codm_player_C_M_Reaper_Ashura_BR_UI_fb.cast"}){
   auto s=scene::buildScene(cast::Document::load(root/file));
   scene::CastScene empty;
   if(!renderer.loadScene(empty,error)||!renderer.loadAuxiliaryScenes(nullptr,nullptr,&s,error)){std::cerr<<error;return 2;}
   std::vector<std::vector<scene::Mat4>> poses(6,s.samplePose(0,0));std::vector<int> variants(6,-1);
   for(int i=0;i<6;++i)for(auto& m:poses[i]){m.v[12]+=(i%3-1)*100;m.v[13]+=(i/3)*100;}
   const scene::Vec3 camera{450,-550,250};renderer.setCameraPosition(camera);
   const auto vp=scene::perspective(65*scene::kPi/180,1280.f/720,1,5000)*scene::lookAt(camera,{0,60,85},{0,0,1});
   for(int lit:{0,1}){
    renderer.setSun(lit,lit,{.4f,.3f,-1},1,.4f,{1,1,1},{1,1,1},1024,800,600,{},false,512,600,1600,100);
    for(int i=0;i<30;++i)renderer.render(empty,{},vp,1280,720,false,false,false,&s,&poses,&variants,nullptr,nullptr,false);
    glFinish();const auto start=Clock::now();
    for(int i=0;i<120;++i){renderer.render(empty,{},vp,1280,720,false,false,false,&s,&poses,&variants,nullptr,nullptr,false);glFinish();}
    std::cout<<file<<" lit="<<lit<<" six_bots_ms="<<std::chrono::duration<double,std::milli>(Clock::now()-start).count()/120<<" gpu_ms="<<renderer.gpuFrameMilliseconds()<<std::endl;
   }
  }
  renderer.shutdown();glfwDestroyWindow(w);glfwTerminate();return 0;
 }
 std::set<std::string> suffixes;
 std::vector<std::string> models;for(const auto& f:std::filesystem::directory_iterator(root/"codm/models"))if(f.path().extension()==".cast")models.push_back(f.path().stem().string());
 const std::filesystem::path reportFolder=argc>2?argv[2]:"diagnostics/v167";
 std::filesystem::create_directories(reportFolder);std::ofstream report(reportFolder/"codm_assets.txt");
 auto* original=std::cout.rdbuf(report.rdbuf());size_t weapons=0,missing=0;
 for(const auto& f:std::filesystem::directory_iterator(root/"codm/models")){
  const auto name=f.path().stem().string();if(f.path().extension()!=".cast")continue;
  if(argc>1&&name.find(argv[1])==std::string::npos)continue;
  if(!name.starts_with("c_codm_player_")&&!name.starts_with("viewmodel_"))continue;
  auto s=scene::buildScene(cast::Document::load(f.path()));size_t verts=0,tris=0;
  for(const auto&m:s.meshes){verts+=m.vertices.size();tris+=m.indices.size()/3;}
  std::cout<<"MODEL "<<name<<" bones="<<s.skeleton.bones.size()<<" meshes="<<s.meshes.size()<<" vertices="<<verts<<" triangles="<<tris<<"\n";
  if(name=="viewmodel_special_raygun")for(const auto&m:s.meshes)std::cout<<"MAT "<<m.materialName<<" spec="<<m.specularPath<<" metal="<<m.metalnessPath<<" lens="<<m.lens<<"\n";
  if(!name.starts_with("viewmodel_"))continue;
  s.codmNativeCentimetres=true;s.codmNativeWeaponStem=name;
  for(const auto&a:std::filesystem::directory_iterator(root/"codm/animations")){
   auto n=a.path().stem().string();if(a.path().extension()!=".cast"||!cadence::codm_actions::belongsToWeapon(n,name,models)||n.ends_with("_camera"))continue;
   // Only summarize suffixes belonging to this exact exported hierarchy.
   const auto meta=scene::codm::metadata(a.path());
   scene::appendAnimations(cast::Document::load(a.path()),s);
   suffixes.insert(n.substr(name.size()+1));
  }
  cadence::codm_actions::prepare(s);weapon::Profile p;cadence::codm_actions::populate(s,p,name);
  ++weapons;for(const char* slot:{"idle","fire","reload","pullout","putaway","ads_up","ads_down"})if(!p.animations.contains(slot))++missing;
  std::cout<<"ACTIONS "<<name<<" clips="<<s.animations.size();
  for(const char*slot:{"idle","fire","ads_fire","reload","reload_empty","pullout","putaway","ads_up","ads_down","rechamber","ads_rechamber","sprint_in","sprint_loop","sprint_out","jump_takeoff","jump_land"})std::cout<<" "<<slot<<"="<<(p.animations.contains(slot)?p.animations.at(slot):"MISSING");
  std::cout<<"\n";
 }
 for(const auto&s:suffixes)std::cout<<"SUFFIX "<<s<<"\n";
 std::cout.rdbuf(original);std::cout<<"Audited "<<weapons<<" weapons; "<<missing<<" missing slots. Report: "<<(reportFolder/"codm_assets.txt")<<"\n";
}
