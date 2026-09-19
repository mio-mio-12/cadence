#pragma once
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
namespace scene {
inline std::string nativeBoneHash(std::string_view name){
    std::uint64_t h=14695981039346656037ull;for(unsigned char c:name){h^=c;h*=1099511628211ull;}
    char hash[24];std::snprintf(hash,sizeof(hash),"string_%016llx",static_cast<unsigned long long>(h));return hash;
}
// Saluki can retain native names as FNV-1a 64-bit string identifiers. Resolve
// only exact known hashes; this changes lookup identity, never pose transforms.
inline std::string resolveNativeBoneName(std::string name){
    if(!name.starts_with("string_")||name.size()!=23)return name;
    static const auto names=[] {
        std::unordered_map<std::string,std::string> result;
        const auto add=[&](std::string value){result.emplace(nativeBoneHash(value),value);};
        for(const char* value:{"tag_origin","tag_view","tag_camera","tag_weapon","tag_weapon_right","tag_weapon_left","tag_aim","tag_ads","tag_clip","tag_clip1","tag_clip2","tag_scope","tag_torso","tag_flash","tag_brass","tag_sights","j_mainroot","j_gun","j_gun1","j_bolt","j_trigger","j_slide","j_neck","j_head","j_spinelower","j_spineupper","j_spine4"})add(value);
        add("tag_rail");
        for(const char* side:{"le","ri"})for(const char* joint:{"j_clavicle_","j_shoulder_","j_elbow_","j_wrist_","j_wristtwist_"})add(std::string(joint)+side);
        return result;
    }();
    const auto found=names.find(name);return found==names.end()?name:found->second;
}
}
