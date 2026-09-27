#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<error<<'\n';return 1;}}while(false)
int main(int argc,char** argv){
    std::string error;CHECK(argc==4);const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
    CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(1920,1080,"Map performance audit",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);glfwSwapInterval(0);
    auto owner=std::make_unique<AppState>();auto& app=*owner;CHECK(app.renderer.initialize(error));
    app.loadedMap.emplace();std::cout<<"Loading map\n"<<std::flush;
    CHECK(scene::glb::load(argv[1],*app.loadedMap,error,1,false));
    std::cout<<"Map meshes="<<app.loadedMap->scene.meshes.size()<<"\n"<<std::flush;
    CHECK(app.renderer.loadScene(app.scene,error));CHECK(app.renderer.loadAuxiliaryScenes(&app.loadedMap->scene,nullptr,nullptr,error));
    CHECK(loadVisualPreset(app,argv[2]));syncVisualRendererSettings(app);
    app.renderer.setHbao(app.hbao);app.renderer.setDepthOfField(app.dof);app.renderer.setVolumetricLighting(app.volumetric);
    app.renderer.setWater(app.water,2);app.renderer.setRain(app.rain,2,&*app.loadedMap);app.renderer.setWeather(app.weather,2,1);app.renderer.setWetSurfaces(app.wet);
    std::vector<float> xs,ys,zs;
    for(const auto& mesh:app.loadedMap->scene.meshes)if(!mesh.vertices.empty()){
        scene::Vec3 mean{};for(const auto& v:mesh.vertices)mean+=scene::transformPoint(mesh.modelTransform,v.position);mean=mean/float(mesh.vertices.size());xs.push_back(mean.x);ys.push_back(mean.y);zs.push_back(mean.z);
    }
    CHECK(!xs.empty());for(auto* v:{&xs,&ys,&zs})std::sort(v->begin(),v->end());
    scene::Vec3 center{xs[xs.size()/2],ys[ys.size()/2],zs[zs.size()/3]+160};
    std::cout<<"camera center="<<center.x<<','<<center.y<<','<<center.z<<"\n"<<std::flush;
    std::ofstream log(out/"timings.csv");log<<"view,mode,completed_ms,gpu_ms,map_draws,shadow_draws,draws\n";
    const float az=app.sunAzimuth*scene::kPi/180,el=app.sunElevation*scene::kPi/180;
    for(int view=0;view<6;++view){
        const auto camera=center+scene::Vec3{view>=4?600.f:0.f,view>=4?-600.f:0.f,0};
        const float yaw=float(view)*scene::kPi*.5f;const auto forward=scene::normalize(scene::Vec3{std::cos(yaw),std::sin(yaw),-.1f});
        app.renderer.setCameraPosition(camera);app.renderer.setCameraDepthRange(1,100000);app.renderer.setEnvironmentCamera(forward,{0,0,1},75,1920.f/1080);
        app.renderer.setSun(app.sunLighting,app.sunShadows,{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),-std::sin(el)},app.sunIntensity,app.sunAmbient,app.sunColor,app.ambientColor,app.shadowResolution,app.shadowDistanceMeters*gameplay::iw::kMetersToUnits,app.shadowFadeStartMeters*gameplay::iw::kMetersToUnits,camera,app.farShadowEnabled,app.farShadowResolution,app.farShadowStartMeters*gameplay::iw::kMetersToUnits,app.farShadowDistanceMeters*gameplay::iw::kMetersToUnits,app.farShadowBlendMeters*gameplay::iw::kMetersToUnits);
        const auto vp=scene::perspective(75*scene::kPi/180,1920.f/1080,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
        const auto draw=[&]{app.renderer.render(app.scene,{},vp,1920,1080,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&app.loadedMap->scene);};
        std::vector<std::uint8_t> reference,pixels;
        for(int mode=0;mode<5;++mode){
            auto dof=app.dof;auto ao=app.hbao;auto fog=app.volumetric;
            if(mode==2)dof.enabled=false;if(mode==3)ao.enabled=false;if(mode==4)fog.enabled=false;
            app.renderer.setDepthOfField(dof);app.renderer.setHbao(ao);app.renderer.setVolumetricLighting(fog);app.renderer.setInactiveDofPassElisionEnabled(mode!=0);
            for(int f=0;f<20;++f)draw();glFinish();const auto start=std::chrono::steady_clock::now();for(int f=0;f<80;++f)draw();glFinish();
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/80;
            const auto s=app.renderer.renderStats(false);log<<view<<','<<mode<<','<<ms<<','<<app.renderer.gpuFrameMilliseconds()<<','<<s.lastVisibleMapMeshes<<','<<s.lastShadowCasterDraws+s.lastFarShadowCasterDraws<<','<<s.lastTotalDrawCalls<<std::endl;
            if(mode==0)CHECK(app.renderer.readColorRgba(reference,error));
            if(mode==1){CHECK(app.renderer.readColorRgba(pixels,error));std::size_t changed{};int maxDelta{};for(std::size_t i=0;i<pixels.size();++i)if(reference[i]!=pixels[i]){++changed;maxDelta=std::max(maxDelta,std::abs(int(reference[i])-int(pixels[i])));}
                std::cout<<"view "<<view<<" changed channels="<<changed<<" max delta="<<maxDelta<<'\n'<<std::flush;
                CHECK(app.renderer.saveColorPng(out/("view_"+std::to_string(view)+".png"),error));
                CHECK(app.renderer.savePixelsPng(out/("reference_"+std::to_string(view)+".png"),1920,1080,reference,error));
                // Isolate nondeterministic content (e.g. progressive shelter work)
                // from an optimization difference using the same reference path.
                app.renderer.setInactiveDofPassElisionEnabled(false);draw();std::vector<std::uint8_t> repeat;CHECK(app.renderer.readColorRgba(repeat,error));std::size_t repeatChanged{};for(std::size_t i=0;i<repeat.size();++i)repeatChanged+=reference[i]!=repeat[i];std::cout<<"reference repeat changed="<<repeatChanged<<'\n'<<std::flush;
            }
        }
        std::cout<<"view "<<view<<" complete\n"<<std::flush;
    }
    owner.reset();glfwDestroyWindow(window);glfwTerminate();return 0;
}
