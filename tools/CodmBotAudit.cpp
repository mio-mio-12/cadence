#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc,char** argv){
 if(!glfwInit())return 1;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(1280,720,"CODM bot audit",nullptr,nullptr);if(!window)return 1;glfwMakeContextCurrent(window);glfwSwapInterval(0);
 auto state=std::make_unique<AppState>();auto& app=*state;std::string error;
 app.defaultSalukiDirectory="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
 if(!app.renderer.initialize(error))return 2;
 app.window=window;
 const bool fullScene=argc>1;
 if(fullScene){
  ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
  app.loadedMap.emplace();if(!scene::glb::load(argv[1],*app.loadedMap,error,1.f,true)){std::cerr<<error;return 7;}
  if(!loadVisualPreset(app,"Cadence Assets/visual_presets/night_23.castvisual")){std::cerr<<"preset failed";return 8;}
  app.water.enabled=true;app.water.height=50;static_cast<render::water::Appearance&>(app.water)=render::water::preset(2);
 }
 for(auto game:{"bo2","codm"})if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))return 3;
 for(size_t i=0;i<app.assetCatalog.entries.size();++i)if(app.assetCatalog.entries[i].name=="t6_wpn_ar_an94_view_LOD0")app.selectedWeaponAsset=i;
 app.enemyBotCount=6;app.botTeamSide=3;app.botAnimationGame="bo2";app.playing=true;app.actorPosition={0,0,0};
 const scene::Vec3 center=argc>2?scene::Vec3{-3152,5559,5}:scene::Vec3{-2723,2136,5};
 if(fullScene){app.actorPosition=center+scene::Vec3{0,-400,0};for(int i=0;i<6;++i)app.botSpawnPoints.push_back(center+scene::Vec3{float(i%3-1)*130,float(i/3)*150,0});}
 std::set<std::string> covered;
 for(int round=0;round<(fullScene?3:9);++round){
  const char* game=round==0?"bo2":"codm";
  app.botGame=game;app.botSystemMode=1;const auto begin=std::chrono::steady_clock::now();
  rebuildBotActors(app);
  std::cout<<"SPAWN "<<game<<" status="<<app.status<<" bots="<<app.bots.size()<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<std::endl;
  if(!app.botActorScene||app.bots.size()!=6)return 4;
  const auto& s=*app.botActorScene;
  for(const auto& part:s.rigParts){std::cout<<"PART "<<part.name<<std::endl;covered.insert(part.name);}
  std::cout<<"SCENE bones="<<s.skeleton.bones.size()<<" meshes="<<s.meshes.size()<<" clips="<<s.animations.size()<<std::endl;
  double sum=0,peak=0;
  for(int i=0;i<300;++i){const auto start=std::chrono::steady_clock::now();updateBotActors(app,1.f/165);const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();sum+=ms;peak=std::max(peak,ms);}
  std::cout<<"CPU "<<game<<" mean="<<sum/300<<" peak="<<peak<<" ai="<<app.botTimingAiMs<<" physics="<<app.botTimingPhysicsMs<<" sample="<<app.botTimingPoseSampleMs<<std::endl;
  scene::CastScene empty;if(!app.renderer.loadScene(empty,error)||!app.renderer.loadAuxiliaryScenes(app.loadedMap?&app.loadedMap->scene:nullptr,nullptr,&s,error)){std::cerr<<error;return 5;}
  auto poses=app.botActorPoses;std::vector<int> variants;
  for(size_t i=0;i<poses.size();++i){variants.push_back(app.bots[i].modelVariant);const auto p=gameplay::bot::presentationPosition(app.bots[i]);for(auto& m:poses[i]){m.v[12]+=(int(i)%3-1)*100-p.x;m.v[13]+=(int(i)/3)*100-p.y;m.v[14]-=p.z;}}
  const scene::Vec3 camera{320,-380,190};app.renderer.setCameraPosition(camera);
  const auto vp=scene::perspective(65*scene::kPi/180,1920.f/1080,1,5000)*scene::lookAt(camera,{0,60,85},{0,0,1});
  app.renderer.setSun(true,true,{.4f,.3f,-1},1,.4f,{1,1,1},{1,1,1},2048,800,600,{},false,512,600,1600,100);
  if(fullScene){
   for(auto& pose:poses)for(auto& m:pose){m.v[12]+=center.x;m.v[13]+=center.y;m.v[14]+=center.z;}
   app.botActorPoses=poses;app.cameraTarget=center+scene::Vec3{0,50,85};app.cameraDistance=500;app.cameraYaw=scene::kPi*.5f;app.cameraPitch=-.1f;
   int width=1920,height=1080;
   const auto draw=[&](){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({float(width),float(height)});ImGui::Begin("diagnostic");drawViewport(app,float(width-20),float(height-40));ImGui::End();ImGui::Render();glFinish();};
   for(int resolution:{1080,1440})for(int rain:{0,1})for(int count:{0,6}){
    height=resolution;width=height*16/9;app.water.enabled=rain!=0;app.botActorPoses=count?poses:std::vector<std::vector<scene::Mat4>>{};
    const auto cold=std::chrono::steady_clock::now();draw();const double first=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-cold).count();
    for(int i=0;i<20;++i)draw();const auto start=std::chrono::steady_clock::now();for(int i=0;i<80;++i)draw();
    auto stats=app.renderer.renderStats();std::cout<<"FULL_VIEWPORT "<<game<<" height="<<height<<" rain="<<rain<<" bots="<<count<<" first_ms="<<first<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/80<<" gpu="<<app.renderer.gpuFrameMilliseconds()<<" textures="<<stats.loadedTextureCount<<" vramMB="<<stats.estimatedVramBytes/1048576<<" freeMB="<<stats.vramFreeMb<<std::endl;
   }
   if(!app.renderer.saveColorPng(std::filesystem::path("diagnostics/v167")/(std::string(game)+"_full_"+std::to_string(round)+".png"),error))return 9;
   continue;
  }
  for(int i=0;i<30;++i)app.renderer.render(empty,{},vp,1920,1080,false,false,false,&s,&poses,&variants,nullptr,nullptr,false);
  glFinish();const auto start=std::chrono::steady_clock::now();
  for(int i=0;i<120;++i){app.renderer.render(empty,{},vp,1920,1080,false,false,false,&s,&poses,&variants,nullptr,nullptr,false);glFinish();}
  std::cout<<"RENDER "<<game<<" six_varied_bots_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/120<<" gpu_ms="<<app.renderer.gpuFrameMilliseconds()<<std::endl;
  std::filesystem::create_directories("diagnostics/v167");if(!app.renderer.saveColorPng(std::filesystem::path("diagnostics/v167")/(std::string(game)+"_six_"+std::to_string(round)+".png"),error)){std::cerr<<error;return 6;}
 }
 std::cout<<"Unique assembled parts: "<<covered.size()<<std::endl;
 if(fullScene){ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();}
 app.renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();
}
