#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(){
 for(int mode:{0,1}){
  auto state=std::make_unique<AppState>();auto& a=*state;a.loadedMap.emplace();auto& map=*a.loadedMap;
  for(auto points:{std::array<scene::Vec3,3>{{{-5000,-5000,0},{5000,-5000,0},{5000,5000,0}}},std::array<scene::Vec3,3>{{{-5000,-5000,0},{5000,5000,0},{-5000,5000,0}}}}){scene::glb::CollisionTriangle t;t.a=points[0];t.b=points[1];t.c=points[2];t.normal={0,0,1};t.minimum={-5000,-5000,0};t.maximum={5000,5000,0};t.walkable=t.blocking=true;map.collision.push_back(t);}map.buildCollisionIndex();
  a.botActorScene.emplace();auto& actor=*a.botActorScene;actor.skeleton.bones.resize(1);actor.skeleton.bones[0].name="tag_origin";actor.skeleton.boneByName["tag_origin"]=0;actor.skeleton.boneByCanonicalName["tag_origin"]=0;
  a.bots.resize(5);for(size_t i=0;i<5;++i){auto& b=a.bots[i];b.id=static_cast<unsigned>(i+1);b.alive=true;b.grounded=true;b.position={-500.f,static_cast<float>(i)*180.f-360.f,0};b.spawnPosition=b.position;}
  a.activeBotSystemMode=mode;a.botSystemMode=mode;a.experimentalBotAreaPing=true;a.botAreaPingPercent=100;a.actorPosition={0,0,0};a.actorViewHeight=170;a.actorYaw=0;a.cameraPitch=-.4f;a.botAllowEquipment=false;a.botAllowJumpDrop=false;a.botSpJumpChance=0;a.botSpIdleChance=a.botSpPainChance=0;
  requestBotAreaPing(a);CHECK(a.botAreaPing.recipients.size()==5);CHECK(a.botAreaPing.point.x>300&&a.botAreaPing.point.x<500);auto point=a.botAreaPing.point;
  a.actorPosition={-4000,-4000,0};std::array<float,5> closest;closest.fill(1e9f);
  for(int f=0;f<900;++f){const auto prior=a.botAreaPing.recipients;updateBotActors(a,1.f/60);for(size_t i=0;i<5;++i){closest[i]=std::min(closest[i],gameplay::bot::horizontalDistance(a.bots[i].position,point));if(std::find(prior.begin(),prior.end(),a.bots[i].id)!=prior.end()&&!a.botAreaPing.contains(a.bots[i].id))std::cout<<"released mode="<<mode<<" bot="<<i<<" tick="<<f<<" distance="<<closest[i]<<" z="<<a.bots[i].position.z<<"\n";}}
  for(auto d:closest){std::cout<<"mode="<<mode<<" closest="<<d<<"\n";CHECK(d<110.f);}
  a.cameraPitch=.4f;const auto before=a.botAreaPing.sequence;requestBotAreaPing(a);CHECK(a.botAreaPing.sequence==before);
  a.experimentalBotAreaPing=false;updateBotActors(a,.016f);CHECK(a.botAreaPing.recipients.empty());
 }
 std::cout<<"v144 Classic/SP map routing and ping request checks passed\n";
}
