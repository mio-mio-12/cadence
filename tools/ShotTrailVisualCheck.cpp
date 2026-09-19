#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

// Production-renderer regression: no simulation movement is needed to see a shot.
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    const auto output=std::filesystem::u8path(argv[2]);
    std::filesystem::create_directories(output);
    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(64,64,"Shot regression",nullptr,nullptr);
    if(!window)return 1;
    glfwMakeContextCurrent(window);
    int status=0;
    {
        render::StageRenderer renderer;std::string error;
        if(!renderer.initialize(error)){std::cerr<<error;return 1;}
        renderer.setViewmodelCapture(true,{0,0,0,1});
        scene::CastScene empty;
        for(const float lateral : {0.0f,10.0f}) {
            const scene::Vec3 camera{0,lateral,0};
            const auto vp=scene::perspective(scene::kPi/3,1,.1f,2000)*scene::lookAt(camera,{100,lateral,0},{0,0,1});
            renderer.clearBulletTrails();renderer.setCameraPosition(camera);
            renderer.render(empty,{},vp,512,512,false,false,false);
            std::vector<std::uint8_t> before,after;
            if(!renderer.readColorRgba(before,error))return 1;
            render::StageRenderer::BulletTrail trail;
            trail.type=render::StageRenderer::BulletTrail::Type::Sniper;
            trail.start={25,0,0};trail.end={1000,0,0};trail.width=1.8f;
            trail.lifetime=.75f;trail.color={1,.8f,.4f};trail.emissiveEnabled=true;
            renderer.addBulletTrail(trail);
            renderer.updateAndRenderBulletTrails(1.0f/60,vp,camera);
            if(!renderer.readColorRgba(after,error))return 1;
            std::size_t changed=0;
            for(std::size_t i=0;i<after.size();i+=4)if(after[i]>before[i]+4||after[i+1]>before[i+1]+4)++changed;
            std::cout<<(lateral==0?"stationary":"offset")<<" trail pixels="<<changed<<'\n';
            if(changed<4)status=1;
            if(!renderer.saveColorPng(output/(lateral==0?"stationary.png":"offset.png"),error))status=1;
        }
        auto value=scene::buildScene(cast::Document::load(std::filesystem::u8path(argv[1])));
        std::vector<scene::Mat4> pose;for(const auto& bone:value.skeleton.bones)pose.push_back(bone.restGlobal);
        const auto nativeMuzzle=scene::resolveMuzzlePosition(value,pose);
        if(!nativeMuzzle){std::cerr<<"Real AWP muzzle unresolved\n";return 1;}
        std::cout<<"real AWP native muzzle="<<nativeMuzzle->x<<','<<nativeMuzzle->y<<','<<nativeMuzzle->z<<'\n';
        scene::CastScene worldWeapon;worldWeapon.skeleton.bones.resize(1);
        scene::appendAttachment(cast::Document::load(std::filesystem::u8path(argv[1])),worldWeapon,0,"AWP");
        if(worldWeapon.attachments.empty())return 1;
        auto& attachment=worldWeapon.attachments.front();attachment.scale={2.4f,2.4f,2.4f};attachment.position={-4,0,0};
        const std::vector<scene::Mat4> socketPose{scene::translation({100,200,300})};
        const auto worldMuzzle=scene::resolveMuzzlePosition(worldWeapon,socketPose);
        const auto expected=scene::transformPoint(socketPose[0]*attachment.localMatrix(),*nativeMuzzle);
        if(!worldMuzzle||scene::length(*worldMuzzle-expected)>.001f){std::cerr<<"Imported AWP attachment lost muzzle\n";return 1;}
        std::cout<<"real AWP flattened attachment muzzle matches transformed native socket\n";
        if(value.meshes.empty()||pose.empty()||!renderer.loadScene(value,error)){std::cerr<<error;return 1;}
        scene::Vec3 low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f};
        for(const auto& mesh:value.meshes)for(const auto& vertex:mesh.vertices){
            const auto p=scene::transformPoint(mesh.modelTransform,vertex.position);
            low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
            high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
        const auto center=(low+high)*.5f;const float radius=std::max(1.0f,scene::length(high-low)*.6f);
        const auto camera=center+scene::Vec3{0,-radius*2,radius*.2f};
        renderer.setCameraPosition(camera);
        renderer.setSun(true,false,{-.4f,.3f,-1},1.0f,.7f,{1,1,1},{1,1,1},512,200,150,center,false,512,200,400,10);
        const auto vp=scene::orthographic(-radius,radius,-radius,radius,.05f,radius*5)*scene::lookAt(camera,center,{0,0,1});
        while(glGetError()!=GL_NO_ERROR){}
        renderer.render(value,pose,vp,512,512,false,false,false);
        const auto glError=glGetError();
        std::vector<std::uint8_t> pixels;
        if(!renderer.readColorRgba(pixels,error))return 1;
        std::size_t lit=0;for(std::size_t i=0;i<pixels.size();i+=4)if(pixels[i]>8||pixels[i+1]>8||pixels[i+2]>8)++lit;
        std::cout<<"skinned model pixels="<<lit<<" GL error="<<glError<<'\n';
        if(lit<100||glError!=GL_NO_ERROR)status=1;
        if(!renderer.saveColorPng(output/"skinned_awp.png",error))status=1;
    }
    glfwDestroyWindow(window);glfwTerminate();return status;
}
