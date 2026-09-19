#pragma once
#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
namespace render {
struct DepthOfFieldSettings {
    bool enabled{},affectViewmodel{},preview{};
    float focusDistance{1000},focusRange{200},nearTransition{300},farTransition{2500};
    float nearRadius{8},farRadius{12},gamma{1},bokehAmount{.5f},bokehThreshold{.8f},hollowness{},anamorphic{1},rotationDegrees{};
    int blades{},samples{24},downsample{2};
    void sanitize(){
        const auto f=[](float v,float l,float h,float d){return std::isfinite(v)?std::clamp(v,l,h):d;};
        focusDistance=f(focusDistance,1,100000,1000);focusRange=f(focusRange,0,100000,200);nearTransition=f(nearTransition,1,100000,300);farTransition=f(farTransition,1,100000,2500);
        nearRadius=f(nearRadius,0,40,8);farRadius=f(farRadius,0,40,12);gamma=std::isfinite(gamma)?std::max(.25f,gamma):1.f;bokehAmount=f(bokehAmount,0,4,.5f);bokehThreshold=f(bokehThreshold,0,4,.8f);hollowness=f(hollowness,0,.95f,0);anamorphic=f(anamorphic,.25f,4,1);rotationDegrees=f(rotationDegrees,-180,180,0);
        blades=blades==0?0:std::clamp(blades,3,10);samples=std::clamp(samples,8,96);downsample=downsample>=4?4:downsample==1?1:2;
    }
};
inline std::ostream& operator<<(std::ostream& o,const DepthOfFieldSettings& s){return o<<s.enabled<<' '<<s.affectViewmodel<<' '<<s.preview<<' '<<s.focusDistance<<' '<<s.focusRange<<' '<<s.nearTransition<<' '<<s.farTransition<<' '<<s.nearRadius<<' '<<s.farRadius<<' '<<s.gamma<<' '<<s.bokehAmount<<' '<<s.bokehThreshold<<' '<<s.hollowness<<' '<<s.anamorphic<<' '<<s.rotationDegrees<<' '<<s.blades<<' '<<s.samples<<' '<<s.downsample;}
inline std::istream& operator>>(std::istream& i,DepthOfFieldSettings& s){return i>>s.enabled>>s.affectViewmodel>>s.preview>>s.focusDistance>>s.focusRange>>s.nearTransition>>s.farTransition>>s.nearRadius>>s.farRadius>>s.gamma>>s.bokehAmount>>s.bokehThreshold>>s.hollowness>>s.anamorphic>>s.rotationDegrees>>s.blades>>s.samples>>s.downsample;}
}
