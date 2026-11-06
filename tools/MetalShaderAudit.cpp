#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
int main(int argc,char** argv){
    try{
        if(argc!=4)throw std::runtime_error("model.cast sky.iwi output_folder");
        auto model=scene::buildScene(cast::Document::load(argv[1]),false);
        for(auto& mesh:model.meshes)mesh.viewmodelWeapon=true;
        std::vector<scene::Mat4> pose;for(const auto& b:model.skeleton.bones)pose.push_back(b.restGlobal);
        if(!glfwInit())throw std::runtime_error("GLFW failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto* window=glfwCreateWindow(960,720,"Metal material audit",nullptr,nullptr);if(!window)throw std::runtime_error("Window failed");glfwMakeContextCurrent(window);glfwSwapInterval(0);
        {
            render::StageRenderer renderer;std::string error;
            if(!renderer.initialize(error)||!renderer.loadScene(model,error)||!renderer.setEnvironmentPanorama(argv[2],error))throw std::runtime_error(error);
            renderer.setDebugView(0);renderer.setTonemapping(0);
            renderer.setMaterialParameters(.8f,{1,1,1},1,.5f,32,.15f,8,1,true,1,.08f,0,true);
            renderer.setEnvironmentParameters(1,0,0);renderer.setViewmodelCapture(false,{});
            renderer.setSun(true,false,scene::normalize(scene::Vec3{-.5f,.6f,-1}),1,.5f,{1,.97f,.9f},{.7f,.8f,1},512,800,600,{},false,512,600,1600,100);
            const auto center=model.bounds.center();const auto radius=std::max(5.f,model.bounds.radius());
            const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);std::ofstream log(out/"results.txt");
            for(const auto& mesh:model.meshes)log<<mesh.materialName<<" spec="<<mesh.specularPath<<" normal="<<mesh.normalPath<<'\n';
            for(int angle=0;angle<2;++angle){
                const auto eye=center+scene::Vec3{radius*.6f,radius*(angle?2.4f:-2.4f),radius*.45f};const auto forward=scene::normalize(center-eye);
                renderer.setCameraPosition(eye);renderer.setEnvironmentCamera(forward,{0,0,1},45,960.f/720);
                const auto vp=scene::perspective(45*scene::kPi/180,960.f/720,.1f,10000)*scene::lookAt(eye,center,{0,0,1});
                for(float metalness:{-1.f,0.f}){
                    renderer.setWeaponMaterialTuning(1,{0,0,0},{1,1,1},1,.08f,metalness);std::vector<std::uint8_t> zero;
                    renderer.setMetalShader({});renderer.render(model,pose,vp,960,720,false,false,false);glFinish();if(!renderer.readColorRgba(zero,error))throw std::runtime_error(error);
                    for(int mode:{1,2}){render::metal::Settings m;m.mode=mode;renderer.setMetalShader(m);renderer.render(model,pose,vp,960,720,false,false,false);glFinish();std::vector<std::uint8_t> pixels;if(!renderer.readColorRgba(pixels,error)||pixels!=zero)throw std::runtime_error("Metal mode changed unassigned/zero metalness");}
                    log<<"PASS zero/unassigned metalness "<<metalness<<" angle "<<angle<<'\n';
                }
                std::vector<std::filesystem::path> originalMasks;for(const auto& mesh:model.meshes)originalMasks.push_back(mesh.metalnessPath);
                for(unsigned char value:{0,128,255}){
                    const auto mask=out/("metal_mask_"+std::to_string(value)+".png");if(!renderer.savePixelsPng(mask,1,1,{value,value,value,255},error))throw std::runtime_error(error);
                    for(auto& mesh:model.meshes)mesh.metalnessPath=mask;if(!renderer.loadScene(model,error))throw std::runtime_error(error);
                    renderer.setWeaponMaterialTuning(1,{0,0,0},{1,1,1},1,.08f,-1);renderer.setMetalShader({});renderer.render(model,pose,vp,960,720,false,false,false);glFinish();std::vector<std::uint8_t> baseline;
                    if(!renderer.readColorRgba(baseline,error))throw std::runtime_error(error);
                    for(int mode:{1,2}){render::metal::Settings m;m.mode=mode;renderer.setMetalShader(m);renderer.render(model,pose,vp,960,720,false,false,false);glFinish();std::vector<std::uint8_t> pixels;if(!renderer.readColorRgba(pixels,error)||(value==0?pixels!=baseline:pixels==baseline))throw std::runtime_error("Mapped metalness gating failed");}
                    log<<"PASS mapped metalness "<<int(value)<<" angle "<<angle<<'\n';
                }
                for(size_t i=0;i<model.meshes.size();++i)model.meshes[i].metalnessPath=originalMasks[i];if(!renderer.loadScene(model,error))throw std::runtime_error(error);
                renderer.setWeaponMaterialTuning(1,{0,0,0},{1,1,1},1,.08f,1);
                std::vector<std::uint8_t> control;
                for(int mode:{0,1,2,0}){
                    render::metal::Settings settings;settings.mode=mode;renderer.setMetalShader(settings);
                    renderer.render(model,pose,vp,960,720,false,false,false);glFinish();
                    std::vector<std::uint8_t> pixels;if(!renderer.readColorRgba(pixels,error)||glGetError()!=GL_NO_ERROR)throw std::runtime_error(error+" GL error");
                    if(mode==0&&!control.empty()){if(control!=pixels)throw std::runtime_error("Current mode changed after override");log<<"PASS exact Current-mode restoration angle "<<angle<<'\n';continue;}
                    if(mode==0)control=pixels;else if(control==pixels)throw std::runtime_error("Metal override had no rendered effect");
                    if(!renderer.saveColorPng(out/("metal_"+std::to_string(angle)+"_"+std::to_string(mode)+".png"),error))throw std::runtime_error(error);
                    log<<"PASS render angle "<<angle<<" mode "<<mode<<'\n';
                }
                for(int mode:{1,2}){
                    render::metal::Settings settings;settings.mode=mode;settings.coverage=0;renderer.setMetalShader(settings);
                    renderer.render(model,pose,vp,960,720,false,false,false);glFinish();std::vector<std::uint8_t> pixels;
                    if(!renderer.readColorRgba(pixels,error)||pixels!=control)throw std::runtime_error("Zero coverage changed Current rendering");
                    settings.coverage=.5f;renderer.setMetalShader(settings);renderer.render(model,pose,vp,960,720,false,false,false);glFinish();
                    if(!renderer.readColorRgba(pixels,error)||pixels==control)throw std::runtime_error("Partial coverage had no effect");
                    log<<"PASS zero and partial coverage mode "<<mode<<" angle "<<angle<<'\n';
                }
                for(auto& mesh:model.meshes){mesh.viewmodelWeapon=false;mesh.camoBlend=false;}
                if(!renderer.loadScene(model,error))throw std::runtime_error(error);
                renderer.setMetalShader({});renderer.render(model,pose,vp,960,720,false,false,false);glFinish();std::vector<std::uint8_t> hands;
                if(!renderer.readColorRgba(hands,error))throw std::runtime_error(error);
                for(int mode:{1,2}){render::metal::Settings settings;settings.mode=mode;renderer.setMetalShader(settings);renderer.render(model,pose,vp,960,720,false,false,false);glFinish();std::vector<std::uint8_t> pixels;if(!renderer.readColorRgba(pixels,error)||pixels!=hands)throw std::runtime_error("Non-weapon surfaces changed");}
                log<<"PASS non-weapon/hands exclusion angle "<<angle<<'\n';
                for(auto& mesh:model.meshes)mesh.viewmodelWeapon=true;if(!renderer.loadScene(model,error))throw std::runtime_error(error);
            }
            renderer.shutdown();
        }
        glfwDestroyWindow(window);glfwTerminate();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
