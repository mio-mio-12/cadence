#include "render/DayNight.h"
#include "render/NightSky.h"
#include "render/DayNightPresets.h"
#include "app/ReplayControls.h"
#include <sstream>
#include <iostream>
#include <limits>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(){using namespace render::daynight;Settings s;s.enabled=true;
 {Settings stars;stars.running=false;stars.starAutoRotate=true;stars.starRotationSpeed=2;CHECK(std::abs(starAngleAt(stars,10)-starAngleAt(stars,0)-20*scene::kPi/180)<.0001);CHECK(hourAt(stars,10)==hourAt(stars,0));stars.running=true;stars.sanitize();CHECK(!stars.starAutoRotate);CHECK(std::abs(starAngleAt(stars,10)-hourAt(stars,10)*scene::kPi/12)<.0001);}
 CHECK(hourAt(s,0)==12);s.daySpeed=s.nightSpeed=1;s.cycleSeconds=240;
 CHECK(std::abs(hourAt(s,60)-18)<.001);CHECK(std::abs(hourAt(s,120))<.001);CHECK(std::abs(hourAt(s,240)-12)<.001);
 auto noon=evaluate(s,0),night=evaluate(s,120);CHECK(noon.sunDirection.z>.99);CHECK(night.sunDirection.z<-.99);CHECK(night.lightDirection.z<-.99);
 CHECK(noon.sun.sunlight>night.sun.sunlight);
 for(double t:{-1e6,-1.,0.,1.,123.4,1e6}){auto a=evaluate(s,t);CHECK(std::isfinite(a.sunDirection.z));CHECK(std::abs(scene::length(a.sunDirection)-1)<.0001);CHECK(a.hour>=0&&a.hour<24);CHECK(a.sun.sunlight>=0);}
 auto sampled=evaluate(s,13.375);evaluate(s,180);auto again=evaluate(s,13.375);CHECK(sampled.hour==again.hour&&sampled.sun.sunlight==again.sun.sunlight);
 for(float h:{0.f,6.f,12.f,18.f,24.f}){s.hour=h;auto a=evaluate(s,-.0001),b=evaluate(s,.0001);CHECK(std::abs(a.film.brightness-b.film.brightness)<.0001);}
 s.running=false;s.hour=17;CHECK(hourAt(s,100)==17);s.rotateSun=false;s.elevation=-20;CHECK(evaluate(s,0).sunDirection.z<0);
 s.starTrailLength=30;s.starTrailFade=2;s.starPoleElevation=60;std::stringstream stream;stream<<s;Settings restored;stream>>restored;CHECK(!stream.fail());CHECK(restored.starTrailLength==30&&restored.starPoleElevation==60&&restored.hour==17);
 std::stringstream broken("1 0 1");Settings unchanged;broken>>unchanged;CHECK(broken.fail());CHECK(unchanged.hour==12&&!unchanged.enabled);
 s.hour=std::numeric_limits<float>::quiet_NaN();s.cycleSeconds=0;s.coverage=20;s.cloudSteps=900;s.sanitize();CHECK(s.hour==12&&s.cycleSeconds==10&&s.coverage==1&&s.cloudSteps==32);
 using namespace cadence::replay;CHECK(speedStep(1,true)==1.25f);CHECK(speedStep(1,false)==.75f);CHECK(speedStep(50,true)==50);CHECK(speedStep(.001f,false)==.001f);
 CHECK(seek(5,true,20,5.5f,12)==5.5f);CHECK(seek(5.5f,true,20,5.5f,12)==6.5f);CHECK(seek(.1f,false,20,0,20)==0);CHECK(seek(19.9f,true,20,0,20)==20);
 render::NightSkySettings photo;photo.exposure=3;photo.starDensity=.4f;photo.moonPhase=.3f;std::stringstream photoStream;photoStream<<photo;render::NightSkySettings photoCopy;photoStream>>photoCopy;CHECK(!photoStream.fail()&&photoCopy.exposure==3&&photoCopy.starDensity==.4f&&photoCopy.moonPhase==.3f);
 std::istringstream partial("1 2");partial>>photoCopy;CHECK(partial.fail()&&photoCopy.exposure==3);photo.exposure=std::numeric_limits<float>::quiet_NaN();photo.moonPhase=9;photo.starSize=999;photo.sanitize();CHECK(photo.exposure==1.5f&&photo.moonPhase==1&&photo.starSize==2);
 for(size_t preset=0;preset<presetNames.size();++preset){auto p=builtinPreset(preset);std::stringstream out;writePreset(out,p);Preset restored;CHECK(readPreset(out,restored));CHECK(restored.cycle.enabled&&restored.cycle.running);CHECK(restored.sky.exposure==p.sky.exposure);CHECK(restored.sky.galaxyNoiseAlgorithm==4);for(float hour:{0.f,6.f,12.f,18.f,24.f}){restored.cycle.hour=hour;const auto sample=evaluate(restored.cycle,0);CHECK(std::isfinite(sample.sun.sunlight)&&sample.sun.sunlight>=0&&std::isfinite(sample.fog.fogHalfDistance));}}
 Preset p;std::istringstream badPreset("CADENCE_DAY_NIGHT 7");CHECK(!readPreset(badPreset,p)&&!p.cycle.enabled);
 p=builtinPreset(2);p.sky.galaxyNoiseAlgorithm=3;p.sky.galaxyNoiseScale=2.5f;p.sky.galaxyNoiseContrast=1.4f;std::stringstream noise;writePreset(noise,p);Preset restoredNoise;CHECK(readPreset(noise,restoredNoise)&&restoredNoise.sky.galaxyNoiseAlgorithm==3&&restoredNoise.sky.galaxyNoiseScale==2.5f&&restoredNoise.sky.galaxyNoiseContrast==1.4f);std::stringstream legacy;legacy<<"CADENCE_DAY_NIGHT 1\n"<<p.cycle<<'\n'<<p.sky<<'\n';CHECK(readPreset(legacy,restoredNoise)&&restoredNoise.sky.galaxyNoiseAlgorithm==0&&restoredNoise.sky.galaxyNoiseScale==1);
 {Preset stars;stars.cycle.running=false;stars.cycle.starAutoRotate=true;stars.cycle.starRotationSpeed=-2;std::stringstream out;writePreset(out,stars);Preset copy;CHECK(readPreset(out,copy)&&copy.cycle.starAutoRotate&&copy.cycle.starRotationSpeed==-2);CHECK(starAngleAt(copy.cycle,4)==starAngleAt(stars.cycle,4));}
 std::cout<<"Day/night clocks, continuity, pure replay seeks, settings and replay controls passed\n";
}
