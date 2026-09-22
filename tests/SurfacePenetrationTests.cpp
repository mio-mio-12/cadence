#include "gameplay/SurfacePenetration.h"
#include "scene/C2MMap.h"
#include "render/ShadowCoverage.h"
#include <chrono>
#include <iostream>
int main(int argc,char** argv){
 {
  const scene::Bounds bounds{{-1000,-1000,-100},{1000,1000,12010},true};
  const auto coverage=render::shadow::coverage({0,0,150},{0,0,-1},5000,bounds);
  const auto roof=scene::transformPoint(coverage.matrix,{0,0,12000});
  if(std::abs(roof.z)>=1||coverage.biasScale<=0||coverage.biasScale>=1)return 11;
  const auto base=render::shadow::coverage({0,0,150},{0,0,-1},5000,{});
  if(base.biasScale!=1)return 12;
 }
 if(argc>1&&std::filesystem::path(argv[1]).extension()==".glb"){
  scene::glb::Map real;std::string error;if(!scene::glb::load(argv[1],real,error)){std::cerr<<error;return 9;}
  const scene::Vec3 origin{1107.8798f,4603.7412f,-4566.5205f};const float az=-97.2f*scene::kPi/180,el=33.3f*scene::kPi/180;
  const scene::Vec3 light{-std::cos(az)*std::cos(el),-std::sin(az)*std::cos(el),std::sin(el)};
  for(float dz:{0.f,60.f,160.f}){const auto eye=origin+scene::Vec3{0,0,dz};const auto hit=real.raycastSurface(eye,light,50000);if(hit){const auto clip=scene::transformPoint(render::shadow::coverage(eye,light*-1.f,4200,real.scene.bounds).matrix,hit->position);if(std::abs(clip.z)>=1)return 10;std::cout<<"Sun blocker distance="<<hit->distance<<" fixed clip Z="<<clip.z<<'\n';}else std::cout<<"No sun blocker\n";}
  std::cout<<"Meshes="<<real.scene.meshes.size()<<" bounds="<<real.scene.bounds.minimum.z<<","<<real.scene.bounds.maximum.z<<'\n';return 0;
 }
 if(argc>1){
  scene::glb::Map real;std::string error;const auto start=std::chrono::steady_clock::now();
  if(!scene::c2m::load(argv[1],real,error)){std::cerr<<error;return 8;}
  const auto ms=[](auto t){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();};
  std::cout<<"Load ms="<<ms(start)<<", shot triangles="<<(real.shotGeometry?real.shotGeometry->collision.size():0)<<'\n';
  double base=0,shots=0;for(int i=0;i<3;++i){real.authoredCollision.present=false;auto t=std::chrono::steady_clock::now();real.buildCollisionIndex();base+=ms(t);real.authoredCollision.present=true;t=std::chrono::steady_clock::now();real.buildCollisionIndex();shots+=ms(t);}
  std::size_t openings=0,solids=0;const auto t0=std::chrono::steady_clock::now();
  for(std::size_t i=0;i<real.collision.size();i+=std::max(std::size_t(1),real.collision.size()/500)){const auto& t=real.collision[i];const auto origin=(t.a+t.b+t.c)/3.f+t.normal*50.f;if(real.raycastSurface(origin,t.normal*-1.f,100.f)){if(real.raycastShot(origin,t.normal*-1.f,100.f))++solids;else ++openings;}}
  std::cout<<"Mean index ms: movement="<<base/3<<", movement+shots="<<shots/3<<", added="<<(shots-base)/3<<"; sampled solid="<<solids<<", open="<<openings<<", query batch ms="<<ms(t0)<<'\n';
 }
 scene::glb::Map map;
 for(float x:{100.f,110.f,200.f,210.f})for(int duplicate=0;duplicate<2;++duplicate){
  scene::glb::CollisionTriangle t;t.a={x,-50,-50};t.b={x,50,-50};t.c={x,0,50};t.normal={1,0,0};t.minimum={x,-50,-50};t.maximum={x,50,50};t.blocking=true;map.collision.push_back(t);
 }
 map.buildCollisionIndex();
 for(int budget=0;budget<=4;++budget){auto hit=gameplay::shots::stoppingSurface(map,{0,0,0},{1,0,0},1000,budget);if(budget==4){if(hit)return 1;}else if(!hit||std::abs(hit->distance-std::array<float,4>{100,110,200,210}[budget])>.001f)return 2;}
 auto normal=map.raycastSurface({0,0,0},{1,0,0},1000),disabled=gameplay::shots::stoppingSurface(map,{0,0,0},{1,0,0},1000,0);
 if(!normal||!disabled||normal->distance!=disabled->distance)return 3;
 if(gameplay::shots::stoppingSurface(map,{0,0,0},{1,0,0},50,16))return 4;
 map.authoredCollision.present=true;
 scene::Mesh wall;wall.vertices.resize(3);wall.vertices[0].position={200,-50,-50};wall.vertices[1].position={200,50,-50};wall.vertices[2].position={200,0,50};wall.indices={0,1,2};map.scene.meshes.push_back(wall);
 map.buildCollisionIndex();
 auto physical=map.raycastSurface({0,0,0},{1,0,0},1000),shot=map.raycastShot({0,0,0},{1,0,0},1000);
 if(!physical||physical->distance!=100||!shot||shot->distance!=200)return 5;
 if(gameplay::shots::stoppingSurface(map,{0,0,0},{1,0,0},1000,1))return 6;
 map.scene.meshes[0].forceAlpha=true;map.buildCollisionIndex();
 if(map.raycastShot({0,0,0},{1,0,0},1000)||!map.raycastSurface({0,0,0},{1,0,0},1000))return 7;
 std::cout<<"Surface budgets, invisible movement hulls, transparent surfaces and solid shot geometry passed\n";
}
