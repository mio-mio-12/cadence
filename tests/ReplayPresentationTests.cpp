#include "app/ReplayPresentation.h"
#include "render/ForegroundDepth.h"
#include "app/ReplayRecoil.h"
#include "render/ReplaySmoke.h"
#include "render/DepthOfField.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x;std::abort();}}while(false)
int main(){
 using cadence::replay::presentationFov;
 {
  weapon::Profile profile;profile.gunPosition={3,4,5};profile.adsGunPosition={8,9,10};profile.separateAdsPosition=true;
  const auto reference=cadence::replay::MountReference::from(profile);
  std::vector<scene::Mat4> original{scene::translation({100,200,300}),scene::translation({110,210,310})};
  for(float ads:{0.f,.25f,.5f,1.f}){
   auto pose=original;cadence::replay::applyMountEdit(pose,0,reference,profile,ads);
   CHECK(pose[1].v==original[1].v); // Never double-apply a nonzero recorded mount.
   auto edited=profile;edited.gunPosition.x+=8;edited.adsGunPosition.x+=4;
   cadence::replay::applyMountEdit(pose,0,reference,edited,ads);
   CHECK(pose[0].v==original[0].v);
   const float expected=8-4*ads*ads*(3-2*ads);
   CHECK(std::abs(pose[1].v[12]-original[1].v[12]-expected)<.0001f);
   CHECK(pose[1].v[13]==original[1].v[13]);
  }
  auto pose=original;cadence::replay::applyMountEdit(pose,99,reference,profile,0);CHECK(pose[1].v==original[1].v);
 }
 {
  take::Take recording;recording.samples.resize(2);recording.samples[1].time=10;recording.shots.resize(1);recording.shots[0].time=1;
  weapon::Stats settings;settings.hipKickPitchMin=settings.hipKickPitchMax=2;settings.hipKickYawMin=settings.hipKickYawMax=0;settings.recoil.rollMin=settings.recoil.rollMax=0;
  const float peak=1+settings.recoil.duration*.12f;
  const auto first=cadence::replay::recoilAt(recording,peak,0,settings);CHECK(std::abs(first.x-2)<.0001f);
  CHECK(scene::length(cadence::replay::recoilAt(recording,1,0,settings))==0);
  CHECK(scene::length(cadence::replay::recoilAt(recording,.9f,0,settings))==0);
  CHECK(scene::length(cadence::replay::recoilAt(recording,2,0,settings))==0);
  CHECK(scene::length(cadence::replay::recoilAt(recording,peak,1,settings))==0);
  for(int i=0;i<100;++i)(void)cadence::replay::recoilAt(recording,float(i)*.025f,0,settings);
  CHECK(scene::length(cadence::replay::recoilAt(recording,peak,0,settings)-first)==0);
  recording.shots.push_back(recording.shots[0]);CHECK(std::abs(cadence::replay::recoilAt(recording,peak,0,settings).x-4)<.0001f);
  settings.recoil.intensity=0;CHECK(scene::length(cadence::replay::recoilAt(recording,peak,0,settings))==0);settings.recoil.intensity=1;
  recording.samples[0].camera.adsBlend=1;settings.adsKickPitchMin=settings.adsKickPitchMax=3;
  CHECK(std::abs(cadence::replay::recoilAt(recording,peak,0,settings).x-6)<.0001f);
  settings.adsKickCenterSpeed=30;CHECK(scene::length(cadence::replay::recoilAt(recording,1.25f,0,settings))==0);
  recording.shots.resize(100000,recording.shots[0]);settings.recoil.maxPitch=7;
  CHECK(cadence::replay::recoilAt(recording,1.024f,0,settings).x<=7);
  settings.recoil.duration=std::numeric_limits<float>::infinity();CHECK(std::isfinite(cadence::replay::recoilAt(recording,peak,0,settings).x));
 }
 const auto clips=cadence::replay::clipRange(.1f,200.f);CHECK(clips.nearPlane==.1f&&clips.farPlane==20000.f);
 // Foreground optics must not consume map near/far precision; depth readback
 // uses the actual first-person projection, including for replay captures.
 for(double nearWorld:{.1,2.5,25.0})for(double farWorld:{10000.,1000000.}){
  for(double distance:{.1,.5,2.,10.,100.,1000.}){
   const double n=render::foreground_depth::nearPlane,f=render::foreground_depth::farPlane;
   const double raw=(f-f*n/distance)/(f-n)*render::foreground_depth::slice;
   CHECK(std::abs(render::foreground_depth::linearize(raw,nearWorld,farWorld,true,true)-distance)<.0001);
  }
  for(double raw:{.05,.5,.95})CHECK(render::foreground_depth::linearize(raw,nearWorld,farWorld,true,true)==render::foreground_depth::linearize(raw,nearWorld,farWorld,false,false));
 }
 for(bool focused:{false,true})for(bool minimized:{false,true}){
  CHECK(!cadence::replay::pauseForInactiveWindow(focused,minimized,true));
  CHECK(cadence::replay::pauseForInactiveWindow(focused,minimized,false)==(!focused||minimized));
 }
 for(float base:{65.f,90.f,110.f})for(float zoom:{1.f,.7f,.2f})for(float edit:{45.f,80.f,130.f}){
  float recorded=base*zoom,changed=presentationFov(recorded,base,edit);
  float oldMagnification=std::tan(base*scene::kPi/360)/std::tan(recorded*scene::kPi/360);
  float newMagnification=std::tan(edit*scene::kPi/360)/std::tan(changed*scene::kPi/360);
  CHECK(std::abs(oldMagnification-newMagnification)<.0001f);
  CHECK(presentationFov(recorded,base,base)==recorded);
 }
 take::Take t;t.actor.viewmodelFov=80;t.actor.viewmodelFovScale=1;
 t.actorSlots[1].baseModel="second";t.actorSlots[1].viewmodelFov=100;
 take::Sample s;s.camera.fov=40;s.weaponSlot=1;
 CHECK(presentationFov(t,s,100,1)==40);s.weaponSlot=0;CHECK(presentationFov(t,s,80,1)==40);
 float begin=0,end=10;cadence::replay::setCaptureBoundary(3.51f,10,30,true,begin,end);CHECK(begin==105.f/30&&end==10);
 cadence::replay::setCaptureBoundary(1.01f,10,30,false,begin,end);CHECK(begin==1&&end==1);
 CHECK(cadence::replay::tickAt(-1,10,60)==0&&cadence::replay::tickAt(15,10,60)==600);
 std::vector<render::SmokeCurvePoint> a,b;render::ReplaySmokeSettings style;
 render::sampleReplaySmoke(a,{10,20,30},{1,0,0},.5f,77,style);CHECK(a.size()>2&&a.size()<=256);
 render::sampleReplaySmoke(b,{10,20,30},{1,0,0},.1f,77,style);
 render::sampleReplaySmoke(b,{10,20,30},{1,0,0},.5f,77,style);CHECK(a.size()==b.size());
 for(std::size_t i=0;i<a.size();++i){CHECK(scene::length(a[i].position-b[i].position)==0);CHECK(a[i].age==b[i].age);}
 style.blastSpeed*=3;style.startWidth=25;render::sampleReplaySmoke(b,{10,20,30},{1,0,0},.5f,77,style);CHECK(scene::length(a[0].position-b[0].position)>1&&b[0].startWidth>a[0].startWidth);
 render::sampleReplaySmoke(b,{},{},30,77,style);CHECK(b.empty());
 style.emissionDuration=1000;style.lifetime=1000;render::sampleReplaySmoke(b,{},{},500,77,style);CHECK(b.size()<=256);
 for(const auto&p:b)CHECK(std::isfinite(p.position.x)&&std::isfinite(p.position.z));
 render::DepthOfFieldSettings dof;dof.gamma=128;dof.sanitize();CHECK(dof.gamma==128);std::stringstream ss;ss<<dof;render::DepthOfFieldSettings loaded;ss>>loaded;loaded.sanitize();CHECK(loaded.gamma==128);
 dof.gamma=std::numeric_limits<float>::infinity();dof.sanitize();CHECK(dof.gamma==1);
 std::cout<<"Replay optical FOV, tick ranges, absolute-time editable smoke, and unrestricted positive DOF gamma passed\n";
}
