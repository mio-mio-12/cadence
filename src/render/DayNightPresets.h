#pragma once
#include "render/DayNight.h"
#include "render/NightSky.h"
#include <string>
#include <string_view>
namespace render::daynight {
struct Preset {Settings cycle;NightSkySettings sky;};
inline constexpr std::array<std::string_view,5> presetNames{
    "Natural daylight to moonlight","Warm cinematic cycle","Clear astronomical night","Moonlit coastal cycle","Urban haze cycle"};
inline Preset builtinPreset(size_t index){
    Preset p;auto&s=p.cycle;auto&n=p.sky;s.enabled=true;s.running=true;s.cycleSeconds=900;s.daySpeed=s.nightSpeed=1;s.coverage=.32f;s.cloudSteps=16;s.lightSteps=2;
    // Balanced grading avoids crushing the new sky or needing excessive radiance.
    auto&day=s.states[0];auto&twilight=s.states[1];auto&night=s.states[2];
    day.sunlight=1.1f;day.desaturation=.02f;day.sun={1,.93f,.85f};
    twilight.sunlight=0;twilight.brightness=0;twilight.contrast=1.02f;twilight.fogAmount=.25f;
    night.brightness=0;night.contrast=1;night.desaturation=.12f;night.dark={.88f,.94f,1};night.light={1,1,1};night.sunlight=.22f;night.ambient=.14f;night.fogAmount=.25f;
    n.shadowLift=.002f;n.exposure=.5f;n.milkyWay=.18f;n.starGlow=.1f;n.horizonGlow=.2f;n.galaxyNoiseAlgorithm=4;
    if(index==1){s.hour=17;s.coverage=.4f;s.sunTransition=s.filmTransition=s.fogTransition=3;s.nightSpeed=1.4f;s.sunsetColor={.95f,.43f,.19f};day.sun={1,.87f,.72f};twilight.sun={1,.58f,.3f};twilight.fog={.36f,.22f,.16f};twilight.ambientColor={.55f,.43f,.5f};night.dark={.8f,.91f,1};n.horizonGlow=.35f;n.horizonR=.15f;n.horizonG=.065f;n.horizonB=.025f;}
    if(index==2){s.hour=0;s.cycleSeconds=1200;s.nightSpeed=.6f;s.coverage=.04f;s.starIntensity=.5f;night.fogAmount=.08f;night.sunlight=.08f;night.ambient=.1f;n.exposure=1.2f;n.moonIntensity=0;n.milkyWay=.4f;n.galaxyTilt=65;n.galaxyAzimuth=180;n.galaxyNoiseAlgorithm=4;n.galaxyNoiseScale=2.8f;n.galaxyNoiseDetail=.7f;n.galaxyNoiseContrast=1.f;n.horizonGlow=.04f;n.starDensity=1;n.starSize=.85f;}
    if(index==3){s.hour=22;s.coverage=.4f;s.windSpeed=.018f;s.starIntensity=.22f;night.sunlight=.38f;night.ambient=.2f;night.sun={.68f,.79f,1};night.fog={.045f,.065f,.09f};night.fogAmount=.3f;night.fogHalfDistance=150;n.moonIntensity=1.5f;n.moonPhase=1;n.moonHalo=.35f;n.moonElevation=42;n.moonAzimuth=25;n.cloudLight=1.5f;n.starDensity=.55f;n.milkyWay=.08f;}
    if(index==4){s.hour=21;s.coverage=.6f;s.starIntensity=.12f;s.windSpeed=.012f;day.fogAmount=.45f;day.fogHalfDistance=180;night.sunlight=.15f;night.ambient=.22f;night.ambientColor={.72f,.62f,.53f};night.fog={.09f,.065f,.05f};night.fogAmount=.5f;night.fogHalfDistance=110;n.exposure=.3f;n.horizonGlow=1.1f;n.horizonR=.15f;n.horizonG=.075f;n.horizonB=.035f;n.milkyWay=0;n.starDensity=.25f;n.moonIntensity=.25f;n.cloudLight=1.4f;}
    s.sanitize();n.sanitize();return p;
}
inline void writePreset(std::ostream&out,const Preset&p){out<<"CADENCE_DAY_NIGHT 3\n"<<p.cycle<<'\n'<<p.sky<<'\n'<<p.sky.galaxyNoiseAlgorithm<<' '<<p.sky.galaxyNoiseScale<<' '<<p.sky.galaxyNoiseDetail<<' '<<p.sky.galaxyNoiseContrast<<'\n'<<p.cycle.starAutoRotate<<' '<<p.cycle.starRotationSpeed<<'\n';}
inline bool readPreset(std::istream&in,Preset&p){std::string magic;int version{};Preset staged;if(!(in>>magic>>version)||magic!="CADENCE_DAY_NIGHT"||(version<1||version>3)||!(in>>staged.cycle>>staged.sky))return false;if(version>=2){if(!(in>>staged.sky.galaxyNoiseAlgorithm>>staged.sky.galaxyNoiseScale>>staged.sky.galaxyNoiseDetail>>staged.sky.galaxyNoiseContrast))return false;staged.sky.sanitize();}if(version>=3){if(!(in>>staged.cycle.starAutoRotate>>staged.cycle.starRotationSpeed))return false;staged.cycle.sanitize();}in>>std::ws;if(!in.eof())return false;p=staged;return true;}
}
