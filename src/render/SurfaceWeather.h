#pragma once
#include <cmath>
#include <istream>
#include <ostream>
namespace render {
struct WetDetail {
    bool viewhands{true},coatAll{};
    float randomness{1.3f},flowDisplacement{.541f},flowDetail{7.721f},edgeNoise{.2f},edgeDetail{8};
    float puddleRoughness{.08f},reflectance{.02037f},reflection{1},normalStrength{.342f};
};
inline std::ostream& operator<<(std::ostream& o,const WetDetail& s){return o<<s.viewhands<<' '<<s.coatAll<<' '<<s.randomness<<' '<<s.flowDisplacement<<' '<<s.flowDetail<<' '<<s.edgeNoise<<' '<<s.edgeDetail<<' '<<s.puddleRoughness<<' '<<s.reflectance<<' '<<s.reflection<<' '<<s.normalStrength;}
inline std::istream& operator>>(std::istream& in,WetDetail& s){WetDetail p;if(in>>p.viewhands>>p.coatAll>>p.randomness>>p.flowDisplacement>>p.flowDetail>>p.edgeNoise>>p.edgeDetail>>p.puddleRoughness>>p.reflectance>>p.reflection>>p.normalStrength){for(float v:{p.randomness,p.flowDisplacement,p.flowDetail,p.edgeNoise,p.edgeDetail,p.puddleRoughness,p.reflectance,p.reflection,p.normalStrength})if(!std::isfinite(v)){in.setstate(std::ios::failbit);return in;}s=p;}return in;}
struct WetSettings {
    WetDetail detail{};
    bool ground{},enabled{},map{true},actors{true},viewmodels{true};
    float amount{.7f},darkening{.22f},roughness{.12f},coverage{.48f},patchSize{180},edge{.1f};
    float droplets{1},dropSize{.23f},rivulets{.277f},flow{1},ripples{.25f},coat{1};
    void sanitize(){WetSettings d;float* p[]={&amount,&darkening,&roughness,&coverage,&patchSize,&edge,&droplets,&dropSize,&rivulets,&flow,&ripples,&coat};
        const float v[]={d.amount,d.darkening,d.roughness,d.coverage,d.patchSize,d.edge,d.droplets,d.dropSize,d.rivulets,d.flow,d.ripples,d.coat};
        for(int i=0;i<12;++i)if(!std::isfinite(*p[i]))*p[i]=v[i];}
};
inline std::ostream& operator<<(std::ostream& o,const WetSettings& s){return o<<s.ground<<' '<<s.enabled<<' '<<s.map<<' '<<s.actors<<' '<<s.viewmodels<<' '<<s.amount<<' '<<s.darkening<<' '<<s.roughness<<' '<<s.coverage<<' '<<s.patchSize<<' '<<s.edge<<' '<<s.droplets<<' '<<s.dropSize<<' '<<s.rivulets<<' '<<s.flow<<' '<<s.ripples<<' '<<s.coat;}
inline std::istream& operator>>(std::istream& in,WetSettings& s){WetSettings p;if(in>>p.ground>>p.enabled>>p.map>>p.actors>>p.viewmodels>>p.amount>>p.darkening>>p.roughness>>p.coverage>>p.patchSize>>p.edge>>p.droplets>>p.dropSize>>p.rivulets>>p.flow>>p.ripples>>p.coat){p.sanitize();s=p;}return in;}
struct MuzzleLightSettings {bool enabled{};int shape{1};float radius{1000},intensity{.07f},falloff{.1f},cone{270},aspect{1};};
inline std::ostream& operator<<(std::ostream& o,const MuzzleLightSettings& s){return o<<s.enabled<<' '<<s.shape<<' '<<s.radius<<' '<<s.intensity<<' '<<s.falloff<<' '<<s.cone<<' '<<s.aspect;}
inline std::istream& operator>>(std::istream& in,MuzzleLightSettings& s){MuzzleLightSettings p;if(in>>p.enabled>>p.shape>>p.radius>>p.intensity>>p.falloff>>p.cone>>p.aspect){for(float v:{p.radius,p.intensity,p.falloff,p.cone,p.aspect})if(!std::isfinite(v)){in.setstate(std::ios::failbit);return in;}s=p;}return in;}
}
