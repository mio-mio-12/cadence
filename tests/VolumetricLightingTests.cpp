#include "render/VolumetricLighting.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <limits>
#include <sstream>
int main(){render::VolumetricLightingSettings s;assert(!s.enabled);s.enabled=true;s.density=.001f;s.steps=64;s.resolutionDivisor=4;std::stringstream data;data<<s;render::VolumetricLightingSettings copy;data>>copy;assert(copy.enabled&&copy.density==s.density&&copy.steps==64&&copy.resolutionDivisor==4);
s.density=std::numeric_limits<float>::quiet_NaN();s.intensity=-4;s.anisotropy=8;s.steps=999;s.resolutionDivisor=999;s.heightFalloff=0;s.sanitize();assert(std::isfinite(s.density)&&s.intensity==0&&s.anisotropy<=.85f&&s.steps==64&&s.resolutionDivisor==4&&s.heightFalloff>=1);}
