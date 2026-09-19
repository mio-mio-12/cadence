#include "scene/GlbMap.h"
#include "scene/C2mxMap.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" "<<error<<"\n";return 1;}}while(false)
int main(int argc,char**argv){
 std::string error;CHECK(argc>1);const std::filesystem::path path=argv[1];
 scene::glb::Map original,optimized;scene::c2m::LoadOptions options;options.allowApproximateCollision=true;
 const auto start=std::chrono::steady_clock::now();
 CHECK(scene::glb::load(path,original,error));
 CHECK(scene::c2mx::loadCollisionSidecar(path.parent_path()/"collision.json",original,error,options));
 const auto middle=std::chrono::steady_clock::now();
 CHECK(scene::glb::load(path,optimized,error,1.f,false));
 CHECK(optimized.collision.empty()&&optimized.gridCollision.empty()&&!optimized.hasDefaultSpawnPoint);
 CHECK(scene::c2mx::loadCollisionSidecar(path.parent_path()/"collision.json",optimized,error,options));
 const auto end=std::chrono::steady_clock::now();
 CHECK(original.collision.size()==optimized.collision.size());
 CHECK(original.scene.meshes.size()==optimized.scene.meshes.size());
 CHECK(original.hasDefaultSpawnPoint==optimized.hasDefaultSpawnPoint);
 if(original.hasDefaultSpawnPoint)CHECK(scene::length(original.defaultSpawnPoint-optimized.defaultSpawnPoint)==0);
 CHECK(original.collisionPreviewPoint.has_value()==optimized.collisionPreviewPoint.has_value());
 if(original.collisionPreviewPoint)CHECK(scene::length(*original.collisionPreviewPoint-*optimized.collisionPreviewPoint)==0);
 for(size_t i=0;i<original.collision.size();++i){const auto&a=original.collision[i];const auto&b=optimized.collision[i];
  CHECK(scene::length(a.a-b.a)==0&&scene::length(a.b-b.b)==0&&scene::length(a.c-b.c)==0&&a.walkable==b.walkable&&a.blocking==b.blocking);
 }
 for(size_t m=0;m<original.scene.meshes.size();++m){const auto&a=original.scene.meshes[m];const auto&b=optimized.scene.meshes[m];
  CHECK(a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&a.albedoPath==b.albedoPath);
  for(size_t i=0;i<a.vertices.size();++i)CHECK(scene::length(a.vertices[i].position-b.vertices[i].position)==0);
 }
 std::cout<<"Exact collision/geometry/spawn parity PASS; legacy path "<<std::chrono::duration<double>(middle-start).count()<<" s; deferred "<<std::chrono::duration<double>(end-middle).count()<<" s (sequential, OS-cache biased)\n";
 original={};
 CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(320,240,"Map upload diagnostic",nullptr,nullptr);CHECK(w);glfwMakeContextCurrent(w);
 render::StageRenderer renderer;CHECK(renderer.initialize(error));scene::CastScene empty;CHECK(renderer.loadScene(empty,error));bool timing=false;
 CHECK(renderer.loadAuxiliaryScenes(&optimized.scene,nullptr,nullptr,error,[&](std::string_view text,float){std::cout<<text<<'\n';if(text.find("[Map textures]")!=std::string_view::npos)timing=true;}));
 CHECK(timing);renderer.shutdown();glfwDestroyWindow(w);glfwTerminate();
 std::cout<<"Map upload timing output PASS\n";
}
