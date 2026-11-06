#pragma once
#include "assets/AssetCatalog.h"
#include <algorithm>
#include <cctype>

namespace assets::source2 {
inline std::string lower(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline std::string weaponFamily(const Asset& a){
    if(lower(a.game)!="cs2"||(a.role!=Role::ViewWeapon&&a.role!=Role::WorldWeapon))return {};
    auto n=lower(a.name);
    if(n.starts_with("worldmodel_"))n.erase(0,11);
    if(n.ends_with("_model"))n.resize(n.size()-6);
    return n;
}
inline std::string skin(const Asset& a){
    if(lower(a.game)!="cs2"||(weaponFamily(a).empty()&&a.role!=Role::ViewHands))return {};
    for(auto p=a.path.parent_path();!p.empty();){
        if(lower(p.filename().string())=="models"){
            const auto relative=a.path.lexically_relative(p);
            if(std::distance(relative.begin(),relative.end())==2){
                const auto folder=relative.begin()->string();
                if(lower(folder)!="weapons"&&lower(folder)!=weaponFamily(a))return folder;
            }
            return {};
        }
        const auto next=p.parent_path();if(next==p)break;p=next;
    }return {};
}
// Prefer a genuine default, otherwise the first skin. Never require a default
// export: skin-only weapons are still complete selectable weapon families.
inline bool precedes(const Asset& a,const Asset& b){
    const auto sa=skin(a),sb=skin(b);
    if(sa.empty()!=sb.empty())return sa.empty();
    if(sa!=sb)return sa<sb;
    return a.path.generic_string()<b.path.generic_string();
}
}
