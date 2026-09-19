#pragma once
#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
namespace render {
// Separate optional preset record keeps v147 day_night records readable.
struct NightSkySettings {
    bool enabled{true};
    float exposure{1.5f},shadowLift{.09f},saturation{.8f};
    float zenithR{.014f},zenithG{.022f},zenithB{.045f};
    float horizonGlow{.3f},horizonR{.12f},horizonG{.09f},horizonB{.065f};
    float starDensity{.8f},starSize{1},starColor{.5f},starGlow{.2f};
    float milkyWay{.3f},galaxyWidth{12},galaxyTilt{55},galaxyAzimuth{25};
    float moonIntensity{.6f},moonSize{.52f},moonPhase{.85f},moonElevation{35},moonAzimuth{30},moonHalo{.2f};
    float cloudLight{1};
    int galaxyNoiseAlgorithm{}; // 0..3 legacy procedural; 4 bundled ESO panorama.
    float galaxyNoiseScale{1},galaxyNoiseDetail{1},galaxyNoiseContrast{1};
    void sanitize(){auto c=[](float&v,float lo,float hi,float d){v=std::isfinite(v)?std::clamp(v,lo,hi):d;};
        galaxyNoiseAlgorithm=std::clamp(galaxyNoiseAlgorithm,0,4);c(galaxyNoiseScale,.2f,6,1);c(galaxyNoiseDetail,.25f,3,1);c(galaxyNoiseContrast,.25f,4,1);
        c(exposure,-4,6,1.5f);c(shadowLift,0,.3f,.09f);c(saturation,0,2,.8f);
        c(zenithR,0,1,.014f);c(zenithG,0,1,.022f);c(zenithB,0,1,.045f);c(horizonGlow,0,3,.3f);c(horizonR,0,1,.12f);c(horizonG,0,1,.09f);c(horizonB,0,1,.065f);
        c(starDensity,0,1,.8f);c(starSize,.4f,2,1);c(starColor,0,1,.5f);c(starGlow,0,2,.2f);c(milkyWay,0,3,.3f);c(galaxyWidth,3,35,12);c(galaxyTilt,-90,90,55);c(galaxyAzimuth,-180,180,25);
        c(moonIntensity,0,5,.6f);c(moonSize,.1f,3,.52f);c(moonPhase,0,1,.85f);c(moonElevation,-10,90,35);c(moonAzimuth,-180,180,30);c(moonHalo,0,3,.2f);c(cloudLight,0,5,1);
    }
};
inline std::ostream& operator<<(std::ostream&o,const NightSkySettings&s){return o<<s.enabled<<' '<<s.exposure<<' '<<s.shadowLift<<' '<<s.saturation<<' '<<s.zenithR<<' '<<s.zenithG<<' '<<s.zenithB<<' '<<s.horizonGlow<<' '<<s.horizonR<<' '<<s.horizonG<<' '<<s.horizonB<<' '<<s.starDensity<<' '<<s.starSize<<' '<<s.starColor<<' '<<s.starGlow<<' '<<s.milkyWay<<' '<<s.galaxyWidth<<' '<<s.galaxyTilt<<' '<<s.galaxyAzimuth<<' '<<s.moonIntensity<<' '<<s.moonSize<<' '<<s.moonPhase<<' '<<s.moonElevation<<' '<<s.moonAzimuth<<' '<<s.moonHalo<<' '<<s.cloudLight;}
inline std::istream& operator>>(std::istream&i,NightSkySettings&s){NightSkySettings t=s;i>>t.enabled>>t.exposure>>t.shadowLift>>t.saturation>>t.zenithR>>t.zenithG>>t.zenithB>>t.horizonGlow>>t.horizonR>>t.horizonG>>t.horizonB>>t.starDensity>>t.starSize>>t.starColor>>t.starGlow>>t.milkyWay>>t.galaxyWidth>>t.galaxyTilt>>t.galaxyAzimuth>>t.moonIntensity>>t.moonSize>>t.moonPhase>>t.moonElevation>>t.moonAzimuth>>t.moonHalo>>t.cloudLight;if(i){t.sanitize();s=t;}return i;}
}
