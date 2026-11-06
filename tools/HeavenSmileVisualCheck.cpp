#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
int main(){if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(64,64,"Death effect check",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
    render::StageRenderer renderer;std::string error;if(!renderer.initialize(error)){std::cerr<<error;return 2;}
    scene::CastScene empty;const scene::Vec3 camera{300,-300,150};renderer.setCameraPosition(camera);
    const auto vp=scene::perspective(scene::kPi/3,1.5f,1,2000)*scene::lookAt(camera,{0,0,85},{0,0,1});
    std::filesystem::create_directories("diagnostics/v277/death-effect");
    for(float age:{.05f,.3f,.8f,1.4f,2.f}){
        renderer.setHeavenSmileBursts({{{0,0,85},age,170,7}},{},{1,0,0},true);renderer.render(empty,{},vp,900,600,false,false,false);
        std::vector<std::uint8_t> first,second;if(!renderer.readColorRgba(first,error))return 3;
        renderer.render(empty,{},vp,900,600,false,false,false);if(!renderer.readColorRgba(second,error)||first!=second)return 4;
        if(!renderer.saveColorPng("diagnostics/v277/death-effect/"+std::to_string(age)+".png",error))return 5;
        if(glGetError()!=GL_NO_ERROR)return 6;
    }
    renderer.setHeavenSmileBursts({{{0,0,85},.3f,170,7}});renderer.render(empty,{},vp,900,600,false,false,false);
    std::vector<std::uint8_t> ungraded,inverted;if(!renderer.readColorRgba(ungraded,error))return 7;
    renderer.setFilmTweaks(true,0,1,0,{1,1,1},{1,1,1},{1,1,1},false,true);
    renderer.render(empty,{},vp,900,600,false,false,false);if(!renderer.readColorRgba(inverted,error))return 8;
    std::size_t bad{};for(std::size_t i=0;i<ungraded.size();i+=4)for(int k=0;k<3;++k)if(std::abs(int(ungraded[i+k])+int(inverted[i+k])-255)>4)++bad;
    if(bad){std::cerr<<"Film inversion mismatch: "<<bad<<'\n';return 9;}
    std::cout<<"Death render deterministic; GL clean; film applied once\n";return 0;
}
