#include "scene/C2MMap.h"
#include "gameplay/AuthoredTraversal.h"
#include <iostream>
#include "gameplay/PlayerMapMovement.h"
#include <map>
int main(int argc,char**argv){
 if(argc<2)return 2;
 scene::glb::Map map;std::string error;
 scene::c2m::LoadOptions loadOptions;loadOptions.renderGeometryCollision=argc>2&&std::string(argv[2])=="--visible";
 if(!scene::c2m::load(argv[1],map,error,1,nullptr,loadOptions)){std::cerr<<error;return 3;}
 if(argc>2&&std::string(argv[2])=="--authored"){
  auto position=map.defaultSpawnPoint;const auto support=gameplay::movement::playerSupport(map,position,scene::Vec2{43,37},164.9f,10);
  std::cout<<"AUTHORED physicsReady="<<map.authoredCollision.physicsReady<<" complete="<<map.authoredCollision.complete<<" colliders="<<map.authoredColliders.size()<<" triangles="<<map.collision.size()<<" spawn="<<map.hasDefaultSpawnPoint<<" support="<<support.value_or(-1e9f)<<" status="<<map.authoredCollision.status<<std::endl;
  if(!map.authoredCollision.physicsReady||!map.hasDefaultSpawnPoint||!support)return 6;
  scene::Vec3 velocity{};for(int frame=0;frame<120;++frame){velocity.z-=981.f/60;auto result=gameplay::movement::slidePlayer(map,position,velocity,1.f/60,scene::Vec2{43,37},164.9f);position=result.position;velocity=result.velocity;}
  std::cout<<"AUTHORED spawn drop settled="<<position.z<<" expected="<<*support<<std::endl;
  return std::abs(position.z-*support)<2?0:7;
 }
 if(loadOptions.renderGeometryCollision){
  const auto p=map.defaultSpawnPoint;size_t floors=0;for(const auto&t:map.collision)floors+=t.walkable;
  std::cout<<"VISIBLE triangles="<<map.collision.size()<<" floors="<<floors<<" hasSpawn="<<map.hasDefaultSpawnPoint<<" spawn="<<p.x<<","<<p.y<<","<<p.z<<" ground="<<map.groundHeight(p.x,p.y,p.z,-1e9f)<<" support="<<gameplay::movement::playerSupport(map,p,scene::Vec2{43,37},164.9f,10).value_or(-1e9f)<<std::endl;
  std::map<std::string,int> missing;
  for(const auto&m:map.scene.meshes)for(size_t i=0;i+2<m.indices.size();i+=303){const auto a=m.vertices[m.indices[i]].position,b=m.vertices[m.indices[i+1]].position,c=m.vertices[m.indices[i+2]].position;const auto n=scene::cross(b-a,c-a);if(std::abs(n.z)<100||std::abs(scene::normalize(n).z)<.95f)continue;const auto q=(a+b+c)/3.f;const auto g=map.groundHeight(q.x,q.y,q.z+.1f-45.f,-1e9f);if(std::abs(q.z-g)>2)missing[m.materialName]++;}
  for(const auto&[name,count]:missing)if(count>=5)std::cout<<"MISSING "<<count<<" "<<name<<std::endl;
  size_t tested=0,failed=0;
  for(const auto&m:map.scene.meshes){
   if(m.materialName.find("grass")==std::string::npos||m.materialName.find("foliage")!=std::string::npos)continue;
   for(size_t i=0;i+2<m.indices.size();i+=3){
    const auto a=m.vertices[m.indices[i]].position,b=m.vertices[m.indices[i+1]].position,c=m.vertices[m.indices[i+2]].position;
    const auto n=scene::cross(b-a,c-a);if(std::abs(n.z)<10000||std::abs(scene::normalize(n).z)<.99f)continue;
    const auto q=(a+b+c)/3.f;auto position=q+scene::Vec3{0,0,5};
    if(gameplay::mantle::sweepRounded(map,position,position,scene::Vec2{43,37},164.9f).startSolid)continue;
    scene::Vec3 velocity{};
    for(int frame=0;frame<120;++frame){velocity.z-=981.f/60;auto result=gameplay::movement::slidePlayer(map,position,velocity,1.f/60,scene::Vec2{43,37},164.9f);position=result.position;velocity=result.velocity;}
    // Gravity may slide a hull along sloping asphalt. Compare against the
    // surface plane at the final XY, not its original centroid height.
    const float planeZ=q.z-(n.x*(position.x-q.x)+n.y*(position.y-q.y))/n.z;
    ++tested;if(position.z<planeZ-2)++failed;
    std::cout<<"FLOOR_DROP "<<m.materialName<<" plane="<<planeZ<<" settled="<<position.z<<" travel="<<scene::length(position-q)<<std::endl;break;
   }
  }
  std::cout<<"FLOOR_DROP tested="<<tested<<" failed="<<failed<<std::endl;
  return failed?5:0;
 }
 if(argc>2&&std::string(argv[2])=="--load-only"){
  std::cout<<"LOAD PASS meshes="<<map.scene.meshes.size()<<" colliders="<<map.authoredColliders.size()<<" triangles="<<map.collision.size()<<" physicsReady="<<map.authoredCollision.physicsReady<<" status="<<map.authoredCollision.status<<std::endl;return 0;
 }
 const auto p=map.defaultSpawnPoint;
 scene::glb::Map visible;scene::c2m::LoadOptions options;options.renderGeometryCollision=true;
 if(!scene::c2m::load(argv[1],visible,error,1,nullptr,options)){std::cerr<<error;return 4;}
 std::cout<<"visible floor at authored spawn="<<visible.groundHeight(p.x,p.y,p.z+200,-1e9f)<<" visible default="<<visible.defaultSpawnPoint.x<<","<<visible.defaultSpawnPoint.y<<","<<visible.defaultSpawnPoint.z<<std::endl;
 std::cout<<"triangles="<<map.collision.size()<<" authored="<<map.authoredCollision.present<<" DEFAULT "<<map.hasDefaultSpawnPoint<<" "<<p.x<<","<<p.y<<","<<p.z<<" ground="<<map.groundHeight(p.x,p.y,p.z,-1e9f)<<std::endl;
 for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){const auto q=p+scene::Vec3{float(x*100),float(y*100),0};const float g=map.navigationGroundHeight(q.x,q.y,q.z+1,-1e9f,20);std::cout<<"PROBE "<<q.x<<","<<q.y<<" ground="<<g<<" solid="<<gameplay::mantle::sweepRounded(map,{q.x,q.y,g},{q.x,q.y,g},38,165).startSolid<<std::endl;}
}
