#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <filesystem>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" "<<error<<'\n';return 1;}}while(false)
static scene::Mesh box(scene::Vec3 lo,scene::Vec3 hi,scene::Vec4 color){
    scene::Mesh m;m.name="rain_fixture";m.color=color;m.doubleSided=true;
    const scene::Vec3 p[]={{lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},{lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}};
    const int faces[][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};
    for(const auto& f:faces){const auto n=scene::normalize(scene::cross(p[f[1]]-p[f[0]],p[f[2]]-p[f[0]]));const auto base=unsigned(m.vertices.size());
        for(int k:f){scene::Vertex v;v.position=p[k];v.normal=n;m.vertices.push_back(v);}m.indices.insert(m.indices.end(),{base,base+1,base+2,base,base+2,base+3});}
    return m;
}
static void collision(scene::glb::Map& m){
    for(const auto& mesh:m.scene.meshes)for(std::size_t i=0;i<mesh.indices.size();i+=3){
        scene::glb::CollisionTriangle t;t.a=mesh.vertices[mesh.indices[i]].position;t.b=mesh.vertices[mesh.indices[i+1]].position;t.c=mesh.vertices[mesh.indices[i+2]].position;
        t.normal=scene::normalize(scene::cross(t.b-t.a,t.c-t.a));
        t.minimum={std::min({t.a.x,t.b.x,t.c.x}),std::min({t.a.y,t.b.y,t.c.y}),std::min({t.a.z,t.b.z,t.c.z})};
        t.maximum={std::max({t.a.x,t.b.x,t.c.x}),std::max({t.a.y,t.b.y,t.c.y}),std::max({t.a.z,t.b.z,t.c.z})};
        t.blocking=true;t.walkable=t.normal.z>.6f;m.collision.push_back(t);
    }m.buildCollisionIndex();
}
int main(int argc,char** argv){
    std::string error;CHECK(argc>1);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(1280,720,"Rain validation",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
    {
        render::StageRenderer renderer;CHECK(renderer.initialize(error));scene::CastScene empty;scene::glb::Map map;
        map.scene.meshes.push_back(box({-4000,-4000,-30},{4000,5000,0},{.16f,.18f,.21f,1}));
        map.scene.meshes.push_back(box({-900,1400,0},{900,1500,1200},{.11f,.14f,.19f,1}));
        map.scene.meshes.push_back(box({-900,0,420},{-40,800,450},{.24f,.28f,.3f,1}));
        map.scene.meshes.push_back(box({-900,0,0},{-870,800,450},{.3f,.31f,.33f,1}));
        map.scene.meshes.push_back(box({-150,700,0},{-110,750,420},{.3f,.31f,.33f,1}));
        map.scene.meshes.push_back(box({300,400,0},{460,580,130},{.25f,.18f,.12f,1}));
        collision(map);CHECK(renderer.loadScene(empty,error));CHECK(renderer.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
        scene::Vec3 camera{0,-600,170};scene::Vec3 forward=scene::normalize(scene::Vec3{0,1,-.09f});
        auto vp=scene::perspective(65*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
        renderer.setCameraPosition(camera);renderer.setCameraDepthRange(1,100000);renderer.setEnvironmentCamera(forward,{0,0,1},65,16.f/9);
        renderer.setSun(true,true,{.3f,.45f,-.8f},1.2f,.6f,{.9f,.95f,1},{.7f,.8f,1},1024,7000,6000,camera,false,512,6000,9000,100);
        auto rain=render::rain::preset(1);
        const auto render=[&](double t,int w=1280,int h=720){renderer.setRain(rain,t,&map);renderer.render(empty,{},vp,w,h,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map.scene);};
        std::vector<std::uint8_t> baseline,first,repeat,later;std::vector<float> depthBefore,depthAfter;
        rain.enabled=false;render(2.25);CHECK(renderer.readColorRgba(baseline,error));CHECK(renderer.readDepthRawFloat(depthBefore,1,100000,1,100000,false,error));
        // Surface effects remain independent from falling rain and never change depth.
        render::WetSettings wet;wet.ground=true;renderer.setWetSurfaces(wet);render(2.25);
        CHECK(renderer.readColorRgba(first,error));CHECK(first!=baseline);CHECK(renderer.saveColorPng(out/"wet_ground.png",error));
        CHECK(renderer.readDepthRawFloat(depthAfter,1,100000,1,100000,false,error));CHECK(depthBefore==depthAfter);
        // Cross a shelter tile boundary without changing the displayed view.
        // Wetness must not jump just because the cached atlas was recentered.
        const float crossingX=-render::rain::slope(rain).x*camera.z;
        renderer.setCameraPosition({crossingX-.01f,camera.y,camera.z});render(2.25);CHECK(renderer.readColorRgba(first,error));
        renderer.setCameraPosition({crossingX+.01f,camera.y,camera.z});render(2.25);CHECK(renderer.readColorRgba(repeat,error));
        double crossingError=0;for(size_t p=0;p<first.size();++p)crossingError+=std::abs(int(first[p])-int(repeat[p]));crossingError/=first.size();
        std::cout<<"Wet shelter crossing mean byte delta="<<crossingError<<'\n';CHECK(crossingError<.1);
        renderer.setCameraPosition(camera);
        wet.enabled=true;wet.map=false;renderer.setWetSurfaces(wet);render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(repeat==baseline);
        wet.ground=false;
        wet.map=true;renderer.setWetSurfaces(wet);render(2.25);CHECK(renderer.readColorRgba(first,error));CHECK(first!=baseline);
        CHECK(renderer.saveColorPng(out/"surface_rain.png",error));render(5.1);CHECK(renderer.readColorRgba(later,error));CHECK(later!=first);
        render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
        renderer.setWetSurfaces({});
        wet.darkening=0;wet.amount=1;wet.droplets=0;wet.rivulets=0;wet.ripples=0;renderer.setWetSurfaces(wet);render(2.25);
        CHECK(renderer.readColorRgba(first,error));for(std::size_t p=0;p<first.size();++p)CHECK(std::abs(int(first[p])-int(baseline[p]))<=2);renderer.setWetSurfaces({});
        render::MuzzleLightSettings light;light.enabled=true;light.radius=600;light.intensity=3;
        for(int shape=0;shape<3;++shape){light.shape=shape;renderer.setMuzzleLight(light);render(2.25);
            renderer.renderMuzzleFlash3D({0,-300,100},20,0,{1,.25f,.05f,1},vp,camera,false);
            CHECK(renderer.readColorRgba(first,error));CHECK(renderer.saveColorPng(out/("muzzle_light_"+std::to_string(shape)+".png"),error));
            renderer.setMuzzleLight({});render(2.25);renderer.renderMuzzleFlash3D({0,-300,100},20,0,{1,.25f,.05f,1},vp,camera,false);
            CHECK(renderer.readColorRgba(repeat,error));CHECK(first!=repeat);
            CHECK(renderer.readDepthRawFloat(depthAfter,1,100000,1,100000,false,error));CHECK(depthBefore==depthAfter);
            for(std::size_t k=3;k<first.size();k+=4)CHECK(first[k]==repeat[k]);CHECK(glGetError()==GL_NO_ERROR);
        }
        for(int n=0;n<5;++n){rain=render::rain::preset(n);render(2.25);CHECK(renderer.rainError().empty());CHECK(renderer.saveColorPng(out/("preset_"+std::to_string(n)+".png"),error));}
        rain=render::rain::preset(1);render(2.25);CHECK(renderer.readColorRgba(first,error));CHECK(first!=baseline);
        rain.style.refractive=true;rain.style.refractionMix=1;rain.style.refractionStrength=20;
        render(2.25);CHECK(renderer.readColorRgba(later,error));CHECK(first!=later);CHECK(glGetError()==GL_NO_ERROR);
        CHECK(renderer.saveColorPng(out/"refractive.png",error));render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(later==repeat);
        rain.style={};render(2.25);CHECK(renderer.readColorRgba(first,error));
        CHECK(renderer.readDepthRawFloat(depthAfter,1,100000,1,100000,false,error));CHECK(depthBefore==depthAfter);
        render(5.1);CHECK(renderer.readColorRgba(later,error));CHECK(first!=later);render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
        rain.enabled=false;render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(baseline==repeat);rain.enabled=true;
        for(bool aa:{false,true})for(bool stencil:{false,true}){renderer.setAntialiasing(aa);render::ActorOverlaySettings o;o.outlines=stencil;renderer.setActorOverlays(o,0);render(2.25);CHECK(glGetError()==GL_NO_ERROR);}
        renderer.setActorOverlays({},0);renderer.setAntialiasing(true);
        renderer.setViewmodelCapture(true,{0,0,0,0});render(2.25);CHECK(renderer.readColorRgba(first,error));rain.enabled=false;render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
        renderer.setViewmodelCapture(false,{0,0,0,1});rain.enabled=true;
        renderer.setDebugView(7);render(2.25);CHECK(renderer.readColorRgba(first,error));rain.enabled=false;render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);renderer.setDebugView(0);rain.enabled=true;
        // Entire camera is under an enormous roof, unseen above the frame.
        auto sheltered=map;sheltered.scene.meshes.push_back(box({-10000,-10000,500},{10000,10000,550},{.3f,.3f,.3f,1}));sheltered.collision.clear();collision(sheltered);
        CHECK(renderer.loadAuxiliaryScenes(&sheltered.scene,nullptr,nullptr,error));
        renderer.setRain(rain,2.25,&sheltered);renderer.render(empty,{},vp,1280,720,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&sheltered.scene);CHECK(renderer.readColorRgba(first,error));
        rain.enabled=false;renderer.setRain(rain,2.25,&sheltered);renderer.render(empty,{},vp,1280,720,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&sheltered.scene);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);rain.enabled=true;
        CHECK(renderer.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
        renderer.setFog(true,{.2f,.24f,.3f},800,4000,.6f,0);render::DepthOfFieldSettings dof;dof.enabled=true;renderer.setDepthOfField(dof);
        render::HbaoSettings ao;ao.enabled=true;renderer.setHbao(ao);render(2.25,1920,1080);CHECK(renderer.saveColorPng(out/"postfx.png",error));CHECK(glGetError()==GL_NO_ERROR);
        renderer.setDepthOfField({});renderer.setHbao({});renderer.setFog(false,{},0,5000,1,0);
        scene::CastScene hand;hand.meshes.push_back(box({10,-560,130},{45,-510,170},{1,.1f,.01f,1}));
        CHECK(renderer.loadScene(hand,error));CHECK(renderer.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));renderer.setFirstPersonProjection(true);renderer.setAntialiasing(false);
        const auto fp=[&](bool enabled){rain.enabled=enabled;renderer.setRain(rain,2.25,&map);renderer.render(hand,{},vp,1280,720,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,true,&map.scene);};
        fp(false);CHECK(renderer.readColorRgba(first,error));CHECK(renderer.saveColorPng(out/"foreground_dry.png",error));fp(true);CHECK(renderer.readColorRgba(repeat,error));CHECK(renderer.saveColorPng(out/"foreground_rain.png",error));CHECK(first!=repeat);
        int protectedPixels=0;for(std::size_t k=0;k<first.size();k+=4)if(first[k]>100&&first[k+1]<60&&first[k+2]<60){CHECK(first[k]==repeat[k]&&first[k+1]==repeat[k+1]&&first[k+2]==repeat[k+2]);++protectedPixels;}
        CHECK(protectedPixels>100);std::cout<<"protected foreground pixels="<<protectedPixels<<'\n';
        renderer.setFirstPersonProjection(false);renderer.setAntialiasing(true);CHECK(renderer.loadScene(empty,error));CHECK(renderer.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
        for(int mode=0;mode<4;++mode){rain.enabled=mode>0;rain.quality=std::max(0,mode-1);
            for(int j=0;j<10;++j){render(j/60.);glFinish();}
            auto start=std::chrono::steady_clock::now();for(int j=0;j<60;++j){render(j/60.);glFinish();}
            std::cout<<"rain="<<mode<<" completed fixture frame ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/60<<'\n';
        }
        rain=render::rain::preset(2);
        for(int j=0;j<60;++j){render(2.25+j/30.);CHECK(renderer.saveColorPng(out/("motion_"+std::to_string(j)+".png"),error));}
        // Wind direction, turbulent streak tangents, and collision-derived wall
        // mist are independently observable, rather than cosmetic UI controls.
        camera={200,850,190};forward=scene::normalize(scene::Vec3{0,1,-.08f});
        vp=scene::perspective(65*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
        renderer.setCameraPosition(camera);renderer.setEnvironmentCamera(forward,{0,0,1},65,16.f/9);
        rain=render::rain::preset(4);render(2.25);CHECK(renderer.readColorRgba(first,error));CHECK(renderer.saveColorPng(out/"wall_wind_mist.png",error));
        rain.wallMist=false;render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first!=repeat);CHECK(renderer.saveColorPng(out/"wall_no_mist.png",error));
        rain.wallMist=true;rain.turbulence=0;rain.gusts=0;render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first!=repeat);
        rain=render::rain::preset(4);rain.direction=-90;render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first!=repeat);
        rain=render::rain::preset(4);
        for(int j=0;j<60;++j){render(2.25+j/30.);CHECK(renderer.saveColorPng(out/("wind_"+std::to_string(j)+".png"),error));}
        // Previously viewed frames remain identical after moving away/back.
        render(2.25);CHECK(renderer.readColorRgba(first,error));renderer.setCameraPosition(camera+scene::Vec3{1000,1000,0});render(9);renderer.setCameraPosition(camera);render(2.25);CHECK(renderer.readColorRgba(repeat,error));CHECK(first==repeat);
        if(argc>2){
            CHECK(scene::glb::load(argv[2],map,error,1.f,false));CHECK(renderer.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
            camera={-2723,2136,180};forward=scene::normalize(scene::Vec3{1,0,-.12f});vp=scene::perspective(65*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
            renderer.setCameraPosition(camera);renderer.setEnvironmentCamera(forward,{0,0,1},65,16.f/9);
            rain=render::rain::preset(1);
            const auto start=std::chrono::steady_clock::now();render(2.25);glFinish();std::cout<<"map first rain frame ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
            CHECK(renderer.saveColorPng(out/"map_rain.png",error));
            for(bool enabled:{false,true}){rain.enabled=enabled;for(int j=0;j<10;++j){render(j/60.);glFinish();}auto t=std::chrono::steady_clock::now();for(int j=0;j<60;++j){render(j/60.);glFinish();}std::cout<<"map rain="<<enabled<<" completed frame ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()/60<<'\n';}
            // Alternate order and use medians to reduce GPU clock/warmup noise.
            std::vector<double> off,on,moving;
            for(int round=0;round<6;++round)for(int mode=0;mode<2;++mode){rain.enabled=((mode+round)%2)!=0;for(int j=0;j<100;++j){render(j/60.);glFinish();}std::vector<double> frames;for(int j=0;j<100;++j){auto t=std::chrono::steady_clock::now();render(j/60.);glFinish();frames.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());}std::sort(frames.begin(),frames.end());(rain.enabled?on:off).push_back(frames[50]);}
            std::sort(off.begin(),off.end());std::sort(on.begin(),on.end());std::cout<<"map median completed frame off="<<off[3]<<" on="<<on[3]<<" delta="<<on[3]-off[3]<<" ms\n";
            rain.enabled=true;for(int j=0;j<120;++j){const auto movingCamera=camera+scene::Vec3{float(j)*25,0,0};renderer.setCameraPosition(movingCamera);
                vp=scene::perspective(65*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(movingCamera,forward,{0,0,1});
                auto t=std::chrono::steady_clock::now();render(j/60.);glFinish();moving.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());}
            std::sort(moving.begin(),moving.end());std::cout<<"map moving rain median="<<moving[60]<<" p95="<<moving[114]<<" max="<<moving.back()<<" ms\n";
            std::vector<double> rebuilds;for(int j=0;j<60;++j){rain.direction=25+float(j)*3;auto t=std::chrono::steady_clock::now();render(j/60.);glFinish();rebuilds.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());}
            std::sort(rebuilds.begin(),rebuilds.end());std::cout<<"map forced shelter rebuild median="<<rebuilds[30]<<" p95="<<rebuilds[57]<<" max="<<rebuilds.back()<<" ms\n";
            std::vector<double> teleports;
            for(int j=0;j<120;++j){const auto pos=camera+scene::Vec3{float((j*31)%97)*83-4000,float((j*17)%89)*71-3000,float(j%4)*240};
                renderer.setCameraPosition(pos);vp=scene::perspective(65*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(pos,forward,{0,0,1});
                auto t=std::chrono::steady_clock::now();render(j/30.);glFinish();teleports.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());}
            std::sort(teleports.begin(),teleports.end());std::cout<<"map teleport/elevation worst-frame="<<teleports.back()<<" p95="<<teleports[114]<<" ms\n";
        }
        if(argc>3){
            auto model=scene::buildScene(cast::Document::load(argv[3]));std::vector<scene::Mat4> pose;for(const auto& b:model.skeleton.bones)pose.push_back(b.restGlobal);
            camera={140,0,145};forward=scene::normalize(scene::Vec3{-1,0,-.06f});
            vp=scene::perspective(40*scene::kPi/180,16.f/9,.1f,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
            renderer.setCameraPosition(camera);renderer.setCameraDepthRange(.1f,100000);renderer.setEnvironmentCamera(forward,{0,0,1},40,16.f/9);
            CHECK(renderer.setT6SkyboxIwi("skyboxes/clouds/sky.iwi",error));
            renderer.setSun(true,false,{-.35f,.4f,-.3f},2,.5f,{1,1,1},{.7f,.8f,1},512,7000,6000,camera,false,512,6000,9000,100);
            rain={};rain.shelter=false;renderer.setRain(rain,2.25);CHECK(renderer.loadScene(model,error));CHECK(renderer.loadAuxiliaryScenes(nullptr,nullptr,nullptr,error));
            wet={};wet.enabled=true;wet.map=false;wet.actors=false;wet.viewmodels=false;
            const auto modelFrame=[&](double t,bool world){renderer.setRain(rain,t);renderer.setWetSurfaces(wet);
                if(world)renderer.render(empty,{},vp,1280,720,false,false,false,nullptr,nullptr,nullptr,&model,&pose,false);
                else renderer.render(model,pose,vp,1280,720,false,false,false);};
            modelFrame(2.25,false);CHECK(renderer.readColorRgba(first,error));CHECK(renderer.saveColorPng(out/"character_dry.png",error));
            wet.viewmodels=true;modelFrame(2.25,false);CHECK(renderer.readColorRgba(repeat,error));CHECK(first!=repeat);CHECK(renderer.saveColorPng(out/"character_wet.png",error));
            modelFrame(4.1,false);CHECK(renderer.readColorRgba(later,error));CHECK(later!=repeat);modelFrame(2.25,false);CHECK(renderer.readColorRgba(later,error));CHECK(later==repeat);
            CHECK(renderer.loadScene(empty,error));CHECK(renderer.loadAuxiliaryScenes(nullptr,&model,nullptr,error));
            modelFrame(2.25,true);CHECK(renderer.readColorRgba(first,error));wet.actors=true;modelFrame(2.25,true);CHECK(renderer.readColorRgba(repeat,error));CHECK(first!=repeat);
            CHECK(renderer.saveColorPng(out/"world_character_wet.png",error));
            for(int j=0;j<24;++j){modelFrame(2.25+j/24.,true);CHECK(renderer.saveColorPng(out/("character_motion_"+std::to_string(j)+".png"),error));}
        }
        CHECK(glGetError()==GL_NO_ERROR);renderer.shutdown();
    }
    {
        render::StageRenderer r;CHECK(r.initialize(error));scene::CastScene empty;scene::glb::Map map;
        const scene::Vec3 camera{0,0,150},forward=scene::normalize(scene::Vec3{0,1,-1});
        const auto vp=scene::perspective(60*scene::kPi/180,1,1,20000)*scene::lookAtDirection(camera,forward,{0,0,1});
        r.setCameraPosition(camera);r.setCameraDepthRange(1,20000);r.setEnvironmentCamera(forward,{0,0,1},60,1);
        r.setSun(false,false,{0,0,-1},1,1,{1,1,1},{1,1,1},512,5000,4000,camera,false,512,4000,5000,100);
        render::WetSettings wet;wet.ground=true;wet.amount=1;wet.darkening=.8f;r.setWetSurfaces(wet);render::rain::Settings rain;rain.wind=0;
        const auto frame=[&](float roof,const char* name,float alpha,float limit){map.scene.meshes.clear();map.scene.meshes.push_back(box({-3000,-3000,-20},{3000,3000,0},{.6f,.6f,.6f,1}));
            if(roof>0){auto m=box({-2500,-2500,roof},{2500,2500,roof+10},{.4f,.4f,.4f,alpha});m.name=name;m.weatherNonBlocking=std::string(name)=="metadata_background";map.scene.meshes.push_back(m);}
            r.loadScene(empty,error);r.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error);rain.blockerDistance=limit;r.setRain(rain,1,&map);r.render(empty,{},vp,512,512,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map.scene);
            std::vector<std::uint8_t> pixels;r.readColorRgba(pixels,error);return pixels;};
        const auto clear=frame(0,"",1,50),distant=frame(10000,"distant_backdrop",1,50),unlimited=frame(10000,"distant_backdrop",1,0);
        CHECK(clear==distant);CHECK(clear!=unlimited);
        CHECK(clear==frame(400,"shipment_skybox",1,50));CHECK(clear==frame(400,"invisible_plane",1,50));CHECK(clear==frame(400,"zero_alpha",0,50));
        CHECK(clear==frame(400,"metadata_background",1,50));
        CHECK(clear!=frame(400,"ordinary_roof",1,50));
        std::cout<<"PASS rain distant/sky/invisible exclusions, adjustable range and nearby roof shelter\n";
        // Shelter refreshes must not change directional sun visibility.
        frame(12000,"high_roof",1,50);
        map.scene.bounds={{-3000,-3000,-20},{3000,3000,12010},true};
        r.setWetSurfaces({});rain.enabled=true;rain.opacity=0;rain.splashes=false;rain.wallMist=false;
        const auto sunFrame=[&](bool shadows,float cutoff,bool shelter){
            r.setSun(true,shadows,{0,0,-1},1,.1f,{1,1,1},{1,1,1},1024,5000,4000,camera,false,512,4000,5000,100);
            rain.blockerDistance=cutoff;rain.shelter=shelter;r.setRain(rain,1,&map);
            r.render(empty,{},vp,512,512,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map.scene);
            std::vector<std::uint8_t> pixels;r.readColorRgba(pixels,error);return pixels;};
        const auto shadowed=sunFrame(true,50,false),sunlit=sunFrame(false,50,false);CHECK(shadowed!=sunlit);
        for(float distance:{0.f,1.f,50.f,200.f}){CHECK(shadowed==sunFrame(true,distance,true));CHECK(shadowed==sunFrame(true,distance,true));}
        CHECK(glGetError()==GL_NO_ERROR);std::cout<<"PASS rain cutoff and cache refresh preserve sun shadows\n";r.shutdown();
    }
    {
        render::StageRenderer r;CHECK(r.initialize(error));scene::CastScene empty,map,actor;
        const scene::Vec3 camera{0,-600,100};
        const auto vp=scene::perspective(60*scene::kPi/180,1,1,10000)*scene::lookAtDirection(camera,{0,1,0},{0,0,1});
        r.setCameraPosition(camera);r.setCameraDepthRange(1,10000);
        r.setSun(false,false,{0,0,-1},1,1,{1,1,1},{1,1,1},512,5000,4000,camera,false,512,4000,5000,100);
        auto mesh=box({-200,0,-100},{200,100,300},{0,0,0,1});mesh.materialPolicyExplicit=true;map.meshes.push_back(mesh);
        mesh.materialPolicyExplicit=false;mesh.skinned=true;actor.meshes.push_back(mesh);actor.skeleton.bones.emplace_back();
        std::vector<std::vector<scene::Mat4>> poses{{scene::Mat4::identity()}};
        CHECK(r.loadScene(empty,error));
        for(float half:{1.f,600.f})for(bool height:{false,true}){
            r.setFog(true,{.21f,.38f,.47f},0,half,1,0,height,50,100);
            CHECK(r.loadAuxiliaryScenes(&map,nullptr,nullptr,error));
            r.render(empty,{},vp,512,512,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map);
            std::vector<std::uint8_t> mapPixels,actorPixels;CHECK(r.readColorRgba(mapPixels,error));
            CHECK(r.saveColorPng(out/"fog_map_probe.png",error));
            CHECK(r.loadAuxiliaryScenes(nullptr,nullptr,&actor,error));
            r.render(empty,{},vp,512,512,false,false,false,&actor,&poses,nullptr,nullptr,nullptr,false,nullptr);
            CHECK(r.readColorRgba(actorPixels,error));
            CHECK(r.saveColorPng(out/"fog_actor_probe.png",error));
            std::size_t different=0;int maximum=0;for(std::size_t p=0;p<mapPixels.size();++p){const int d=std::abs(int(mapPixels[p])-int(actorPixels[p]));different+=d!=0;maximum=std::max(maximum,d);}std::cout<<"fog half="<<half<<" height="<<height<<" changed channels="<<different<<" max="<<maximum<<'\n';
            CHECK(mapPixels==actorPixels);CHECK(r.saveColorPng(out/("fog_actor_"+std::to_string(int(half))+"_"+std::to_string(height)+".png"),error));
        }
        CHECK(glGetError()==GL_NO_ERROR);r.shutdown();std::cout<<"PASS explicit map/legacy actor full, partial and height fog parity\n";
    }
    if(argc>2){
        scene::glb::Map map;CHECK(scene::glb::load(argv[2],map,error));render::StageRenderer r;CHECK(r.initialize(error));scene::CastScene empty;
        CHECK(r.loadScene(empty,error));CHECK(r.loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error));
        const scene::Vec3 camera{1107.8798f,4603.7412f,-4506.5205f};
        const auto forward=scene::normalize(scene::Vec3{0,1,.12f});
        const auto vp=scene::perspective(65*scene::kPi/180,16.f/9,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
        r.setCameraPosition(camera);r.setCameraDepthRange(1,100000);r.setEnvironmentCamera(forward,{0,0,1},65,16.f/9);
        const float az=-97.2f*scene::kPi/180,el=33.3f*scene::kPi/180;
        const scene::Vec3 direction{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),-std::sin(el)};
        std::vector<std::uint8_t> shaded,lit;
        for(bool shadows:{false,true}){
            r.setSun(true,shadows,direction,1.86f,.44f,{1,.938304f,.883372f},{1,.900512f,.818593f},2048,4200,3000,camera,false,1024,3600,11000,800);
            r.render(empty,{},vp,1280,720,false,false,false,nullptr,nullptr,nullptr,nullptr,nullptr,false,&map.scene);
            CHECK(r.readColorRgba(shadows?shaded:lit,error));CHECK(r.saveColorPng(out/(shadows?"default_map_shadow_fixed.png":"default_map_unshadowed.png"),error));
        }
        CHECK(shaded!=lit);CHECK(glGetError()==GL_NO_ERROR);r.shutdown();
    }
    glfwDestroyWindow(window);glfwTerminate();std::cout<<"PASS rain shader, deterministic seek, depth preservation, shelter, disabled restoration, debug/viewmodel exclusions, MSAA/stencil and postFX\n";
}
