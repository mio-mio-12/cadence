#pragma once

#include "assets/AssetCatalog.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string_view>

namespace benchmark {
inline constexpr std::size_t npos=std::numeric_limits<std::size_t>::max();

inline std::string normalized(std::string_view text){
    std::string result;
    for(unsigned char c:text)if(std::isalnum(c))result.push_back(static_cast<char>(std::tolower(c)));
    return result;
}

inline bool hasWeaponFamily(std::string_view name,std::string_view family){
    std::string token;
    for(std::size_t i=0;i<=name.size();++i){
        const auto c=i<name.size()?static_cast<unsigned char>(name[i]):static_cast<unsigned char>(0);
        if(std::isalnum(c)){token.push_back(static_cast<char>(std::tolower(c)));continue;}
        if(token==family)return true;
        token.clear();
    }
    return false;
}

// These are known exclusions, not a claim that all other assets are authentic.
inline bool isPlaceholder(std::string_view game,std::string_view name){
    const auto g=normalized(game),n=normalized(name);
    return n.find("placeholder")!=std::string::npos||n.find("dummy")!=std::string::npos||
        ((g=="mw3"||g=="iw5"||g=="modernwarfare3")&&n.find("ak47")!=std::string::npos);
}

// Deliberately refuse unknown classes rather than silently selecting a knife,
// sniper, attachment, or incomplete model as a representative rifle/pistol.
inline std::size_t selectWeapon(const assets::Catalog& catalog,std::string_view game,bool pistol){
    static constexpr std::string_view rifles[]={"m4a1","m4","an94","hk416","m16","ak47","acr","scar","bal27","arx160","g36","famas","galil","commando","ak12","hbra3","nv4"};
    static constexpr std::string_view pistols[]={"m1911","1911","usp","p99","fiveseven","beretta","m9","glock","p2000","deagle","deserteagle","b23r","tac45","kap40","rw1","atlas45","emc"};
    const auto wantedGame=normalized(game);
    std::size_t best=npos;
    int bestScore=-1;
    for(std::size_t i=0;i<catalog.entries.size();++i){
        const auto& asset=catalog.entries[i];
        if(asset.role!=assets::Role::ViewWeapon||normalized(asset.game)!=wantedGame||isPlaceholder(asset.game,asset.name))continue;
        const auto category=normalized(asset.category);
        const bool pistolClass=category=="pistol"||category=="pistols"||category=="handgun"||category=="handguns"||category=="hg";
        const bool rifleClass=category=="ar"||category=="rifle"||category=="rifles"||category=="assaultrifle"||category=="assaultrifles"||category=="assault";
        const bool excludedClass=category=="sniper"||category=="snipers"||category=="sniperrifles"||category=="smg"||category=="smgs"||category=="lmg"||category=="lmgs"||category=="shotgun"||category=="shotguns"||category=="knife"||category=="knives"||category=="melee"||category=="launcher"||category=="launchers";
        if(excludedClass||(pistol?rifleClass:pistolClass))continue;
        int score=(pistol?pistolClass:rifleClass)?10:-1;
        const auto findPreferred=[&](const auto& choices){
            for(std::size_t j=0;j<std::size(choices);++j)if(hasWeaponFamily(asset.name,choices[j])){score=100-static_cast<int>(j);break;}
        };
        if(pistol)findPreferred(pistols);else findPreferred(rifles);
        if(score>bestScore){best=i;bestScore=score;}
    }
    return best;
}

// Percent is in [0,100]; linear interpolation, ignoring invalid timing samples.
inline double percentile(std::vector<double> values,double percent){
    values.erase(std::remove_if(values.begin(),values.end(),[](double v){return !std::isfinite(v);}),values.end());
    if(values.empty())return 0.0;
    if(!std::isfinite(percent))percent=0.0;
    std::sort(values.begin(),values.end());
    const double position=std::clamp(percent,0.0,100.0)*0.01*static_cast<double>(values.size()-1);
    const auto lower=static_cast<std::size_t>(position),upper=std::min(lower+1,values.size()-1);
    return values[lower]+(values[upper]-values[lower])*(position-static_cast<double>(lower));
}
} // namespace benchmark
