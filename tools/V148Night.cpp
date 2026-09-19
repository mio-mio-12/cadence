#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(int argc,char**argv){
 CHECK(argc>1);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,540,"Day/night validation",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
 auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;std::string error;CHECK(app.renderer.initialize(error));CHECK(app.renderer.setNightSkyPanorama(cadenceAssetsDirectory()/"sky"/"eso0932a.jpg"));
 app.dayNight.enabled=true;app.dayNight.running=false;app.dayNight.starTrailLength=35;app.dayNight.starTrailBrightness=2;
 CHECK(saveVisualPreset(app,out/"cycle.castvisual"));auto copy=std::make_unique<AppState>();std::ifstream file(out/"cycle.castvisual");VisualPresetResources resources;CHECK(parseVisualPreset(*copy,file,resources));CHECK(copy->dayNight.starTrailLength==35&&copy->dayNight.enabled);CHECK(copy->nightSky.enabled&&copy->nightSky.exposure==1.5f);
 std::istringstream old("CASTVISUAL 5\n");CHECK(parseVisualPreset(*copy,old,resources));CHECK(!copy->dayNight.enabled);
 // Tiny lit scene makes color, shadows and sky changes easy to judge without asset loading.
 scene::CastScene stage;scene::Mesh floor;floor.doubleSided=true;floor.color={.6f,.6f,.6f,1};floor.vertices={{{-1000,-1000,0}},{{1000,-1000,0}},{{1000,1000,0}},{{-1000,1000,0}}};floor.indices={0,1,2,0,2,3};for(auto&v:floor.vertices)v.normal={0,0,1};stage.meshes.push_back(floor);
 scene::Mesh wall;wall.doubleSided=true;wall.color={.7f,.7f,.7f,1};wall.vertices={{{150,-80,0}},{{150,80,0}},{{150,80,180}},{{150,-80,180}}};wall.indices={0,1,2,0,2,3};for(auto&v:wall.vertices)v.normal={-1,0,0};stage.meshes.push_back(wall);
 CHECK(app.renderer.loadScene(stage,error));const scene::Vec3 camera{-350,0,130},forward=scene::normalize(scene::Vec3{1,0,.35f});const auto vp=scene::perspective(70*scene::kPi/180,960.f/540,1,3000)*scene::lookAtDirection(camera,forward,{0,0,1});
 app.renderer.setEnvironmentCamera(forward,{0,0,1},70,960.f/540);app.renderer.setCameraPosition(camera);
 std::vector<std::uint8_t> noon,repeat,disabled,disabledAfter;
 auto render=[&](){app.renderer.setNightSky(app.nightSky);app.renderer.setDayNight(app.dayNight,0);auto c=render::daynight::evaluate(app.dayNight,0);app.renderer.setSun(true,true,c.lightDirection,c.sun.sunlight,c.sun.ambient,c.sun.sun,c.sun.ambientColor,512,2000,1800,camera,false,512,1800,3000,100);app.renderer.setFilmTweaks(true,c.film.brightness,c.film.contrast,c.film.desaturation,c.film.dark,{1,1,1},c.film.light,false,false);app.renderer.render(stage,{},vp,960,540,false,false,false);};
 app.dayNight.enabled=false;render();CHECK(app.renderer.readColorRgba(disabled,error));
 app.dayNight.enabled=true;app.dayNight.hour=12;render();CHECK(app.renderer.saveColorPng(out/"noon.png",error));CHECK(app.renderer.readColorRgba(noon,error));
 app.dayNight.hour=18;render();CHECK(app.renderer.saveColorPng(out/"sunset.png",error));
 app.dayNight.hour=0;app.dayNight.coverage=.15f;app.dayNight.starTrailLength=0;app.nightSky.enabled=false;render();CHECK(app.renderer.saveColorPng(out/"night_legacy.png",error));std::vector<std::uint8_t> dark,photo;CHECK(app.renderer.readColorRgba(dark,error));app.nightSky.enabled=true;render();CHECK(app.renderer.saveColorPng(out/"photographic.png",error));CHECK(app.renderer.readColorRgba(photo,error));
 double oldLight=0,newLight=0;for(size_t i=0;i<dark.size();i+=4){oldLight+=dark[i]+dark[i+1]+dark[i+2];newLight+=photo[i]+photo[i+1]+photo[i+2];}CHECK(newLight>oldLight*1.5);std::cout<<"Night image RGB sum: "<<oldLight<<" -> "<<newLight<<"\n";
 for(const auto index:{size_t((50*960+480)*4),size_t((100*960+480)*4)})for(int k=0;k<3;++k)CHECK(dark[index+k]==photo[index+k]);
 render();CHECK(app.renderer.saveColorPng(out/"night_points.png",error));
 app.nightSky.moonIntensity=0;app.nightSky.milkyWay=1.5f;app.nightSky.galaxyTilt=25;app.nightSky.galaxyAzimuth=90;app.nightSky.exposure=2;render();CHECK(app.renderer.saveColorPng(out/"dark_sky.png",error));app.nightSky={};
 app.dayNight.starTrailLength=35;render();CHECK(app.renderer.saveColorPng(out/"night_trails.png",error));
 app.dayNight.hour=12;app.dayNight.coverage=.55f;render();CHECK(app.renderer.readColorRgba(repeat,error));CHECK(noon==repeat);
 app.dayNight.enabled=false;render();CHECK(app.renderer.readColorRgba(disabledAfter,error));CHECK(disabled==disabledAfter);
 CHECK(glGetError()==GL_NO_ERROR);
 for(size_t i=0;i<render::daynight::presetNames.size();++i){
     applyDayNightPreset(app,render::daynight::builtinPreset(i));CHECK(saveDayNightPreset(app,out/("preset_"+std::to_string(i)+".castdaynight")));CHECK(loadDayNightPreset(app,out/("preset_"+std::to_string(i)+".castdaynight")));
     app.dayNight.running=false;
     for(float hour:{0.f,12.f,18.f}){app.dayNight.hour=hour;render();CHECK(app.renderer.saveColorPng(out/("preset_"+std::to_string(i)+"_hour_"+std::to_string(int(hour))+".png"),error));}
 }
 CHECK(glGetError()==GL_NO_ERROR);
 applyDayNightPreset(app,render::daynight::builtinPreset(2));app.dayNight.running=false;app.dayNight.hour=0;
 for(int algorithm=0;algorithm<4;++algorithm){app.nightSky.galaxyNoiseAlgorithm=algorithm;app.nightSky.galaxyNoiseScale=2.8f;render();CHECK(app.renderer.saveColorPng(out/("galaxy_algorithm_"+std::to_string(algorithm)+".png"),error));}
 for(float scale:{1.f,2.8f,4.f}){app.nightSky.galaxyNoiseAlgorithm=2;app.nightSky.galaxyNoiseScale=scale;render();CHECK(app.renderer.saveColorPng(out/("galaxy_scale_"+std::to_string(scale)+".png"),error));}app.nightSky.galaxyNoiseAlgorithm=3;app.nightSky.galaxyNoiseScale=2.8f;
 CHECK(saveVisualPreset(app,out/"noise.castvisual"));std::ifstream noisePreset(out/"noise.castvisual");CHECK(parseVisualPreset(*copy,noisePreset,resources));CHECK(copy->nightSky.galaxyNoiseAlgorithm==3&&copy->nightSky.galaxyNoiseScale==2.8f);
 app.recordedTake.boneCount=1;app.recordedTake.sampleRate=30;for(int i=0;i<=300;++i){take::Sample s;s.time=i/30.f;s.pose.push_back(scene::Mat4::identity());app.recordedTake.samples.push_back(s);}
 CHECK(take::save(app.recordedTake,out/"before.take",error));
 ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={960,540};io.DeltaTime=1.f/60;unsigned char*pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 auto press=[&](ImGuiKey key){io.AddKeyEvent(key,true);ImGui::NewFrame();handleTakeCameraInput(app,1.f/60);ImGui::EndFrame();io.AddKeyEvent(key,false);ImGui::NewFrame();handleTakeCameraInput(app,1.f/60);ImGui::EndFrame();};
 app.takeTime=4;app.takePreview=false;press(ImGuiKey_RightArrow);CHECK(app.takeTime==4);
 app.takePreview=true;app.exportEnd=10;press(ImGuiKey_RightArrow);CHECK(app.takeTime==5);press(ImGuiKey_LeftArrow);CHECK(app.takeTime==4);
 press(ImGuiKey_B);CHECK(app.exportStart==4);app.takeTime=7;press(ImGuiKey_N);CHECK(app.exportEnd==7);
 app.takePlaybackSpeed=1;press(ImGuiKey_UpArrow);CHECK(app.takePlaybackSpeed==1.25f);press(ImGuiKey_DownArrow);CHECK(app.takePlaybackSpeed==1);
 app.exportActive=true;press(ImGuiKey_LeftArrow);CHECK(app.takeTime==7);app.exportActive=false;
 app.takeFirstPersonView=false;press(ImGuiKey_F2);CHECK(app.takeFirstPersonView);press(ImGuiKey_F8);CHECK(!app.takeFirstPersonView);
 app.dayNight.enabled=true;app.dayNight.hour=2;app.dayNight.starTrailLength=90;app.nightSky.exposure=4;app.nightSky.moonIntensity=3;CHECK(take::save(app.recordedTake,out/"after.take",error));
 auto bytes=[](const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};CHECK(bytes(out/"before.take")==bytes(out/"after.take"));ImGui::DestroyContext();
 std::cout<<"PASS visual presets, shader, noon/sunset/night/trails, byte-identical seek/restoration; actual replay hotkeys, live/export exclusion, and byte-identical take files after visual changes\n";
 app.renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();
}
