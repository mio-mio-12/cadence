#define main existingRainValidation
#include "RainVisualCheck.cpp"
#undef main
#include <fstream>
#include <sstream>
int main(int argc,char** argv){
    std::string error;CHECK(argc>1);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(640,360,"Rain moving regression",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
    render::StageRenderer r;CHECK(r.initialize(error));scene::CastScene empty;scene::glb::Map map;
    map.scene.meshes.push_back(box({-6000,-6000,-30},{6000,6000,0},{.2f,.2f,.2f,1}));
    for(int n=0;n<8;++n){const float x=(n%4-2)*750.f,y=(n/4)*1300.f;map.scene.meshes.push_back(box({x,y,0},{x+300,y+300,350},{.3f,.2f,.1f,1}));}
    collision(map);CHECK(r.loadScene(empty,error));CHECK(r.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
    render::rain::Settings rain=render::rain::preset(2);render::water::Settings water;water.enabled=true;water.height=15;water.radius=100;
    render::WetSettings wet;render::VolumetricLightingSettings volume;render::HbaoSettings ao;
    std::ifstream preset("Cadence Assets/visual_presets/RAINYWET2.castvisual");std::string line,key;
    while(std::getline(preset,line)){std::istringstream in(line);in>>key;
        if(key=="rain_v1")in>>rain;else if(key=="rain_style_v1")in>>rain.style;
        else if(key=="wet_surfaces_v1")in>>wet;else if(key=="wet_detail_v1")in>>wet.detail;
        else if(key=="water_appearance")in>>static_cast<render::water::Appearance&>(water);
        else if(key=="water_optics")in>>water.optics;
        else if(key=="volumetric")in>>volume;else if(key=="hbao")in>>ao;
    }
    r.setCameraDepthRange(1,100000);r.setWetSurfaces(wet);r.setDepthOfField({});
    std::vector<float> hdr(640*360*4);int failures=0;
    r.setVolumetricLighting({});r.setHbao({});
    // Raw HDR: no post pass may hide a NaN by mapping it to black.
    r.setLensTweaks(false,.439f,.19f,1,1,0,.257f,false,0,1,1);r.setPostEffects(2,0,1,false,0,false,1);r.setTonemapping(0);
    rain.shelter=false;
    const auto referenceWater=water;
    for(int shader=0;shader<4;++shader)for(int mode=0;mode<10;++mode){water=referenceWater;water.enabled=mode!=8;rain.enabled=mode==9;r.setWetSurfaces(mode==9?wet:render::WetSettings{});
        r.setMaterialParameters(.6f,{1,1,1},1,1,85,.219f,2,4,false,1,.07f,shader,true);
        if(mode==1)water.rain=0;
        if(mode==2)water.foam=0;
        if(mode==3)water.reflection=0;
        if(mode==4)water.optics.depthEnabled=false;
        if(mode==6)water.detail=0;
        if(mode==7)water.roughness=1;
        for(int frame=372;frame<379;++frame){
            const float t=frame/30.f;const scene::Vec3 eye{std::sin(t*.7f)*1700,std::cos(t*.51f)*1700,170+std::sin(t)*80};
            const scene::Vec3 dir=scene::normalize(scene::Vec3{std::sin(t),std::cos(t),std::sin(t*.43f)*.8f});
            const auto vp=scene::perspective(80*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(eye,dir,{0,0,1});
            r.setCameraPosition(eye);r.setEnvironmentCamera(dir,{0,0,1},80,16.f/9);
            r.setSun(mode!=5,false,{.3f,.45f,-.8f},1,.5f,{1,1,1},{1,1,1},512,7000,6000,eye,false,512,6000,9000,100);
            r.setWater(water,t);r.setRain(rain,t,&map);
            r.render(empty,{},vp,640,360,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map.scene);
            glBindTexture(GL_TEXTURE_2D,GLuint(r.colorTexture()));glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,hdr.data());CHECK(glGetError()==GL_NO_ERROR);
            std::size_t invalid=0;float peak=0;for(std::size_t p=0;p<hdr.size();++p)if(p%4!=3){if(!std::isfinite(hdr[p]))++invalid;else peak=std::max(peak,hdr[p]);}
            if(invalid||peak>100||peak<.001f){std::cout<<"FAIL shader="<<shader<<" mode="<<mode<<" frame="<<frame<<" invalid="<<invalid<<" peak="<<peak<<std::endl;++failures;}
            if(frame==375||invalid||peak>100){std::cout<<"shader="<<shader<<" mode="<<mode<<" frame="<<frame<<" peak="<<peak<<std::endl;
                CHECK(r.saveColorPng(out/("shader_"+std::to_string(shader)+"_mode_"+std::to_string(mode)+"_"+std::to_string(frame)+".png"),error));}
        }
    }
    r.shutdown();glfwDestroyWindow(window);glfwTerminate();CHECK(failures==0);std::cout<<"PASS 280 raw-HDR frames: four material paths, water component isolation, rain/wet on and off"<<std::endl;
}
