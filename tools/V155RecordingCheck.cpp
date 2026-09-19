#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(){
 CHECK(glfwInit());
 const std::filesystem::path root="D:/Editing/COD Resource/3D Rip/saluki/exported_files/bo2";
 auto rig=scene::buildScene(cast::Document::load(root/"models/playermodels/isa/c_usa_mp_isa_assault_fb/c_usa_mp_isa_assault_fb_LOD0.cast"),false);CHECK(!rig.skeleton.bones.empty());
 for(const auto* folder:{"pb","pt"})for(const auto& file:std::filesystem::recursive_directory_iterator(root/"animations"/folder))
  if(file.is_regular_file()&&file.path().extension()==".cast"){const auto first=rig.animations.size();scene::appendAnimations(cast::Document::load(file.path()),rig);for(size_t i=first;i<rig.animations.size();++i)rig.animations[i].sourceGame="bo2";}
 CHECK(rig.animations.size()>100);
 // Selection and sampled results must match even when an action lacks a candidate.
 auto reference=std::make_unique<AppState>(),optimized=std::make_unique<AppState>();
 for(auto* a:{reference.get(),optimized.get()}){
  a->scene=rig;a->scene.animations.resize(1);a->hiddenWorldActor=rig;
  a->actorMode=true;a->gameplayLogic=true;a->playerWorldAnimationGame="bo2";a->actorGrounded=true;
  a->recordedTake.sampleRate=30;a->recordedTake.boneCount=rig.skeleton.bones.size();
  a->recordedTake.actorSlotBoneCounts={rig.skeleton.bones.size(),rig.skeleton.bones.size(),rig.skeleton.bones.size()};
  a->recordedTake.worldBoneCount=rig.skeleton.bones.size();a->botActorScene=rig;
  a->recordedTake.actor.baseModel=a->recordedTake.worldActor.baseModel=a->recordedTake.botActor.baseModel=(root/"models/playermodels/isa/c_usa_mp_isa_assault_fb/c_usa_mp_isa_assault_fb_LOD0.cast").string();
  for(size_t slot=0;slot<3;++slot){a->recordedTake.actorSlots[slot]=a->recordedTake.actor;a->recordedTake.worldActorSlots[slot]=a->recordedTake.worldActor;}
  a->recordedTake.botCount=16;a->recordedTake.botBoneCount=rig.skeleton.bones.size();a->bots.resize(16);
  a->botActorPoses.assign(16,rig.samplePose(0,0));a->takeRecording=true;
  for(size_t i=0;i<16;++i){a->bots[i].id=static_cast<unsigned>(i+1);a->bots[i].alive=true;a->bots[i].health=100;}
 }
 reference->worldSelectionCacheEnabled=false;
 auto drive=[](AppState& a,int frame){
  a.gameplayClock=frame/30.f;a.animationFrame=std::fmod(float(frame),10.f);
  const int phase=(frame/30)%12;
  a.actorVelocity=a.actorWishVelocity={phase?200.f:0.f,phase==3?130.f:0.f,0};
  a.actorMoveInputForward=phase?1.f:0.f;a.actorMoveInputSide=phase==3?1.f:0.f;
  a.actorSprinting=phase==2||phase==3;a.actorSliding=phase==7;
  a.actorGrounded=phase!=6;a.actorMantling=phase==8;
  a.actorMantleHeight=100;a.actorMantleDuration=1;a.actorMantleElapsed=(frame%30)/30.f;
  a.actorMantleStart={0,0,0};a.actorMantleEnd={100,0,100};
  a.gameplayStance=phase==4?scene::Stance::Crouch:phase==5?scene::Stance::Prone:scene::Stance::Stand;
  a.gameplayWeapon=phase==9?scene::WeaponClass::Sniper:scene::WeaponClass::Rifle;
  a.weaponProfile.meleeWeapon=phase==10;
  a.actionActive=phase==1||phase==11;a.activeAction=phase==11?scene::ActionRole::Reload:scene::ActionRole::Fire;a.actionFrame=float(frame%30);
  a.cameraPitch=phase==3?.3f:0.f;
  for(size_t i=0;i<a.botActorPoses.size();++i)for(auto& bone:a.botActorPoses[i])bone.v[12]=float(frame)+i;
 };
 for(int frame=0;frame<720;++frame)for(auto* a:{reference.get(),optimized.get()}){drive(*a,frame);captureTakeSample(*a,frame/30.f);}
 CHECK(reference->recordedTake.samples.size()==720&&optimized->recordedTake.samples.size()==720);
 const auto out=std::filesystem::path("diagnostics/v155");std::filesystem::create_directories(out);std::string error;
 CHECK(take::save(reference->recordedTake,out/"reference.casttake",error));CHECK(take::save(optimized->recordedTake,out/"optimized.casttake",error));
 const auto bytes=[](const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
 CHECK(bytes(out/"reference.casttake")==bytes(out/"optimized.casttake"));
 take::Take loaded;CHECK(take::load(out/"optimized.casttake",loaded,error));
 for(float t:{0.f,.125f,5.77f,23.9f,3.11f}){
  const auto a=reference->recordedTake.interpolatedSample(t),b=loaded.interpolatedSample(t);CHECK(a.pose.size()==b.pose.size()&&a.bots.size()==b.bots.size());
  for(size_t i=0;i<a.pose.size();++i)CHECK(a.pose[i].v==b.pose[i].v);
  CHECK(a.worldActor.pose.size()==b.worldActor.pose.size());for(size_t i=0;i<a.worldActor.pose.size();++i)CHECK(a.worldActor.pose[i].v==b.worldActor.pose[i].v);
  for(size_t bot=0;bot<a.bots.size();++bot)for(size_t i=0;i<a.bots[bot].pose.size();++i)CHECK(a.bots[bot].pose[i].v==b.bots[bot].pose[i].v);
 }
 for(auto* a:{reference.get(),optimized.get()}){const auto& c=a->takeRecordingCost;std::cout<<(a==reference.get()?"REFERENCE":"OPTIMIZED")<<" clips="<<rig.animations.size()<<" bones="<<rig.skeleton.bones.size()<<" samples="<<c.samples<<" mean="<<c.total/c.samples<<" peak="<<c.peak<<" view="<<c.sum[0]/c.samples<<" world="<<c.sum[1]/c.samples<<" bots="<<c.sum[2]/c.samples<<" append="<<c.sum[3]/c.samples<<" ms\n";}
 std::cout<<"16 bots / 720 samples: byte-identical serialized takes; fractional/backward replay pose parity PASS\n";
 glfwTerminate();
}
