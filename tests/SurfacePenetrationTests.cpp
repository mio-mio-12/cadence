#include "gameplay/SurfacePenetration.h"
#include <iostream>
int main(){
 scene::glb::Map map;
 for(float x:{100.f,110.f,200.f,210.f})for(int duplicate=0;duplicate<2;++duplicate){
  scene::glb::CollisionTriangle t;t.a={x,-50,-50};t.b={x,50,-50};t.c={x,0,50};t.normal={1,0,0};t.minimum={x,-50,-50};t.maximum={x,50,50};t.blocking=true;map.collision.push_back(t);
 }
 map.buildCollisionIndex();
 for(int budget=0;budget<=4;++budget){auto hit=gameplay::shots::stoppingSurface(map,{0,0,0},{1,0,0},1000,budget);if(budget==4){if(hit)return 1;}else if(!hit||std::abs(hit->distance-std::array<float,4>{100,110,200,210}[budget])>.001f)return 2;}
 auto normal=map.raycastSurface({0,0,0},{1,0,0},1000),disabled=gameplay::shots::stoppingSurface(map,{0,0,0},{1,0,0},1000,0);
 if(!normal||!disabled||normal->distance!=disabled->distance)return 3;
 if(gameplay::shots::stoppingSurface(map,{0,0,0},{1,0,0},50,16))return 4;
 std::cout<<"Surface budgets, coincident faces, range and disabled parity passed\n";
}
