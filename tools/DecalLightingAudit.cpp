#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<error<<'\n';return 1;}}while(false)
int main(){std::string error;CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(320,240,"Decal lighting audit",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
 {render::StageRenderer renderer;CHECK(renderer.initialize(error));renderer.setCameraPosition({0,0,3});renderer.setCameraDepthRange(1,100);
  renderer.setSun(true,true,{.3f,.4f,-1.f},1.3f,.2f,{1,.7f,.5f},{.3f,.4f,.6f},256,100,80,{},true,256,50,150,10);
  renderer.setFilmTweaks(true,.02f,.8f,.1f,{.8f,.9f,1},{1,1,1},{1,.9f,.8f},true,false);
  int cases{},positive{};
  for(int material=0;material<2;++material)for(int explicitMaterial=0;explicitMaterial<2;++explicitMaterial)for(int fog=0;fog<2;++fog){
   scene::CastScene fixture;for(int i=0;i<3;++i){scene::Mesh m;m.name="decal fixture";m.doubleSided=true;m.materialPolicyExplicit=explicitMaterial;m.decal=i<2;m.decalMultiply=i<2&&material==0;m.decalAdditive=i<2&&material==1;m.color={.12f+i*.1f,.2f,.3f,i==0?.5f:1.f};
    float x=-.9f+i*.6f;m.vertices={{{x,-.7f,0}},{{x+.5f,-.7f,0}},{{x+.25f,.7f,0}}};m.indices={0,1,2};fixture.meshes.push_back(m);}
   renderer.setFog(fog,{.2f,.3f,.4f},0,1,1,0);CHECK(renderer.loadScene(fixture,error));
   std::vector<std::uint8_t> baseline;std::vector<float> baselineDepth;
   for(int pass=0;pass<3;++pass){renderer.setDiscardedDecalLightingElision(pass==1);renderer.render(fixture,{},scene::Mat4::identity(),320,240,false,false,false);glFinish();std::vector<std::uint8_t> image;std::vector<float> depth;CHECK(renderer.readColorRgba(image,error));CHECK(renderer.readDepthRawFloat(depth,1,100,1,100,false,error));CHECK(glGetError()==GL_NO_ERROR);
    if(!pass){baseline=image;baselineDepth=depth;}else{CHECK(image==baseline);CHECK(depth==baselineDepth);}}
   // Remove only target decals to prove both occupy visible pixels, rather
   // than accepting equality from empty or fully occluded geometry.
   auto absent=fixture;absent.meshes.erase(absent.meshes.begin(),absent.meshes.begin()+2);CHECK(renderer.loadScene(absent,error));renderer.render(absent,{},scene::Mat4::identity(),320,240,false,false,false);std::vector<std::uint8_t> image;CHECK(renderer.readColorRgba(image,error));
   for(int triangle=0;triangle<2;++triangle){int changed{};for(int y=40;y<200;++y)for(int x=20+triangle*96;x<92+triangle*96;++x){const auto at=(size_t(y)*320+x)*4;for(int c=0;c<3;++c)changed+=image[at+c]!=baseline[at+c];}CHECK(changed>100);positive+=changed;}
   ++cases;
  }
  std::cout<<"PASS decal lighting "<<cases<<" cases, positive decal channel samples="<<positive<<"; exact color/depth off/on/off\n";
 }glfwDestroyWindow(window);glfwTerminate();return 0;}
