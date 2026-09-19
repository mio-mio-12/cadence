#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(){
 auto state=std::make_unique<AppState>();auto& app=*state;
 // Actual selection path: overrides, replacement, removal, fallback game.
 scene::CastScene actor;
 for(const char* name:{"pb_combatrun_forward_loop","custom_run_a","custom_run_b"}){
  scene::Animation a;scene::classifyAnimationName(name,a);a.name=name;a.sourceName=name;a.sourceGame="bo2";a.tracks.resize(1);actor.animations.push_back(a);
 }
 scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Run;q.stance=scene::Stance::Stand;q.direction=scene::Direction::Forward;q.weapon=scene::WeaponClass::Rifle;
 AppState::BotLocomotionOverrideKey key{"bo2",q.motion,q.stance,q.direction,q.weapon};
 resetBotLocomotionCache();const auto automatic=botLocomotionAnimation(actor,q,&app);
 app.botLocomotionOverrides[key]="custom_run_a";++app.botLocomotionOverrideRevision;
 CHECK(botLocomotionAnimation(actor,q,&app)==1);const auto misses=g_botLocoCache.misses;
 for(int i=0;i<1000;++i)CHECK(botLocomotionAnimation(actor,q,&app)==1);
 CHECK(g_botLocoCache.misses==misses&&g_botLocoCache.hits>=1000);
 app.botLocomotionOverrides[key]="custom_run_b";++app.botLocomotionOverrideRevision;
 CHECK(botLocomotionAnimation(actor,q,&app)==2);
 app.botLocomotionOverrides.clear();++app.botLocomotionOverrideRevision;
 CHECK(botLocomotionAnimation(actor,q,&app)==automatic);
 std::cout<<"Override cache: 1000 hits; replace/remove parity PASS\n";
 std::string error;app.defaultSalukiDirectory="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
 for(auto game:{"pointblank","bo2"})CHECK(assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error));
 auto asset=[&](const std::string& name)->const assets::Asset*{for(const auto& a:app.assetCatalog.entries)if(a.name==name)return &a;return nullptr;};
 const auto* body=asset("c_usa_mp_isa_smg_fb_LOD0");CHECK(body);
 const auto target=scene::buildScene(cast::Document::load(body->path),false).skeleton;
 const auto bone=pointBlankWorldGripBone(target);CHECK(bone>=0);
 for(auto name:{"viewmodel_pistol_ColtPython","viewmodel_AR_AK-47_DualMag","viewmodel_AR_SC-2010"}){
  const auto* weapon=asset(name);CHECK(weapon);const auto doc=cast::Document::load(weapon->path);CHECK(doc.valid());
  const auto cold=preparePointBlankWorldWeapon(app,doc,target,bone);CHECK(!cold.meshes.empty());
  PointBlankPreparationBatch batch;const auto begin=std::chrono::steady_clock::now();
  const auto first=preparePointBlankWorldWeapon(app,doc,target,bone);
  const auto mid=std::chrono::steady_clock::now();
  const auto cached=preparePointBlankWorldWeapon(app,doc,target,bone);
  const auto end=std::chrono::steady_clock::now();
  CHECK(batch.cache.weapons.size()==1&&cold.meshes.size()==cached.meshes.size());
  for(size_t m=0;m<cold.meshes.size();++m){
   CHECK(cold.meshes[m].vertices.size()==cached.meshes[m].vertices.size());
   for(size_t v=0;v<cold.meshes[m].vertices.size();++v){
    const auto&a=cold.meshes[m].vertices[v];const auto&b=cached.meshes[m].vertices[v];
    CHECK(a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z);
    CHECK(a.normal.x==b.normal.x&&a.normal.y==b.normal.y&&a.normal.z==b.normal.z);
   }
  }
  CHECK(cold.skeleton.bones[0].restGlobal.v==cached.skeleton.bones[0].restGlobal.v);
  auto altered=target;altered.bones[bone].restGlobal=altered.bones[bone].restGlobal*scene::rotation(scene::fromEulerRadians({0,0,.1f}));
  preparePointBlankWorldWeapon(app,doc,altered,bone);CHECK(batch.cache.weapons.size()==2);
  std::cout<<name<<" exact vertex/normal/muzzle parity PASS; first "<<std::chrono::duration<double,std::milli>(mid-begin).count()<<" ms; cached "<<std::chrono::duration<double,std::milli>(end-mid).count()<<" ms\n";
 }
 CHECK(activePointBlankPreparation==nullptr);
 {PointBlankPreparationBatch fresh;CHECK(fresh.cache.documents.empty()&&fresh.cache.weapons.empty()&&fresh.cache.clips.empty());}
 std::cout<<"Batch invalidation and target-frame separation PASS\n";
}
