#include "render/ActorOverlays.h"
#include <iostream>
#include <limits>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<#x<<" line "<<__LINE__<<"\n";return 1;}}while(0)
int main(){
 render::ActorOverlaySettings s;render::ActorOverlayHistory h;
 std::vector<scene::Mat4> pose(2,scene::Mat4::identity());render::ActorOverlayInput in{1,100,true,false};
 h.begin(0,true);CHECK(h.update(in,pose,{},s)==nullptr);
 s.echoes=true;s.hitGhosts=true;s.landingRings=true;s.historySeconds=3;s.historySpacing=.05f;
 auto* p=h.update(in,pose,{},s);CHECK(p&&p->samples.size()==1&&p->samples.back().pose.size()==2);
 h.begin(.1,true);in.health=50;in.airborne=false;p=h.update(in,pose,{},s);CHECK(p->hitPose.size()==2&&p->hitTime==.1&&p->landingTime==.1);
 for(int i=2;i<1000;++i){h.begin(i*.05,true);h.update(in,pose,{float(i),0,0},s);}CHECK(h.actors[1].samples.size()<=64);CHECK(h.actors[1].hitPose.empty());
 h.begin(0,true);CHECK(h.actors.empty());h.update(in,pose,{},s);h.update(in,pose,{1000,0,0},s);CHECK(h.actors[1].samples.size()==1);
 h.begin(10,true);CHECK(h.actors.empty());h.update(in,pose,{},s);h.begin(10,false);CHECK(h.actors.empty());
 h.begin(11,true);for(int i=0;i<1000;++i)h.update({std::uint64_t(i),100,false,false},pose,{},s);CHECK(h.actors.size()==64);
 s={};s.ribbons=true;h.begin(0,true);p=h.update(in,pose,{},s);CHECK(p->samples.back().pose.empty());
 s.opacity=std::numeric_limits<float>::quiet_NaN();s.echoCount=100;s.material=-1;s.sanitize();CHECK(std::isfinite(s.opacity)&&s.echoCount==6&&s.material==0);
 std::cout<<"PASS bounded histories, optional pose copies, rewind, discontinuity, teleport, damage, landing, disabled, finite settings\n";
}

