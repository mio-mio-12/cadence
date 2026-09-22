#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
int main(int argc,char**argv){
 if(argc!=3){std::cerr<<"Expected asset root and output directory\n";return 1;}
 glfwSetErrorCallback([](int code,const char* message){std::cerr<<"GLFW "<<code<<": "<<message<<'\n';});
 if(!glfwInit())return 1;const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");
 glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);auto*w=glfwCreateWindow(960,720,"CODM foreign weapon hands audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,true);ImGui_ImplOpenGL3_Init("#version 330");
 auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;app.defaultSalukiDirectory=argv[1];std::string error;if(!app.renderer.initialize(error)){std::cerr<<error;return 3;}
 for(const auto game:{"bo","bo2","mw","mw2","mw3","ghosts","aw","iw_sp","mwr","codm","pointblank","bocw_sp"})if(std::filesystem::is_directory(app.defaultSalukiDirectory/game)&&!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error)){std::cerr<<game<<": "<<error;return 4;}
 report<<"catalog="<<app.assetCatalog.entries.size()<<std::endl;
 const auto find=[&](std::string name){for(std::size_t i=0;i<app.assetCatalog.entries.size();++i)if(lowerText(app.assetCatalog.entries[i].name)==lowerText(name))return i;return std::size_t(-1);};
 int failures=0;
 std::vector<std::size_t> weapons;
 for(const auto* game:{"bo","bo2","mw","mw2","mw3","ghosts","aw","iw_sp","mwr","bocw_sp","pointblank"}){
  int count=0;for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& asset=app.assetCatalog.entries[i];const auto n=lowerText(asset.name);
   if(asset.game!=game||asset.role!=assets::Role::ViewWeapon)continue;
   if(std::string(game)=="mwr" && n!="viewmodel_ak47_lod0" && n!="wpn_h1_lmg_m60_vm_camo_lod0" && n!="wpn_h1_pst_de50_vm_camo_lod0")continue;
   if(std::string(game)=="aw" && n!="viewmodel_ak47_lod0" && n!="viewmodel_m16_lod0" && n!="vm_bal27_base_atlas_lod0")continue;
   if(n.starts_with("prop_")||n.starts_with("com_")||n.find("knife")!=std::string::npos||n.find("basketball")!=std::string::npos||n.find("default")!=std::string::npos)continue;
   weapons.push_back(i);if(++count==3)break;
  }
 }
 for(const auto wi:weapons){const auto weaponName=app.assetCatalog.entries[wi].name;const char* weapon=weaponName.c_str();
  report<<"GAME "<<app.assetCatalog.entries[wi].game<<std::endl;
  for(const auto* hand:{"codm_viewhands_C_M_Ghost_1P","codm_viewhands_C_F_Charly_Sinister_white_1P"}){
   app.selectedBaseAsset=find(hand);app.selectedWeaponAsset=wi;app.deferSceneUpload=true;app.animationDocuments.clear();app.animationSources.clear();equipViewWeapon(app,wi,nullptr);
   if(!app.scene.codmRigAdapter||(app.scene.viewHandsDriverGame.empty()&&!app.scene.pointBlankNativeCentimetres)){report<<"FAIL "<<hand<<" "<<weapon<<" "<<app.status;for(auto&s:app.scene.warnings)report<<" | "<<s;report<<std::endl;++failures;continue;}
   app.deferSceneUpload=false;app.showGrid=false;app.assetFirstPerson=true;app.viewmodelCamera=true;app.renderer.setDebugView(1);
   float wristError=0;int samples=0;
   for(const char* action:{"idle","fire","reload"}){
    auto clip=findViewmodelClip(app,action);if(!clip){report<<"MISSING action "<<action<<std::endl;++failures;continue;}
    app.animationIndex=*clip;app.actionActive=false;app.transitioning=false;
    for(int phase=0;phase<5;++phase){app.animationFrame=app.scene.animations[*clip].durationFrames*phase*.25f;const auto pose=app.scene.samplePose(*clip,app.animationFrame);++samples;
     for(const auto& m:pose)for(float f:m.v)if(!std::isfinite(f)){++failures;break;}
     for(const char* side:{"le","ri"}){const auto& names=app.scene.skeleton.boneByName;const auto name=std::string("j_wrist_")+side;const auto s=names.at(names.contains(name)?name:std::string("b_")+(std::string(side)=="le"?"Left":"Right")+"Hand"),d=names.at(std::string("codm_legacy|j_wrist_")+side);wristError=std::max(wristError,scene::length(scene::transformPoint(pose[s],{})-scene::transformPoint(pose[d],{})));}
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
