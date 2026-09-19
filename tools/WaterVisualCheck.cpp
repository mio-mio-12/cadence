#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <chrono>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" "<<error<<'\n';return 1;}}while(false)
static scene::Mesh box(scene::Vec3 lo,scene::Vec3 hi,scene::Vec4 color){
    scene::Mesh m;m.name="water_test_block";m.color=color;m.doubleSided=true;
    const scene::Vec3 p[]={{lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},
        {lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}};
    const int faces[][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};
    for(const auto& f:faces){const auto n=scene::normalize(scene::cross(p[f[1]]-p[f[0]],p[f[2]]-p[f[0]]));const auto base=static_cast<unsigned>(m.vertices.size());
        for(int id:f){scene::Vertex v;v.position=p[id];v.normal=n;m.vertices.push_back(v);}
        m.indices.insert(m.indices.end(),{base,base+1,base+2,base,base+2,base+3});}
    return m;
}
int main(int argc,char** argv){
    std::string error;CHECK(argc>1);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(1280,720,"Water validation",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
    {
    render::StageRenderer renderer;CHECK(renderer.initialize(error));scene::CastScene empty,map;
    if(argc>2){CHECK(renderer.setT6SkyboxIwi(argv[2],error));renderer.setEnvironmentParameters(2.38f,1.5f,0,1,1,.75f,1);}
    map.meshes.push_back(box({-20000,-20000,-200},{20000,20000,-180},{.3f,.3f,.25f,1}));
    map.meshes.push_back(box({-400,500,-300},{400,900,180},{.65f,.34f,.15f,1}));
    map.meshes.push_back(box({650,1700,-300},{1100,2000,350},{.2f,.48f,.25f,1}));
    map.meshes.push_back(box({-1200,900,-200},{-1000,1500,450},{.6f,.58f,.53f,1}));
    CHECK(renderer.loadScene(empty,error));CHECK(renderer.loadAuxiliaryScenes(&map,nullptr,nullptr,error));
    scene::Vec3 camera{100,-1100,260};const auto forward=scene::normalize(scene::Vec3{0,1,-.1f});
    const auto vp=scene::perspective(65*scene::kPi/180,1280.f/720,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
    renderer.setCameraPosition(camera);renderer.setCameraDepthRange(1,100000);renderer.setEnvironmentCamera(forward,{0,0,1},65,1280.f/720);
    renderer.setSun(true,true,{.3f,.45f,-.8f},2,.55f,{1,.92f,.8f},{.75f,.85f,1},1024,7000,6000,camera,false,512,6000,9000,100);
    render::water::Settings water;water.height=0;water.radius=250;
    const auto render=[&](double t,int w=1280,int h=720){renderer.setWater(water,t);renderer.render(empty,{},vp,w,h,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map);};
    std::vector<std::uint8_t> before,after,first,repeat,other,rain;
    render(0);CHECK(renderer.readColorRgba(before,error));water.enabled=true;
    for(int n=0;n<5;++n){static_cast<render::water::Appearance&>(water)=render::water::preset(n);render(3.25);
        CHECK(renderer.waterError().empty());CHECK(renderer.saveColorPng(out/("look_"+std::to_string(n)+".png"),error));}
    static_cast<render::water::Appearance&>(water)=render::water::preset(0);render(2.25);CHECK(renderer.readColorRgba(first,error));
    render(4.125);CHECK(renderer.readColorRgba(other,error));CHECK(first!=other);
    render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
    water.rain=1;render(2.25);CHECK(renderer.readColorRgba(rain,error));CHECK(rain!=repeat);
    water.enabled=false;render(0);CHECK(renderer.readColorRgba(after,error));CHECK(before==after);
    // Above-water solid geometry must remain unchanged; floor behind the
    // surface must be occluded. Pixel positions come from the actual VP.
    water.enabled=true;water.waveHeight=0;water.detail=0;water.rain=0;
    render(0);CHECK(renderer.readColorRgba(after,error));
    const auto pixel=[&](scene::Vec3 p){const auto& m=vp.v;
        const float x=m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],y=m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],w=m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15];
        return (std::size_t((y/w*.5f+.5f)*720)*1280+std::size_t((x/w*.5f+.5f)*1280))*4;};
    const auto solid=pixel({0,500,140}),submerged=pixel({650,100,-180});
    CHECK(solid+3<before.size()&&submerged+3<before.size());
    for(int c=0;c<3;++c)CHECK(before[solid+c]==after[solid+c]);
    CHECK(before[submerged]!=after[submerged]||before[submerged+1]!=after[submerged+1]||before[submerged+2]!=after[submerged+2]);
    renderer.setDebugView(7);render(0);CHECK(renderer.saveColorPng(out/"water_depth.png",error));
    renderer.setDebugView(4);render(0);CHECK(renderer.saveColorPng(out/"water_normals.png",error));
    renderer.setDebugView(0);
    water.optics.depthEnabled=true;water.optics.transmission=1;
    render(0);CHECK(renderer.readColorRgba(first,error));
    CHECK(renderer.saveColorPng(out/"depth_on.png",error));
    water.optics.depthEnabled=false;render(0);CHECK(renderer.readColorRgba(repeat,error));
    CHECK(first!=repeat);CHECK(renderer.saveColorPng(out/"depth_off.png",error));
    water.optics.depthEnabled=true;
    const auto savedWater=water;
    water.color={0,0,0};water.optics.shallow={0,1,0};water.reflection=0;water.foam=0;
    water.optics.transmission=0;water.optics.crestLight=0;water.optics.shoreFoam=0;
    render(0);CHECK(renderer.readColorRgba(first,error));
    water.optics.depthEnabled=false;render(0);CHECK(renderer.readColorRgba(repeat,error));
    CHECK(first[submerged+1]>repeat[submerged+1]+20); // Actual shallow depth, not merely a uniform toggle.
    water=savedWater;
    // Resolve both supported depth formats and restore texture bindings.
    for(bool msaa:{false,true})for(bool stencil:{false,true}){
        renderer.setAntialiasing(msaa);render::ActorOverlaySettings overlay;overlay.outlines=stencil;
        renderer.setActorOverlays(overlay,0);render(0);CHECK(glGetError()==GL_NO_ERROR);
    }
    renderer.setActorOverlays({},0);renderer.setAntialiasing(true);
    // Transparent geometry above water must still composite after the surface.
    auto glass=box({500,50,30},{700,65,330},{.2f,.65f,.95f,.5f});glass.forceAlpha=true;
    map.meshes.push_back(glass);CHECK(renderer.loadAuxiliaryScenes(&map,nullptr,nullptr,error));render(0);
    std::vector<std::uint8_t> withGlass;CHECK(renderer.readColorRgba(withGlass,error));
    const auto glassPixel=pixel({600,50,200});
    CHECK(withGlass[glassPixel]!=after[glassPixel]||withGlass[glassPixel+1]!=after[glassPixel+1]||withGlass[glassPixel+2]!=after[glassPixel+2]);
    CHECK(renderer.saveColorPng(out/"transparent_above_water.png",error));
    map.meshes.pop_back();CHECK(renderer.loadAuxiliaryScenes(&map,nullptr,nullptr,error));
    renderer.setViewmodelCapture(true,{0,0,0,0});render(0);CHECK(renderer.readColorRgba(first,error));
    water.enabled=false;render(0);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
    renderer.setViewmodelCapture(false,{0,0,0,1});water.enabled=true;
    renderer.setFog(true,{.5f,.55f,.62f},1000,5000,1,0);
    render::HbaoSettings ao;ao.enabled=true;renderer.setHbao(ao);
    render::DepthOfFieldSettings dof;dof.enabled=true;renderer.setDepthOfField(dof);
    render(2.25,1920,1080);CHECK(renderer.saveColorPng(out/"postfx_export.png",error));
    CHECK(glGetError()==GL_NO_ERROR);
    renderer.setHbao({});renderer.setDepthOfField({});
    renderer.setFog(false,{},0,5000,1,0);
    for(int n=0;n<3;++n){water.quality=n;render(.5);}
    // Measure synchronous completed renders, not CPU submission alone.
    for(int n=0;n<4;++n){water.enabled=n!=0;water.quality=1;static_cast<render::water::Appearance&>(water)=render::water::preset(n==2?2:n==3?3:0);
        for(int i=0;i<20;++i){render(i/60.0);glFinish();}
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<120;++i){render(i/60.0);glFinish();}
        std::cout<<"water="<<n<<" fixture frame ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/120<<'\n';}
    if(argc>3){
        scene::glb::Map imported;CHECK(scene::glb::load(argv[3],imported,error,1.f,false));
        CHECK(renderer.loadAuxiliaryScenes(&imported.scene,nullptr,nullptr,error));
        camera={-2723,2136,180};const auto f=scene::normalize(scene::Vec3{1,0,-.12f});
        const auto mapVP=scene::perspective(65*scene::kPi/180,1280.f/720,1,100000)*scene::lookAtDirection(camera,f,{0,0,1});
        renderer.setCameraPosition(camera);renderer.setEnvironmentCamera(f,{0,0,1},65,1280.f/720);
        water.enabled=true;water.height=50;static_cast<render::water::Appearance&>(water)=render::water::preset(0);
        renderer.setWater(water,3.25);
        renderer.render(empty,{},mapVP,1280,720,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&imported.scene);
        CHECK(renderer.saveColorPng(out/"default_map_water.png",error));CHECK(glGetError()==GL_NO_ERROR);
    }
    // Elevated ocean views expose repeating wave rows and shoreline/depth errors
    // that the low-angle regression fixture can hide.
    scene::CastScene ocean;
    ocean.meshes.push_back(box({-60000,-60000,-3500},{60000,60000,-3400},{.3f,.26f,.18f,1}));
    auto bank=box({-6000,-4000,-3000},{-2500,7000,-20},{.52f,.46f,.3f,1});
    for(auto& vertex:bank.vertices)if(vertex.position.z>-100)
        vertex.position.z=-20-(vertex.position.x+6000)/3500*900;
    ocean.meshes.push_back(bank);
    CHECK(renderer.loadAuxiliaryScenes(&ocean,nullptr,nullptr,error));
    camera={-2300,-3400,1800};const auto seaForward=scene::normalize(scene::Vec3{.1f,1,-.85f});
    const auto seaVP=scene::perspective(65*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(camera,seaForward,{0,0,1});
    renderer.setCameraPosition(camera);renderer.setEnvironmentCamera(seaForward,{0,0,1},65,16.f/9);
    renderer.setSun(true,true,{.3f,.45f,-.8f},1.2f,.8f,{1,.94f,.86f},{.75f,.88f,1},1024,7000,6000,camera,false,512,6000,9000,100);
    renderer.setEnvironmentParameters(1,0,0,1,1,.75f,1);
    water.height=0;water.quality=2;water.enabled=true;
    for(int look:{3,4}){
        static_cast<render::water::Appearance&>(water)=render::water::preset(look);
        for(int frame=0;frame<5;++frame){
            renderer.setWater(water,3.25+frame*.25);
            renderer.render(empty,{},seaVP,1920,1080,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&ocean);
            CHECK(renderer.saveColorPng(out/("sea_"+std::to_string(look)+"_"+std::to_string(frame)+".png"),error));
        }
    }
    // Identical settings and topology: compare soft/max crests and all quality
    // tiers, then a close low-angle continuous sequence for motion inspection.
    ocean.meshes.resize(1);CHECK(renderer.loadAuxiliaryScenes(&ocean,nullptr,nullptr,error));
    camera={-2300,-3400,550};
    const auto closeForward=scene::normalize(scene::Vec3{.1f,1,-.28f});
    const auto closeVP=scene::perspective(65*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(camera,closeForward,{0,0,1});
    renderer.setCameraPosition(camera);renderer.setEnvironmentCamera(closeForward,{0,0,1},65,16.f/9);
    static_cast<render::water::Appearance&>(water)=render::water::preset(3);
    water.radius=120;
    const auto closeRender=[&](double t,int width=1280,int height=720){
        renderer.setWater(water,t);
        renderer.render(empty,{},closeVP,width,height,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&ocean);
    };
    for(int quality=0;quality<3;++quality){
        water.quality=quality;
        for(int tight=0;tight<2;++tight){
            water.surface.tightness=float(tight);closeRender(3.25);
            CHECK(renderer.saveColorPng(out/("crest_q"+std::to_string(quality)+"_t"+std::to_string(tight)+".png"),error));
        }
    }
    water.quality=1;water.surface.tightness=.82f;
    for(int frame=0;frame<90;++frame){
        closeRender(3.25+frame/30.0);
        CHECK(renderer.waterError().empty()&&glGetError()==GL_NO_ERROR);
        CHECK(renderer.saveColorPng(out/("motion_"+std::to_string(frame)+".png"),error));
    }
    closeRender(3.25);CHECK(renderer.readColorRgba(first,error));
    closeRender(8);closeRender(3.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
    // Projected-grid coverage under pitch, roll, FOV and aspect changes.
    // Every tested ray that hits well inside the finite water extent must see
    // water, not a hole exposing the beige seabed.
    water.waveHeight=0;water.detail=0;water.rain=0;water.foam=0;water.reflection=0;
    water.optics.depthEnabled=false;water.color={0,.1f,1};
    int covered=0;
    for(float pitch:{-.1f,-.7f,-1.55f})for(float roll:{0.f,.78f,1.57f})
    for(float fov:{40.f,100.f})for(float aspect:{16.f/9,32.f/9}){
        const scene::Vec3 cf{0,std::cos(pitch),std::sin(pitch)};
        const scene::Vec3 cr{1,0,0},cu=scene::cross(cr,cf);
        const auto up=cu*std::cos(roll)+cr*std::sin(roll),right=scene::cross(cf,up);
        const float tangent=std::tan(fov*scene::kPi/360);
        camera={0,0,550};
        const auto sweepVP=scene::perspective(fov*scene::kPi/180,aspect,1,100000)*scene::lookAtDirection(camera,cf,up);
        renderer.setCameraPosition(camera);renderer.setEnvironmentCamera(cf,up,fov,aspect);
        renderer.setWater(water,3.25);
        renderer.render(empty,{},sweepVP,960,int(960/aspect),false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&ocean);
        CHECK(renderer.readColorRgba(first,error));CHECK(glGetError()==GL_NO_ERROR);
        const int height=int(960/aspect);
        for(float sy:{-.8f,-.4f,0.f,.4f,.8f})for(float sx:{-.8f,-.4f,0.f,.4f,.8f}){
            const auto ray=cf+right*(sx*tangent*aspect)+up*(sy*tangent);
            if(ray.z>=-.01f)continue;
            const auto hit=camera+ray*(-camera.z/ray.z);
            if(std::sqrt(hit.x*hit.x+hit.y*hit.y)>water.radius*75)continue;
            const auto pixel=(std::size_t((sy*.5f+.5f)*height)*960+std::size_t((sx*.5f+.5f)*960))*4;
            CHECK(first[pixel+2]>first[pixel]+20);++covered;
        }
    }
    CHECK(covered>400);std::cout<<"projected coverage rays="<<covered<<'\n';
    CHECK(glGetError()==GL_NO_ERROR);
    renderer.shutdown();
    }
    glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"PASS shader, time seeking, disabled restoration, rain, occlusion, transparent surfaces, isolated viewmodel capture, quality changes, postFX/export and GL state\n";
}
