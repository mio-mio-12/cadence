#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main

int main(int argc,char**argv){
 if(argc!=2||!glfwInit())return 1;
 const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");
 glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,720,"CODM firing audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,true);ImGui_ImplOpenGL3_Init("#version 330");
 auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;std::string error;if(!app.renderer.initialize(error))return 3;
 app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(const auto game:{"bo2","codm"})if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))return 4;
 for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto&a=app.assetCatalog.entries[i];if(a.name=="viewmodel_pistol_50gs")app.classPrimaryAsset=i;if(a.name=="viewmodel_pistol_50gs_GirlsFrontline")app.classSecondaryAsset=i;}
 for(const auto hands:{"codm_viewhands_C_M_Ghost_1P","c_usa_mp_isa_smg_viewhands_LOD0"}){
  for(size_t i=0;i<app.assetCatalog.entries.size();++i)if(app.assetCatalog.entries[i].name==hands)app.classViewhandsOverride=i;
  loadBothClassSlots(app);while(app.pendingClassFuture){processPendingClassLoad(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}if(!app.classGpuResident)return 5;
  for(int slot=0;slot<2;++slot){
   activateClassSlot(app,slot);app.showGrid=false;app.actorMode=false;app.viewmodelCamera=true;app.gameplayLogic=true;app.weaponTiming.hideWeaponOnAds=false;
   for(bool ads:{false,true}){
    app.actionActive=app.actionOverlay=app.transitioning=false;app.interruptPoseSource.clear();app.runtimeLayers.clear();app.viewmodelAdsEngaged=app.viewmodelAdsExiting=false;app.viewmodelAdsBaseAnimation=size_t(-1);app.gameplayAds=ads;
    const auto idle=findViewmodelClip(app,"idle"),fire=findViewmodelClip(app,ads?"ads_fire":"fire");if(!idle||!fire)return 6;
    app.animationIndex=*idle;app.animationFrame=0;
    if(ads){const auto up=findViewmodelClip(app,"ads_up");if(!up)return 7;startViewmodelAimClip(app,*up);app.viewmodelAdsBaseFrame=app.scene.animations[*up].durationFrames;app.viewmodelAdsTransitionElapsed=app.weaponTiming.adsIn;app.actionActive=false;app.interruptPoseSource.clear();}
    app.gameplayAction=scene::ActionRole::Fire;triggerGameplayAction(app);app.interruptPoseSource.clear();app.transitioning=false;
    if(app.actionAnimationIndex!=*fire||!app.actionOverlay)return 8;
    app.actionFrame=0;app.actionElapsed=0;const auto first=evaluateCurrentPose(app);float movement=0;
    for(int step=0;step<=8;++step){app.actionFrame=app.scene.animations[*fire].durationFrames*step/8.f;app.actionElapsed=app.actionFrame/app.scene.animations[*fire].framerate;
     const auto pose=evaluateCurrentPose(app);for(size_t b=0;b<pose.size();++b)for(int k=0;k<16;++k){if(!std::isfinite(pose[b].v[k]))return 9;movement=std::max(movement,std::abs(pose[b].v[k]-first[b].v[k]));}
     ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({960,720});ImGui::Begin("Audit",nullptr,ImGuiWindowFlags_NoDecoration);drawViewport(app,940,690);ImGui::End();ImGui::Render();glFinish();
     if(step%2==0&&!app.renderer.saveColorPng(out/(std::string(hands)+"_"+std::to_string(slot)+(ads?"_ads_":"_hip_")+std::to_string(step)+".png"),error))return 10;
    }
    report<<(movement>.1f?"PASS ":"FAIL ")<<hands<<" slot="<<slot<<" ads="<<ads<<" motion="<<movement<<std::endl;if(movement<=.1f)return 11;
   }
   app.actorMode=true;app.actionActive=false;app.actionOverlay=false;app.viewmodelAdsEngaged=app.viewmodelAdsExiting=false;app.viewmodelAdsBaseAnimation=size_t(-1);app.gameplayAds=false;app.interruptPoseSource.clear();
   app.animationIndex=*findViewmodelClip(app,"idle");app.animationFrame=0;
   app.weaponProfile.gunPosition={3,-2,1};app.weaponProfile.adsGunPosition={-5,4,7};
   for(float progress:{0.f,.25f,.5f,.75f,1.f,.5f,0.f}){
    app.adsCameraBlend=progress;app.weaponProfile.separateAdsPosition=false;const auto hip=evaluateCurrentPose(app);const auto camera=app.actorViewCamera;
    app.weaponProfile.separateAdsPosition=true;const auto p=evaluateCurrentPose(app);
    const auto b=app.scene.skeleton.boneByName.at("Bone_RightHand");scene::Vec3 d{p[b].v[12]-hip[b].v[12],p[b].v[13]-hip[b].v[13],p[b].v[14]-hip[b].v[14]};
    const float expected=scene::length(weapon::gunPositionAt(app.weaponProfile,progress)-app.weaponProfile.gunPosition);
    if(std::abs(scene::length(d)-expected)>.002f)return 12;
    for(int k=0;k<16;++k)if(std::abs(camera.v[k]-app.actorViewCamera.v[k])>.0001f)return 13;
   }
   app.weaponProfile.separateAdsPosition=false;app.weaponProfile.gunPosition={};app.adsCameraBlend=0;
   report<<"PASS hip/ADS position interpolation and reversal, unchanged gameplay camera "<<hands<<" slot="<<slot<<std::endl;
  }
 }
 app.renderer.shutdown();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();return 0;
}
