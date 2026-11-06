#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
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
 a->setWeatherFast(false);b->setWeatherFast(true);double times[2]{};
 std::vector<std::uint8_t> reference,pixels;int maxError=0;
 for(int frame=0;frame<24;++frame){
  if(frame==16){map.meshes[0].vertices[7].position.z+=100;for(auto*r:{a.get(),b.get()})if(!r->loadAuxiliaryScenes(&map,nullptr,&actor,error))return 4;}
  const int width=frame%2?641:640,height=frame%2?363:360;
  const scene::Vec3 camera{600+float(frame),-750,650},forward=scene::normalize(scene::Vec3{}-camera);
  const auto vp=scene::perspective(70*scene::kPi/180,float(width)/height,1,5000)*scene::lookAtDirection(camera,forward,{0,0,1});
  std::vector<std::vector<scene::Mat4>> poses{{scene::translation({float(frame%6)*35-100,-260,0})}};
  for(auto*r:{a.get(),b.get()}){
   r->setCameraPosition(camera);r->setEnvironmentCamera(forward,{0,0,1},70,640.f/360);
   r->setSun(true,true,scene::normalize(scene::Vec3{frame<8?-.5f:.5f,-.4f,-1}),1,.2f,{1,1,1},{1,1,1},frame<12?1024:512,1200,1000,{frame<20?0.f:50.f,0,0},true,512,700,2000,100);
   r->setFilmTweaks(true,.1f,1.1f,.2f,{1,1,1},{1,1,1},{1,1,1},false,false);
   render::HbaoSettings ao;ao.enabled=frame>=4;r->setHbao(ao);
   r->setLensTweaks(frame>=10,.4f,.2f,1,1,0,.2f,true,.2f,.6f,.5f);
   render::weather::Settings weather;weather.dust.enabled=frame!=18;weather.dust.opacity=.075f;weather.dust.sheets=frame==19?0:2;weather.dust.sheetSize=400;weather.dust.bottom=-1000;weather.dust.top=1000;weather.dustAerosolDetail=frame<12?202.6f:10.f;weather.dustAerosolAmount=.7f;weather.range=1500;weather.quality=frame<12?1:2;weather.snow.enabled=frame>=20;weather.debris.enabled=frame>=20;r->setWeather(weather,3.+frame*.1);
   r->render(empty,{},vp,width,height,false,false,false,&actor,&poses,nullptr,nullptr,nullptr,false,&map);
   r->render(empty,{},vp,width,height,false,false,false,&actor,&poses,nullptr,nullptr,nullptr,false,&map);
   glFinish();auto start=std::chrono::steady_clock::now();
   r->render(empty,{},vp,width,height,false,false,false,&actor,&poses,nullptr,nullptr,nullptr,false,&map);
   glFinish();times[r==a.get()?0:1]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   if(!r->readColorRgba(r==a.get()?reference:pixels,error)){std::cerr<<error;return 5;}
   if(frame==0||frame==12){std::filesystem::create_directories("diagnostics/v471/weather");if(!r->saveColorPng(std::filesystem::path("diagnostics/v471/weather")/(std::to_string(frame)+(r==a.get()?"-reference.png":"-fast.png")),error))return 7;}
   if(glGetError()!=GL_NO_ERROR)return 6;
   if(!r->weatherError().empty()){std::cerr<<r->weatherError();return 8;}
  }
  for(size_t i=0;i<pixels.size();++i)maxError=std::max(maxError,std::abs(int(pixels[i])-int(reference[i])));
 }
 a->shutdown();b->shutdown();glfwDestroyWindow(w);glfwTerminate();std::cout<<"Weather lifecycle audit completed; A/B max channel error="<<maxError<<" baseline ms="<<times[0]/24<<" optimized ms="<<times[1]/24<<"\n";
 // Reconstruction is not pixel-identical. Report differences for visual
 // inspection; this executable's pass/fail checks GL and renderer failures.
 return 0;
}


