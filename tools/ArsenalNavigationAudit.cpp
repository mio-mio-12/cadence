#include "scene/C2MMap.h"
#include "gameplay/BotAreaPing.h"
#include "gameplay/BotMapPhysics.h"
#include "gameplay/BotMapRouting.h"
#include "gameplay/BotRouteProbeCache.h"
#include "gameplay/BotSpTraversal.h"
#include "gameplay/BotSpawnPolicy.h"
#include "gameplay/AuthoredTraversal.h"
#include <memory>
#include <cstdlib>
#include <iostream>
struct AppState {
 std::optional<scene::glb::Map> loadedMap;
 std::vector<gameplay::bot::Actor> bots;
 std::vector<gameplay::bot::BotMapRoute> botSpRoutes;
 struct Context {float coverSeek{};};std::vector<Context> botSpContextStates;
 std::size_t botRouteWorkTurn{};
 gameplay::bot::RouteProbeCache botRouteProbeCache;
 gameplay::bot::AreaPing botAreaPing;
 gameplay::bot::NavigationGraph navigation;
};
const auto& activeBotNavigation(const AppState&a){return a.navigation;}
#include "../src/app/BotMapSteering.inl"

int main(int argc,char**argv)try{
 if(argc<2)return 2;
 auto state=std::make_unique<AppState>();auto& a=*state;
 a.loadedMap.emplace();std::string error;
 if(!scene::c2m::load(argv[1],*a.loadedMap,error)){std::cerr<<error;return 3;}
 auto& map=*a.loadedMap;
 std::cout<<"MAP triangles="<<map.collision.size()<<" authored="<<map.authoredCollision.present<<std::endl;
 const scene::Vec3 goal{-265.2279f,1162.0034f,437.0056f};
 const scene::Vec3 starts[]={{-690.5220f,6495.5278f,325.1193f},{-1430.5879f,-6696.9355f,325.1212f},{2877.8936f,-3844.4771f,87.8323f}};
 const int count=argc>2?std::atoi(argv[2]):18;
 const std::string mode=argc>3?argv[3]:"mixed";
 const bool policy=argc>4&&std::string(argv[4])=="policy";
 gameplay::bot::BehaviorConfig config;config.spPlayerPolicy=true;config.spUseSceneVisibilityOnly=true;
 if(mode=="probe"){
  const scene::Vec3 start{-335.246f,1263.08f,326.338f},target{-265.228f,1162.f,437.041f};
  const float r=scene::course::kPlayerRadius,h=gameplay::iw::worldUnits(72);
  const auto sweep=[&](scene::Vec3 x,scene::Vec3 y){return gameplay::mantle::sweepRounded(map,x,y,r,h);};
  const auto up=sweep(start,{start.x,start.y,target.z});std::cout<<"UP "<<up.fraction<<" solid="<<up.startSolid<<std::endl;
  auto p=start;for(int i=1;i<=120;++i){const auto q=scene::lerp(start,scene::Vec3{start.x,start.y,target.z},float(i)/120);const auto hit=sweep(p,q);if(hit.fraction<.9999f||hit.startSolid){std::cout<<"STEP "<<i<<" z="<<p.z<<" next="<<q.z<<" fraction="<<hit.fraction<<" solid="<<hit.startSolid<<" normal="<<hit.normal.x<<","<<hit.normal.y<<","<<hit.normal.z<<std::endl;break;}p=q;}
  p=up.end;
  for(int i=0;i<int(map.collision.size());++i){const auto&t=map.collision[i];for(float z:{r,h*.5f,h-r}){const auto c=p+scene::Vec3{0,0,z},near=gameplay::mantle::closestOnTriangle(c,t.a,t.b,t.c);const float d=scene::length(c-near);if(d<r+1.f)std::cout<<"TRI "<<i<<" sample="<<z<<" distance="<<d<<" boundsz="<<t.minimum.z<<","<<t.maximum.z<<" closest="<<near.x<<","<<near.y<<","<<near.z<<std::endl;}}
  return 0;
 }
 for(int i=0;i<count;++i){
  auto pos=starts[mode=="north"?0:mode=="south"?1:mode=="barrels"?2:(i/9)%2];
  const int columns=mode=="mixed"||count<=9?3:6;
  const int lane=mode=="mixed"?i%9:i;pos.x+=(lane%columns-(columns-1)*.5f)*100.f;pos.y+=(lane/columns-1)*100.f;
  const auto project=[&](scene::Vec3 p)->std::optional<scene::Vec3>{const float g=map.navigationGroundHeight(p.x,p.y,p.z+46-45,-1e9f,scene::course::kPlayerRadius*.55f);if(g<-1e8f||std::abs(g-p.z)>scene::course::kStepHeight)return {};p.z=g;if(gameplay::mantle::sweepRounded(map,p,p,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72)).startSolid)return {};return p;};
  const auto occupied=[&](scene::Vec3 p){return std::any_of(a.bots.begin(),a.bots.end(),[&](const auto&b){return gameplay::bot::horizontalDistance(b.position,p)<scene::course::kPlayerRadius*2+8;});};
  if(auto clear=gameplay::bot::nearbyClearSpawn(pos,project,occupied))pos=*clear;else {std::cerr<<"No clear spawn "<<i<<std::endl;return 5;}
  gameplay::bot::Actor b;b.id=i+1;b.alive=true;b.position=b.spawnPosition=pos;b.spWantsMove=true;b.spMoveGoal=goal;b.spMoveThrottle=1;b.spDecisionSequence=1;b.yaw=std::atan2(goal.y-pos.y,goal.x-pos.x);
  std::cout<<"START "<<i<<" "<<pos.x<<","<<pos.y<<","<<pos.z<<" solid="<<gameplay::mantle::sweepRounded(map,pos,pos,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72)).startSolid<<std::endl;
  a.bots.push_back(b);
 }
 a.botSpRoutes.resize(a.bots.size());a.botSpContextStates.resize(a.bots.size());
 a.botAreaPing.set(goal,100,a.bots);
 constexpr float dt=1.f/120;std::vector<bool> arrived(count);int total=0;float maxGroundGap=0;
 for(int f=0;f<180*120;++f){
  a.botRouteWorkTurn=f%a.bots.size();
  for(int i=0;i<count;++i){auto&b=a.bots[i];if(arrived[i])continue;
   const float yawBeforePolicy=b.yaw;
   if(policy)gameplay::bot::updateBehavior(b,goal,config,dt,nullptr,map.lineOfSight(b.position+scene::Vec3{0,0,150},goal+scene::Vec3{0,0,150}));
   b.spWantsMove=true;b.spMoveGoal=goal;
   b.spMoveThrottle=1;b.spCombatMoving=b.targetVisible;
   steerSpBotOnMap(a,b,dt,yawBeforePolicy);
   if(b.mantling){b.mantleElapsed=std::min(b.mantleDuration,b.mantleElapsed+dt);const auto intended=gameplay::bot::spMantlePosition(b.mantleStart,b.mantleEnd,b.mantleElapsed/b.mantleDuration,b.mantleClearanceZ);const auto hit=gameplay::mantle::sweepRounded(map,b.position,intended,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72));const auto resolved=hit.end;if(scene::length(resolved-intended)>1)std::cout<<"ABORT "<<i<<" t="<<f*dt<<" phase="<<b.mantleElapsed/b.mantleDuration<<" solid="<<hit.startSolid<<" fraction="<<hit.fraction<<" start="<<b.mantleStart.x<<","<<b.mantleStart.y<<","<<b.mantleStart.z<<" end="<<b.mantleEnd.x<<","<<b.mantleEnd.y<<","<<b.mantleEnd.z<<" apex="<<b.mantleClearanceZ<<std::endl;b.position=resolved;b.velocity={};b.grounded=false;if(scene::length(resolved-intended)>1||b.mantleElapsed>=b.mantleDuration)b.mantling=false;}
   else {const float target=gameplay::bot::movementThrottleTarget(b.input);b.movementThrottle+=(target-b.movementThrottle)*(1-std::exp(-(target>b.movementThrottle?5.2f:8.f)*dt));gameplay::bot::stepOnMap(b,dt,policy?b.movementThrottle:1.f,map);}
   const float ground=map.navigationGroundHeight(b.position.x,b.position.y,b.position.z+.1f-45,-1e9f,scene::course::kPlayerRadius*.55f);
   if(b.grounded)maxGroundGap=std::max(maxGroundGap,std::abs(ground-b.position.z));
   if(!b.mantling&&b.grounded&&gameplay::bot::horizontalDistance(b.position,goal)<a.botAreaPing.arrivalRadius&&std::abs(b.position.z-goal.z)<65){arrived[i]=true;b.spWantsMove=false;b.input={};b.velocity={};++total;std::cout<<"ARRIVE "<<i<<" t="<<f*dt<<" pos="<<b.position.x<<","<<b.position.y<<","<<b.position.z<<std::endl;}
  }
  if(f%1200==0){std::cout<<"TIME "<<f*dt<<" arrived="<<total<<std::endl;for(int i=0;i<count;++i)if(!arrived[i]){auto&b=a.bots[i];auto&r=a.botSpRoutes[i];std::cout<<i<<" pos="<<b.position.x<<","<<b.position.y<<","<<b.position.z<<" route="<<int(r.status)<<" searches="<<r.searches<<" expanded="<<r.totalExpanded<<" nodes="<<r.nodes.size()<<" ground="<<b.grounded<<std::endl;}}
  if(f%1200==0)for(int i=0;i<count;++i)if(!arrived[i]){const auto& b=a.bots[i];const auto& r=a.botSpRoutes[i];if(r.cursor<r.path.size()){const auto p=r.path[r.cursor];std::cout<<"NEXT "<<i<<" "<<p.x<<","<<p.y<<","<<p.z<<" yaw="<<b.yaw<<" mantle="<<b.mantling<<" input="<<b.input.forward<<","<<b.input.right<<std::endl;}}
  if(f%1200==0&&f>=12000)for(int i=0;i<count;++i)if(!arrived[i]){auto&b=a.bots[i];auto&r=a.botSpRoutes[i];if(r.cursor<r.path.size()){const auto target=r.path[r.cursor];const auto climb=gameplay::mantle::findClimbTo(map,b.position,target,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight,gameplay::iw::worldUnits(57),gameplay::iw::worldUnits(42),gameplay::iw::worldUnits(24));std::cout<<"CLIMB "<<i<<" available="<<bool(climb)<<" stationarySolid="<<gameplay::mantle::sweepRounded(map,b.position,b.position,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72)).startSolid<<std::endl;}}
  if(total==count)break;
  a.botAreaPing.advance(dt,a.bots);
  if(a.botAreaPing.remaining<=0){std::cout<<"PING TIMEOUT"<<std::endl;break;}
 }
 std::cout<<"RESULT arrived="<<total<<"/"<<count<<" groundedGap="<<maxGroundGap<<std::endl;return total==count&&maxGroundGap<3?0:1;
}catch(const std::exception&e){std::cerr<<e.what();return 4;}
