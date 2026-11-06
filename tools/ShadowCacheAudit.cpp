#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
scene::CastScene box(float size,bool skin){
 scene::CastScene s;scene::Mesh m;m.skinned=skin;
 for(int i=0;i<8;++i){scene::Vertex v;v.position={(i&1)?size:-size,(i&2)?size:-size,(i&4)?size*2:0};m.vertices.push_back(v);}
 m.indices={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
 s.meshes.push_back(m);s.bounds={{-size,-size,0},{size,size,size*2},true};
 if(skin){scene::Bone b;b.name="root";b.parent=-1;b.restGlobal=b.inverseBind=scene::Mat4::identity();s.skeleton.bones.push_back(b);}return s;
}
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(640,360,"Shadow cache audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
 auto a=std::make_unique<render::StageRenderer>(),b=std::make_unique<render::StageRenderer>();std::string error;
 scene::CastScene empty;auto map=box(220,false),actor=box(35,true);
 for(auto*r:{a.get(),b.get()})if(!r->initialize(error)||!r->loadScene(empty,error)||!r->loadAuxiliaryScenes(&map,nullptr,&actor,error)){std::cerr<<error;return 3;}
 a->setPostTargetExchange(false);b->setStaticShadowCache(true);
 std::vector<std::uint8_t> reference,pixels;int maxError=0,hits=0,rebuilds=0;
 for(int frame=0;frame<24;++frame){
  if(frame==16){map.meshes[0].vertices[7].position.z+=100;for(auto*r:{a.get(),b.get()})if(!r->loadAuxiliaryScenes(&map,nullptr,&actor,error))return 4;}
  const scene::Vec3 camera{600,-750,650},forward=scene::normalize(scene::Vec3{}-camera);
  const auto vp=scene::perspective(70*scene::kPi/180,640.f/360,1,5000)*scene::lookAtDirection(camera,forward,{0,0,1});
  std::vector<std::vector<scene::Mat4>> poses{{scene::translation({float(frame%6)*35-100,-260,0})}};
  for(auto*r:{a.get(),b.get()}){
   r->setCameraPosition(camera);r->setEnvironmentCamera(forward,{0,0,1},70,640.f/360);
   r->setSun(true,true,scene::normalize(scene::Vec3{frame<8?-.5f:.5f,-.4f,-1}),1,.2f,{1,1,1},{1,1,1},frame<12?1024:512,1200,1000,{frame<20?0.f:50.f,0,0},true,512,700,2000,100);
   r->setFilmTweaks(true,.1f,1.1f,.2f,{1,1,1},{1,1,1},{1,1,1},false,false);
   render::HbaoSettings ao;ao.enabled=frame>=4;r->setHbao(ao);
   r->setLensTweaks(frame>=10,.4f,.2f,1,1,0,.2f,true,.2f,.6f,.5f);
   r->render(empty,{},vp,640,360,false,false,false,&actor,&poses,nullptr,nullptr,nullptr,false,&map);
   if(!r->readColorRgba(r==a.get()?reference:pixels,error)){std::cerr<<error;return 5;}
   if(glGetError()!=GL_NO_ERROR)return 6;
   const auto stats=r->renderStats(false);
   if(r==a.get()&&stats.lastShadowCasterDraws==0){std::cerr<<"No reference map shadow draws";return 7;}
   if(r==b.get()){if(stats.lastShadowCasterDraws==0)++hits;else ++rebuilds;}
  }
  for(size_t i=0;i<pixels.size();++i)maxError=std::max(maxError,std::abs(int(pixels[i])-int(reference[i])));
 }
 a->shutdown();b->shutdown();glfwDestroyWindow(w);glfwTerminate();std::cout<<"24 frames: static reuse, moving actor, stationary sun change, resolution, map reload, focus change, AO and bloom; max channel error="<<maxError<<" cache hits="<<hits<<" rebuilds="<<rebuilds<<"\n";return maxError>1||hits==0||rebuilds!=5?1:0;
}
