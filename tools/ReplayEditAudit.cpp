#include "take/Take.h"
#include <iostream>
int main(){
 take::Take t;t.sampleRate=30;t.boneCount=1;
 for(int i=0;i<4;++i){take::Sample s;s.time=i/30.f;s.weaponSlot=i<1?0:1;s.camera.target={float(i),0,0};s.pose={scene::translation({float(i),0,0})};take::RecordedActorState b;b.id=1;b.pose=s.pose;s.bots.push_back(b);t.samples.push_back(s);}
 const auto middle=t.interpolatedSample(.25f/30);std::cout<<"Weapon swap: desired camera X=.25, actual="<<middle.camera.target.x<<"; bot X="<<middle.bots[0].pose[0].v[12]<<"\n";
 t.dollyCamera={{0,{0,0,0},{},90},{3,{3,0,0},{},90}};take::ShotEvent shot;shot.time=.01f;t.shots.push_back(shot);
 const bool trimmed=t.trim(.005f,.08f);std::cout<<"Trim [.005,.08]: success="<<trimmed<<" samples="<<t.samples.size()<<" camera keys="<<t.dollyCamera.size()<<" first shot time="<<t.shots[0].time<<"\n";
 return 0;
}
