#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <array>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<error<<'\n';return 1;}}while(false)
int main(){
 std::string error;CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
 auto* window=glfwCreateWindow(320,240,"Cull state regression",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
 {
  render::StageRenderer renderer;CHECK(renderer.initialize(error));
  scene::CastScene fixture;
  for(int i=0;i<4;++i){scene::Mesh mesh;mesh.name="cull fixture";mesh.materialPolicyExplicit=true;mesh.doubleSided=i%2;
   float x=-.85f+i*.45f;mesh.vertices={{{x,-.6f,0}},{{x+.4f,-.6f,0}},{{x+.2f,.6f,0}}};
   mesh.indices=i<2?std::vector<std::uint32_t>{0,1,2}:std::vector<std::uint32_t>{2,1,0};mesh.color={.2f+i*.2f,.7f,.3f,1};fixture.meshes.push_back(mesh);}
  CHECK(renderer.loadScene(fixture,error));
  render::StageRenderer interleaved;CHECK(interleaved.initialize(error));CHECK(interleaved.loadScene(fixture,error));interleaved.setCullStateCacheEnabled(true);
  const auto vp=scene::Mat4::identity();
  renderer.render(fixture,{},vp,320,240,false,false,false);
  CHECK(renderer.renderStats(false).cull.queries==0); // production default
  for(int variant=0;variant<8;++variant){
   std::vector<std::uint8_t> baseline;std::vector<float> depths;std::array<int,5> state{};int draws{};
   for(int iteration=0;iteration<3;++iteration){
    renderer.setCullStateCacheEnabled(iteration==2);
    if(variant&1)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
    glCullFace(variant&2?GL_FRONT:GL_BACK);glFrontFace(variant&4?GL_CW:GL_CCW);
    renderer.render(fixture,{},vp,320,240,false,false,variant==7);glFinish();
    std::array<int,5> current{};current[0]=glIsEnabled(GL_CULL_FACE);glGetIntegerv(GL_CULL_FACE_MODE,&current[1]);glGetIntegerv(GL_FRONT_FACE,&current[2]);glGetIntegerv(GL_DEPTH_FUNC,&current[3]);glGetIntegerv(GL_DEPTH_WRITEMASK,&current[4]);
    CHECK(glGetError()==GL_NO_ERROR);std::vector<std::uint8_t> image;std::vector<float> depth;
    CHECK(renderer.readColorRgba(image,error));CHECK(renderer.readDepthRawFloat(depth,1,100,1,100,false,error));
    CHECK(depth.size()==320*240);CHECK(image.size()==320*240*4);
    CHECK(depth[0]==0);
    for(int triangle=0;triangle<4;++triangle){
      int covered{};bool differentColor=false;
      const int left=24+triangle*72,right=left+64;
      for(int y=40;y<200;++y)for(int x=left;x<right;++x){const auto index=size_t(y)*320+x;
        if(depth[index]>0){++covered;for(int c=0;c<3;++c)differentColor|=image[index*4+c]!=image[c];}}
      const bool shouldShow=variant==7||triangle%2||((triangle==0)==!(variant&4));
      CHECK((covered>0)==shouldShow);if(shouldShow)CHECK(differentColor);
    }
    auto stats=renderer.renderStats(false);
    CHECK(stats.lastTotalDrawCalls==4);
    if(!iteration){baseline=image;depths=depth;state=current;draws=stats.lastTotalDrawCalls;}
    else{CHECK(image==baseline);CHECK(depth==depths);CHECK(state==current);CHECK(draws==stats.lastTotalDrawCalls);}
    if(iteration==2)CHECK(stats.cull.queries==0);
    // A separate renderer may leave a different face mode in the same context;
    // the next invocation must not inherit another object's cached assumptions.
    glFrontFace(GL_CW);glCullFace(GL_FRONT);glEnable(GL_CULL_FACE);
    interleaved.render(fixture,{},vp,320,240,false,false,false);
   }
  }
 }
 glfwDestroyWindow(window);glfwTerminate();std::cout<<"PASS cull GL: mixed winding/materials, inherited face/enable, wire, off/off/on color/depth/state parity\n";
}
