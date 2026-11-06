#include "scene/C2mMap.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 if(argc<3)return 2;std::filesystem::path input=argv[1];if(std::filesystem::is_directory(input))for(auto& entry:std::filesystem::directory_iterator(input))if(entry.path().extension()==".c2m"){input=entry.path();break;}
 std::filesystem::create_directories(std::filesystem::path(argv[2]).parent_path());std::ofstream log(argv[2]);
 const auto start=std::chrono::steady_clock::now();
 auto seconds=[&]{return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();};
 auto progress=[&](std::string_view message,float p){std::cout<<seconds()<<"s "<<p<<" "<<message<<std::endl;log<<seconds()<<"s "<<p<<" "<<message<<std::endl;};
 scene::glb::Map map;std::string error;
 scene::c2m::LoadOptions options;for(int i=3;i<argc;++i)if(std::string_view(argv[i])=="geometry")options.renderGeometryCollision=true;
 if(!scene::c2m::load(input,map,error,1,progress,options)){std::cerr<<error;return 3;}
 const double cpu=seconds();progress("CPU load complete",.9f);
 std::uint64_t hash=1469598103934665603ull;size_t vertices=0,indices=0;
 auto bytes=[&](const void* p,size_t n){for(size_t i=0;i<n;++i){hash^=static_cast<const unsigned char*>(p)[i];hash*=1099511628211ull;}};
 for(auto& mesh:map.scene.meshes){vertices+=mesh.vertices.size();indices+=mesh.indices.size();for(auto& v:mesh.vertices){bytes(&v.position,sizeof(v.position));bytes(&v.normal,sizeof(v.normal));bytes(&v.uv,sizeof(v.uv));bytes(&v.color,sizeof(v.color));}bytes(mesh.indices.data(),mesh.indices.size()*sizeof(uint32_t));for(auto& path:{mesh.albedoPath,mesh.normalPath,mesh.specularPath}){auto s=path.string();bytes(s.data(),s.size());}}
 for(auto& t:map.collision){bytes(&t.a,sizeof(t.a));bytes(&t.b,sizeof(t.b));bytes(&t.c,sizeof(t.c));bytes(&t.walkable,sizeof(t.walkable));bytes(&t.blocking,sizeof(t.blocking));}
 log<<"CPU_SECONDS "<<cpu<<" meshes "<<map.scene.meshes.size()<<" vertices "<<vertices<<" indices "<<indices<<" collision "<<map.collision.size()<<" fingerprint "<<hash<<std::endl;
 if(argc>3&&std::string_view(argv[3])=="cpu")return 0;
 if(!glfwInit())return 4;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(640,360,"Map loading audit",nullptr,nullptr);if(!window)return 5;glfwMakeContextCurrent(window);
 auto renderer=std::make_unique<render::StageRenderer>();if(!renderer->initialize(error)||!renderer->loadScene({},error)){std::cerr<<error;return 6;}
 const auto uploadStart=seconds();if(!renderer->loadAuxiliaryScenes(&map.scene,nullptr,nullptr,error,progress)){std::cerr<<error;return 7;}glFinish();
 log<<"UPLOAD_SECONDS "<<seconds()-uploadStart<<std::endl;progress("Upload complete",1);
 renderer->shutdown();glfwDestroyWindow(window);glfwTerminate();return 0;
}
