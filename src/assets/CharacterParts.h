#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace assets::character {
// pb2cast preserves the same complete source identity in body and hand names.
inline std::string pointBlankIdentity(std::string_view text){
 std::string n(text);std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 if(n.starts_with("playermode_")&&n.ends_with("_fb"))return n.substr(11,n.size()-14);
 if(n.starts_with("viewmodel_")&&n.ends_with("_hands"))return n.substr(10,n.size()-16);
 return {};
}
inline std::string pointBlankFaction(std::string_view text){
 const auto n=pointBlankIdentity(text);
 const auto family=[&](std::string_view s){return n==s||n.starts_with(std::string(s)+"_");};
 if(family("swat")||family("hide")||family("leopard")||family("chou"))return "CT-Force";
 if(family("rebel")||family("d-fox"))return "Free Rebels";
 // Bella's export report has no team field. Keep unknowns manually selectable.
 return {};
}
enum class Part { None, Torso, Head, Headgear, Eyewear, Gloves, Pants, Kneepads, Shinguards, Boots, Loadout, Exo };
inline std::string lower(std::string_view text){std::string s(text);std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;}
inline Part awPart(std::string_view text){
 const auto n=lower(text);
 if(n.starts_with("mp_view_")||n.find("fso_vest")!=std::string::npos)return Part::None;
 if(n.starts_with("mp_top_m_")||n.starts_with("mp_top_f_"))return Part::Torso;
 if(n.starts_with("mp_head_"))return Part::Head;
 if(n.starts_with("mp_headgear_"))return Part::Headgear;
 if(n.starts_with("mp_eyewear_"))return Part::Eyewear;
 if(n.starts_with("mp_glove_")||n.starts_with("mp_gloves_"))return Part::Gloves;
 if(n.starts_with("mp_pants_"))return Part::Pants;
 if(n.starts_with("mp_kneepad_"))return Part::Kneepads;
 if(n.starts_with("mp_shinguard_"))return Part::Shinguards;
 if(n.starts_with("mp_boot_"))return Part::Boots;
 if(n.starts_with("mp_loadouts_"))return Part::Loadout;
 if(n.starts_with("mp_exo_")&&n.size()>7&&std::isdigit(static_cast<unsigned char>(n[7])))return Part::Exo;
 return Part::None;
}
inline bool headCompatible(std::string_view game,std::string_view body,std::string_view head){
 const auto g=lower(game),b=lower(body),h=lower(head);
 if(g=="ghosts"){
  const bool bodyJugger=b.find("jugger")!=std::string::npos,headJugger=h.find("jugger")!=std::string::npos;
  if(bodyJugger!=headJugger)return false;
 }
 if(g=="mw3"&&h.find("sniper")!=std::string::npos&&b.find("sniper")==std::string::npos)return false;
 if(g=="aw")return awPart(h)==Part::Head;
 return h.starts_with("head_")||h.starts_with("head_mp")||h.starts_with("mp_head")||h.find("_head_")!=std::string::npos;
}
inline bool genderCompatible(std::string_view body,std::string_view part){
 const auto b=lower(body),p=lower(part);const bool female=b.starts_with("mp_top_f_");
 if(p.starts_with("mp_pants_f_")||p.starts_with("mp_loadouts_f_"))return female;
 if(p.starts_with("mp_pants_m_")||p.starts_with("mp_loadouts_m_"))return !female;
 return true;
}
inline bool matchesSlot(std::string_view game,std::string_view name,std::string_view label){
 if(lower(game)!="aw")return true;
 const auto p=awPart(name);
 if(label=="Helmet / Hat")return p==Part::Headgear;
 if(label=="Eyewear / Visor")return p==Part::Eyewear;
 if(label=="Gloves / Hands")return p==Part::Gloves;
 if(label=="Pants / Legs")return p==Part::Pants;
 if(label=="Kneepads")return p==Part::Kneepads;
 if(label=="Shinguards")return p==Part::Shinguards;
 if(label=="Boots / Shoes")return p==Part::Boots;
 if(label=="Exo")return p==Part::Exo;
 if(label=="Loadout / Gear")return p==Part::Loadout;
 return false;
}
}
