#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(){
 CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,720,"Grip validation",nullptr,nullptr);CHECK(w);glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;a.window=w;a.deferSceneUpload=true;std::string error;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(auto g:{"pointblank","bo2"})CHECK(assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error));
 const auto out=std::filesystem::path("diagnostics/v151");std::filesystem::create_directories(out);std::ofstream report(out/"grips.txt");
 auto idx=[&](const std::string&name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return SIZE_MAX;};
 a.enemyBotCount=5;a.botTeamSide=0;a.botUseSalukiWeaponPool=false;a.actorMode=a.playing=a.gameplayLogic=true;a.actorPosition={0,0,0};a.botAutoRespawn=false;a.botFactionModelVariety=false;
 a.botModelAsset=idx("c_usa_mp_isa_smg_fb_LOD0");CHECK(a.botModelAsset<a.assetCatalog.entries.size());a.botAnimationGame="bo2";a.botSystemMode=0;
 for(auto weapon:{"t6_wpn_ar_an94_view_LOD0","viewmodel_pistol_ColtPython","viewmodel_AR_AK-47_DualMag","viewmodel_AR_SC-2010"}){
  a.selectedWeaponAsset=idx(weapon);CHECK(a.selectedWeaponAsset<a.assetCatalog.entries.size());a.classPrimaryAsset=a.selectedWeaponAsset;
  a.autoPlayerModel=false;a.manualPlayerModelAsset=a.botModelAsset;a.playerWorldAnimationGame="bo2";configureClassActor(a);
  CHECK(a.hiddenWorldActor&&!a.hiddenWorldActor->attachments.empty());
  if(std::string(weapon).starts_with("viewmodel_"))CHECK(a.hiddenWorldActor->attachments[0].boneIndex==a.hiddenWorldActor->skeleton.boneByCanonicalName.at("j_wrist_ri"));
  rebuildBotActors(a);CHECK(a.bots.size()==5&&a.botActorScene);auto&actor=*a.botActorScene;CHECK(!actor.attachments.empty());
  if(std::string(weapon).starts_with("viewmodel_"))CHECK(actor.attachments[0].boneIndex==actor.skeleton.boneByCanonicalName.at("j_wrist_ri"));
  CHECK(a.renderer.loadScene(actor,error));a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.13f,.14f,.16f,1});
  const bool pistol=std::string(weapon).find("pistol")!=std::string::npos;
  for(const auto&cue:std::vector<std::string>{pistol?"pb_stand_alert_pistol.cast":"pb_stand_alert.cast",pistol?"pb_combatrun_back_loop_pistol.cast":"pb_combatrun_forward_loop.cast",pistol?"pb_crouch_alert_pistol.cast":"pb_crouch_alert.cast"}){
   auto clip=std::find_if(actor.animations.begin(),actor.animations.end(),[&](const auto&c){return c.sourceName==cue;});if(clip==actor.animations.end()){report<<"missing cue "<<cue<<"\n";continue;}
   for(int frame=0;frame<3;++frame){const float f=clip->durationFrames*frame/2.f;auto pose=actor.samplePose(clip-actor.animations.begin(),f);const auto wrist=actor.skeleton.boneByCanonicalName.at("j_wrist_ri");const auto center=scene::transformPoint(pose[wrist],{});
    for(int side=0;side<4;++side){const auto offset=(side%2?scene::Vec3{110,130,40}:scene::Vec3{110,-130,40})*(side<2?1.f:.42f);const auto vp=scene::perspective(45*scene::kPi/180,960.f/720,1,4000)*scene::lookAt(center+offset,center,{0,0,1});a.renderer.render(actor,pose,vp,960,720,false,false,false);CHECK(a.renderer.saveColorPng(out/(std::string(weapon)+"_"+cue+"_"+std::to_string(frame)+"_"+std::to_string(side)+".png"),error));}
   }
  }
  report<<weapon<<" mounted to "<<actor.skeleton.bones[actor.attachments[0].boneIndex].name<<"\n";
 }
 // Exercise the real viewport: a nearby bot must not steer a stationary camera.
 a.actorThirdPerson=true;a.actorYaw=0;a.cameraPitch=0;a.actorRenderPosition=a.actorPosition={0,0,0};a.actorPresentationZValid=false;
 auto&io=ImGui::GetIO();io.DisplaySize={960,720};io.DeltaTime=1.f/60;unsigned char*pixels;int fw,fh;io.Fonts->GetTexDataAsRGBA32(&pixels,&fw,&fh);
 for(float distance:{100.f,500.f,5000.f,100.f}){for(auto&bot:a.bots)bot.position={distance,0,0};ImGui::NewFrame();ImGui::Begin("Camera test");drawViewport(a,960,720);ImGui::End();ImGui::EndFrame();CHECK(a.lastRenderedCameraValid);CHECK(std::abs(a.lastRenderedCameraRotationDegrees.x)<.001f&&std::abs(a.lastRenderedCameraRotationDegrees.y)<.001f);}
 report<<"Shoulder target-crossing camera rotation: PASS\n";
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();std::cout<<"PASS ISA SMG PB weapon and camera matrix\n";
}
