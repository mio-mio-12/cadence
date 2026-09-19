#pragma once
#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
namespace render {
struct VolumetricLightingSettings {
    bool enabled{};
    float density{.0004f},intensity{1.f}; // Density per scene centimetre.
    float red{1.f},green{.95f},blue{.85f};
    float height{300.f},heightFalloff{500.f},anisotropy{.35f},maxDistance{3000.f};
    int steps{16},resolutionDivisor{4};
    void sanitize(){const auto c=[](float v,float lo,float hi,float fallback){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
        density=c(density,0,.02f,.0004f);intensity=c(intensity,0,10,1);
        red=c(red,0,4,1);green=c(green,0,4,.95f);blue=c(blue,0,4,.85f);
        height=c(height,-100000,100000,300);heightFalloff=c(heightFalloff,1,100000,500);
        anisotropy=c(anisotropy,-.85f,.85f,.35f);maxDistance=c(maxDistance,1,50000,3000);
        steps=steps<=16?16:steps<=32?32:64;resolutionDivisor=resolutionDivisor<=1?1:resolutionDivisor<=2?2:4;
    }
};
inline std::ostream& operator<<(std::ostream&o,const VolumetricLightingSettings&s){return o<<s.enabled<<' '<<s.density<<' '<<s.intensity<<' '<<s.red<<' '<<s.green<<' '<<s.blue<<' '<<s.height<<' '<<s.heightFalloff<<' '<<s.anisotropy<<' '<<s.maxDistance<<' '<<s.steps<<' '<<s.resolutionDivisor;}
inline std::istream& operator>>(std::istream&i,VolumetricLightingSettings&s){return i>>s.enabled>>s.density>>s.intensity>>s.red>>s.green>>s.blue>>s.height>>s.heightFalloff>>s.anisotropy>>s.maxDistance>>s.steps>>s.resolutionDivisor;}
}
