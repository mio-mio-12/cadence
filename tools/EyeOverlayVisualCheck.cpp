#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

// Render the real imported eye shells, not substitute meshes or textures.
int main(int argc,char** argv){
    if(argc!=3)return 2;
    auto value=scene::buildScene(cast::Document::load(std::filesystem::u8path(argv[1])));
    scene::Vec3 low{},high{};bool found=false;int shells=0;
    for(const auto& mesh:value.meshes)if(mesh.eyeOverlay){
        ++shells;std::cout<<"Overlay: "<<mesh.materialName<<'\n';
        for(const auto& vertex:mesh.vertices){
            const auto p=scene::transformPoint(mesh.modelTransform,vertex.position);
            if(!found){low=high=p;found=true;}
            low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
            high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
    }
    if(!found){std::cerr<<"No eye overlay found\n";return 1;}
    std::cout<<"Shells: "<<shells<<" bounds "<<low.x<<','<<low.y<<','<<low.z<<" to "<<high.x<<','<<high.y<<','<<high.z<<'\n';
    const auto center=(low+high)*0.5f;
    const float radius=std::max({high.x-low.x,high.y-low.y,high.z-low.z,1.0f});
    const auto output=std::filesystem::u8path(argv[2]);std::filesystem::create_directories(output);
    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(64,64,"Eye material check",nullptr,nullptr);if(!window)return 1;
    glfwMakeContextCurrent(window);
    int result=0;
    {
        render::StageRenderer renderer;std::string error;
        if(!renderer.initialize(error)){std::cerr<<error;return 1;}
        std::vector<scene::Mat4> pose;for(const auto& bone:value.skeleton.bones)pose.push_back(bone.restGlobal);
        for(int mode=0;mode<3;++mode){
            auto test=value;
            if(mode==0)for(auto& mesh:test.meshes)if(mesh.eyeOverlay){mesh.eyeOverlay=false;mesh.lens=false;mesh.forceAlpha=false;}
            if(mode==2)std::erase_if(test.meshes,[](const auto& mesh){return mesh.eyeOverlay;});
            if(!renderer.loadScene(test,error)){std::cerr<<error;return 1;}
            const bool localHead=(high.z-low.z)>(high.y-low.y);
            const auto direction=localHead?scene::Vec3{0,center.y<0?-1.0f:1.0f,0}:scene::Vec3{1,0,0};
            const auto up=localHead?scene::Vec3{1,0,0}:scene::Vec3{0,0,1};
            const auto camera=center+direction*(radius*2.0f);
            renderer.setCameraPosition(camera);
            renderer.setSun(true,false,direction*(-1.0f)+up*(-0.4f),1.0f,0.65f,{1,1,1},{1,1,1},512,100,80,center,false,512,100,200,10);
            const auto vp=scene::perspective(45.0f*scene::kPi/180.0f,1.5f,0.05f,10000)*scene::lookAt(camera,center,up);
            renderer.render(test,pose,vp,900,600,false,false,false);
            if(!renderer.saveColorPng(output/(mode==0?"opaque-shell.png":mode==1?"clear-shell.png":"iris-only.png"),error)){std::cerr<<error;result=1;}
        }
    }
    glfwDestroyWindow(window);glfwTerminate();return result;
}
