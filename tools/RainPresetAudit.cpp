#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" "<<error<<std::endl;return 1;}}while(false)
int main(int argc,char** argv){
    std::string error;CHECK(argc>2);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(960,540,"Hackney preset regression",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);ImGui::CreateContext();
    auto app=std::make_unique<AppState>();auto& r=app->renderer;CHECK(r.initialize(error));
    scene::glb::Map map;CHECK(scene::c2m::load(argv[2],map,error));std::cout<<"map meshes="<<map.scene.meshes.size()<<std::endl;
    scene::CastScene empty;CHECK(r.loadScene(empty,error));CHECK(r.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
    CHECK(loadVisualPreset(*app,argc>3?argv[3]:"Cadence Assets/visual_presets/RAINYWET2.castvisual"));
    app->dof.enabled=false;r.setDepthOfField(app->dof);r.setCameraDepthRange(2.5f,100000);
    r.setHbao(app->hbao);r.setVolumetricLighting(app->volumetric);r.setWetSurfaces(app->wet);
    const float az=app->sunAzimuth*scene::kPi/180,el=app->sunElevation*scene::kPi/180;
    const scene::Vec3 sun{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),-std::sin(el)};
    std::vector<float> hdr(960*540*4);int failures=0;
    const int frameBegin=argc>4?std::atoi(argv[4]):0,frameEnd=argc>4?frameBegin+1:240;
    for(int mode=0;mode<4;++mode){
        app->water.enabled=mode==0;app->water.height=100;app->rain.enabled=mode<2;
        r.setWetSurfaces(mode==3?render::WetSettings{}:app->wet);
        for(int frame=frameBegin;frame<frameEnd;++frame){const float t=frame/60.f,k=(1-std::cos(t*scene::kPi))*.5f;
            const scene::Vec3 eye{771.7471f+(903.8175f-771.7471f)*k,-2166.0654f+340.4034f*k,48.1913f-3.7239f*k+gameplay::iw::worldUnits(60)};
            const float yaw=1.19022f+.03960f*k,pitch=-.192f+.0484f*k;
            const scene::Vec3 dir{std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),std::sin(pitch)};
            const auto vp=scene::perspective(90*scene::kPi/180,16.f/9,2.5f,100000)*scene::lookAtDirection(eye,dir,{0,0,1});
            r.setCameraPosition(eye);r.setEnvironmentCamera(dir,{0,0,1},90,16.f/9);
            r.setSun(app->sunLighting,app->sunShadows,sun,app->sunIntensity,app->sunAmbient,app->sunColor,app->ambientColor,2048,4200,3000,eye,false,1024,3600,11000,800);
            r.setWater(app->water,t);r.setRain(app->rain,t,&map);
            r.render(empty,{},vp,960,540,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map.scene);
            glBindTexture(GL_TEXTURE_2D,GLuint(r.colorTexture()));glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,hdr.data());CHECK(glGetError()==GL_NO_ERROR);
            std::size_t invalid=0;float peak=0;double mean=0;for(std::size_t p=0;p<hdr.size();++p)if(p%4!=3){if(!std::isfinite(hdr[p]))++invalid;else{peak=std::max(peak,hdr[p]);mean+=hdr[p];}}mean/=960*540*3;
            if(invalid||peak<.001f){std::cout<<"FAIL mode="<<mode<<" frame="<<frame<<" invalid="<<invalid<<" mean="<<mean<<std::endl;++failures;}
            if(frame%30==0||frame==101||frame==139||invalid||peak<.001f){std::cout<<"mode="<<mode<<" frame="<<frame<<" peak="<<peak<<" mean="<<mean<<std::endl;CHECK(r.saveColorPng(out/("frame_"+std::to_string(mode)+"_"+std::to_string(frame)+".png"),error));}
        }
    }
    r.shutdown();app.reset();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();CHECK(failures==0);
}
