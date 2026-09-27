#include "render/StageRenderer.h"
#include "scene/C2MMap.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <filesystem>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" "<<error<<'\n';return 1;}}while(false)
static scene::Mesh box(scene::Vec3 lo,scene::Vec3 hi,scene::Vec4 color){
    scene::Mesh m;m.name="weather_fixture";m.color=color;m.doubleSided=true;
    const scene::Vec3 p[]={{lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},{lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}};
    const int faces[][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};
    for(const auto& f:faces){const auto n=scene::normalize(scene::cross(p[f[1]]-p[f[0]],p[f[2]]-p[f[0]]));const auto base=unsigned(m.vertices.size());for(int k:f){scene::Vertex v;v.position=p[k];v.normal=n;m.vertices.push_back(v);}m.indices.insert(m.indices.end(),{base,base+1,base+2,base,base+2,base+3});}return m;
}
int main(int argc,char** argv){
    std::string error;CHECK(argc>1);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(1280,720,"Weather validation",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);glfwSwapInterval(0);
    {
        render::StageRenderer renderer;CHECK(renderer.initialize(error));scene::CastScene empty;scene::glb::Map map;
        map.scene.meshes.push_back(box({-4000,-4000,-30},{4000,5000,0},{.16f,.18f,.21f,1}));
        map.scene.meshes.push_back(box({-900,1400,0},{900,1500,1200},{.11f,.14f,.19f,1}));
        map.scene.meshes.push_back(box({-900,0,420},{-40,800,450},{.24f,.28f,.3f,1}));
        map.scene.meshes.push_back(box({-900,0,0},{-870,800,450},{.3f,.31f,.33f,1}));
        map.scene.meshes.push_back(box({-150,700,0},{-110,750,420},{.3f,.31f,.33f,1}));
        map.scene.meshes.push_back(box({300,400,0},{460,580,130},{.25f,.18f,.12f,1}));
        CHECK(renderer.loadScene(empty,error));CHECK(renderer.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
        const scene::Vec3 origin{0,-600,170};scene::Vec3 camera=origin,forward=scene::normalize(scene::Vec3{0,1,-.09f});
        renderer.setCameraDepthRange(1,100000);renderer.setEnvironmentCamera(forward,{0,0,1},65,16.f/9);
        renderer.setSun(true,true,{.3f,.45f,-.8f},1.2f,.6f,{.9f,.95f,1},{.7f,.8f,1},1024,7000,6000,origin,false,512,6000,9000,100);
        render::weather::Settings weather;render::rain::Settings rain;render::water::Settings water;
        const auto render=[&](double t,int w=1280,int h=720,int mode=0){
            renderer.setCameraPosition(camera);auto vp=scene::perspective(65*scene::kPi/180,float(w)/h,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
            renderer.setWeather(weather,t,mode);renderer.setRain(rain,t,&map);renderer.setWater(water,t);
            renderer.render(empty,{},vp,w,h,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map.scene);renderer.finishLensWeather();
        };
        std::vector<std::uint8_t> baseline,first,repeat,later;std::vector<float> depthBefore,depthAfter;
        render(2.25);CHECK(renderer.readColorRgba(baseline,error));CHECK(renderer.saveColorPng(out/"baseline.png",error));CHECK(renderer.readDepthRawFloat(depthBefore,1,100000,1,100000,false,error));
        for(int n=0;n<8;++n){
            if(n==6)continue; // Removed lens preset; old numbering remains readable.
            weather=render::weather::preset(n);render(2.25);error=renderer.weatherError();CHECK(error.empty());CHECK(renderer.readColorRgba(first,error));CHECK(first!=baseline);CHECK(glGetError()==GL_NO_ERROR);
            CHECK(renderer.saveColorPng(out/("preset_"+std::to_string(n)+".png"),error));
            render(9.75);CHECK(renderer.readColorRgba(later,error));CHECK(later!=first);camera=origin+scene::Vec3{2500,1000,900};render(100);camera=origin;
            render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
            CHECK(renderer.readDepthRawFloat(depthAfter,1,100000,1,100000,false,error));CHECK(depthBefore==depthAfter);
            weather={};render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(baseline==repeat);
            std::cout<<"preset "<<n<<" seek/depth/bypass passed\n";
        }
        // Above a fog bank, looking down must enter it; looking up must not.
        // This catches a flipped camera-forward extraction that generic image
        // differences and deterministic-seek checks cannot detect.
        const auto savedForward=forward;const std::size_t center=(360*1280+640)*4;
        for(bool down:{true,false}){
            forward=scene::normalize(scene::Vec3{0,1,down?-.7f:.7f});weather={};render(2);CHECK(renderer.readColorRgba(first,error));
            weather=render::weather::preset(4);weather.fog.bottom=0;weather.fog.top=120;weather.fog.breakup=0;weather.fog.density=100;
            render(2);CHECK(renderer.readColorRgba(repeat,error));int difference=0;for(int c=0;c<3;++c)difference+=std::abs(int(first[center+c])-int(repeat[center+c]));
            CHECK(down?difference>24:difference==0);
        }
        forward=savedForward;weather={};std::cout<<"Ground-fog camera direction passed\n";
        weather=render::weather::preset(0);weather.snow.density=1;render(2);CHECK(renderer.readColorRgba(first,error));weather.snow.density=3;render(2);CHECK(renderer.readColorRgba(repeat,error));CHECK(first!=repeat);
        weather=render::weather::preset(3);weather.dustAerosolAmount=0;render(2);CHECK(renderer.readColorRgba(first,error));weather.dustAerosolAmount=2;weather.dustAerosolDetail=80;render(2);CHECK(renderer.readColorRgba(repeat,error));CHECK(first!=repeat);
        weather={};weather.lens.enabled=true;render(2.25);CHECK(renderer.readColorRgba(first,error));CHECK(first==baseline);
        std::cout<<"Density override, dust aerosols and removed lens bypass passed\n";
        // Clouds must leave ambient-only illumination and emissive output alone.
        renderer.setSun(true,false,{.3f,.45f,-.8f},0,1,{1,1,1},{.7f,.8f,1},512,7000,6000,origin,false,512,6000,9000,100);
        weather={};render(2);CHECK(renderer.readColorRgba(first,error));weather=render::weather::preset(5);render(2);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
        renderer.setSun(true,true,{.3f,.45f,-.8f},1.2f,.6f,{.9f,.95f,1},{.7f,.8f,1},1024,7000,6000,origin,false,512,6000,9000,100);
        weather=render::weather::preset(6);weather.lens.freeCamera=false;render(2,1280,720,1);CHECK(renderer.readColorRgba(first,error));weather={};render(2);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
        weather=render::weather::preset(1);weather.dust.enabled=weather.debris.enabled=weather.cloud.enabled=weather.lens.enabled=true;weather.lens.frost=.5f;
        for(int debug:{3,7,9}){renderer.setDebugView(debug);render(2);CHECK(renderer.readColorRgba(first,error));auto saved=weather;weather={};render(2);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);weather=saved;}renderer.setDebugView(0);
        rain=render::rain::preset(1);water.enabled=true;water.height=30;render::HbaoSettings ao;ao.enabled=true;renderer.setHbao(ao);renderer.setFog(true,{.3f,.4f,.45f},1000,3000,.8f,0);
        for(int w:{640,1280,960}){render(4,w,w==960?960:w*9/16);CHECK(glGetError()==GL_NO_ERROR);CHECK(renderer.saveColorPng(out/("combined_"+std::to_string(w)+".png"),error));}
        renderer.setHbao({});renderer.setFog(false,{},0,3000,1,0);rain.enabled=false;water.enabled=false;
        for(int n=-1;n<8;++n){weather=n<0?render::weather::Settings{}:render::weather::preset(n);for(int j=0;j<10;++j){render(j/60.);glFinish();}std::vector<double> frames;
            for(int j=0;j<60;++j){auto start=std::chrono::steady_clock::now();render(j/60.);glFinish();frames.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
            std::sort(frames.begin(),frames.end());std::cout<<"preset="<<n<<" completed frame ms p50="<<frames[30]<<" p95="<<frames[57]<<" p99="<<frames[59]<<" GPU="<<renderer.gpuFrameMilliseconds()<<'\n';
        }
        weather=render::weather::preset(0);for(int i=0;i<24;++i){camera=origin+scene::Vec3{float(i)*12,0,0};render(2+i/24.);CHECK(renderer.saveColorPng(out/("snow_motion_"+std::to_string(i)+".png"),error));}
        if(argc>2){
            bool c2m=std::filesystem::path(argv[2]).extension()==".c2m";
            CHECK(c2m?scene::c2m::load(argv[2],map,error):scene::glb::load(argv[2],map,error,1.f,false));CHECK(renderer.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
            camera=c2m?scene::Vec3{800,-2050,200}:scene::Vec3{1107.8798f,4603.7412f,-4400.f};forward=scene::normalize(c2m?scene::Vec3{.37f,.925f,-.13f}:scene::Vec3{-.5f,-.85f,-.04f});renderer.setEnvironmentCamera(forward,{0,0,1},65,16.f/9);
            renderer.setSun(true,true,{.3f,.45f,-.8f},1.2f,.6f,{.9f,.95f,1},{.7f,.8f,1},2048,7000,6000,camera,false,512,6000,9000,100);
            for(int n=-1;n<8;++n){weather=n<0?render::weather::Settings{}:render::weather::preset(n);weather.fog.bottom=camera.z-170;weather.fog.top=camera.z+(n==1?900.f:-60.f);weather.dust.bottom=camera.z-165;weather.dust.top=camera.z+(n==3?900.f:190.f);
                render(2.25);CHECK(renderer.saveColorPng(out/("map_"+std::to_string(n)+".png"),error));CHECK(glGetError()==GL_NO_ERROR);}
            weather=render::weather::preset(6);
            for(int i=0;i<48;++i){render(2.+i/24.);CHECK(renderer.saveColorPng(out/("lens_motion_"+std::to_string(i)+".png"),error));}
            // Interleave warmed full-frame timings to reduce clock/warmup bias.
            // glFinish includes the separate lens composite (the scene GPU timer doesn't).
            std::vector<double> plain,lens;
            for(int i=0;i<144;++i){const bool on=(i%4==1||i%4==2);weather=on?render::weather::preset(6):render::weather::Settings{};
                auto start=std::chrono::steady_clock::now();render(2.+i/60.);glFinish();
                if(i>=24)(on?lens:plain).push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
            std::sort(plain.begin(),plain.end());std::sort(lens.begin(),lens.end());
            std::cout<<"Map full-frame ms: off p50="<<plain[30]<<" p95="<<plain[57]<<"; lens p50="<<lens[30]<<" p95="<<lens[57]<<'\n';
        }
        CHECK(glGetError()==GL_NO_ERROR);std::cout<<"Weather renderer checks passed\n";
    }
    glfwDestroyWindow(window);glfwTerminate();
}
