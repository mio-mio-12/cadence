#pragma once
#include "scene/Math3D.h"
#include <cmath>
namespace gameplay::shake {
struct Settings{bool enabled{},viewmodel{};float speed{1},amount{1},position{1},rotation{1};};
inline float wave(double t,int axis){return float((std::sin(t*(1.73+axis*.31)+axis*2.1)+.5*std::sin(t*(4.11+axis*.47)+axis*.71))/1.5);}
struct Sample{scene::Vec3 position,rotation;};
inline Sample sample(const Settings& s,double seconds){if(!s.enabled||!std::isfinite(seconds))return {};const double t=seconds*s.speed*6.283185307;return {{wave(t,0)*s.amount*s.position,wave(t,1)*s.amount*s.position,wave(t,2)*s.amount*s.position},{wave(t,3)*s.amount*s.rotation,wave(t,4)*s.amount*s.rotation,wave(t,5)*s.amount*s.rotation}};}
}
