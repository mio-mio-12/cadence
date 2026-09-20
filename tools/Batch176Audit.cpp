#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#include "take/PoseInterpolation.h"

// Isolated production-path audit. Never reads/writes personal settings.
int main(int argc,char**argv){
 if(argc<2||!glfwInit())return 1;
 const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);
 std::ofstream log(out/"results.txt");int failures=0;
 const auto check=[&](bool ok,const std::string& text){log<<(ok?"PASS ":"FAIL ")<<text<<std::endl;std::cout<<(ok?"PASS ":"FAIL ")<<text<<std::endl;failures+=!ok;};
 glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,720,"Batch audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,true);ImGui_ImplOpenGL3_Init("#version 330");
 auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;std::string error;if(!app.renderer.initialize(error))return 3;
 app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 const auto root=app.defaultSalukiDirectory;
 const auto maxError=[](const auto&a,const auto&b){float e=0;if(a.size()!=b.size())return 1e20f;for(size_t i=0;i<a.size();++i)for(int j=0;j<16;++j)e=std::max(e,std::abs(a[i].v[j]-b[i].v[j]));return e;};
 if(argc<3||std::string(argv[2])=="codm"){
 std::vector<std::string> codmStems;for(const auto& model:std::filesystem::directory_iterator(root/"codm/models"))if(model.path().extension()==".cast"&&model.path().stem().string().starts_with("viewmodel_"))codmStems.push_back(model.path().stem().string());
 for(const std::string name:{"viewmodel_ar_bal27","viewmodel_ar_ak47","viewmodel_pistol_50gs","viewmodel_special_m1887","viewmodel_sniper_locus","viewmodel_sniper_arctic50","viewmodel_ar_ak117_CrazeGirl"}){
  if(!std::filesystem::exists(root/"codm/models"/(name+".cast")))continue;
  app.scene={};check(scene::codm::assemble(cast::Document::load(root/"codm/models"/(name+".cast")),cast::Document::load(root/"codm/models/codm_viewhands_C_M_Ghost_1P.cast"),app.scene,error),name+" assemble "+error);
  std::vector<std::filesystem::path> paths;for(const auto& f:std::filesystem::directory_iterator(root/"codm/animations"))if(f.path().extension()==".cast"&&cadence::codm_actions::belongsToWeapon(f.path().stem().string(),name,codmStems)&&!f.path().stem().string().ends_with("_camera"))paths.push_back(f.path());std::sort(paths.begin(),paths.end());for(const auto& path:paths)scene::appendAnimations(cast::Document::load(path),app.scene);
  cadence::codm_actions::prepare(app.scene);app.weaponProfile={};cadence::codm_actions::populate(app.scene,app.weaponProfile,name);cadence::codm_actions::defaultTimings(app.scene,app.weaponProfile);app.weaponTiming=app.weaponProfile.stats;
  app.viewmodelAnimationPrefix=name+"_";app.assetFirstPerson=true;app.viewmodelCamera=true;app.gameplayLogic=true;app.actorMode=true;app.actorPosition={};app.actorRenderPosition={};app.gameplayAds=false;app.viewmodelAdsEngaged=app.viewmodelAdsExiting=false;app.actionActive=app.actionOverlay=false;app.runtimeLayers.clear();app.interruptPoseSource.clear();
  for(const char* slot:{"idle","reload","reload_empty","pullout","putaway","ads_up","ads_down"})check(findViewmodelClip(app,slot).has_value(),name+" "+slot);
  if(auto idle=findViewmodelClip(app,"idle")){app.animationIndex=*idle;app.animationFrame=0;}
  app.viewmodelAdsBaseAnimation=static_cast<size_t>(-1);
  const auto up=findViewmodelClip(app,"ads_up"),down=findViewmodelClip(app,"ads_down"),fire=findViewmodelClip(app,"fire");
  if(!up||!down)continue;
  if(fire){app.gameplayAction=scene::ActionRole::Fire;triggerGameplayAction(app);app.actionFrame=1;app.actionElapsed=.01f;}
  const auto before=evaluateCurrentPose(app);const auto oldAction=app.actionAnimationIndex;const auto oldElapsed=app.actionElapsed;
  app.gameplayAction=scene::ActionRole::Aim;triggerGameplayAction(app);
  check(app.viewmodelAdsEngaged&&!app.viewmodelAdsExiting,name+" starts ADS up");
  if(fire)check(app.activeAction==scene::ActionRole::Fire&&app.actionAnimationIndex==oldAction&&app.actionElapsed==oldElapsed,name+" ADS preserves shot clock");
  check(maxError(before,evaluateCurrentPose(app))<.01f,name+" continuous ADS entry pose");
  app.interruptPoseSource.clear();app.transitioning=false;
  app.renderer.loadScene(app.scene,error);app.renderer.setSun(false,false,{0,0,-1},1,.5f,{1,1,1},{1,1,1},512,800,600,{},false,512,600,1600,100);
  for(int frame=0;frame<=60;++frame){
   const float t=frame/60.f;app.viewmodelAdsBaseFrame=app.scene.animations[*up].durationFrames*t;app.viewmodelAdsTransitionElapsed=app.weaponTiming.adsIn*t;
   if(fire){app.actionAnimationIndex=*fire;app.actionFrame=app.scene.animations[*fire].durationFrames*std::fmod(t*3.f,1.f);}
   const auto pose=evaluateCurrentPose(app);check(std::all_of(pose.begin(),pose.end(),[](const auto&m){return std::all_of(m.v.begin(),m.v.end(),[](float v){return std::isfinite(v);});}),name+" finite ADS/fire "+std::to_string(frame));
   if(frame%15==0){const auto c=pose[app.scene.skeleton.boneByCanonicalName.at("tag_camera")];scene::Vec3 eye{c.v[12],c.v[13],c.v[14]},f{c.v[0],c.v[1],c.v[2]},u{c.v[8],c.v[9],c.v[10]};app.renderer.setCameraPosition(eye);app.renderer.render(app.scene,pose,scene::perspective(65*scene::kPi/180,960.f/720,.1f,2000)*scene::lookAtDirection(eye,f,u),960,720,false,false,false);app.renderer.saveColorPng(out/(name+"_ads_fire_"+std::to_string(frame)+".png"),error);}
  }
  check(startViewmodelAimClip(app,*down)&&app.viewmodelAdsExiting,name+" mapped ADS-down direction");
  for(const char* slot:{"idle","reload","ads_up","ads_down","rechamber","jump_takeoff","jump_land"})if(auto index=findViewmodelClip(app,slot)){
   const auto& clip=app.scene.animations[*index];
   for(float fraction:{0.f,.25f,.5f,.75f,1.f}){
    const auto p=app.scene.samplePose(*index,clip.durationFrames*fraction);
    check(std::all_of(p.begin(),p.end(),[](const auto&m){return std::all_of(m.v.begin(),m.v.end(),[](float v){return std::isfinite(v);});}),name+" finite "+slot+" "+std::to_string(fraction));
    if(fraction!=.5f&&!(std::string(slot)=="ads_up"&&fraction==1.f))continue;
    const auto c=p[app.scene.skeleton.boneByCanonicalName.at("tag_camera")];scene::Vec3 eye{c.v[12],c.v[13],c.v[14]},f{c.v[0],c.v[1],c.v[2]},u{c.v[8],c.v[9],c.v[10]};
    app.renderer.setCameraPosition(eye);app.renderer.render(app.scene,p,scene::perspective(65*scene::kPi/180,960.f/720,.1f,2000)*scene::lookAtDirection(eye,f,u),960,720,false,false,false);
    check(app.renderer.saveColorPng(out/(name+"_"+slot+"_"+std::to_string(int(fraction*100))+".png"),error),name+" capture "+slot);
   }
  }
 }
 }
 if(argc>=3&&std::string(argv[2])=="mw3"){
  check(assets::appendScan(root/"mw3","mw3",app.assetCatalog,error),"scan MW3 library");
  std::size_t paired=0,unpaired=0;
  for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){
   const auto& a=app.assetCatalog.entries[i];if(a.role!=assets::Role::ViewWeapon)continue;
   const auto world=findWorldWeaponForViewWeapon(app,a);
   if(world<app.assetCatalog.entries.size()){++paired;log<<"PAIR "<<a.name<<" -> "<<app.assetCatalog.entries[world].name<<'\n';check(matchingWorldWeapon(app,i)==world,"bot/player counterpart agrees "+a.name);}
   else {++unpaired;log<<"UNPAIRED "<<a.name<<'\n';}
  }
  log<<"TOTAL paired="<<paired<<" unpaired="<<unpaired<<std::endl;
  for(const std::string gun:{"model_1887","ak47_iw5","fn_five_seven_iw5"}){
   auto view=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[&](const auto&a){return a.name=="viewmodel_"+gun+"_LOD0";});
   if(view==app.assetCatalog.entries.end()){check(false,"representative view exists "+gun);continue;}
   const auto world=findWorldWeaponForViewWeapon(app,*view);check(world<app.assetCatalog.entries.size(),"representative world found "+gun);if(world>=app.assetCatalog.entries.size())continue;
   const auto original=app.assetCatalog.entries[world].name;
   app.assetCatalog.entries[world].name="wpn_"+original.substr(7);
   check(findWorldWeaponForViewWeapon(app,*view)==world,"wpn prefix production pairing "+gun);
   auto alternate=*view;alternate.name="view_"+gun+"_LOD0";check(findWorldWeaponForViewWeapon(app,alternate)==world,"view prefix production pairing "+gun);
   alternate.name="wpn_"+gun+"_viewmodel_LOD0";check(findWorldWeaponForViewWeapon(app,alternate)==world,"viewmodel suffix production pairing "+gun);
   const auto doc=cast::Document::load(app.assetCatalog.entries[world].path);check(doc.valid()&&!scene::buildScene(doc).meshes.empty(),"world CAST contains renderable geometry "+gun);
   app.assetCatalog.entries[world].name=original;
  }
 }
 if(argc<3||std::string(argv[2])=="bots"){
  for(const auto game:{"bo2","bo2_sp","mw3"})check(assets::appendScan(root/game,game,app.assetCatalog,error),std::string("scan ")+game);
  for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto&a=app.assetCatalog.entries[i];if(a.name=="t6_wpn_ar_an94_view_LOD0")app.classPrimaryAsset=i;if(a.name=="t6_wpn_sniper_ballista_view_LOD0")app.classSecondaryAsset=i;if(a.game=="bo2"&&a.role==assets::Role::ViewHands&&app.classViewhandsOverride>=app.assetCatalog.entries.size())app.classViewhandsOverride=i;}
  loadBothClassSlots(app);while(app.pendingClassFuture){processPendingClassLoad(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(app.classGpuResident,"class loaded");app.botGame="mw3";app.botAnimationGame="bo2+bo2_sp";app.botTeamSide=3;app.enemyBotCount=5;app.botSystemMode=1;app.botUseSalukiWeaponPool=false;rebuildBotActors(app);
  check(app.botActorScene&&app.bots.size()==5,"five MW3 assembled bots with BO2+SP");
  if(app.botActorScene&&!app.bots.empty()){
   app.playing=true;app.botFoundationMovement=true;app.showBotAnimationClips=true;updateBotActors(app,.016f);
   auto source=*app.botActorScene;auto poses=app.botActorPoses;for(size_t i=0;i<poses.size();++i){const auto same=take::interpolatePose(poses[i],poses[i],.5f,&source.skeleton);const float e=maxError(poses[i],same);check(e<.01f,"constant bot interpolation "+std::to_string(i)+" error="+std::to_string(e));}
   captureTakeBotManifest(app);const auto manifest=app.recordedTake.botActor;const auto boneCount=app.recordedTake.botBoneCount;
   check(restoreTakeBotActor(app,manifest,boneCount,error),"restore bot manifest "+error);
   if(app.botActorScene){const auto& rebuilt=*app.botActorScene;bool same=source.skeleton.bones.size()==rebuilt.skeleton.bones.size();for(size_t i=0;i<std::min(source.skeleton.bones.size(),rebuilt.skeleton.bones.size());++i)same&=source.skeleton.bones[i].name==rebuilt.skeleton.bones[i].name;check(same,"bot rebuilt bone order");
    for(int variant=0;variant<2;++variant){std::vector<std::vector<scene::Mat4>> selected=poses;if(variant)for(auto& p:selected)p=take::interpolatePose(p,p,.5f,&rebuilt.skeleton);std::vector<int> ids;for(size_t i=0;i<poses.size();++i)ids.push_back(int(i));const scene::Vec3 eye{600,-600,350};app.renderer.setCameraPosition(eye);app.renderer.render(app.scene,{},scene::perspective(65*scene::kPi/180,960.f/720,1,5000)*scene::lookAt(eye,{0,0,90},{0,0,1}),960,720,false,false,false,&rebuilt,&selected,&ids,nullptr,nullptr,false);app.renderer.saveColorPng(out/(variant?"bots_fractional.png":"bots_exact.png"),error);}
   }
  }
  app.gameplayLogic=true;app.weaponSwitchAlgorithm=4;app.weaponSwitchStage=1;app.gameplayAction=scene::ActionRole::Unequip;triggerGameplayAction(app);const auto before=evaluateCurrentPose(app);const int slot=app.activeClassSlot;cancelYyV4Drop(app);check(app.weaponSwitchStage==0&&app.activeClassSlot==slot&&!app.yyReverse,"YY v4 immediate ready without swapping");check(maxError(before,evaluateCurrentPose(app))<.01f,"YY v4 visual continuity");
 }
 app.renderer.shutdown();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();return failures?1:0;
}
