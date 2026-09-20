#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <cstdlib>
#include "scene/ColdWarWorld.h"
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<'\n';std::exit(1);}}while(false)
int main(int argc,char**argv){
 CHECK(argc>1);std::filesystem::path out=argv[1];std::filesystem::create_directories(out);std::cout<<std::unitbuf;
 CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"Cadence validation",nullptr,nullptr);CHECK(w);glfwMakeContextCurrent(w);
 auto state=std::make_unique<AppState>();auto&a=*state;std::string error;CHECK(a.renderer.initialize(error));a.window=w;a.deferSceneUpload=true;
 if(argc>2&&std::string(argv[2])=="slide"){
  const std::filesystem::path root=cadence::local_assets::exportPath("");
  auto actor=scene::buildScene(cast::Document::load(root/"bo2/models/playermodels/isa/c_usa_mp_isa_assault_fb/c_usa_mp_isa_assault_fb_LOD0.cast"),false);
  const std::string direction=argc>3?argv[3]:"l";
  auto doc=cast::Document::load(root/("bocw_sp/animations/pb/run/assault rifle/pb_rifle_run_slide_land_"+direction+".cast"));CHECK(doc.valid());scene::appendAnimations(doc,actor);CHECK(!actor.animations.empty());
  const auto adapter=actor.animations[0].coldWarWorldPose;CHECK(adapter);
  float maxSocketError=0;
  for(int n=0;n<=120;++n){const float f=n/120.f*actor.animations[0].durationFrames;const auto p=actor.samplePose(0,f),s=adapter->source->samplePose(0,f);for(const auto& m:p)for(float v:m.v)CHECK(std::isfinite(v));const auto wrist=actor.skeleton.boneByCanonicalName.at("j_wrist_ri"),socket=actor.skeleton.boneByCanonicalName.at("tag_weapon_right");const auto sw=adapter->sourceBones[wrist],ss=adapter->sourceBones[socket];for(int k=0;k<16;++k){float expected=s[ss].v[k];if(k>=12&&k<=14)expected+=p[wrist].v[k]-s[sw].v[k];maxSocketError=std::max(maxSocketError,std::abs(p[socket].v[k]-expected));}}
  CHECK(maxSocketError<.001f);std::cout<<"PASS 121 fractional slide samples: native socket anchored at fitted wrist, max matrix error="<<maxSocketError<<'\n';
  for(auto name:{"j_wrist_ri","j_wrist_le","tag_weapon_right","tag_weapon","j_gun"}){
   const auto t=actor.skeleton.boneByCanonicalName.find(name);const auto s=adapter->source->skeleton.boneByCanonicalName.find(name);
   std::cout<<"BONE "<<name<<" target="<<(t==actor.skeleton.boneByCanonicalName.end()?-1:int(t->second))<<" source="<<(s==adapter->source->skeleton.boneByCanonicalName.end()?-1:int(s->second))<<'\n';
   if(t!=actor.skeleton.boneByCanonicalName.end()){auto i=t->second;std::cout<<" target parent="<<actor.skeleton.bones[i].parent<<" mapped="<<adapter->sourceBones[i]<<'\n';}
   if(s!=adapter->source->skeleton.boneByCanonicalName.end()){auto i=s->second;std::cout<<" source parent="<<adapter->source->skeleton.bones[i].parent<<'\n';}
  }
  auto weapon=cast::Document::load(root/"bocw_sp/models/weapons/world/sniper rifles/sniper_standard/wpn_t9_sniper_standard_world/wpn_t9_sniper_standard_world_LOD0.cast");CHECK(weapon.valid());scene::appendAttachment(weapon,actor,preferredAttachmentBone(actor.skeleton),"CW sniper");CHECK(a.renderer.loadScene(actor,error));
  for(int f=0;f<=4;++f){const auto frame=f*.25f*actor.animations[0].durationFrames;const auto pose=actor.samplePose(0,frame);const auto native=adapter->source->samplePose(0,frame);
   for(auto name:{"j_wrist_ri","tag_weapon_right"}){auto i=actor.skeleton.boneByCanonicalName.at(name);auto p=scene::transformPoint(pose[i],{});std::cout<<f<<' '<<name<<" position="<<p.x<<","<<p.y<<","<<p.z<<'\n';}
   const auto vp=scene::perspective(55*scene::kPi/180,960.f/540,1.f,1000.f)*scene::lookAt({200,-230,120},{0,0,60},{0,0,1});a.renderer.render(actor,pose,vp,960,540,false,false,false);CHECK(a.renderer.saveColorPng(out/("slide_"+std::to_string(f)+".png"),error));
  }
  a.renderer.shutdown();state.reset();glfwDestroyWindow(w);glfwTerminate();return 0;
 }
 a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(auto g:{"codm","bo2"})CHECK(assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error));
 auto idx=[&](const char*name){for(std::size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return a.assetCatalog.entries.size();};
 for(const char*hand:{"codm_viewhands_c_m_ghost_Default","c_usa_mp_isa_smg_viewhands_LOD0"}){
  a.selectedBaseAsset=idx(hand);a.selectedWeaponAsset=idx("viewmodel_pistol_50gs_Default");CHECK(a.selectedBaseAsset<a.assetCatalog.entries.size());CHECK(a.selectedWeaponAsset<a.assetCatalog.entries.size());equipViewWeapon(a,a.selectedWeaponAsset);
  a.gameplayLogic=true;a.viewmodelCamera=true;a.actorMode=false;a.actionActive=false;a.gameplayAds=false;resolveGameplayAnimation(a);
  const auto up=findViewmodelClip(a,"ads_up"),down=findViewmodelClip(a,"ads_down"),fire=findViewmodelClip(a,"ads_fire");CHECK(up&&down&&fire);
  CHECK(startViewmodelAimClip(a,*up));a.viewmodelAdsBaseFrame=a.scene.animations[*up].durationFrames;a.viewmodelAdsTransitionElapsed=a.weaponTiming.adsIn;a.interruptPoseElapsed=a.interruptPoseDuration;a.transitioning=false;
  a.actionActive=a.actionOverlay=true;a.activeAction=scene::ActionRole::Fire;a.actionAnimationIndex=*fire;a.actionFrame=5;a.actionElapsed=5.f/a.scene.animations[*fire].framerate;a.acceptedShotSerial++;
  auto before=evaluateCurrentPose(a);const auto elapsed=a.actionElapsed;CHECK(startViewmodelAimClip(a,*down));a.actionActive=a.actionOverlay=true;a.activeAction=scene::ActionRole::Fire;a.actionAnimationIndex=*fire;a.actionFrame=5;a.actionElapsed=elapsed;
  CHECK(a.codmReleasedFireReference==*up);a.interruptPoseElapsed=a.interruptPoseDuration;
  CHECK(a.renderer.loadScene(a.scene,error));auto gun=a.scene.skeleton.boneByName.find("Bone_RightHand");CHECK(gun!=a.scene.skeleton.boneByName.end());
  std::vector<scene::Mat4> first,last;
  for(int q=0;q<=4;++q){float t=q/4.f;a.viewmodelAdsBaseFrame=t*a.scene.animations[*down].durationFrames;a.viewmodelAdsTransitionElapsed=t*a.weaponTiming.adsOut;auto pose=evaluateCurrentPose(a);for(const auto&m:pose)for(float v:m.v)CHECK(std::isfinite(v));if(q==0)first=pose;last=pose;
   // Camera matches normalized native view (Z-up, looking along -Y).
   const auto vp=scene::perspective(65*scene::kPi/180,960.f/540,.1f,1000.f)*scene::lookAt({0,0,0},{0,-1,0},{0,0,1});a.renderer.render(a.scene,pose,vp,960,540,false,false,false);
   CHECK(a.renderer.saveColorPng(out/(std::string(hand)+"_release_"+std::to_string(q)+".png"),error));
   std::cout<<hand<<" phase="<<t<<" gun="<<pose[gun->second].v[12]<<","<<pose[gun->second].v[13]<<","<<pose[gun->second].v[14]<<'\n';
  }
  CHECK(std::memcmp(first.data(),last.data(),last.size()*sizeof(scene::Mat4))!=0);CHECK(a.actionFrame==5&&a.actionElapsed==elapsed);
  float entryDelta=0;for(std::size_t b=0;b<first.size();++b)for(int k=0;k<16;++k)entryDelta=std::max(entryDelta,std::abs(first[b].v[k]-before[b].v[k]));std::cout<<"release entry matrix delta="<<entryDelta<<'\n';CHECK(entryDelta<.01f);
  a.viewmodelAdsExiting=a.viewmodelAdsEngaged=false;auto hip=evaluateCurrentPose(a);float delta=0;for(std::size_t b=0;b<hip.size();++b)for(int k=0;k<16;++k)delta=std::max(delta,std::abs(hip[b].v[k]-last[b].v[k]));CHECK(delta<.001f);
 }
 std::cout<<"PASS actual 50GS CODM/T6 hands ADS release moves during fire; no end handoff pop; fire clock unchanged\n";
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,false);ImGui_ImplOpenGL3_Init("#version 330");
 a.recordedTake.clear();a.recordedTake.boneCount=a.scene.skeleton.bones.size();a.recordedTake.sampleRate=30;a.takePlaybackSlot=0;a.takeFirstPersonView=false;a.actorMode=false;
 for(int i=0;i<2;++i){take::Sample s;s.time=i*.1f;s.pose=a.scene.samplePose(0,0);a.recordedTake.samples.push_back(s);}
 a.recordedTake.dollyCamera={{0,{0,30,5},{2,-90,12},65},{3,{10,30,8},{3,-85,18},70}};a.cameraEditMode=a.dollyCameraActive=true;a.freeCameraActive=false;
 a.viewportAspectLocked=true;a.viewportAspect=1;a.viewportResolutionScale=.5f;
 for(bool only:{true,false}){
  a.exportFolder=out/(only?"camera_only":"png");std::filesystem::create_directories(a.exportFolder);
  a.exportWidth=320;a.exportHeight=180;a.exportFps=30;a.exportStart=0;a.exportEnd=.1f;a.exportFrame=0;a.exportFrameCount=4;a.exportCameraOnly=only;a.exportCameraData=true;a.exportActive=true;a.exportProRes=false;a.takePreview=true;a.takeTime=0;a.takePlaybackSpeed=1;a.exportDepthPass=false;
  for(int f=0;f<4;++f){ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowSize({960,540});ImGui::Begin("capture check");drawViewport(a,900,500);ImGui::End();ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());}
  CHECK(!a.exportActive);CHECK(a.exportFrame==4);CHECK(a.lastViewportWidth==320&&a.lastViewportHeight==180);CHECK(std::filesystem::exists(a.exportFolder/"camera.csv"));CHECK(std::filesystem::exists(a.exportFolder/"frame_000000.png")==!only);
 }
 std::ifstream csv1(out/"camera_only/camera.csv"),csv2(out/"png/camera.csv");CHECK(std::string(std::istreambuf_iterator<char>(csv1),{})==std::string(std::istreambuf_iterator<char>(csv2),{}));
 std::cout<<"PASS replay camera-only vs PNG camera CSV identical, 4 output frames, offline resolution independent of live aspect/render scale\n";
 a.actorOverlays.chams=true;a.actorOverlays.rainbow=true;a.actorOverlays.opacity=.37f;a.debugWallhack=true;a.wallhackThickness=3.5f;
 CHECK(saveActorOverlayPreset(a,out/"overlays.castoverlay"));auto look=a.actorOverlays;a.actorOverlays={};a.debugWallhack=false;
 const auto oldFog=a.fogEnabled;CHECK(loadActorOverlayPreset(a,out/"overlays.castoverlay"));CHECK(a.actorOverlays.chams&&a.actorOverlays.rainbow&&a.actorOverlays.opacity==look.opacity&&a.debugWallhack&&a.wallhackThickness==3.5f&&a.fogEnabled==oldFog);
 {std::ofstream broken(out/"bad.castoverlay");broken<<"CADENCE_ACTOR_OVERLAYS 1\n0 0";}
 CHECK(!loadActorOverlayPreset(a,out/"bad.castoverlay"));CHECK(a.actorOverlays.chams&&a.debugWallhack);
 std::cout<<"PASS overlay preset roundtrip and malformed-file atomic rejection\n";
 ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();
 a.renderer.shutdown();state.reset();glfwDestroyWindow(w);glfwTerminate();
}
