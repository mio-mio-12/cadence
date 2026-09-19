#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(){
 CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,720,"PB world checks",nullptr,nullptr);CHECK(w);glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;a.window=w;a.deferSceneUpload=true;std::string error;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
 for(auto g:{"pointblank","bo2","bo2_sp","mw3"})CHECK(assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error));
 const auto out=std::filesystem::path("diagnostics/v149");std::filesystem::create_directories(out);std::ofstream report(out/"world.txt");
 auto idx=[&](const std::string&name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return SIZE_MAX;};
 a.enemyBotCount=5;a.botTeamSide=0;a.botUseSalukiWeaponPool=false;a.actorMode=a.playing=a.gameplayLogic=true;a.actorPosition={0,0,0};a.botAutoRespawn=false;a.botFactionModelVariety=false;
 for(auto weapon:{"t6_wpn_ar_an94_view_LOD0","t6_wpn_pistol_fiveseven_view_LOD0","viewmodel_m4_iw5_LOD0"})for(auto model:{"playermode_SWAT_Male_fb","playermode_SWAT_Female_SWAT-Sniper_fb"}){
  a.selectedWeaponAsset=idx(weapon);CHECK(a.selectedWeaponAsset<a.assetCatalog.entries.size());a.classPrimaryAsset=a.selectedWeaponAsset;
  a.botModelAsset=idx(model);CHECK(a.botModelAsset<a.assetCatalog.entries.size());a.botAnimationGame="bo2";a.botSystemMode=0;rebuildBotActors(a);CHECK(a.bots.size()==5&&a.botActorScene);
  auto&actor=*a.botActorScene;CHECK(!actor.attachments.empty());CHECK(a.renderer.loadScene(actor,error));a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.13f,.14f,.16f,1});
  for(int f=0;f<30;++f)updateBotActors(a,1.f/60);
  const std::string hold=std::string(weapon).find("pistol")!=std::string::npos?"pb_stand_alert_pistol.cast":"pb_stand_alert.cast";
  for(size_t c=0;c<actor.animations.size();++c)if(actor.animations[c].sourceName==hold){a.botActorPoses[0]=actor.samplePose(c,0);break;}
  for(int side=0;side<2;++side){const auto wrist=actor.skeleton.boneByName.at("R Hand");const auto center=scene::transformPoint(a.botActorPoses[0][wrist],{});const auto vp=scene::perspective(45*scene::kPi/180,960.f/720,1,4000)*scene::lookAt(center+(side?scene::Vec3{160,180,65}:scene::Vec3{160,-180,65}),center,{0,0,1});a.renderer.render(actor,a.botActorPoses[0],vp,960,720,false,false,false);CHECK(a.renderer.saveColorPng(out/(std::string(model)+weapon+"_grip"+std::to_string(side)+".png"),error));}
  report<<model<<" weapon="<<weapon<<" attachments="<<actor.attachments.size()<<"\n";
 }
 for(auto model:{"playermode_SWAT_Male_fb","playermode_SWAT_Female_SWAT-Sniper_fb"}){
  a.botModelAsset=idx(model);a.botAnimationGame="pointblank";rebuildBotActors(a);CHECK(a.bots.size()==5&&a.botActorScene);auto&actor=*a.botActorScene;CHECK(a.renderer.loadScene(actor,error));
  for(int f=0;f<20;++f)updateBotActors(a,1.f/60);
  CHECK(a.botDeathChoices.size()>=5);
  for(size_t i=0;i<5;++i){auto&b=a.bots[i];auto death=botDeathAnimation(actor,b,b.weaponClass,999,&a);CHECK(death);b.previousAnimation=b.animation;b.previousAnimationFrame=b.animationFrame;b.animation=*death;b.animationFrame=0;b.animationBlendElapsed=0;b.alive=false;b.health=0;b.respawnTime=20;b.velocity={};b.actionBlendWeight=b.spScenarioWeight=0;report<<"death="<<actor.animations[*death].sourceName<<"\n";}
  for(int f=0;f<=240;++f){updateBotActors(a,1.f/60);if(f%60==0)for(size_t i=0;i<5;++i){const auto p=gameplay::bot::presentationPosition(a.bots[i]);const auto vp=scene::perspective(48*scene::kPi/180,960.f/720,1,4000)*scene::lookAt(p+scene::Vec3{230,-260,115},p+scene::Vec3{0,0,50},{0,0,1});a.renderer.render(actor,a.botActorPoses[i],vp,960,720,true,false,false);CHECK(a.renderer.saveColorPng(out/(std::string(model)+"_death"+std::to_string(i)+"_"+std::to_string(f)+".png"),error));}}
  for(size_t i=0;i<5;++i){float lo=1e9f;for(auto&m:actor.meshes){if(m.attachmentIndex>=0)continue;for(auto&v:m.vertices){scene::Vec3 p{};for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<a.botActorPoses[i].size())p+=scene::transformPoint(a.botActorPoses[i][v.bones[k]]*actor.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];lo=std::min(lo,p.z);}}const float clearance=lo-gameplay::bot::presentationPosition(a.bots[i]).z;report<<"final floor delta="<<clearance<<"\n";CHECK(std::abs(clearance)<2.f);}
 }
 report.flush();a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();std::cout<<"PASS v149 world checks\n";
}
