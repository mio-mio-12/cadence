#include "gameplay/WeaponTurn.h"
#include <cmath>
#include <iostream>
int main(){int failures{};const auto check=[&](bool b){if(!b)++failures;};
    using namespace gameplay::weapon_turn;
    State a;a.update(0,0,0,1.f/60);a.update(6,12,0,1.f/60);
    check(std::abs(a.pitch+.4f)<1e-5f);check(std::abs(a.yaw+.8f)<1e-5f);
    check(std::abs(a.horizontal-.24f)<1e-5f);check(std::abs(a.vertical-.04f)<1e-5f);
    State crossing;crossing.update(0,179,0,1.f/60);crossing.update(0,-179,0,1.f/60);
    check(std::abs(crossing.yaw+2.f/15)<1e-5f);
    State aiming;aiming.update(0,0,1,1.f/60);aiming.update(12,12,1,1.f/60);
    check(std::abs(aiming.yaw-.02f)<1e-5f);
    const auto before=a.transform();a.update(90,90,0,0);check(before.v==a.transform().v);
    check(a.transform(0,0,1).v==scene::Mat4::identity().v);
    check(a.transform(1,1,0).v==scene::Mat4::identity().v);
    check(a.transform(.5f,0,2).v==before.v);
    check(a.transform(0,2,0).v==a.transform(1,0,2).v);
    for(int i=0;i<300;++i)a.update(6,12,0,1.f/60);check(std::abs(a.yaw)<1e-4f);
    a.update(90,90,0,1);check(a.yaw==0&&a.horizontal==0);
    check(wrap(358)==-2);std::cout<<"Weapon turn failures: "<<failures<<'\n';return failures?1:0;
}
