#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" status="<<a.status<<"\n";return 1;}}while(false)
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"PB world checks",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;a.window=w;a.deferSceneUpload=true;std::string error;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(auto g:{"pointblank","bo2","mw3"})CHECK(assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error));
 const auto out=std::filesystem::path("diagnostics/v141/world");std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");
 auto idx=[&](std::string name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return SIZE_MAX;};
 a.selectedWeaponAsset=idx("viewmodel_pistol_DesertEagle");CHECK(a.selectedWeaponAsset<a.assetCatalog.entries.size());a.classPrimaryAsset=a.selectedWeaponAsset;
 a.enemyBotCount=5;a.botTeamSide=0;a.botUseSalukiWeaponPool=false;a.botSystemMode=0;a.actorMode=a.playing=a.gameplayLogic=true;a.actorPosition={0,0,0};
 for(auto game:{"pointblank","bo2","mw3"})for(auto model:{"playermode_SWAT_Male_fb","playermode_SWAT_Female_SWAT-Sniper_fb","playermode_SWAT_Male_Madness_fb"}){
  a.botModelAsset=idx(model);CHECK(a.botModelAsset<a.assetCatalog.entries.size());a.botAnimationGame=game;a.botFactionModelVariety=false;rebuildBotActors(a);CHECK(a.bots.size()==5&&a.botActorScene);CHECK(!a.botActorScene->attachments.empty());
  const auto start=a.bots[0].position;for(int frame=0;frame<240;++frame){a.gameplayClock+=1.f/60;updateBotActors(a,1.f/60);}
  CHECK(a.botActorPoses.size()==5);for(const auto& p:a.botActorPoses){CHECK(p.size()==a.botActorScene->skeleton.bones.size());for(const auto& m:p)for(auto v:m.v)CHECK(std::isfinite(v));}
  auto& actor=*a.botActorScene;size_t animated{};for(const auto& clip:actor.animations)if(clip.coldWarWorldPose&&!clip.tracks.empty())++animated;CHECK(animated>0);
  CHECK(a.renderer.loadScene(actor,error));a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.08f,.08f,.1f,1});
  for(size_t bot=0;bot<5;++bot){const auto center=gameplay::bot::presentationPosition(a.bots[bot]);const auto vp=scene::perspective(55*scene::kPi/180,960.f/540,1.f,4000.f)*scene::lookAt(center+scene::Vec3{230,-260,135},center+scene::Vec3{0,0,85},{0,0,1});a.renderer.render(actor,a.botActorPoses[bot],vp,960,540,false,false,false);CHECK(a.renderer.saveColorPng(out/(std::string(model)+"_"+game+"_bot"+std::to_string(bot)+".png"),error));}
  CHECK(scene::length(a.bots[0].position-start)>10);
  report<<model<<" source="<<game<<" clips="<<actor.animations.size()<<" adapted="<<animated<<" movement="<<scene::length(a.bots[0].position-start)<<" primary="<<actor.animations[a.bots[0].animation].sourceName<<"\n";report.flush();
  if(std::string(game)=="pointblank"){
   for(const auto* suffix:{"handgun_Common_Up_Attack.cast","handgun_DesertEagle_Up_Reload.cast","common_death_Stand_DeathFrontA1.cast"}){
    auto it=std::find_if(actor.animations.begin(),actor.animations.end(),[&](const auto& c){return c.sourceName.ends_with(suffix)&&c.sourceName.find("SWAT_Male_")!=std::string::npos;});CHECK(it!=actor.animations.end());const auto clip=it-actor.animations.begin();
    for(int frame=0;frame<=4;++frame){auto p=actor.samplePoseSlots(a.bots[0].animation,0,{{{{static_cast<size_t>(clip),it->durationFrames*frame/4.f,1,scene::LayerMode::Override,false}}}});for(auto&m:p)for(auto v:m.v)CHECK(std::isfinite(v));const auto vp=scene::perspective(45*scene::kPi/180,960.f/540,1.f,2000.f)*scene::lookAt({220,-240,145},{0,0,90},{0,0,1});a.renderer.render(actor,p,vp,960,540,false,false,false);CHECK(a.renderer.saveColorPng(out/(std::string(model)+"_"+suffix+"_"+std::to_string(frame)+".png"),error));}
    report<<"AUTHORED "<<it->sourceName<<" checked five frames\n";report.flush();
   }
  }
  a.autoPlayerModel=false;a.manualPlayerModelAsset=a.botModelAsset;a.playerWorldAnimationGame=game;configureClassActor(a);CHECK(a.hiddenWorldActor);auto playerPose=evaluateHiddenWorldActorPose(a);CHECK(!playerPose.empty());for(auto&m:playerPose)for(auto v:m.v)CHECK(std::isfinite(v));
 }
 // PB authored poses on legacy bodies use the same source-space adapter.
 for(auto game:{"bo2","mw3"}){auto it=std::find_if(a.assetCatalog.entries.begin(),a.assetCatalog.entries.end(),[&](const auto& x){return x.game==game&&x.role==assets::Role::PlayerModel;});CHECK(it!=a.assetCatalog.entries.end());a.botModelAsset=it-a.assetCatalog.entries.begin();a.botAnimationGame="pointblank";rebuildBotActors(a);CHECK(a.bots.size()==5&&a.botActorScene);updateBotActors(a,.02f);for(auto& p:a.botActorPoses)for(auto& m:p)for(auto v:m.v)CHECK(std::isfinite(v));CHECK(a.renderer.loadScene(*a.botActorScene,error));for(size_t b=0;b<5;++b){const auto center=gameplay::bot::presentationPosition(a.bots[b]);const auto vp=scene::perspective(45*scene::kPi/180,960.f/540,1.f,3000.f)*scene::lookAt(center+scene::Vec3{220,-240,145},center+scene::Vec3{0,0,85},{0,0,1});a.renderer.render(*a.botActorScene,a.botActorPoses[b],vp,960,540,false,false,false);CHECK(a.renderer.saveColorPng(out/(std::string("PB_to_")+game+"_bot"+std::to_string(b)+".png"),error));}report<<"PB -> "<<game<<" bots=5 passed\n";report.flush();}
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();std::cout<<"v141 world checks passed\n";
}
