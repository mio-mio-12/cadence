#pragma once
#include "scene/Math3D.h"
#include "render/WaterDetail.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <istream>
#include <ostream>

namespace render::water {
struct Optics {
    bool depthEnabled{true};
    scene::Vec3 shallow{.035f,.48f,.43f};
    float absorption{4.f}, transmission{.65f}, crestLight{.65f};
    float foamCoverage{.48f}, foamScale{1.4f}, shoreFoam{.55f}, shoreWidth{.5f};
    void sanitize(){
        const auto c=[](float& v,float,float,float d){if(!std::isfinite(v))v=d;};
        c(shallow.x,0,1,.035f);c(shallow.y,0,1,.48f);c(shallow.z,0,1,.43f);
        c(absorption,.1f,100,4);c(transmission,0,1,.65f);c(crestLight,0,3,.65f);
        c(foamCoverage,0,1,.48f);c(foamScale,.1f,10,1.4f);c(shoreFoam,0,2,.55f);c(shoreWidth,.01f,5,.5f);
    }
};
inline std::ostream& operator<<(std::ostream& o,const Optics& p){
    return o<<p.depthEnabled<<' '<<p.shallow.x<<' '<<p.shallow.y<<' '<<p.shallow.z<<' '
        <<p.absorption<<' '<<p.transmission<<' '<<p.crestLight<<' '<<p.foamCoverage<<' '
        <<p.foamScale<<' '<<p.shoreFoam<<' '<<p.shoreWidth;
}
inline std::istream& operator>>(std::istream& in,Optics& p){
    Optics a;
    if(in>>a.depthEnabled>>a.shallow.x>>a.shallow.y>>a.shallow.z>>a.absorption>>a.transmission
        >>a.crestLight>>a.foamCoverage>>a.foamScale>>a.shoreFoam>>a.shoreWidth){
        for(float v:{a.shallow.x,a.shallow.y,a.shallow.z,a.absorption,a.transmission,a.crestLight,
            a.foamCoverage,a.foamScale,a.shoreFoam,a.shoreWidth})
            if(!std::isfinite(v)){in.setstate(std::ios::failbit);return in;}
        a.sanitize();p=a;
    }
    return in;
}
// Visual-only, centimetre scene space. Placement is deliberately not part of a
// visual preset: loading a look must not flood a different map.
struct Appearance {
    Optics optics{};
    Surface surface{};
    float crossSwell{.35f},swellAngle{65.f},swellLength{1.7f};
    int seed{17};
    float waveHeight{.65f}, wavelength{18.f}, choppiness{.8f}, direction{35.f}, spread{.65f};
    float speed{1.f}, roughness{.18f}, reflection{1.f}, detail{.35f}, foam{.35f};
    scene::Vec3 color{.025f,.13f,.17f};
    float rain{0.f}, rainScale{.55f}, rainSpeed{1.f};
    void sanitize(){
        const auto clamp=[](float& v,float,float,float fallback){if(!std::isfinite(v))v=fallback;};
        optics.sanitize();
        surface.sanitize();
        clamp(crossSwell,0,1,.35f);clamp(swellAngle,-180,180,65);clamp(swellLength,.5f,4,1.7f);
        clamp(waveHeight,0,12,.65f);clamp(wavelength,1,100,18);clamp(choppiness,0,8,.8f);
        clamp(direction,-360,360,35);clamp(spread,0,1,.65f);clamp(speed,0,4,1);
        clamp(roughness,.03f,1,.18f);clamp(reflection,0,3,1);clamp(detail,0,2,.35f);clamp(foam,0,2,.35f);
        clamp(color.x,0,1,.025f);clamp(color.y,0,1,.13f);clamp(color.z,0,1,.17f);
        clamp(rain,0,2,0);clamp(rainScale,.1f,3,.55f);clamp(rainSpeed,.1f,4,1);
    }
};
struct Settings : Appearance {
    bool enabled{},animate{true};
    float height{},radius{250.f},timeOffset{};
    int quality{1};
    void sanitize(){
        Appearance::sanitize();
        height=std::isfinite(height)?height:0;
        radius=std::isfinite(radius)?radius:250.f;
        timeOffset=std::isfinite(timeOffset)?timeOffset:0;
        quality=std::clamp(quality,0,2);
    }
};
inline Appearance preset(int n){
    Appearance a;
    if(n==1){a.waveHeight=.055f;a.wavelength=12;a.choppiness=.1f;a.surface.tightness=.05f;a.detail=.08f;a.roughness=.1f;a.foam=0;}
    if(n==2){a=preset(1);a.waveHeight=.025f;a.rain=.8f;a.detail=.05f;a.roughness=.16f;}
    if(n==3){a.waveHeight=3.2f;a.wavelength=22;a.choppiness=6.f;a.surface.tightness=.82f;a.spread=.72f;a.crossSwell=.5f;a.detail=.4f;a.foam=.9f;a.roughness=.2f;a.optics.foamCoverage=.68f;a.optics.foamScale=.55f;}
    if(n==4){a=preset(3);a.waveHeight=2.2f;a.wavelength=20;a.color={.008f,.075f,.16f};
        a.optics.shallow={.02f,.6f,.48f};a.optics.crestLight=1.5f;a.optics.absorption=5;
        a.optics.foamCoverage=.70f;a.optics.shoreFoam=.85f;}
    return a;
}
inline std::ostream& operator<<(std::ostream& o,const Appearance& a){
    return o<<a.waveHeight<<' '<<a.wavelength<<' '<<a.choppiness<<' '<<a.direction<<' '<<a.spread<<' '
        <<a.speed<<' '<<a.roughness<<' '<<a.reflection<<' '<<a.detail<<' '<<a.foam<<' '
        <<a.color.x<<' '<<a.color.y<<' '<<a.color.z<<' '<<a.rain<<' '<<a.rainScale<<' '<<a.rainSpeed;
}
inline std::istream& operator>>(std::istream& in,Appearance& a){
    Appearance p;
    if(in>>p.waveHeight>>p.wavelength>>p.choppiness>>p.direction>>p.spread>>p.speed>>p.roughness>>p.reflection>>p.detail>>p.foam
        >>p.color.x>>p.color.y>>p.color.z>>p.rain>>p.rainScale>>p.rainSpeed){
        const float values[]={p.waveHeight,p.wavelength,p.choppiness,p.direction,p.spread,p.speed,p.roughness,p.reflection,
            p.detail,p.foam,p.color.x,p.color.y,p.color.z,p.rain,p.rainScale,p.rainSpeed};
        for(float v:values)if(!std::isfinite(v)){in.setstate(std::ios::failbit);return in;}
        p.sanitize();a=p;
    }
    return in;
}
struct Wave {float x,y,amplitude,k,omega,phase;};
inline constexpr int waveCount=16;
inline std::array<Wave,waveCount> waves(Appearance a){
    a.sanitize();std::array<Wave,waveCount> result{};float weightSum=0;
    // Significant wave height (4 sigma), rather than the unattainable sum of all
    // crests. Preserve the old calm/rain amplitude at small settings.
    // Three separately directed, detuned components per wind-wave band prevent
    // one dominant infinite sinusoidal ridge. Four longer waves supply swell.
    const auto weight=[&](int i){return i<12?std::pow(.48f,float(i/3)):.65f*a.crossSwell*std::pow(.7f,float(i-12));};
    for(int i=0;i<waveCount;++i)weightSum+=weight(i)*weight(i);
    const float gain=std::clamp((a.waveHeight-.1f)/.5f,0.f,1.f);
    const float amplitude=50*a.waveHeight*((1-gain)*.248f+gain/std::sqrt(2*weightSum));
    std::uint32_t rng=static_cast<std::uint32_t>(a.seed)+0x6d2b79f5u;
    const auto random=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return float(rng&0xffffff)/16777216.f;};
    for(int i=0;i<waveCount;++i){
        const bool secondary=i>=12;const int band=secondary?i-12:i/3;
        const float angle=(a.direction+(secondary?a.swellAngle:0)+a.spread*(secondary?45.f:90.f)*(random()*2-1))*scene::kPi/180;
        const float wavelength=std::max(.001f,std::abs(100*a.wavelength*(secondary?a.swellLength:1)*std::pow(secondary?.72f:.48f,float(band))*(.76f+.48f*random())));
        auto& w=result[i];w={std::cos(angle),std::sin(angle),amplitude*weight(i),
            2*scene::kPi/wavelength,std::sqrt(981.f*2*scene::kPi/wavelength),random()*2*scene::kPi};
    }
    return result;
}
inline float safeChop(const std::array<Wave,waveCount>& w,float chop){
    // The largest eigenvalue of sum(A*k*d*d^T) bounds compression in ANY
    // direction. Unlike a scalar sum it doesn't unnecessarily penalize crossed
    // waves. Stay below folding while allowing genuinely pointed crests.
    float xx=0,xy=0,yy=0;
    for(const auto& v:w){const float s=v.amplitude*v.k;xx+=s*v.x*v.x;xy+=s*v.x*v.y;yy+=s*v.y*v.y;}
    const float maximum=.5f*(xx+yy+std::sqrt((xx-yy)*(xx-yy)+4*xy*xy));
    return .97f/std::max(.0001f,maximum)*std::tanh(std::max(0.f,chop)*maximum/.97f);
}
inline scene::Vec3 displacement(const std::array<Wave,waveCount>& waves,float chop,float x,float y,double time,float tightness=.55f){
    scene::Vec3 p{};
    for(const auto& w:waves){const double phase=w.k*(w.x*x+w.y*y)-time*w.omega+w.phase;
        const float c=static_cast<float>(std::cos(phase));
        p.x+=chop*w.amplitude*w.x*c;p.y+=chop*w.amplitude*w.y*c;p.z+=w.amplitude*crestProfile(phase,tightness)[0];}
    return p;
}
} // namespace render::water
