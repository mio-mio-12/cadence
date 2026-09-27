#include "render/Weather.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <limits>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(){using namespace render::weather;
    Settings s;CHECK(!s.active());CHECK(!s.sheltered());
    for(int n=0;n<8;++n){s=preset(n);std::ostringstream out;out<<std::setprecision(9)<<s;Settings p;std::istringstream in(out.str());in>>p;CHECK(in);std::ostringstream again;again<<std::setprecision(9)<<p;CHECK(out.str()==again.str());CHECK(p.active()==(n!=6));}
    CHECK(safe(500,0,1)==500&&safe(4000,.1f,20)==4000);
    CHECK(particleBudget(500,1,0).slots==500*particleBudget(1,1,0).slots);
    CHECK(particleBudget(1,1,0).slots==32);CHECK(particleBudget(0,1,0).probability==0);
    s={};s.lens.enabled=true;s.sanitize();CHECK(!s.lens.enabled&&!s.active());
    s=preset(1);std::ostringstream before;before<<s;std::istringstream broken("1 173 1 nan");broken>>s;CHECK(!broken);std::ostringstream after;after<<s;CHECK(before.str()==after.str());
    s={};s.snow.size=4000;s.fog.density=400;s.lens.blur=90;s.sanitize();CHECK(s.snow.size==4000&&s.fog.density==400&&s.lens.blur==90);
    s.range=std::numeric_limits<float>::infinity();s.sanitize();CHECK(std::isfinite(s.range));
    s.timeScale=.25f;s.offset=7;CHECK(clock(s,100)==32);s.animate=false;s.freezeTime=100;CHECK(clock(s,900)==32);
    s.animate=true;CHECK(clock(s,std::numeric_limits<double>::infinity())==0);
    Wind w;const auto first=drift(w,25);drift(w,1e7);const auto repeat=drift(w,25);CHECK(first.x==repeat.x&&first.y==repeat.y);
    w.period=0;w.speed=1e30f;w.gust=1e30f;auto d=drift(w,1e7);CHECK(std::isfinite(d.x)&&std::isfinite(d.y));
    std::cout<<"Weather serialization, defaults, seek clocks, artistic overrides and finite resource evaluation passed\n";
}
