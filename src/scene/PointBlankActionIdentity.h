#pragma once

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace scene::pointblank {
inline std::string actionIdentityStem(std::string_view path) {
    const auto slash=path.find_last_of("/\\");
    if(slash!=std::string_view::npos)path.remove_prefix(slash+1);
    std::string value(path);
    for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(value.ends_with(".cast"))value.resize(value.size()-5);
    return value;
}

// Literal native pt_*_Common_Up families observed in the PB export inventory.
// Do not collapse these into Knife: they have different grips/action poses.
inline constexpr std::array<std::string_view,16> meleeFamilies{
    "badmintonracket","bamboo-runcing","black-pearl","brass_knuckle",
    "chinesecleaver","death-scythe","dualknife","dualsword_th","fryingpan",
    "karambit","keris","knife","kukrii","mini_axe","nunchaku",
    "spetnatz_field_shovel"
};

inline std::string torsoWeaponFamily(std::string_view path) {
    const auto name=actionIdentityStem(path);
    if(!name.starts_with("pt_"))return {};
    const auto marker=name.find("_up_");
    if(marker==std::string::npos||marker+4>=name.size())return {};
    const std::string_view prefix(name.data(),marker);
    // The primary family precedes the action descriptor. For example,
    // dualknife_Dual_Throwing_Knife is not a single knife action.
    std::size_t first=std::string_view::npos;std::string_view selected;
    for(const auto family:meleeFamilies){
        const std::string token="_"+std::string(family);
        for(auto at=prefix.find(token);at!=std::string_view::npos;at=prefix.find(token,at+1)){
            const auto end=at+token.size();
            if(end<prefix.size()&&prefix[end]!='_')continue;
            if(at<first||(at==first&&family.size()>selected.size())){first=at;selected=family;}
        }
    }
    return std::string(selected);
}

inline std::string meleeFamilyFromViewmodel(std::string_view path) {
    auto name=actionIdentityStem(path);
    if(!name.starts_with("viewmodel_"))return {};
    name.erase(0,10);
    // More specific native family names win over the broad knife export prefix.
    for(const auto family:meleeFamilies){
        if(family=="knife")continue;
        if(name==family||(name.size()>family.size()&&name.ends_with(family)&&name[name.size()-family.size()-1]=='_'))return std::string(family);
    }
    if(name.starts_with("knife_"))return name.ends_with("_dual")?"dualknife":"knife";
    return name=="knife"?"knife":"";
}

inline bool torsoFamilyCompatible(std::string_view preferredFamily,
    std::string_view sourceGame,std::string_view sourceName) {
    if(preferredFamily.empty()||sourceGame!="pointblank")return true;
    // Caller scopes this to torso selection. Unknown PB family is not evidence
    // of compatibility; foreign fallback remains available to the query policy.
    return torsoWeaponFamily(sourceName)==preferredFamily;
}
} // namespace scene::pointblank
