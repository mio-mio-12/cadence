#include "app/PointBlankAnimationPolicy.h"
#include "scene/PointBlankNative.h"
#include "scene/VolumePreservingFit.h"
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed line "<<__LINE__<<": "<<#x;std::abort();}}while(false)
#include "scene/GripLandmarks.h"
int main(){
 {scene::Animation a;scene::classifyAnimationName("pb_stand_alert_akimbo.cast",a);CHECK(a.weapon==scene::WeaponClass::DualWield);
 scene::classifyAnimationName("mp_akimbo_crouch_aim_1.cast",a);CHECK(a.weapon==scene::WeaponClass::DualWield&&a.contextual);
 scene::classifyAnimationName("pb_laststand_idle.cast",a);CHECK(a.contextual);
 scene::classifyAnimationName("mp_akimbo_crouch_wpnswp_lg2pistol.cast",a);CHECK(a.contextual);
 scene::classifyAnimationName("mp_stand_reload.cast",a);CHECK(a.domain==scene::AnimationDomain::PlayerTorso&&a.action==scene::ActionRole::Reload&&a.weapon==scene::WeaponClass::Rifle);
 scene::classifyAnimationName("mp_pistol_stand_reload.cast",a);CHECK(a.domain==scene::AnimationDomain::PlayerTorso&&a.weapon==scene::WeaponClass::Pistol);
 scene::classifyAnimationName("mp_pistol_crouch_reload.cast",a);CHECK(a.domain==scene::AnimationDomain::PlayerTorso&&a.weapon==scene::WeaponClass::Pistol&&a.stance==scene::Stance::Crouch);
 scene::classifyAnimationName("mp_akimbo_crouch_fire.cast",a);CHECK(a.domain==scene::AnimationDomain::PlayerTorso&&a.weapon==scene::WeaponClass::DualWield&&a.stance==scene::Stance::Crouch);
 scene::classifyAnimationName("mp_rpg_crouch_knife_hold.cast",a);CHECK(a.contextual);
 scene::classifyAnimationName("mp_bomb_stand_idle.cast",a);CHECK(a.contextual);
 a.sourceName="pt_SWAT_Male_Chara_SWAT_Male_3PV_DualHandGun_Common_Up_AttackIdle_Left.cast";scene::pointblank::classifyPlayer(a);CHECK(a.weapon==scene::WeaponClass::DualWield);
 a.sourceName="pt_SWAT_Male_handgun_Common_Up_AttackIdle.cast";scene::pointblank::classifyPlayer(a);CHECK(a.weapon==scene::WeaponClass::Pistol);
 a.sourceName="pt_SWAT_Male_knife_Common_Up_Attack.cast";scene::pointblank::classifyPlayer(a);CHECK(a.weapon==scene::WeaponClass::Knife&&a.action==scene::ActionRole::Melee);}
 {std::vector<scene::Vec3> from{{0,0,0},{2,1,0},{0,3,1},{-1,1,2},{1,-2,0}},to;
 const auto expected=scene::trs({-2,1,3},scene::normalize(scene::Quat{.2f,.4f,-.1f,.8f}),{1,1,1});for(auto p:from)to.push_back(scene::transformPoint(expected,p));auto fit=scene::fitGripLandmarks(from,to);CHECK(fit);for(size_t i=0;i<from.size();++i)CHECK(scene::length(scene::transformPoint(*fit,from[i])-to[i])<.0001f);
 CHECK(!scene::fitGripLandmarks({{0,0,0},{1,0,0},{2,0,0},{3,0,0}},{{0,0,0},{1,0,0},{2,0,0},{3,0,0}}));
 for(bool pistol:{false,true}){const auto m=scene::pointBlankGripSurfaceCalibration(pistol);auto a=scene::transformPoint(m,{1,2,3}),b=scene::transformPoint(m,{4,-2,1});CHECK(std::abs(scene::length(a-b)-scene::length(scene::Vec3{1,2,3}-scene::Vec3{4,-2,1}))<.0001f);}}
 scene::CastScene s;s.pointBlankWeaponStem="viewmodel_sniper_test";s.skeleton.bones.resize(1);
 auto add=[&](const char*n,int duration){scene::Animation a;a.sourceName=s.pointBlankWeaponStem+"_"+n+".cast";a.durationFrames=duration;a.framerate=30;scene::Track t;t.frames={0,static_cast<unsigned>(duration)};t.scalarValues={0,1};a.tracks={t};s.animations.push_back(a);};
 add("AttackIdle",60);add("Attack",15);add("Reload",90);add("ReloadC",40);add("Change",25);add("JumpStart_AttackIdle",21);add("JumpEnd_AttackIdle",21);add("Weapon _ Reload_Male_3PV",90);
 cadence::pointblank_actions::prepare(s);
 CHECK(s.animations[0].looping);CHECK(!s.animations[5].looping&&!s.animations[6].looping);CHECK(s.animations[1].action==scene::ActionRole::Fire);
 weapon::Profile p;p.archetype=weapon::Archetype::BoltSniper;cadence::pointblank_actions::populate(s,p,s.pointBlankWeaponStem);cadence::pointblank_actions::defaults(s,p);
 CHECK(p.animations.at("rechamber").ends_with("_ReloadC.cast"));CHECK(p.animations.at("reload").ends_with("_Reload.cast"));CHECK(p.animations.at("ads_fire")==p.animations.at("fire"));CHECK(!p.animations.contains("ads_down"));CHECK(!p.animations.contains("inspect"));CHECK(!p.animations.contains("sprint_loop"));CHECK(p.stats.adsIn==0&&p.stats.adsOut==0&&p.stats.boltAction&&p.stats.hideWeaponOnAds);CHECK(p.animations.at("jump_land").ends_with("_JumpEnd_AttackIdle.cast"));
 scene::CastScene units;units.meshes.resize(1);units.meshes[0].vertices.resize(1);units.meshes[0].vertices[0].position={1,2,3};units.meshes[0].vertices[0].uv={.2f,.3f};units.skeleton.bones.resize(1);units.skeleton.bones[0].restLocal.position={0,0,1.65f};scene::pointblank::normalize(units);CHECK(units.meshes[0].vertices[0].position.x==100);CHECK(std::abs(units.meshes[0].vertices[0].uv.y-.7f)<1e-6f);scene::pointblank::normalize(units);CHECK(units.meshes[0].vertices[0].position.x==100);CHECK(std::abs(units.skeleton.bones[0].restLocal.position.z-165)<1e-4f);CHECK(!units.codmNativeCentimetres);
 s.animations.clear();s.pointBlankWeaponStem="viewmodel_knife_test";add("AttackIdle",60);add("Attack_A",33);add("Attack_B",44);add("Attack_Stab",30);cadence::pointblank_actions::prepare(s);
 weapon::Profile knife;knife.meleeWeapon=true;cadence::pointblank_actions::populate(s,knife,s.pointBlankWeaponStem);cadence::pointblank_actions::defaults(s,knife);
 CHECK(knife.stats.meleeTime==0);CHECK(knife.animationVariants.at("melee").size()==3);CHECK(s.animations[1].action==scene::ActionRole::Melee&&!s.animations[1].looping);CHECK(s.animations[2].durationFrames==44);
 std::array<uint32_t,4> ids{0,1,0,0};std::array<float,4> weights{.5f,.5f,0,0};std::vector<scene::volume_fit::Bone> fits;
 for(float angle:{-80.f,80.f})fits.push_back(scene::volume_fit::prepare(scene::rotation(scene::fromAxisAngle({0,0,1},angle*scene::kPi/180)),{}));
 auto preserved=scene::volume_fit::position(fits,{1,0,0},ids,weights);CHECK(std::abs(scene::length(preserved)-1)<1e-5f);
 const auto transform=scene::trs({7,2,-4},scene::fromEulerRadians({.3f,.9f,-.4f}),{.6f,1.5f,2});fits={scene::volume_fit::prepare(transform,{1,2,3})};ids={0,0,0,0};weights={1,0,0,0};scene::Mat4 differential;
 auto single=scene::volume_fit::position(fits,{4,5,6},ids,weights,&differential);CHECK(scene::length(single-scene::transformPoint(transform,{4,5,6}))<1e-4f);for(float v:differential.v)CHECK(std::isfinite(v));
 scene::CastScene muzzleScene;muzzleScene.pointBlankNativeCentimetres=true;muzzleScene.skeleton.boneByName["pb2cast_weapon__FXDummy"]=0;
 std::vector<scene::Mat4> muzzlePose{scene::trs({12,95,147},scene::fromEulerRadians({.2f,.4f,.7f}),{1,1,1})};
 auto barrel=scene::resolveMuzzlePosition(muzzleScene,muzzlePose);CHECK(barrel&&scene::length(*barrel-scene::Vec3{12,95,147})<1e-5f);
 muzzlePose[0].v[13]+=31;barrel=scene::resolveMuzzlePosition(muzzleScene,muzzlePose);CHECK(barrel&&barrel->y==126);
 CHECK(!scene::resolveMuzzlePosition(muzzleScene,{}));
 muzzleScene.skeleton.bones.resize(2);muzzleScene.skeleton.bones[0].parent=1;
 muzzleScene.skeleton.bones[0].restGlobal=scene::trs({0,120,5},{0,0,0,1},{1,1,1});muzzleScene.skeleton.bones[1].restGlobal=scene::Mat4::identity();
 muzzlePose={scene::trs({0,90,5},{0,0,0,1},{1,1,1}),scene::trs({10,20,30},{0,0,0,1},{1,1,1})};
 barrel=scene::resolveMuzzlePosition(muzzleScene,muzzlePose);CHECK(barrel&&scene::length(*barrel-scene::Vec3{10,140,35})<1e-5f);
 scene::Animation world;world.sourceName="pt_SWAT_Female_SWAT-Sniper_assultrifle_Common_Up_Attack.cast";scene::pointblank::classifyPlayer(world);CHECK(world.weapon==scene::WeaponClass::Rifle&&world.action==scene::ActionRole::Fire);
 world.sourceName="pt_SWAT_Female_SWAT-Sniper_handgun_Common_Up_Move_AttackIdle.cast";scene::pointblank::classifyPlayer(world);CHECK(world.weapon==scene::WeaponClass::Pistol&&world.motion==scene::MotionRole::Run&&world.action==scene::ActionRole::None);
 CHECK(scene::pointblank::worldSemantic("R Hand")=="j_wrist_ri");CHECK(scene::pointblank::worldSemantic("L Thigh")=="j_hip_le");
 CHECK(scene::pointblank::worldSemantic("R Index1")=="j_index_ri_1");CHECK(scene::pointblank::worldSemantic("L Thumb3")=="j_thumb_le_3");
 world.sourceName="pt_SWAT_Male_death-scythe_Common_Up_Move_AttackIdle.cast";scene::pointblank::classifyPlayer(world);CHECK(world.action==scene::ActionRole::None&&world.motion==scene::MotionRole::Run);
 world.sourceName="pt_SWAT_Female_SWAT-Sniper_death-scythe_Death-Scythe_Up_Change.cast";scene::pointblank::classifyPlayer(world);CHECK(world.action==scene::ActionRole::Equip);
 world.sourceName="pb_SWAT_Female_SWAT-Sniper_common_death_Stand_DeathFrontA1.cast";scene::pointblank::classifyPlayer(world);CHECK(world.action==scene::ActionRole::Death&&world.weapon==scene::WeaponClass::Any&&!world.looping);
 {
  auto src=std::make_shared<scene::CastScene>();src->skeleton.bones.resize(3);
  for(int i=0;i<3;++i){auto&b=src->skeleton.bones[i];b.name=i==0?"tag_origin":i==1?"j_mainroot":"pelvis";b.parent=i-1;if(i==1)b.restLocal.position.z=90;b.restGlobal=scene::translation({0,0,i?90.f:0.f});src->skeleton.boneByName[b.name]=i;src->skeleton.boneByCanonicalName[b.name]=i;}
  scene::Animation clip;scene::Track z;z.boneIndex=1;z.property=scene::TrackProperty::TranslationZ;z.frames={0};z.scalarValues={45};clip.tracks.push_back(z);
  scene::Track r;r.boneIndex=2;r.property=scene::TrackProperty::Rotation;r.frames={0};r.rotationValues={scene::fromEulerRadians({0,scene::kPi*.5f,0})};clip.tracks.push_back(r);src->animations.push_back(clip);
  scene::CastScene dst;dst.skeleton.bones.resize(2);dst.skeleton.bones[0].name="Root";dst.skeleton.bones[0].parent=-1;dst.skeleton.bones[0].restLocal.position.z=100;dst.skeleton.bones[1].name="Pelvis";dst.skeleton.bones[1].parent=0;for(auto&b:dst.skeleton.bones)b.restGlobal=scene::translation({0,0,100});dst.animations.resize(1);
  scene::pointblank::bridgeWorld(dst,0,src,0);const auto pose=dst.samplePose(0,0);CHECK(std::abs(pose[1].v[14]-55)<.001f);CHECK(std::abs(pose[1].v[0])<.001f);
  // Blending must include the upstream mainroot translation, not jump 45cm
  // when the blend completes because the source pelvis has only rotation.
  dst.animations.push_back({});
  for(float w:{0.f,.25f,.5f,.75f,1.f}){auto blend=dst.samplePoseSlots(1,0,{{{{0,0,w,scene::LayerMode::Override,false}}}});CHECK(std::abs(blend[1].v[14]-(100-45*w))<.001f);}
  // CODM has distinct root and pelvis joints. Their native names must not
  // suppress the translated root channel when an animation is blended.
  scene::CastScene codm;codm.skeleton=src->skeleton;
  codm.skeleton.bones[0].name="b_Root";codm.skeleton.bones[1].name="Bip01";codm.skeleton.bones[2].name="b_Hips";codm.animations.resize(1);
  scene::pointblank::bridgeWorld(codm,0,src,0);codm.animations.push_back({});
  for(float w:{0.f,.25f,.5f,.75f,1.f}){auto blend=codm.samplePoseSlots(1,0,{{{{0,0,w,scene::LayerMode::Override,false}}}});CHECK(std::abs(blend[1].v[14]-(90-45*w))<.001f);}
  CHECK(scene::codm::worldSemantic("b_RightFinger2")=="j_mid_ri_1");
  CHECK(scene::codm::worldSemantic("b_RightFinger21")=="j_mid_ri_2");
  CHECK(scene::codm::worldSemantic("b_RightHand__bind_abc")=="b_RightHand__bind_abc");
 }
 scene::Skeleton accessoryRig;accessoryRig.bones.resize(2);accessoryRig.bones[0].name="Root";accessoryRig.bones[1].name="Spine3";
 {
  scene::Mesh parked;parked.name="Arbitrary_Hair";parked.skinned=true;parked.vertices.resize(2);
  for(auto& v:parked.vertices){v.position={0,0,250};v.bones={0,0,0,0};v.weights={1,0,0,0};}
  CHECK(scene::pointblank::detachedPresentationMesh(parked,accessoryRig,200));
  for(const char* name:{"A_R_Kopassus01_175","A_R_Recon_195","A_B_Kopassus_210","A_B_Recon_210","Bella_Equip_185","Equipments_007_162","O_R_Tarantula_Ori_160"}){
   auto prop=parked;prop.name=name;CHECK(scene::pointblank::detachedPresentationMesh(prop,accessoryRig));
   prop.vertices[0].bones[0]=1;CHECK(!scene::pointblank::detachedPresentationMesh(prop,accessoryRig));
  }
  parked.vertices[1].position.z=180;CHECK(!scene::pointblank::detachedPresentationMesh(parked,accessoryRig,200));
  parked.vertices[1].position.z=250;parked.vertices[1].bones[0]=1;CHECK(!scene::pointblank::detachedPresentationMesh(parked,accessoryRig,200));
  parked.vertices[1].bones[0]=0;CHECK(!scene::pointblank::detachedPresentationMesh(parked,accessoryRig));
  parked.modelTransform=scene::translation({0,0,-100});CHECK(!scene::pointblank::detachedPresentationMesh(parked,accessoryRig,200));
 }
 scene::Mesh accessory;accessory.name="Model_Clan_176";accessory.skinned=true;accessory.vertices.resize(1);accessory.vertices[0].weights={1,0,0,0};accessory.vertices[0].bones={0,0,0,0};CHECK(scene::pointblank::detachedPresentationMesh(accessory,accessoryRig));accessory.vertices[0].bones[0]=1;CHECK(!scene::pointblank::detachedPresentationMesh(accessory,accessoryRig));accessory.vertices[0].bones[0]=0;accessory.name="Model_Head_142";CHECK(!scene::pointblank::detachedPresentationMesh(accessory,accessoryRig));
 {
  auto source=std::make_shared<scene::CastScene>();scene::Bone root;root.name="tag_origin";root.restGlobal=scene::Mat4::identity();root.inverseBind=scene::Mat4::identity();source->skeleton.bones.push_back(root);source->skeleton.boneByCanonicalName[root.name]=0;scene::Animation death;death.durationFrames=100;death.framerate=30;source->animations.push_back(death);
  auto adapter=std::make_shared<scene::ColdWarWorldPose>(*scene::makeColdWarWorldPose(source->skeleton,source,0));adapter->deathGroundLift=10;
  CHECK(std::abs(adapter->sample(source->skeleton,0)[0].position.z)<.001f);
  CHECK(std::abs(adapter->sample(source->skeleton,100)[0].position.z-10)<.001f);
  CHECK(adapter->sample(source->skeleton,70)[0].position.z>0&&adapter->sample(source->skeleton,70)[0].position.z<10);
  CHECK(source->skeleton.bones[0].restLocal.position.z==0);
 }
 {
  // Foreign body rotations must be evaluated in donor space, not copied
  // into an AW-style differently oriented bind hierarchy.
  auto source=std::make_shared<scene::CastScene>();
  for(int i=0;i<3;++i){scene::Bone b;b.name=i==0?"tag_origin":i==1?"j_spineupper":"j_shoulder_ri";b.parent=i-1;b.restLocal.position={0,0,i?30.f:0.f};b.restGlobal=(i?source->skeleton.bones[i-1].restGlobal:scene::Mat4::identity())*scene::translation(b.restLocal.position);b.inverseBind=scene::inverseAffine(b.restGlobal);source->skeleton.boneByCanonicalName[b.name]=i;source->skeleton.bones.push_back(b);}
  scene::Animation clip;clip.durationFrames=20;clip.domain=scene::AnimationDomain::PlayerTorso;scene::Track t;t.boneIndex=1;t.property=scene::TrackProperty::Rotation;t.frames={0,20};t.rotationValues={{0,0,0,1},scene::fromEulerRadians({.4f,.8f,-.3f})};clip.tracks.push_back(t);source->animations.push_back(clip);
  scene::CastScene target;target.skeleton=source->skeleton;
  target.skeleton.bones[1].restLocal.rotation=scene::fromEulerRadians({0,0,scene::kPi});
  for(size_t i=0;i<3;++i){auto&b=target.skeleton.bones[i];b.restGlobal=(i?target.skeleton.bones[i-1].restGlobal:scene::Mat4::identity())*scene::trs(b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);b.inverseBind=scene::inverseAffine(b.restGlobal);b.name=scene::nativeBoneHash(b.name);}
  target.animations.resize(1);scene::pointblank::bridgeWorld(target,0,source,0);
  CHECK(target.animations[0].coldWarWorldPose->sourceBones[1]==1);
  CHECK(std::any_of(target.animations[0].tracks.begin(),target.animations[0].tracks.end(),[](const auto&t){return t.boneIndex==2&&t.ownsLayer;}));
  for(float frame:{0.f,2.5f,10.f,20.f,4.f}){const auto a=target.samplePose(0,frame);const auto b=source->samplePose(0,frame);const auto expected=b[1]*scene::inverseAffine(source->skeleton.bones[1].restGlobal)*target.skeleton.bones[1].restGlobal;for(int k=0;k<12;++k)CHECK(std::abs(a[1].v[k]-expected.v[k])<.001f);}
 }
 std::cout<<"Point Blank knife policy, units/UV, barrel sockets and volume-preserving calibration passed\n";
}
