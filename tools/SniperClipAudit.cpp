#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main

// Real class assembly and viewport, isolated from user settings and running UI.
int main(int argc,char**argv){
 if(argc!=2||!glfwInit())return 1;
 const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);
 glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(1100,720,"Sniper clipping audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,true);ImGui_ImplOpenGL3_Init("#version 330");
 auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;std::string error;
 if(!app.renderer.initialize(error))return 3;
 app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 if(!assets::appendScan(app.defaultSalukiDirectory/"bo2","bo2",app.assetCatalog,error))return 4;
 for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto&a=app.assetCatalog.entries[i];
  if(a.name=="t6_wpn_sniper_dsr50_view_LOD0")app.classPrimaryAsset=i;
  if(a.name=="t6_wpn_sniper_ballista_view_LOD0")app.classSecondaryAsset=i;
  if(a.name=="c_usa_mp_isa_smg_viewhands_LOD0")app.classViewhandsOverride=i;
 }
 loadBothClassSlots(app);while(app.pendingClassFuture){processPendingClassLoad(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 if(!app.classGpuResident){std::cerr<<app.status;return 5;}
 std::ofstream report(out/"results.txt");
 app.showGrid=false;
 const auto draw=[&](const std::string& name){
  ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
  ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1100,720});ImGui::Begin("Audit",nullptr,ImGuiWindowFlags_NoDecoration);
  drawViewport(app,1080,690);ImGui::End();ImGui::Render();glFinish();
  if(!app.renderer.saveColorPng(out/(name+".png"),error))throw std::runtime_error(error);
  report<<name<<" camera "<<app.lastRenderedCameraPosition.x<<","<<app.lastRenderedCameraPosition.y<<","<<app.lastRenderedCameraPosition.z<<" fov "<<app.lastRenderedCameraFov<<'\n';
 };
 for(int slot=0;slot<2;++slot){
  activateClassSlot(app,slot);const std::string gun=slot?"ballista":"dsr50";
  app.weaponTiming.hideWeaponOnAds=false;app.gameplayLogic=false;app.viewmodelCamera=true;app.assetFirstPerson=true;
  app.actionActive=app.actionOverlay=app.transitioning=false;app.interruptPoseSource.clear();app.runtimeLayers.clear();
  app.viewmodelAdsEngaged=app.viewmodelAdsExiting=false;app.adsCameraBlend=0;app.viewmodelFov=65;app.viewmodelFovScale=1;
  for(const char* action:{"idle","ads_up"}){
   const auto clip=findViewmodelClip(app,action);if(!clip){report<<"MISSING "<<gun<<" "<<action;return 6;}
   app.animationIndex=*findViewmodelClip(app,"idle");app.animationFrame=app.scene.animations[app.animationIndex].durationFrames*.5f;
   if(std::string(action)=="ads_up"){
    startViewmodelAimClip(app,*clip);app.viewmodelAdsBaseFrame=app.scene.animations[*clip].durationFrames;
    app.actionFrame=app.viewmodelAdsBaseFrame;app.actionElapsed=app.viewmodelAdsTransitionElapsed=app.weaponTiming.adsIn;
    app.interruptPoseSource.clear();app.transitioning=false;
   }
   report<<gun<<" "<<action<<" "<<app.scene.animations[*clip].sourceName<<'\n';
   for(bool live:{false,true}){app.actorMode=live;app.actorViewCameraValid=false;app.actorThirdPerson=false;
    std::vector<std::uint8_t> reference;
    for(float nearCm:{2.5f,.1f,.01f,25.f,50.f}){
     app.cameraNearClipCm=nearCm;draw(gun+"_"+action+(live?"_live_":"_preview_")+std::to_string(nearCm));
     std::vector<std::uint8_t> pixels;if(!app.renderer.readColorRgba(pixels,error))return 7;
     if(reference.empty())reference=pixels;else {size_t changed=0;int maxDelta=0;for(size_t p=0;p<pixels.size();++p){const int d=std::abs(int(pixels[p])-int(reference[p]));changed+=d!=0;maxDelta=std::max(maxDelta,d);}if(changed>64||maxDelta>1){report<<"FAIL weapon color changes with world near "<<nearCm<<" channels="<<changed<<" maxDelta="<<maxDelta<<std::endl;return 8;}}
    }
    report<<"PASS weapon pixels stable across world near .01/.1/2.5/25/50 cm (max 1/255 in 64 channels) "<<gun<<" "<<action<<" live="<<live<<std::endl;
   }
  }
 }
 // The close-range path must leave world depth AND color bit-identical.
 std::vector<std::uint8_t> worldColor;std::vector<float> worldDepth;
 scene::CastScene world;scene::Mesh wall;wall.name="world precision fixture";
 for(auto p:{scene::Vec3{300,-300,0},scene::Vec3{300,300,0},scene::Vec3{300,300,300},scene::Vec3{300,-300,300}}){scene::Vertex v;v.position=p;v.normal={-1,0,0};wall.vertices.push_back(v);}
 wall.indices={0,1,2,0,2,3};world.meshes.push_back(wall);
 if(!app.renderer.loadAuxiliaryScenes(&world,nullptr,nullptr,error))return 11;
 const auto pose=evaluateCurrentPose(app);const scene::Vec3 eye{100,-200,180};
 const auto vp=scene::perspective(65*scene::kPi/180,1.5f,5.f,100000.f)*scene::lookAt(eye,{300,0,0},{0,0,1});
 app.renderer.setCameraPosition(eye);app.renderer.setCameraDepthRange(5,100000);
 for(bool enabled:{false,true}){
  app.renderer.setFirstPersonProjection(enabled);
  app.renderer.render(app.scene,pose,vp,480,320,true,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&world);
  std::vector<std::uint8_t> color;std::vector<float> depth;
  if(!app.renderer.readColorRgba(color,error)||!app.renderer.readDepthRawFloat(depth,5,100000,0,100000,false,error))return 9;
  if(!enabled){worldColor=color;worldDepth=depth;}else if(color!=worldColor||depth!=worldDepth){report<<"FAIL world precision changed"<<std::endl;return 10;}
 }
 report<<"PASS world-only color and depth bit-identical with first-person range enabled/disabled"<<std::endl;
 app.renderer.shutdown();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();return 0;
}
