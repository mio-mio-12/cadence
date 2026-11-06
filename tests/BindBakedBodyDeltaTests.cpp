#include "scene/BindBakedBodyDelta.h"
#include <iostream>
#include <stdexcept>
static void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
static scene::Quat axis(float x,float y,float z,float angle){return {x*std::sin(angle/2),y*std::sin(angle/2),z*std::sin(angle/2),std::cos(angle/2)};}
int main(){try{
    using namespace scene;CastScene rig;rig.skeleton.bones.resize(3);rig.skeleton.bones[0].parent=-1;rig.skeleton.bones[1].parent=0;rig.skeleton.bones[2].parent=1;
    const auto bind=axis(1,0,0,.7f),hold=axis(0,1,0,.9f),kick=axis(0,0,1,.2f);
    rig.skeleton.bones[1].restLocal.rotation=bind;rig.skeleton.bones[1].restLocal.position={2,3,4};
    Animation base,delta;base.durationFrames=delta.durationFrames=1;
    Track q;q.boneIndex=1;q.property=TrackProperty::Rotation;q.frames={0,1};q.rotationValues={hold,hold};base.tracks.push_back(q);
    q.rotationValues={bind,multiply(bind,kick)};delta.tracks.push_back(q);
    Track pos;pos.boneIndex=1;pos.property=TrackProperty::TranslationX;pos.frames={0,1};pos.scalarValues={2,3};delta.tracks.push_back(pos);
    Track pelvis=pos;pelvis.boneIndex=0;pelvis.scalarValues={90,100};delta.tracks.push_back(pelvis);
    rig.animations={base,delta};rig.animations.push_back(imported::composeBindBakedPostDelta(rig,0,1,1));
    const auto first=rig.sampleLocalPose(2,0),last=rig.sampleLocalPose(2,1);
    const auto expected=normalize(multiply(hold,kick));
    const auto qdiff=[](Quat a,Quat b){return std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);};
    check(qdiff(first[1].rotation,hold)>.99999f,"Bind delta must preserve actual holding pose");
    check(qdiff(last[1].rotation,expected)>.99999f,"POST composition order with nonidentity bind");
    check(std::abs(last[1].position.x-3)<1e-5f,"Bind-relative translation");
    check(std::abs(last[0].position.x)<1e-5f,"Pelvis delta leaked into result");
    for(const auto& t:rig.animations[2].tracks)check(t.ownsLayer==(t.boneIndex!=0),"Torso-only layer ownership");
    check(rig.animations[1].tracks.back().scalarValues[1]==100,"Input delta mutated");
    rig.animations[1].tracks[0].mode=TrackMode::Additive;bool rejected=false;try{imported::composeBindBakedPostDelta(rig,0,1,1);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Non bind-baked input accepted");std::cout<<"Bind-baked POST delta checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
