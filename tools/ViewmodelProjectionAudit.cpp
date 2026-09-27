#include "scene/CastScene.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
int main(int argc,char** argv){
    if(argc!=5)return 2;
    try{
        const auto hands=cast::Document::load(argv[1]),gun=cast::Document::load(argv[2]),clip=cast::Document::load(argv[3]);
        auto scene=scene::buildScene(hands,false);
        if(!hands.valid()||!gun.valid()||!clip.valid()||!scene::appendRigModel(gun,scene,"weapon")||!scene::appendAnimations(clip,scene))return 2;
        if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto* window=glfwCreateWindow(960,540,"Projection audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
        render::StageRenderer renderer;std::string error;
        if(!renderer.initialize(error)||!renderer.loadScene(scene,error)){std::cerr<<error;return 2;}
        renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
        renderer.setEnvironmentCamera({1,0,0},{0,0,1},75,960.f/540.f);
        const auto pose=scene.samplePose(0,0);
        const auto vp=scene::perspective(75*scene::kPi/180,960.f/540.f,.1f,5000)*scene::lookAtDirection({},{1,0,0},{0,0,1});
        std::filesystem::create_directories(argv[4]);
        for(int i=0;i<10;++i){
            const int mode=i%5;
            renderer.setFirstPersonProjection(mode<3);
            renderer.setViewmodelProjection(mode==2?1.4f:mode==4?1.7f:1.f,mode==1||mode==4);
            renderer.render(scene,pose,vp,960,540,false,false,false);
            if(const auto muzzle=scene::resolveMuzzlePosition(scene,pose)){
                renderer.clearBulletTrails();render::StageRenderer::BulletTrail trail;trail.type=render::StageRenderer::BulletTrail::Type::Sniper;
                trail.start=*muzzle;trail.end={1000,0,0};trail.lifetime=1;trail.width=.25f;trail.color={1,.1f,.1f};trail.viewmodelOrigin=true;
                if(i>=5){trail.type=render::StageRenderer::BulletTrail::Type::Projectile;trail.distance=scene::length(trail.end-trail.start);trail.dir=scene::normalize(trail.end-trail.start);trail.currentDist=20;trail.speed=0;}
                renderer.addBulletTrail(trail);renderer.updateAndRenderBulletTrails(0,vp,{});
                renderer.renderMuzzleFlash3D(*muzzle,.4f,0,{.1f,1,.1f,1},vp,{},mode<3,{1,0,0});
            }
            if(!renderer.saveColorPng(std::filesystem::path(argv[4])/(std::to_string(i)+".png"),error)){std::cerr<<error;return 2;}
        }
        renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();
        std::cout<<"Saved default, flipped, 1.4x FOV, non-first-person default and non-first-person edited renders\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what();return 1;}
}
