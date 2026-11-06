#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc,char** argv)try{
 if(argc!=4&&argc!=5)return 2;const bool street=argc==5&&std::string_view(argv[4])=="street";
 const bool rig=street||(argc==5&&std::string_view(argv[4])=="motion-rig");
 const bool breakdown=argc==5&&std::string_view(argv[4])=="breakdown",motion=rig||(argc==5&&std::string_view(argv[4])=="motion");std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
 if(!glfwInit())return 3;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(1920,1080,"Weather audit",nullptr,nullptr);if(!window)return 4;
 glfwMakeContextCurrent(window);glfwSwapInterval(0);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto owner=std::make_unique<AppState>();auto& app=*owner;app.window=window;std::string error;
 const auto check=[&](bool ok){if(!ok)throw std::runtime_error(error+" "+app.status);};
 check(app.renderer.initialize(error));
 if(rig){
  const std::filesystem::path root="exported_files/cs2";
  app.scene=scene::buildScene(cast::Document::load(root/"models/viewhands/viewhands_model.cast"));
  scene::appendRigModel(cast::Document::load(root/"models/weapons/knife_css/knife_css_model.cast"),app.scene,"knife",scene::Mat4::identity(),1);
  scene::appendAnimations(cast::Document::load(root/"animations/viewmodel/knife_css/idle1_css.cast"),app.scene);
  if(app.scene.meshes.empty()||app.scene.animations.empty())throw std::runtime_error("Missing motion-rig assets");
 }
 check(app.renderer.loadScene(app.scene,error));
 app.renderer.setGpuPassProfiling(breakdown);
 std::cout<<"Loading Rust\n"<<std::flush;app.loadedMap.emplace();check(scene::c2m::load(argv[1],*app.loadedMap,error));
 check(app.renderer.loadAuxiliaryScenes(&app.loadedMap->scene,nullptr,nullptr,error));
 check(loadVisualPreset(app,argv[2]));syncVisualRendererSettings(app);
 app.renderer.setHbao(app.hbao);app.renderer.setDepthOfField(app.dof);app.renderer.setVolumetricLighting(app.volumetric);
 app.renderer.setWater(app.water,2);app.renderer.setRain(app.rain,2,&*app.loadedMap);app.renderer.setWetSurfaces(app.wet);
 std::vector<float> xs,ys;for(auto& mesh:app.loadedMap->scene.meshes)if(!mesh.vertices.empty()){
  scene::Vec3 mean{};for(auto& v:mesh.vertices)mean+=scene::transformPoint(mesh.modelTransform,v.position);mean=mean/float(mesh.vertices.size());xs.push_back(mean.x);ys.push_back(mean.y);
 }
 if(xs.empty())return 5;std::sort(xs.begin(),xs.end());std::sort(ys.begin(),ys.end());
 gameplay::bot::CodmSpawnSet spawns;
 if(std::filesystem::exists(std::filesystem::path(argv[1]).parent_path()/"spawns.json"))check(gameplay::bot::loadCodmSpawns(std::filesystem::path(argv[1]).parent_path()/"spawns.json",app.loadedMap->scaleMultiplier,spawns,error));
 else if(app.loadedMap->hasDefaultSpawnPoint)spawns.spawns.push_back(app.loadedMap->defaultSpawnPoint);
 if(spawns.spawns.empty())return 6;
 std::ofstream log(out/"timings.csv");log<<"view,mode,median_completed_ms,mean_abs_channel_error,max_channel_error\n";
 std::ofstream passLog(out/"passes.csv");passLog<<"view,mode,pass,milliseconds\n";
 const float az=app.sunAzimuth*scene::kPi/180,el=app.sunElevation*scene::kPi/180;
 if(motion){
  auto base=spawns.spawns.front()+scene::Vec3{0,0,app.actorViewHeight};
  if(street){base.x=xs[xs.size()/2];base.y=ys[ys.size()/2];}
  std::cout<<"Motion center "<<base.x<<','<<base.y<<','<<base.z<<'\n'<<std::flush;
  std::ofstream frames(out/"motion.csv");frames<<"frame,mode,mean_luma,black_fraction,camera_x,camera_y,camera_z\n";
  const int frameCount=street?600:180;
  for(int frame=0;frame<frameCount;++frame){
   const float phase=float(frame)/(frameCount-1),angle=phase*scene::kPi*2;
   auto camera=base+scene::Vec3{std::sin(angle)*(street?1200:60),std::cos(angle)*(street?600:60),0};
   if(street)camera.z=app.loadedMap->groundHeight(camera.x,camera.y,base.z,base.z-app.actorViewHeight)+app.actorViewHeight;
   const scene::Vec3 forward{std::cos(angle),std::sin(angle),-.08f};
   app.renderer.setCameraPosition(camera);app.renderer.setCameraDepthRange(1,100000);app.renderer.setEnvironmentCamera(forward,{0,0,1},75,1920.f/1080);
   app.renderer.setSun(app.sunLighting,app.sunShadows,{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),-std::sin(el)},app.sunIntensity,app.sunAmbient,app.sunColor,app.ambientColor,1024,1000,800,camera,false,512,700,2000,100);
   const auto vp=scene::perspective(75*scene::kPi/180,1920.f/1080,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
   std::vector<scene::Mat4> pose;if(rig){pose=app.scene.samplePose(0,float(frame%60));const auto cameraTransform=scene::inverseAffine(scene::lookAtDirection(camera,forward,{0,0,1}))*scene::lookAtDirection({},{1,0,0},{0,0,1});for(auto& bone:pose)bone=cameraTransform*bone;}
   app.renderer.setWeather(app.weather,3+frame/60.);app.renderer.setRain(app.rain,3+frame/60.,&*app.loadedMap);
   for(int mode=street?1:0;mode<2;++mode){
    app.renderer.setWeatherFast(mode==1);app.renderer.setPostTargetExchange(mode==1);
    app.renderer.render(app.scene,pose,vp,1920,1080,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,rig,&app.loadedMap->scene);
    std::vector<uint8_t> pixels;check(app.renderer.readColorRgba(pixels,error));double luma{};size_t black{};
    for(size_t i=0;i<pixels.size();i+=4){const int sum=pixels[i]+pixels[i+1]+pixels[i+2];luma+=sum/3.;black+=sum<6;}
    frames<<frame<<','<<mode<<','<<luma/(pixels.size()/4)<<','<<double(black)/(pixels.size()/4)<<','<<camera.x<<','<<camera.y<<','<<camera.z<<'\n';frames.flush();
      if(frame%30==0||frame==358||black>pixels.size()/8)check(app.renderer.saveColorPng(out/("motion_"+std::to_string(frame)+"_"+std::to_string(mode)+".png"),error));
    if(black>pixels.size()/8){
     std::cout<<"Black frame "<<frame<<" camera "<<camera.x<<','<<camera.y<<','<<camera.z<<'\n'<<std::flush;
      for(int isolate=0;isolate<16;++isolate){
       syncVisualRendererSettings(app);
       app.renderer.setSun(app.sunLighting,isolate!=15&&app.sunShadows,{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),-std::sin(el)},app.sunIntensity,app.sunAmbient,app.sunColor,app.ambientColor,1024,1000,800,camera,false,512,700,2000,100);
       auto volumetric=app.volumetric;if(isolate==6)volumetric.enabled=false;app.renderer.setVolumetricLighting(volumetric);
       if(isolate==7)app.renderer.setLensTweaks(false,0,0,0,1,0,0,false,0,1,1);
       if(isolate==8)app.renderer.setPostEffects(app.bloomRadius,app.lensDistortion,app.lutIntensity,false,0,false,0);
       if(isolate==9)app.renderer.setFilmTweaks(false,0,1,0,{1,1,1},{1,1,1},{1,1,1},false,false);
       if(isolate==10)app.renderer.setDebugView(1);
       if(isolate==11)app.renderer.setNormalMapIntensity(0);
       if(isolate==12)app.renderer.setSun(false,false,{0,0,-1},0,1,{1,1,1},{1,1,1},1024,1000,800,camera,false,512,700,2000,100);
       if(isolate==14)app.renderer.setBrdfModel(1);
      auto weather=app.weather;auto dof=app.dof;auto ao=app.hbao;
      if(isolate==1)weather={};if(isolate==2)dof.enabled=false;if(isolate==3)ao.enabled=false;
      app.renderer.setWeather(weather,3+frame/60.);app.renderer.setDepthOfField(dof);app.renderer.setHbao(ao);
      app.renderer.setWeatherFast(isolate!=4);app.renderer.setPostTargetExchange(isolate!=5);
       app.renderer.render(app.scene,pose,vp,1920,1080,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,rig&&isolate!=13,&app.loadedMap->scene);
      check(app.renderer.saveColorPng(out/("isolate_"+std::to_string(frame)+"_"+std::to_string(isolate)+".png"),error));
     }
      syncVisualRendererSettings(app);app.renderer.setVolumetricLighting(app.volumetric);
      app.renderer.setWeather(app.weather,3+frame/60.);app.renderer.setDepthOfField(app.dof);app.renderer.setHbao(app.hbao);
     app.renderer.setWeatherFast(mode==1);app.renderer.setPostTargetExchange(mode==1);
    }
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Motion GL error");
   }
  }
  app.renderer.shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
 }
 for(int view=0;view<6;++view){
  scene::Vec3 camera=spawns.spawns[(view/2)*(spawns.spawns.size()/3)]+scene::Vec3{0,0,app.actorViewHeight};
  auto forward=scene::normalize(scene::Vec3{xs[xs.size()/2]-camera.x,ys[ys.size()/2]-camera.y,0});
  if(view%2)forward=scene::normalize(scene::Vec3{forward.x-forward.y*.5f,forward.y+forward.x*.5f,-.1f});
  std::cout<<"spawn eye "<<camera.x<<','<<camera.y<<','<<camera.z<<" height "<<app.actorViewHeight<<'\n'<<std::flush;
  app.renderer.setCameraPosition(camera);app.renderer.setCameraDepthRange(1,100000);app.renderer.setEnvironmentCamera(forward,{0,0,1},75,1920.f/1080);
  app.renderer.setSun(app.sunLighting,false,{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),-std::sin(el)},app.sunIntensity,app.sunAmbient,app.sunColor,app.ambientColor,1024,1000,800,camera,false,512,700,2000,100);
  auto vp=scene::perspective(75*scene::kPi/180,1920.f/1080,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
  std::vector<uint8_t> reference,pixels;
  for(int mode=0;mode<(breakdown?6:3);++mode){
   auto weather=app.weather;auto rain=app.rain;auto wet=app.wet;auto water=app.water;
   if(breakdown){
    if(mode==1)rain.enabled=false;
    if(mode==2)wet.enabled=false;
    if(mode==3)rain.wallMist=false;
    if(mode==4)rain.splashes=false;
    if(mode==5){rain.enabled=false;wet.enabled=false;water.enabled=false;weather={};}
   }else if(mode==0)weather.dust.enabled=false;
   app.renderer.setWeatherFast(breakdown||mode!=1);app.renderer.setWeather(weather,3);
   app.renderer.setRain(rain,2,&*app.loadedMap);app.renderer.setWetSurfaces(wet);app.renderer.setWater(water,2);
   const auto draw=[&]{app.renderer.render(app.scene,{},vp,1920,1080,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&app.loadedMap->scene);};
   for(int i=0;i<20;++i)draw();glFinish();std::vector<double> times;
   for(int i=0;i<15;++i){auto start=std::chrono::steady_clock::now();draw();glFinish();times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
   std::sort(times.begin(),times.end());check(app.renderer.readColorRgba(pixels,error));double mae=0;int maxError=0;
   if(breakdown){const auto& result=app.renderer.gpuPassTimings();for(int i=0;i<result.count;++i)passLog<<view<<','<<mode<<','<<result.samples[i].name<<','<<result.samples[i].milliseconds<<'\n';passLog.flush();}
   if(!breakdown&&mode==1)reference=pixels;
   if(!breakdown&&mode==2){for(size_t i=0;i<pixels.size();++i){int e=std::abs(int(pixels[i])-int(reference[i]));mae+=e;maxError=std::max(maxError,e);}mae/=pixels.size();}
   check(app.renderer.saveColorPng(out/("view"+std::to_string(view)+"-mode"+std::to_string(mode)+".png"),error));
   if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GL error");
   if(!app.renderer.weatherError().empty())throw std::runtime_error(app.renderer.weatherError());
   log<<view<<','<<mode<<','<<times[7]<<','<<mae<<','<<maxError<<'\n';log.flush();
   std::cout<<"view "<<view<<" mode "<<mode<<" ms "<<times[7]<<" MAE "<<mae<<" max "<<maxError<<'\n'<<std::flush;
  }
 }
 app.renderer.shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}
