#include "scene/ColdWarWorld.h"
#include <iostream>
int main(){int failures=0;auto check=[&](bool b,const char*m){if(!b){++failures;std::cerr<<m<<'\n';}};
 auto source=std::make_shared<scene::CastScene>();
 auto bone=[&](const char*name,int parent,scene::Vec3 p){scene::Bone b;b.name=name;b.parent=parent;b.restLocal.position=p;b.restGlobal=(parent<0?scene::Mat4::identity():source->skeleton.bones[parent].restGlobal)*scene::translation(p);b.inverseBind=scene::inverseAffine(b.restGlobal);const auto i=source->skeleton.bones.size();source->skeleton.boneByName[name]=source->skeleton.boneByCanonicalName[name]=i;source->skeleton.bones.push_back(b);};
 bone("tag_origin",-1,{});bone("j_mainroot",0,{0,0,90});bone("j_spinelower",1,{0,0,20});
 source->skeleton.boneByCanonicalName["pelvis"]=1;
 scene::Animation clip;clip.durationFrames=10;scene::Track t;t.boneIndex=1;t.property=scene::TrackProperty::TranslationZ;t.frames={0,10};t.scalarValues={90,50};clip.tracks.push_back(t);source->animations.push_back(clip);
 auto target=source->skeleton;auto helper=target.bones[1];helper.name="pelvis";helper.parent=1;helper.restLocal.position={};target.bones.insert(target.bones.begin()+2,helper);target.bones[3].parent=2;target.boneByCanonicalName["pelvis"]=2;
 auto adapter=scene::makeColdWarWorldPose(target,source,0);check(bool(adapter),"create immutable pair adapter");
 auto first=adapter->sample(target,0),last=adapter->sample(target,10);check(std::abs(first[1].position.z-90)<.001,"bind pose preserved");check(std::abs(last[1].position.z-50)<.001,"authored pelvis motion transferred");check(scene::length(last[2].position)<.001,"pelvis helper does not duplicate translation");
 auto repeated=adapter->sample(target,10);for(size_t i=0;i<last.size();++i)check(scene::length(last[i].position-repeated[i].position)<.0001,"sampling deterministic");
 target.bones.push_back(helper);target.bones.back().parent=3;check(adapter->sample(target,5).size()==target.bones.size(),"later modular bones inherit pose safely");
 scene::Skeleton hashed;auto b=helper;b.name=scene::nativeBoneHash("j_knee_le");hashed.bones.push_back(b);hashed.boneByName[b.name]=0;scene::addColdWarWorldAliases(hashed);check(hashed.boneByCanonicalName.contains("j_knee_le"),"exact native hash identity");
 check(!scene::makeColdWarWorldPose(target,source,99),"invalid animation rejected");
 // A torso layer must consume the blended locomotion pose, not resample
 // the incoming slide/sprint and erase the transition underneath it.
 scene::Animation torso;scene::Track upper;upper.boneIndex=2;upper.property=scene::TrackProperty::TranslationX;upper.frames={0};upper.scalarValues={7};torso.tracks.push_back(upper);source->animations.push_back(torso);
 for(float blend:{0.f,.25f,.5f,.75f,1.f}){
  const auto local=source->sampleBlendedLocalPose(0,0,0,10,blend);
  const auto expected=source->globalPose(local);
  const auto layered=source->sampleLayeredPose(0,10,1,0,1,scene::LayerMode::Override,true,&local);
  for(int k=0;k<16;++k)check(std::abs(layered[1].v[k]-expected[1].v[k])<.0001f,"torso keeps blended pelvis, including endpoints");
  check(std::abs(layered[2].v[12]-7)<.0001f,"torso channel remains active over blend");
  const auto wrapper=source->sampleBlendedPose(0,0,0,10,blend);check(wrapper[1].v==expected[1].v,"existing blended pose behavior unchanged");
 }
 // Socket origins belong to the native weapon contract. Legacy bind socket
 // offsets (often ~30 cm) must not displace a mounted gun during a CW slide.
 auto mountSource=std::make_shared<scene::CastScene>();
 for(int i=0;i<2;++i){scene::Bone b;b.name=i?"tag_weapon_right":"j_wrist_ri";b.parent=i?0:-1;b.restLocal.position=i?scene::Vec3{6,1,2}:scene::Vec3{10,20,30};b.restGlobal=(i?mountSource->skeleton.bones[0].restGlobal:scene::Mat4::identity())*scene::translation(b.restLocal.position);b.inverseBind=scene::inverseAffine(b.restGlobal);mountSource->skeleton.boneByCanonicalName[b.name]=i;mountSource->skeleton.bones.push_back(b);}
 scene::Animation moving;moving.durationFrames=10;scene::Track socket;socket.boneIndex=1;socket.property=scene::TrackProperty::TranslationX;socket.mode=scene::TrackMode::Absolute;socket.frames={0,10};socket.scalarValues={6,8};moving.tracks.push_back(socket);mountSource->animations.push_back(moving);
 auto mountTarget=mountSource->skeleton;mountTarget.bones[1].restLocal.position={-19,13,-18};mountTarget.bones[1].restGlobal=mountTarget.bones[0].restGlobal*scene::translation(mountTarget.bones[1].restLocal.position);
 auto mountAdapter=scene::makeColdWarWorldPose(mountTarget,mountSource,0);
 scene::CastScene fitted;fitted.skeleton=mountTarget;
 for(float frame:{0.f,2.5f,5.f,7.5f,10.f}){const auto pose=fitted.globalPose(mountAdapter->sample(mountTarget,frame));const auto native=mountSource->samplePose(0,frame);for(int k=0;k<16;++k)check(std::abs(pose[1].v[k]-native[1].v[k])<.001f,"native weapon socket preserved independent of legacy bind offset");}
 // Compare prepared and original fallback operations, including fractional and
 // reverse-seek samples. No reordering of affine multiplications is permitted.
 auto parity=[&](scene::ColdWarWorldPose prepared,const scene::Skeleton& skeleton){
  prepared.prepareSampling(skeleton);auto baseline=prepared;
  baseline.boneFlags.clear();baseline.translationMatrices.clear();baseline.sourceActionInverse.clear();
  scene::CastScene scene;scene.skeleton=skeleton;
  for(float f:{0.f,2.25f,9.75f,4.5f,10.f,0.f}){
   const auto a=scene.globalPose(prepared.sample(skeleton,f)),b=scene.globalPose(baseline.sample(skeleton,f));
   for(size_t i=0;i<a.size();++i)for(int k=0;k<16;++k)check(a[i].v[k]==b[i].v[k],"prepared adapter exact matrix parity");
  }
 };
 parity(*adapter,target);parity(*mountAdapter,mountTarget);
 auto action=*mountAdapter;action.sourceActionReference=mountSource->samplePose(0,0);
 action.targetActionReference=fitted.globalPose(action.targetReference);action.globalTranslation.assign(action.targetReference.size(),true);
 action.actionBasis=scene::fromEulerRadians({0,0,.73f});parity(action,mountTarget);
 return failures?1:0;
}
