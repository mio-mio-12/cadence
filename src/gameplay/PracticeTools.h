#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace gameplay::practice {
inline bool directSwapKey(bool thirdWeapon,int algorithm){return thirdWeapon&&algorithm==5;}
inline bool swapPressed(bool key,bool previous,bool wheel,bool thirdWeapon,bool directRequest=false){return !directRequest&&((key&&!previous)||(!thirdWeapon&&wheel));}
inline bool ebHit(bool sniper,bool replay,int radius,float distanceCm){return sniper&&!replay&&radius>0&&distanceCm<float(radius)*2.54f;}
inline scene::Vec3 roamOffset(std::uint32_t id,std::uint32_t sequence,float radius){
    const auto random=[](std::uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;return float(v&0xffffffu)/float(0x1000000u);};
    const float angle=random(id*7919u+sequence*104729u)*scene::kPi*2;
    const float distance=std::max(0.f,radius)*(.25f+.6f*random(sequence*65537u+id));
    return {std::cos(angle)*distance,std::sin(angle)*distance,0};
}
}
