#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <istream>
#include <ostream>

// Independent, seekable time-of-day controller. No IW3XO controller code is used.
namespace render::daynight {
inline float bounded(float v,float lo,float hi,float fallback){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;}
inline float smooth(float v){v=std::clamp(v,0.f,1.f);return v*v*(3-2*v);}
struct State {
    float sunlight{1.5f},ambient{.34f};
    scene::Vec3 sun{1,.84f,.75f},ambientColor{.65f,.75f,1};
    float brightness{},contrast{1},desaturation{.1f};
    scene::Vec3 dark{1,1,1},light{1,1,1};
    float fogAmount{.3f},fogStart{10},fogHalfDistance{100}; // Metres, not engine units.
    scene::Vec3 fog{.55f,.6f,.7f};
    void sanitize(){sunlight=bounded(sunlight,0,10,1);ambient=bounded(ambient,0,3,.3f);brightness=bounded(brightness,-1,1,0);contrast=bounded(contrast,0,4,1);desaturation=bounded(desaturation,-1,1,0);fogAmount=bounded(fogAmount,0,1,0);fogStart=bounded(fogStart,0,10000,10);fogHalfDistance=bounded(fogHalfDistance,.1f,10000,100);for(auto* c:{&sun,&ambientColor,&dark,&light,&fog}){c->x=bounded(c->x,0,4,1);c->y=bounded(c->y,0,4,1);c->z=bounded(c->z,0,4,1);}}
};
inline std::ostream& operator<<(std::ostream&o,const State&s){auto v=[&](scene::Vec3 c){o<<' '<<c.x<<' '<<c.y<<' '<<c.z;};o<<s.sunlight<<' '<<s.ambient;v(s.sun);v(s.ambientColor);o<<' '<<s.brightness<<' '<<s.contrast<<' '<<s.desaturation;v(s.dark);v(s.light);o<<' '<<s.fogAmount<<' '<<s.fogStart<<' '<<s.fogHalfDistance;v(s.fog);return o;}
inline std::istream& operator>>(std::istream&i,State&s){auto v=[&](scene::Vec3&c){i>>c.x>>c.y>>c.z;};i>>s.sunlight>>s.ambient;v(s.sun);v(s.ambientColor);i>>s.brightness>>s.contrast>>s.desaturation;v(s.dark);v(s.light);i>>s.fogAmount>>s.fogStart>>s.fogHalfDistance;v(s.fog);s.sanitize();return i;}
struct Settings {
    bool enabled{},running{true},rotateSun{true},sky{true},film{true},fog{true};
    float hour{12},cycleSeconds{360},daySpeed{.7f},nightSpeed{2.2f},azimuth{-38},elevation{55};
    float sunTransition{2},filmTransition{2},fogTransition{2}; // Hours around twilight, independent channels.
    float skyIntensity{1},sunDisk{1},starIntensity{.35f};
    float rayleigh{1},mie{1},mieDirection{.8f};
    bool starAutoRotate{};
    float starRotationSpeed{1}; // degrees per playback second, independent of lighting
    float starTrailLength{},starTrailBrightness{1},starTrailFade{1},starPoleElevation{45},starPoleAzimuth{};
    scene::Vec3 sunsetColor{.86f,.45f,.2f};
    float coverage{.55f},thickness{.45f},absorption{1.5f},cloudScale{2},cloudExposure{1};
    float windAngle{45},windSpeed{.025f};
    int cloudSteps{12},lightSteps{2};
    std::array<State,3> states{}; // Day, twilight, night.
    Settings(){auto&t=states[1];t.sunlight=1.3f;t.ambient=.22f;t.sun={1,.55f,.32f};t.ambientColor={.6f,.45f,.7f};t.brightness=-.03f;t.contrast=1.155f;t.desaturation=0;t.fog={.44f,.33f,.2f};t.fogAmount=.5f;t.fogHalfDistance=60;
        auto&n=states[2];n.sunlight=.18f;n.ambient=.09f;n.sun={.4f,.4f,.7f};n.ambientColor={.3f,.4f,.7f};n.brightness=-.075f;n.contrast=1.06f;n.desaturation=.375f;n.dark={.413f,.638f,1.008f};n.light={.838f,.689f,.618f};n.fog={.012f,.018f,.04f};n.fogAmount=.6f;n.fogHalfDistance=70;}
    void sanitize(){if(running)starAutoRotate=false;starRotationSpeed=bounded(starRotationSpeed,-30,30,1);starTrailLength=bounded(starTrailLength,0,180,0);starTrailBrightness=bounded(starTrailBrightness,0,10,1);starTrailFade=bounded(starTrailFade,0,5,1);starPoleElevation=bounded(starPoleElevation,-90,90,45);starPoleAzimuth=bounded(starPoleAzimuth,-180,180,0);hour=bounded(hour,0,24,12);cycleSeconds=bounded(cycleSeconds,10,86400,360);daySpeed=bounded(daySpeed,.05f,10,.7f);nightSpeed=bounded(nightSpeed,.05f,10,2.2f);azimuth=bounded(azimuth,-360,360,0);elevation=bounded(elevation,-90,90,55);sunTransition=bounded(sunTransition,.1f,6,2);filmTransition=bounded(filmTransition,.1f,6,2);fogTransition=bounded(fogTransition,.1f,6,2);skyIntensity=bounded(skyIntensity,0,10,1);sunDisk=bounded(sunDisk,0,10,1);starIntensity=bounded(starIntensity,0,5,.35f);rayleigh=bounded(rayleigh,.1f,5,1);mie=bounded(mie,0,5,1);mieDirection=bounded(mieDirection,0,.98f,.8f);coverage=bounded(coverage,0,1,.55f);thickness=bounded(thickness,.05f,2,.45f);absorption=bounded(absorption,.05f,8,1.5f);cloudScale=bounded(cloudScale,.1f,10,2);cloudExposure=bounded(cloudExposure,0,5,1);windAngle=bounded(windAngle,-360,360,45);windSpeed=bounded(windSpeed,0,.5f,.025f);cloudSteps=std::clamp(cloudSteps,4,32);lightSteps=std::clamp(lightSteps,0,4);sunsetColor.x=bounded(sunsetColor.x,0,4,.86f);sunsetColor.y=bounded(sunsetColor.y,0,4,.45f);sunsetColor.z=bounded(sunsetColor.z,0,4,.2f);for(auto&s:states)s.sanitize();}
};
inline std::ostream& operator<<(std::ostream&o,const Settings&s){o<<s.enabled<<' '<<s.running<<' '<<s.rotateSun<<' '<<s.sky<<' '<<s.film<<' '<<s.fog<<' '<<s.hour<<' '<<s.cycleSeconds<<' '<<s.daySpeed<<' '<<s.nightSpeed<<' '<<s.azimuth<<' '<<s.elevation<<' '<<s.sunTransition<<' '<<s.filmTransition<<' '<<s.fogTransition<<' '<<s.skyIntensity<<' '<<s.sunDisk<<' '<<s.starIntensity<<' '<<s.rayleigh<<' '<<s.mie<<' '<<s.mieDirection<<' '<<s.sunsetColor.x<<' '<<s.sunsetColor.y<<' '<<s.sunsetColor.z<<' '<<s.coverage<<' '<<s.thickness<<' '<<s.absorption<<' '<<s.cloudScale<<' '<<s.cloudExposure<<' '<<s.windAngle<<' '<<s.windSpeed<<' '<<s.cloudSteps<<' '<<s.lightSteps<<' '<<s.starTrailLength<<' '<<s.starTrailBrightness<<' '<<s.starTrailFade<<' '<<s.starPoleElevation<<' '<<s.starPoleAzimuth;for(auto&t:s.states)o<<' '<<t;return o;}
inline std::istream& operator>>(std::istream&i,Settings&s){Settings t;i>>t.enabled>>t.running>>t.rotateSun>>t.sky>>t.film>>t.fog>>t.hour>>t.cycleSeconds>>t.daySpeed>>t.nightSpeed>>t.azimuth>>t.elevation>>t.sunTransition>>t.filmTransition>>t.fogTransition>>t.skyIntensity>>t.sunDisk>>t.starIntensity>>t.rayleigh>>t.mie>>t.mieDirection>>t.sunsetColor.x>>t.sunsetColor.y>>t.sunsetColor.z>>t.coverage>>t.thickness>>t.absorption>>t.cloudScale>>t.cloudExposure>>t.windAngle>>t.windSpeed>>t.cloudSteps>>t.lightSteps>>t.starTrailLength>>t.starTrailBrightness>>t.starTrailFade>>t.starPoleElevation>>t.starPoleAzimuth;for(auto&v:t.states)i>>v;if(i){t.sanitize();s=t;}return i;}
inline double wrap(double v,double length){return v-length*std::floor(v/length);}
// Piecewise time parameterization gives exact independent day/night rates and
// reproducible backward seeks; no history-dependent frame-by-frame interpolation.
inline float hourAt(const Settings&s,double seconds){if(!s.running||!s.rotateSun)return float(wrap(s.hour,24));double h=wrap(s.hour-6,24),d=12/s.daySpeed,n=12/s.nightSpeed;double t=h<=12?h/s.daySpeed:d+(h-12)/s.nightSpeed;t=wrap(t+(std::isfinite(seconds)?seconds:0)*24/s.cycleSeconds,d+n);return float(wrap(6+(t<d?t*s.daySpeed:12+(t-d)*s.nightSpeed),24));}
inline float starAngleAt(const Settings&s,double seconds){return float(wrap(hourAt(s,seconds)*15.0+(!s.running&&s.starAutoRotate&&std::isfinite(seconds)?seconds*s.starRotationSpeed:0.0),360.0))*scene::kPi/180.f;}
inline State blend(const State&a,const State&b,float t){State r;auto f=[&](float x,float y){return x+(y-x)*t;};auto v=[&](scene::Vec3 x,scene::Vec3 y){return x+(y-x)*t;};r.sunlight=f(a.sunlight,b.sunlight);r.ambient=f(a.ambient,b.ambient);r.sun=v(a.sun,b.sun);r.ambientColor=v(a.ambientColor,b.ambientColor);r.brightness=f(a.brightness,b.brightness);r.contrast=f(a.contrast,b.contrast);r.desaturation=f(a.desaturation,b.desaturation);r.dark=v(a.dark,b.dark);r.light=v(a.light,b.light);r.fogAmount=f(a.fogAmount,b.fogAmount);r.fogStart=f(a.fogStart,b.fogStart);r.fogHalfDistance=f(a.fogHalfDistance,b.fogHalfDistance);r.fog=v(a.fog,b.fog);return r;}
inline State stateAt(const Settings&s,float hour,float width){const float height=std::sin((hour-6)*scene::kPi/12),edge=std::sin(width*scene::kPi/12);return height>=0?blend(s.states[1],s.states[0],smooth(height/edge)):blend(s.states[1],s.states[2],smooth(-height/edge));}
struct Sample {float hour{};scene::Vec3 sunDirection{},lightDirection{};State sun,film,fog;};
inline Sample evaluate(const Settings&s,double seconds){Sample r;r.hour=hourAt(s,seconds);const float a=s.azimuth*scene::kPi/180,p=(r.hour-6)*scene::kPi/12;const float z=s.rotateSun?std::sin(p):std::sin(s.elevation*scene::kPi/180),h=s.rotateSun?std::cos(p):std::cos(s.elevation*scene::kPi/180);r.sunDirection={std::cos(a)*h,std::sin(a)*h,z}; // Direction toward the visible sun.
    r.lightDirection=z>=0?r.sunDirection*-1.f:r.sunDirection; // Moonlight uses the opposite hemisphere.
    const float phase=s.rotateSun?r.hour:6+std::asin(z)*12/scene::kPi;r.sun=stateAt(s,phase,s.sunTransition);r.film=stateAt(s,phase,s.filmTransition);r.fog=stateAt(s,phase,s.fogTransition);return r;}
}
