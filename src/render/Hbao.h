#pragma once
#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>

namespace render {
struct HbaoSettings {
    bool enabled{},preview{},weaponBackgroundHalo{};
    bool separateTransparent{true};
    bool beforeFog{}; // Optional fog-aware composition; old presets retain their appearance.
    float transparentGap{8.f}; // World-space separation, never camera distance.
    float radius{60.f},intensity{1.5f},power{1.8f},biasDegrees{10.f},falloff{1.f};
    int directions{8},steps{6},blurRadius{4};
    float blurSharpness{16.f},maxPixels{160.f};
    void sanitize(){
        const auto clamp=[](float v,float lo,float hi,float fallback){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
        radius=clamp(radius,1,500,60);intensity=clamp(intensity,0,4,1.5f);power=clamp(power,.25f,4,1.8f);
        transparentGap=clamp(transparentGap,.1f,100,8);
        biasDegrees=clamp(biasDegrees,0,45,10);falloff=clamp(falloff,.25f,4,1);
        directions=std::clamp(directions,4,16);steps=std::clamp(steps,2,12);blurRadius=std::clamp(blurRadius,0,8);
        blurSharpness=clamp(blurSharpness,1,128,16);maxPixels=clamp(maxPixels,8,512,160);
    }
};
inline std::ostream& operator<<(std::ostream& o,const HbaoSettings& s){return o<<s.enabled<<' '<<s.preview<<' '<<s.radius<<' '<<s.intensity<<' '<<s.power<<' '<<s.biasDegrees<<' '<<s.falloff<<' '<<s.directions<<' '<<s.steps<<' '<<s.blurRadius<<' '<<s.blurSharpness<<' '<<s.maxPixels<<' '<<s.weaponBackgroundHalo;}
inline std::istream& operator>>(std::istream& i,HbaoSettings& s){return i>>s.enabled>>s.preview>>s.radius>>s.intensity>>s.power>>s.biasDegrees>>s.falloff>>s.directions>>s.steps>>s.blurRadius>>s.blurSharpness>>s.maxPixels>>s.weaponBackgroundHalo;}
}
