#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
int main(int argc,char**argv){
 if(argc!=3||!glfwInit())return 1;const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");
 glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,720,"PB COD hands audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,true);ImGui_ImplOpenGL3_Init("#version 330");
 auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;app.defaultSalukiDirectory=argv[1];std::string error;if(!app.renderer.initialize(error))return 3;
 for(const auto game:{"bo2","mw3","pointblank"})if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))return 4;
 const auto find=[&](std::string name){for(std::size_t i=0;i<app.assetCatalog.entries.size();++i)if(lowerText(app.assetCatalog.entries[i].name)==lowerText(name))return i;return std::size_t(-1);};
 int failures=0;
 for(const auto* weapon:{"t6_wpn_ar_an94_view_LOD0","t6_wpn_pistol_fiveseven_view_LOD0","viewmodel_ak47_iw5_LOD0"}){
  const auto wi=find(weapon);if(wi>=app.assetCatalog.entries.size()){report<<"MISSING "<<weapon<<std::endl;++failures;continue;}
  for(const auto* hand:{"viewmodel_SWAT_Male_hands","viewmodel_Hide_Black_hands","viewmodel_REBEL_Female_GRS_hands"}){
   app.selectedBaseAsset=find(hand);app.selectedWeaponAsset=wi;app.deferSceneUpload=true;app.animationDocuments.clear();app.animationSources.clear();equipViewWeapon(app,wi,nullptr);
   if(!app.scene.codmRigAdapter||app.scene.viewHandsDriverGame.empty()){report<<"FAIL "<<hand<<" "<<weapon<<" "<<app.status;for(auto&s:app.scene.warnings)report<<" | "<<s;report<<std::endl;++failures;continue;}
   app.deferSceneUpload=false;app.showGrid=false;app.assetFirstPerson=true;app.viewmodelCamera=true;app.renderer.setDebugView(1);
   float wristError=0;int samples=0;
   for(const char* action:{"idle","fire","reload"}){
    auto clip=findViewmodelClip(app,action);if(!clip){report<<"MISSING action "<<action<<std::endl;++failures;continue;}
    app.animationIndex=*clip;app.actionActive=false;app.transitioning=false;
    for(int phase=0;phase<5;++phase){app.animationFrame=app.scene.animations[*clip].durationFrames*phase*.25f;const auto pose=app.scene.samplePose(*clip,app.animationFrame);++samples;
     for(const auto& m:pose)for(float f:m.v)if(!std::isfinite(f)){++failures;break;}
     for(const char* side:{"le","ri"}){const auto s=app.scene.skeleton.boneByName.at(std::string("j_wrist_")+side),d=app.scene.skeleton.boneByName.at(std::string("codm_legacy|j_wrist_")+side);wristError=std::max(wristError,scene::length(scene::transformPoint(pose[s],{})-scene::transformPoint(pose[d],{})));}
     if(phase==2){uploadMainScene(app);ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({960,720});ImGui::Begin("Audit",nullptr,ImGuiWindowFlags_NoDecoration);drawViewport(app,940,690);ImGui::End();ImGui::Render();glFinish();if(!app.renderer.saveColorPng(out/(std::string(hand)+"_"+weapon+"_"+action+".png"),error))return 5;}
    }
   }
   if(wristError>.01f)++failures;
   // Reconstruct the same driver via the replay manifest path, not live state.
   const auto before=app.scene.skeleton.bones.size();const auto paths=app.loadedRigModelPaths;take::ActorManifest manifest;manifest.baseModel=app.loadedBaseModelPath.string();for(const auto& p:paths)manifest.rigModels.push_back(p.string());
   if(!restoreTakeActor(app,manifest,before,error)||!app.scene.codmRigAdapter){report<<"REPLAY FAIL "<<error<<std::endl;++failures;}
   if(std::string(hand)=="viewmodel_SWAT_Male_hands"){
    auto old=scene::buildScene(cast::Document::load(manifest.baseModel));for(const auto& p:paths)scene::appendRigModel(cast::Document::load(p),old,p.stem().string());
    if(!restoreTakeActor(app,manifest,old.skeleton.bones.size(),error)||!app.scene.viewHandsDriverGame.empty()){report<<"OLD REPLAY FAIL "<<error<<std::endl;++failures;}
    if(!restoreTakeActor(app,manifest,before,error)||!app.scene.codmRigAdapter)++failures;
   }
   report<<"PAIR "<<hand<<" "<<weapon<<" samples="<<samples<<" wristErrorCm="<<wristError<<" replayBones="<<app.scene.skeleton.bones.size()<<std::endl;
  }
 }
 report<<"failures="<<failures<<std::endl;return failures?6:0;
}
