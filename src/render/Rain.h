#pragma once
#include "scene/Math3D.h"
#include "render/RainStyle.h"
#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>

namespace render::rain {
// Visual settings only. Never serialized into a take.
struct Settings {
    Style style{};
    float blockerDistance{50}; // Metres upstream from camera height; 0 = unlimited.
    bool enabled{}, shelter{true}, splashes{true}, animate{true};
    int quality{1}, seed{73};
    float density{.65f}, radius{18}, speed{9}, wind{1.5f}, direction{25};
    float width{.14f}, shutter{.018f}, opacity{.55f}, brightness{1.4f};
    float nearFade{35}, softness{12}, splashSize{11.1f}, splashAmount{5};
    float timeScale{1}, timeOffset{}, freezeTime{};
    scene::Vec3 tint{.72f,.82f,1};
    float gusts{.4f}, gustPeriod{4.5f}, turbulence{.35f}, turbulenceScale{3.f}, turbulenceRate{.65f};
    bool wallMist{true};
    float mistAmount{.49f},mistSize{150};
    void sanitize(){
        // Slider bounds are suggestions, not limits on typed values. Mathematical
        // and allocation guards belong at evaluation, never in saved settings.
        const auto c=[](float& v,float,float,float d){if(!std::isfinite(v))v=d;};
        quality=std::clamp(quality,0,2);
        c(density,0,1,.65f);c(radius,5,50,18);c(speed,2,20,9);c(wind,0,12,1.5f);c(direction,-180,180,25);
        c(width,.01f,.5f,.14f);c(shutter,.002f,.1f,.018f);c(opacity,0,1,.55f);c(brightness,0,8,1.4f);
        c(nearFade,5,200,35);c(softness,1,100,12);c(splashSize,.5f,15,3.5f);c(splashAmount,0,1,.6f);
        c(timeScale,0,4,1);c(timeOffset,-86400,86400,0);c(freezeTime,-86400,86400,0);
        c(tint.x,0,4,.72f);c(tint.y,0,4,.82f);c(tint.z,0,4,1);
        c(gusts,0,2,.4f);c(gustPeriod,.5f,20,4.5f);c(turbulence,0,3,.35f);
        c(turbulenceScale,.3f,15,3);c(turbulenceRate,0,4,.65f);c(mistAmount,0,2,.35f);c(mistSize,5,150,45);
    }
};
inline Settings preset(int n){
    Settings s;s.enabled=true;
    if(n==0){s.density=.23f;s.wind=.5f;s.width=.08f;s.opacity=.4f;s.shutter=.012f;s.splashAmount=.25f;s.gusts=.15f;s.turbulence=.15f;s.mistAmount=.1f;}
    if(n==2){s.density=1;s.wind=4;s.width=.18f;s.opacity=.65f;s.shutter=.028f;s.splashAmount=1;s.gusts=.85f;s.turbulence=.8f;s.mistAmount=.7f;}
    if(n==3){s.density=.55f;s.wind=2;s.brightness=2.1f;s.opacity=.46f;s.tint={.6f,.73f,1};}
    if(n==4){s=preset(2);s.wind=8;s.direction=90;s.gusts=1;s.turbulence=1.2f;s.turbulenceScale=2.2f;s.turbulenceRate=.9f;s.mistAmount=1;s.mistSize=60;}
    return s;
}
inline std::ostream& operator<<(std::ostream& o,const Settings& s){
    return o<<s.enabled<<' '<<s.shelter<<' '<<s.splashes<<' '<<s.animate<<' '<<s.quality<<' '<<s.seed<<' '
      <<s.density<<' '<<s.radius<<' '<<s.speed<<' '<<s.wind<<' '<<s.direction<<' '<<s.width<<' '<<s.shutter<<' '
      <<s.opacity<<' '<<s.brightness<<' '<<s.nearFade<<' '<<s.softness<<' '<<s.splashSize<<' '<<s.splashAmount<<' '
      <<s.timeScale<<' '<<s.timeOffset<<' '<<s.tint.x<<' '<<s.tint.y<<' '<<s.tint.z<<' '
      <<s.gusts<<' '<<s.gustPeriod<<' '<<s.turbulence<<' '<<s.turbulenceScale<<' '<<s.turbulenceRate<<' '
      <<s.wallMist<<' '<<s.mistAmount<<' '<<s.mistSize<<' '<<s.freezeTime;
}
inline std::istream& operator>>(std::istream& in,Settings& s){
    Settings p;
    if(in>>p.enabled>>p.shelter>>p.splashes>>p.animate>>p.quality>>p.seed>>p.density>>p.radius>>p.speed
      >>p.wind>>p.direction>>p.width>>p.shutter>>p.opacity>>p.brightness>>p.nearFade>>p.softness
      >>p.splashSize>>p.splashAmount>>p.timeScale>>p.timeOffset>>p.tint.x>>p.tint.y>>p.tint.z
      >>p.gusts>>p.gustPeriod>>p.turbulence>>p.turbulenceScale>>p.turbulenceRate>>p.wallMist>>p.mistAmount>>p.mistSize>>p.freezeTime){
        for(float v:{p.density,p.radius,p.speed,p.wind,p.direction,p.width,p.shutter,p.opacity,p.brightness,
            p.nearFade,p.softness,p.splashSize,p.splashAmount,p.timeScale,p.timeOffset,p.tint.x,p.tint.y,p.tint.z,
            p.gusts,p.gustPeriod,p.turbulence,p.turbulenceScale,p.turbulenceRate,p.mistAmount,p.mistSize,p.freezeTime})
            if(!std::isfinite(v)){in.setstate(std::ios::failbit);return in;}
        p.sanitize();s=p;
    }return in;
}
inline std::uint32_t hash(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline float random(std::uint32_t x){return (hash(x)>>8)*(1.f/16777216.f);}
inline float safeSpeed(const Settings& s){return std::max(.001f,std::min(std::abs(s.speed),100000.f));}
inline float safeRadius(const Settings& s){return std::clamp(std::abs(s.radius),.1f,10000.f);}
inline scene::Vec3 slope(const Settings& s){const float a=std::remainder(s.direction,360.f)*scene::kPi/180;const float ratio=std::clamp(s.wind/safeSpeed(s),-100.f,100.f);return {std::cos(a)*ratio,std::sin(a)*ratio,0};}
inline double clock(double time,const Settings& s){return (s.animate?time:s.freezeTime)*s.timeScale+s.timeOffset;}
inline double phase(double time,const Settings& s){return std::fmod(clock(time,s)*s.speed*100.,2400.);}
}
