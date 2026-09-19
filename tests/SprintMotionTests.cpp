#include "gameplay/SprintMotion.h"
#include <iostream>

int main(){
    int errors=0;
    const auto equal=[&](const scene::Mat4& a,const scene::Mat4& b,float tolerance=.0002f){
        for(int i=0;i<16;++i)if(!std::isfinite(a.v[i])||!std::isfinite(b.v[i])||std::abs(a.v[i]-b.v[i])>tolerance){++errors;return;}
    };
    gameplay::SprintMotion motion;motion.duration=.6f;
    for(int i=0;i<64;++i){
        const float phase=2*scene::kPi*i/64;
        scene::Transform sample;sample.position={2*std::sin(phase),3*std::cos(phase),-8};
        sample.rotation=scene::fromEulerRadians({.1f*std::sin(phase),.4f,.1f});motion.samples.push_back(sample);
    }
    const auto hand=scene::trs({15,8,-12},scene::fromEulerRadians({.5f,.3f,.1f}),{1,1,1});
    const auto gun=scene::trs({32,4,-8},scene::fromEulerRadians({.1f,.2f,.3f}),{1,1,1});
    const auto grip=scene::inverseAffine(hand)*gun;
    for(int i=0;i<240;++i){
        const auto m=motion.sample(i/120.0f,(i%60)/60.0f);
        equal(scene::inverseAffine(m*hand)*(m*gun),grip);
        equal(motion.sample(i/120.0f,0),scene::Mat4::identity());
    }
    equal(motion.sample(0,1),motion.sample(.6f,1));
    equal(motion.sample(.6f-.000001f,1),motion.sample(.000001f,1),.001f);
    float weight=0;
    for(int i=0;i<60;++i)weight=gameplay::sprintEnvelope(weight,true,1/120.0f,.22f);
    if(std::abs(weight-1)>1e-6f)++errors;
    for(int i=0;i<60;++i)weight=gameplay::sprintEnvelope(weight,false,1/120.0f,.22f);
    if(std::abs(weight)>1e-6f)++errors;
    if(gameplay::sprintEnvelope(0,true,-1,.22f)!=0)++errors;
    equal(gameplay::SprintMotion{}.sample(2,1),scene::Mat4::identity());
    std::cout<<"Sprint rigid-grip / blend / loop tests: "<<errors<<" failures\n";
    return errors?1:0;
}
