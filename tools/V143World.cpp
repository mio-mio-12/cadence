#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" status="<<a.status<<"\n";return 1;}}while(false)
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"PB v143 checks",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;a.window=w;a.deferSceneUpload=true;std::string error;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(auto g:{"pointblank","bo2","bo2_sp"})CHECK(assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error));
 const auto out=std::filesystem::path("diagnostics/v143");std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");
 auto idx=[&](std::string name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return SIZE_MAX;};
 a.selectedWeaponAsset=idx("viewmodel_pistol_DesertEagle");CHECK(a.selectedWeaponAsset<a.assetCatalog.entries.size());a.classPrimaryAsset=a.selectedWeaponAsset;
 a.enemyBotCount=5;a.botTeamSide=0;a.botUseSalukiWeaponPool=false;a.actorMode=a.playing=a.gameplayLogic=true;a.actorPosition={0,0,0};a.botAutoRespawn=false;
 for(auto game:{"pointblank","bo2+bo2_sp"})for(auto model:{"playermode_SWAT_Male_fb","playermode_SWAT_Female_SWAT-Sniper_fb","playermode_SWAT_Male_Madness_fb"}){
  a.botModelAsset=idx(model);a.botAnimationGame=game;a.botSystemMode=std::string(game)=="pointblank"?0:1;a.botFactionModelVariety=false;rebuildBotActors(a);CHECK(a.bots.size()==5&&a.botActorScene);
  auto&actor=*a.botActorScene;report<<model<<" set="<<game<<" meshes="<<actor.meshes.size()<<" clips="<<actor.animations.size()<<" deaths="<<a.botDeathChoices.size()<<"\n";report.flush();CHECK(!a.botDeathChoices.empty());
  CHECK(a.renderer.loadScene(actor,error));a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.08f,.08f,.1f,1});
  for(const auto& c:a.botDeathChoices)CHECK(c.key.find("death-scythe")==std::string::npos);
  for(const auto&m:actor.meshes)CHECK(!scene::pointblank::detachedPresentationMesh(m,actor.skeleton));
  auto clipIndex=[&](const std::string& n){for(size_t i=0;i<actor.animations.size();++i)if(actor.animations[i].sourceName==n)return i;return SIZE_MAX;};
  const auto stand=clipIndex("pb_stand_alert_pistol.cast");CHECK(stand!=SIZE_MAX);
  for(const auto* target:{"pb_crouch_alert_pistol.cast","pb_pistol_run_fast.cast","ai_stand_exposed_extendedpain_b.cast"}){
   const auto clip=clipIndex(target);if(clip==SIZE_MAX){CHECK(std::string(game)=="pointblank");continue;}CHECK(actor.animations[clip].coldWarWorldPose);
   const float frame=actor.animations[clip].durationFrames*.35f;const auto end=actor.sampleLocalPose(clip,frame);float maxError=0;
   for(int f=0;f<=24;++f){const float weight=f/24.f;const auto local=actor.sampleLocalPoseSlots(stand,0,{{{{clip,frame,weight,scene::LayerMode::Override,false}}}});const auto p=actor.globalPose(local);
    if(f==24)for(size_t b=0;b<local.size();++b){maxError=std::max(maxError,scene::length(local[b].position-end[b].position));maxError=std::max(maxError,1-std::abs(local[b].rotation.x*end[b].rotation.x+local[b].rotation.y*end[b].rotation.y+local[b].rotation.z*end[b].rotation.z+local[b].rotation.w*end[b].rotation.w));}
    if(f%6==0){const auto vp=scene::perspective(45*scene::kPi/180,960.f/540,1.f,3000.f)*scene::lookAt({230,-260,145},{0,0,85},{0,0,1});a.renderer.render(actor,p,vp,960,540,false,false,false);CHECK(a.renderer.saveColorPng(out/(std::string(model)+"_"+game+"_blend_"+target+"_"+std::to_string(f)+".png"),error));}
   }report<<"blend "<<target<<" endpoint error="<<maxError<<"\n";report.flush();CHECK(maxError<.001f);
  }
  for(int f=0;f<60;++f)updateBotActors(a,1.f/60);
  for(size_t i=0;i<5;++i){auto&b=a.bots[i];auto death=botDeathAnimation(actor,b,b.weaponClass,999,&a);CHECK(death);report<<"death "<<actor.animations[*death].sourceName<<" tracks="<<actor.animations[*death].tracks.size()<<"\n";b.previousAnimation=b.animation;b.previousAnimationFrame=b.animationFrame;b.animation=*death;b.animationFrame=0;b.animationBlendElapsed=0;b.alive=false;b.health=0;b.respawnTime=10;b.velocity={};b.actionBlendWeight=b.spScenarioWeight=0;}
  for(int frame=0;frame<=120;++frame){updateBotActors(a,1.f/60);for(auto&b:a.bots)CHECK(actor.animations[b.animation].action==scene::ActionRole::Death);if(frame%30==0){for(size_t i=0;i<5;++i){const auto p=gameplay::bot::presentationPosition(a.bots[i]);const auto vp=scene::perspective(55*scene::kPi/180,960.f/540,1.f,4000.f)*scene::lookAt(p+scene::Vec3{230,-260,135},p+scene::Vec3{0,0,85},{0,0,1});for(auto&m:a.botActorPoses[i])for(auto v:m.v)CHECK(std::isfinite(v));a.renderer.render(actor,a.botActorPoses[i],vp,960,540,false,false,false);CHECK(a.renderer.saveColorPng(out/(std::string(model)+"_"+game+"_death"+std::to_string(i)+"_"+std::to_string(frame)+".png"),error));}}}
  report.flush();
 }
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();std::cout<<"v143 world checks passed\n";
}
