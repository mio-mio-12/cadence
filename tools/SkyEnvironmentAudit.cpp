#include "render/StageRenderer.h"
#include "render/EnvironmentSource.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs=std::filesystem;
static void require(bool condition,const std::string& error){if(!condition)throw std::runtime_error(error);}
int main(int argc,char** argv){
    GLFWwindow* window=nullptr;
    try{
        require(argc>=3,"Usage: sky_environment_audit SKYBOX_ROOT OUTPUT_DIRECTORY [IWI_CONTROL]");
        const fs::path skies=fs::absolute(argv[1]),output=fs::absolute(argv[2]);fs::create_directories(output);
        std::vector<fs::path> manifests;fs::path effect,iwi=argc>3?fs::path(argv[3]):fs::path{};
        for(const auto& entry:fs::recursive_directory_iterator(skies))if(entry.is_regular_file()){
            if(entry.path().filename()=="skybox.json")manifests.push_back(entry.path());
            if(iwi.empty()&&render::environment::lower(entry.path().extension().string())==".iwi")iwi=entry.path();
        }
        std::sort(manifests.begin(),manifests.end());require(manifests.size()==17,"Expected 17 native sky manifests");require(!iwi.empty(),"No IWI control");
        for(const auto& manifest:manifests){std::ifstream input(manifest);const std::string text((std::istreambuf_iterator<char>(input)),{});const auto j=scene::codm::parseJson(text);for(const auto& texture:j.at("textures"))if(texture.value("layout",std::string{})=="native_effect_mesh_uv")effect=manifest.parent_path()/texture.at("exportedPath").get<std::string>();}
        require(!effect.empty(),"No unsupported effect fixture");
        require(glfwInit()!=0,"GLFW initialization failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        window=glfwCreateWindow(640,360,"Sky environment audit",nullptr,nullptr);require(window!=nullptr,"Hidden GL window failed");glfwMakeContextCurrent(window);glfwSwapInterval(0);
        {
            render::StageRenderer renderer;std::string error;require(renderer.initialize(error),error);
            scene::CastScene empty;empty.bounds.valid=true;empty.bounds.minimum={-1,-1,-1};empty.bounds.maximum={1,1,1};require(renderer.loadScene(empty,error),error);
            renderer.setEnvironmentParameters(1,0,0,1,1,1,0);renderer.setCameraPosition({0,0,0});renderer.setCameraDepthRange(.1f,1000);
            struct Direction{const char* name;scene::Vec3 forward,up;};
            const std::array<Direction,6> directions{{{"east",{1,0,0},{0,0,1}},{"north",{0,1,0},{0,0,1}},{"west",{-1,0,0},{0,0,1}},{"south",{0,-1,0},{0,0,1}},{"zenith",{0,0,1},{0,1,0}},{"nadir",{0,0,-1},{0,1,0}}}};
            auto draw=[&](const Direction& d){renderer.setEnvironmentCamera(d.forward,d.up,90,640.f/360);const auto vp=scene::perspective(scene::kPi/2,640.f/360,.1f,1000)*scene::lookAtDirection({0,0,0},d.forward,d.up);renderer.render(empty,{},vp,640,360,false,false,false);glFinish();std::vector<std::uint8_t> pixels;require(renderer.readColorRgba(pixels,error),error);require(glGetError()==GL_NO_ERROR,"GL error after sky rendering");return pixels;};
            std::ofstream report(output/"results.csv");report<<"source,load_ms,result\n";
            auto capture=[&](const fs::path& file,const std::string& label){const auto start=std::chrono::steady_clock::now();require(renderer.setEnvironmentPanorama(file,error),file.string()+": "+error);const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();for(const auto& direction:directions){draw(direction);require(renderer.saveColorPng(output/(label+"_"+direction.name+".png"),error),error);}report<<'"'<<file.generic_string()<<"\","<<elapsed<<",PASS\n";report.flush();std::cout<<label<<" loaded "<<elapsed<<" ms\n";};
            for(std::size_t i=0;i<manifests.size();++i){const auto label="native_"+std::to_string(i)+"_"+manifests[i].parent_path().filename().string();capture(manifests[i],label);}
            const auto before=draw(directions[0]);require(!renderer.setEnvironmentPanorama(effect,error)&&!error.empty(),"Unsupported effect accepted");require(draw(directions[0])==before,"Rejected effect replaced current sky");report<<"unsupported_effect_retains_previous,0,PASS\n";
            require(!renderer.setEnvironmentPanorama(output/"missing.hdr",error),"Missing HDR accepted");require(draw(directions[0])==before,"Missing image replaced current sky");report<<"missing_image_retains_previous,0,PASS\n";
            capture(iwi,"iwi_control");
            const auto hdr=output/"control.hdr";{std::ofstream file(hdr,std::ios::binary);file<<"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 4 +X 8\n";for(int y=0;y<4;++y){const unsigned char marker[]{2,2,0,8};file.write(reinterpret_cast<const char*>(marker),4);for(unsigned char value:{128,64,32,129}){const unsigned char run[]{136,value};file.write(reinterpret_cast<const char*>(run),2);}}}
            capture(hdr,"hdr_control");
            const auto hdrPixels=draw(directions[0]);const auto center=(180*640+320)*4;require(hdrPixels[center]>hdrPixels[center+1]&&hdrPixels[center+1]>hdrPixels[center+2],"HDR control channel ordering lost");
            const std::array<Direction,4> quadrants{{{"upper_left",{0,-1,1},{0,0,1}},{"upper_right",{0,1,1},{0,0,1}},{"lower_left",{0,-1,-1},{0,0,1}},{"lower_right",{0,1,-1},{0,0,1}}}};
            const std::array<std::array<unsigned char,4>,4> colors{{{128,8,8,129},{8,128,8,129},{8,8,128,129},{128,128,8,129}}};
            std::array<std::vector<std::uint8_t>,4> expected;
            for(char ySign:{'-','+'})for(char xSign:{'+','-'}){
                const auto fixture=output/(std::string("quadrants_")+(ySign=='-'?"minusY":"plusY")+(xSign=='+'?"plusX":"minusX")+".hdr");
                {std::ofstream file(fixture,std::ios::binary);file<<"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n"<<ySign<<"Y 8 "<<xSign<<"X 16\n";for(int y=0;y<8;++y){const unsigned char marker[]{2,2,0,16};file.write(reinterpret_cast<const char*>(marker),4);for(int channel=0;channel<4;++channel){file.put(16);for(int x=0;x<16;++x){const int logicalTopY=ySign=='-'?y:7-y,logicalX=xSign=='+'?x:15-x;file.put(colors[(logicalTopY>=4?2:0)+(logicalX>=8?1:0)][channel]);}}}}
                const bool loaded=renderer.setEnvironmentPanorama(fixture,error);require(loaded,error);
                for(std::size_t q=0;q<quadrants.size();++q){const auto pixels=draw(quadrants[q]);if(ySign=='-'&&xSign=='+'){
                    expected[q]=pixels;const auto& color=colors[q];for(int c=0;c<3;++c)require(color[c]>100?pixels[center+c]>150:pixels[center+c]<100,"HDR quadrant orientation mismatch: "+std::string(quadrants[q].name));require(renderer.saveColorPng(output/(std::string("hdr_")+quadrants[q].name+".png"),error),error);
                }else require(pixels==expected[q],"HDR signed-axis encoding changed rendered orientation");}
            }
            const auto lastGood=draw(directions[0]);const auto oversized=output/"oversized.hdr";{std::ofstream file(oversized);file<<"#?RADIANCE\n\n-Y 32768 +X 32768\n";}
            require(!renderer.setEnvironmentPanorama(oversized,error),"Oversized HDR accepted");require(draw(directions[0])==lastGood,"Oversized HDR replaced previous sky");
            report<<"hdr_four_quadrants_all_axis_signs,0,PASS\noversized_hdr_retains_previous,0,PASS\n";
            std::ofstream(output/"completed.txt")<<"PASS: 17 native manifests, IWI, HDR, rejected-load preservation. Direction images require visual review.\n";
        }
        glfwDestroyWindow(window);glfwTerminate();return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(window)glfwDestroyWindow(window);glfwTerminate();return 1;}
}
