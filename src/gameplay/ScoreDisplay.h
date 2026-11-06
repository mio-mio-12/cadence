#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <tuple>

namespace gameplay::score {
// Animation controls match the BO2 Toolkit score workspace; font is a desktop
// family name rather than a BO2 resource enum.
struct Settings {
    float x{18},y{-32},moveIn{.12f},startScale{1},finalScale{1},overshootScale{1.12f};
    float impactIn{.08f},impactOut{.08f},enterX{},enterY{12},exitX{},exitY{-8};
    int shuffleFrames{5};float shuffleStep{.05f},hold{.85f},fade{.45f};
    float color[3]{1,1,1};bool glow{};float glowColor[3]{.72f,.82f,1},glowAlpha{.65f};
    int perKill{100};bool stacking{true};float window{1.f};std::string font{"Segoe UI"};
    int anchor{},pivot{}; // Center/corners/custom; text center/top-left/bottom/custom/point centroid.
    float anchorX{.5f},anchorY{.5f},pivotX{.5f},pivotY{.5f};
    float rotation{},enterRotation{},exitRotation{};
    bool shadow{};float shadowColor[3]{0,0,0},shadowAlpha{.65f},shadowDirection{45},shadowOffset{3},shadowSoftness{2};
    float glowDirection{},glowOffset{},glowSoftness{3};
};
struct State {
    double last{-1e30};std::int64_t amount{};unsigned count{};
    void kill(const Settings& s,double time){
        if(!std::isfinite(time))return;
        if(!s.stacking||time<last||time-last>std::max(0.f,s.window)){amount=0;count=0;}
        amount=std::clamp<std::int64_t>(amount+s.perKill,-999999999,999999999);++count;last=time;
    }
};
struct Sample {float x{},y{},scale{1},alpha{};std::string text;float rotation{};};
inline float duration(const Settings& s){return std::max(s.moveIn,s.impactIn+s.impactOut)+s.shuffleFrames*s.shuffleStep+s.hold+s.fade;}
inline Sample sample(const Settings& s,const State& state,double now){
    Sample p{s.x,s.y,s.finalScale,0,{}};const float t=float(now-state.last);if(t<0||t>=duration(s))return p;
    p.alpha=1;const float move=std::clamp(t/std::max(.0001f,s.moveIn),0.f,1.f);
    p.x+=s.enterX*(1-move);p.y+=s.enterY*(1-move);p.rotation=s.rotation+s.enterRotation*(1-move);
    if(t<s.impactIn)p.scale=s.startScale+(s.overshootScale-s.startScale)*t/std::max(.0001f,s.impactIn);
    else p.scale=s.overshootScale+(s.finalScale-s.overshootScale)*std::clamp((t-s.impactIn)/std::max(.0001f,s.impactOut),0.f,1.f);
    const float f=std::clamp((t-(duration(s)-s.fade))/std::max(.0001f,s.fade),0.f,1.f);p.alpha=1-f;p.x+=s.exitX*f;p.y+=s.exitY*f;p.rotation+=s.exitRotation*f;
    auto amount=state.amount;const float enter=std::max(s.moveIn,s.impactIn+s.impactOut);
    if(t>=enter&&t<enter+s.shuffleFrames*s.shuffleStep&&s.shuffleStep>0){const auto tick=unsigned((t-enter)/s.shuffleStep);amount=(tick*747796405u+state.count*2891336453u)%1000;}
    p.text=(amount>=0?"+":"")+std::to_string(amount);return p;
}
inline auto fields(Settings& s){return std::tie(s.x,s.y,s.moveIn,s.startScale,s.finalScale,s.overshootScale,s.impactIn,s.impactOut,s.enterX,s.enterY,s.exitX,s.exitY,s.shuffleFrames,s.shuffleStep,s.hold,s.fade,s.color[0],s.color[1],s.color[2],s.glow,s.glowColor[0],s.glowColor[1],s.glowColor[2],s.glowAlpha,s.perKill,s.stacking,s.window);}
inline auto transformFields(Settings& s){return std::tie(s.anchor,s.pivot,s.anchorX,s.anchorY,s.pivotX,s.pivotY,s.rotation,s.enterRotation,s.exitRotation);}
inline auto effectFields(Settings& s){return std::tie(s.shadow,s.shadowColor[0],s.shadowColor[1],s.shadowColor[2],s.shadowAlpha,s.shadowDirection,s.shadowOffset,s.shadowSoftness,s.glowDirection,s.glowOffset,s.glowSoftness);}
inline bool save(Settings s,const std::filesystem::path& path){std::error_code ec;if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),ec);if(ec)return false;std::ofstream o(path);o<<"CADENCESCORE 3\n"<<std::setprecision(9);std::apply([&](auto&...v){((o<<v<<' '),...);},fields(s));o<<std::quoted(s.font)<<'\n';std::apply([&](auto&...v){((o<<v<<' '),...);},transformFields(s));o<<'\n';std::apply([&](auto&...v){((o<<v<<' '),...);},effectFields(s));o<<'\n';return bool(o);}
inline bool load(Settings& s,const std::filesystem::path& path){
    std::ifstream i(path);std::string magic;int version{};Settings next;
    if(!(i>>magic>>version)||magic!="CADENCESCORE"||version<1||version>3)return false;
    std::apply([&](auto&...v){((i>>v),...);},fields(next));i>>std::quoted(next.font);
    if(version>=2)std::apply([&](auto&...v){((i>>v),...);},transformFields(next));else next.pivot=1; // Preserve v1 top-left positioning.
    if(version>=3)std::apply([&](auto&...v){((i>>v),...);},effectFields(next));
    bool finite=true;const auto validate=[&](auto&...v){((finite=finite&&std::isfinite(double(v))),...);};
    std::apply(validate,fields(next));std::apply(validate,transformFields(next));std::apply(validate,effectFields(next));
    if(!i||!finite||next.moveIn<0||next.impactIn<0||next.impactOut<0||next.hold<0||next.fade<0||next.shuffleFrames<0||next.shuffleFrames>100||next.shuffleStep<0||next.window<0||next.finalScale<=0||next.startScale<=0||next.overshootScale<=0||next.anchor<0||next.anchor>5||next.pivot<0||next.pivot>4)return false;
    if(next.shadowAlpha<0||next.shadowAlpha>1||next.glowAlpha<0||next.glowAlpha>1||next.shadowSoftness<0||next.shadowSoftness>64||next.glowSoftness<0||next.glowSoftness>64||std::abs(next.shadowOffset)>1000||std::abs(next.glowOffset)>1000)return false;
    s=next;return true;
}
}
