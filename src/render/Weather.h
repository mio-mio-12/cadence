#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
#include <type_traits>

namespace render::weather {
// Presentation only: this structure deliberately has no connection to Take.
struct Wind {
    float speed{2},direction{25},gust{.4f},period{7},turbulence{.6f},scale{3},rate{.7f};
    template<class F> void fields(F f){f(speed);f(direction);f(gust);f(period);f(turbulence);f(scale);f(rate);}
};
struct Particles {
    bool enabled{},shelter{true},ownWind{};
    int style{};
    float density{.7f},size{1.4f},variation{.8f},speed{1},opacity{.8f},brightness{1.4f};
    float flutter{1},tumble{2},response{1},bottom{-10000},top{10000},softness{15},nearFade{30};
    float sheets{.35f},sheetSize{160};
    scene::Vec3 color{.87f,.94f,1};Wind wind;
    template<class F> void fields(F f){f(enabled);f(shelter);f(ownWind);f(style);f(density);f(size);f(variation);f(speed);f(opacity);f(brightness);f(flutter);f(tumble);f(response);f(bottom);f(top);f(softness);f(nearFade);f(sheets);f(sheetSize);f(color.x);f(color.y);f(color.z);wind.fields(f);}
};
struct Cloud {
    bool enabled{},ownWind{},dayNight{true};
    float coverage{.5f},softness{.2f},size{1600},strength{.75f},breakup{.55f},height{4000};Wind wind;
    template<class F> void fields(F f){f(enabled);f(ownWind);f(dayNight);f(coverage);f(softness);f(size);f(strength);f(breakup);f(height);wind.fields(f);}
};
struct Fog {
    bool enabled{};
    float bottom{-20},top{120},density{.7f},opacity{1},size{600},breakup{.65f},edge{40},distance{6000},start{},drift{.25f};
    scene::Vec3 color{.55f,.64f,.7f};
    template<class F> void fields(F f){f(enabled);f(bottom);f(top);f(density);f(opacity);f(size);f(breakup);f(edge);f(distance);f(start);f(drift);f(color.x);f(color.y);f(color.z);}
};
struct Lens {
    bool enabled{},firstPerson{true},freeCamera{true},dolly{true};
    float coverage{.3f},size{.075f},variation{.8f},randomness{1},speed{.45f},direction{-90},turbulence{.7f},trail{.6f};
    float refraction{1.2f},ior{1.333f},roughness{.04f},softness{.18f},opacity{1},colorMix{};
    float beads{.35f},merge{.5f},reflectivity{.12f},dispersion{.04f},shadow{.15f},highlight{.35f},lightAngle{125};
    float backgroundBlur{1.5f},refractionMin{20},refractionMax{180};
    float condensation{},blur{3},frost{},frostDetail{14};scene::Vec3 color{.75f,.85f,.95f};
    template<class F> void fields(F f){f(enabled);f(firstPerson);f(freeCamera);f(dolly);f(coverage);f(size);f(variation);f(randomness);f(speed);f(direction);f(turbulence);f(trail);f(refraction);f(ior);f(roughness);f(softness);f(opacity);f(colorMix);f(condensation);f(blur);f(frost);f(frostDetail);f(color.x);f(color.y);f(color.z);f(beads);f(merge);f(reflectivity);f(dispersion);f(shadow);f(highlight);f(lightAngle);f(backgroundBlur);f(refractionMin);f(refractionMax);}
};
struct Settings {
    float dustAerosolDetail{120},dustAerosolAmount{.6f},dustAerosolContrast{1.2f};
    bool animate{true};int seed{173},quality{1};float range{2200},timeScale{1},offset{},freezeTime{},blockerDistance{5000};
    Wind wind;Cloud cloud;Fog fog;Particles snow,dust,debris;Lens lens;
    Settings(){dust.color={.58f,.42f,.24f};dust.size=.4f;dust.speed=.08f;dust.opacity=.4f;dust.bottom=-20;dust.top=250;dust.sheetSize=240;dust.shelter=false;debris.color={.45f,.23f,.07f};debris.size=3;debris.density=.1f;debris.speed=.3f;debris.tumble=4;}
    template<class F> void fields(F f){f(animate);f(seed);f(quality);f(range);f(timeScale);f(offset);f(freezeTime);f(blockerDistance);wind.fields(f);cloud.fields(f);fog.fields(f);snow.fields(f);dust.fields(f);debris.fields(f);lens.fields(f);}
    bool active()const{return cloud.enabled||fog.enabled||snow.enabled||dust.enabled||debris.enabled;}
    bool sheltered()const{return (snow.enabled&&snow.shelter)||(dust.enabled&&dust.shelter)||(debris.enabled&&debris.shelter);}
    void sanitize(){fields([](auto& v){if constexpr(std::is_floating_point_v<std::remove_reference_t<decltype(v)>>)if(!std::isfinite(v))v=0;});lens.enabled=false;for(auto* v:{&dustAerosolDetail,&dustAerosolAmount,&dustAerosolContrast})if(!std::isfinite(*v))*v=0;quality=std::clamp(quality,0,2);debris.style=std::clamp(debris.style,0,2);}
};
// UI ranges are suggestions, not renderer caps. Keep only positive-domain
// guards and a numerical overflow ceiling far beyond useful artistic values.
inline float safe(float v,float lo,float /*hi*/){v=std::isfinite(v)?std::clamp(v,-1e12f,1e12f):0.f;return lo>=0?std::max(lo>0?.0001f:0.f,v):v;}
struct ParticleBudget {int slots;float probability;};
inline ParticleBudget particleBudget(float density,int quality,int kind){
    const int base=(quality==0?2:quality==1?4:8)*(kind==0?8:1);
    const double wanted=std::max(0.,double(density))*base;
    // glDrawArrays uses a signed count. No saturation at density 1.
    const int slots=int(std::clamp(std::ceil(wanted),1.,double(2147483647/(13*13*13*6))));
    return {slots,float(std::clamp(wanted/slots,0.,1.))};
}
inline double clock(const Settings& s,double time){double t=(s.animate?time:double(s.freezeTime))*double(s.timeScale)+s.offset;return std::isfinite(t)?std::clamp(t,-1.e7,1.e7):0.;}
inline scene::Vec3 drift(const Wind& w,double t){
    const double a=std::remainder(double(w.direction),360.)*scene::kPi/180.;
    const double frequency=2.*scene::kPi/std::max(.01,double(std::abs(w.period)));
    const double d=safe(w.speed,-1000,1000)*100.*(t+safe(w.gust,-10,10)*std::sin(std::remainder(t*frequency,2.*scene::kPi))/frequency);
    return {float(std::remainder(std::cos(a)*d,1.e8)),float(std::remainder(std::sin(a)*d,1.e8)),0};
}
inline std::ostream& operator<<(std::ostream& o,const Settings& s){auto copy=s;copy.fields([&](auto& v){o<<v<<' ';});return o;}
inline std::istream& operator>>(std::istream& i,Settings& s){Settings p;bool finite=true;p.fields([&](auto& v){i>>v;if constexpr(std::is_floating_point_v<std::remove_reference_t<decltype(v)>>)finite&=std::isfinite(v);});if(!finite)i.setstate(std::ios::failbit);if(i){p.sanitize();s=p;}return i;}
inline Settings preset(int n){Settings s;
    if(n==0){s.snow.enabled=true;s.snow.density=.65f;s.range=1400;s.snow.size=1.8f;s.wind.speed=.8f;}
    if(n==1){s.snow.enabled=true;s.snow.density=1;s.snow.speed=2.4f;s.wind.speed=9;s.wind.turbulence=2;s.snow.flutter=2;s.fog.enabled=true;s.fog.top=1600;s.fog.density=.18f;}
    if(n==2){s.dust.enabled=true;s.wind.speed=4;s.wind.gust=.85f;s.dust.sheets=.35f;}
    if(n==3){s.dust.enabled=true;s.dust.density=1;s.dust.sheets=1;s.dust.top=1500;s.wind.speed=11;s.wind.turbulence=1.5f;}
    if(n==4){s.fog.enabled=true;s.wind.speed=.5f;}
    if(n==5){s.cloud.enabled=true;s.cloud.coverage=.65f;s.wind.speed=3;}
    // Slot 6 was lens weather; retain numbering for older preset callers.
    if(n==7){s.debris.enabled=true;s.wind.speed=4;s.debris.density=.22f;}
    return s;
}
}
