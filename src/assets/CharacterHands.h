#pragma once
#include "assets/AssetCatalog.h"
#include "assets/CharacterParts.h"
#include <set>

namespace assets::character {
struct HandsMatch { std::size_t base{std::size_t(-1)}; std::vector<std::size_t> parts; std::string error; };
inline std::string identity(std::string name){name=lower(name);if(name.ends_with("_lod0"))name.resize(name.size()-5);return name;}
inline HandsMatch matchHands(const std::vector<Asset>& entries,const Asset& body,const std::vector<std::size_t>& parts){
 HandsMatch result;
 const auto exact=[&](const std::string& name){for(std::size_t i=0;i<entries.size();++i)if(entries[i].game==body.game&&identity(entries[i].name)==name)return i;return std::size_t(-1);};
 if(body.game=="aw"){
  const auto n=identity(body.name);const auto at=n.rfind('_');
  if(awPart(n)!=Part::Torso||at==std::string::npos){result.error="No matching AW sleeves";return result;}
  result.base=exact("mp_view_top_"+n.substr(at+1));
  if(result.base>=entries.size()){result.error="Matching AW sleeves are not exported";return result;}
  for(auto p:parts)if(p<entries.size()){
   const auto type=awPart(entries[p].name);if(type!=Part::Gloves&&type!=Part::Exo)continue;
   const auto name=identity(entries[p].name);const auto suffix=name.substr(name.rfind('_')+1);
   const auto view=exact(std::string(type==Part::Gloves?"mp_view_gloves_":"mp_view_exo_")+suffix);
   if(view>=entries.size()){result.error="Matching first-person part is not exported: "+entries[p].name;result.base=std::size_t(-1);return result;}
   result.parts.push_back(view);
  }
  if(std::none_of(result.parts.begin(),result.parts.end(),[&](auto i){return identity(entries[i].name).starts_with("mp_view_gloves_");})){
   result.error="Select exported gloves before matching hands";result.base=std::size_t(-1);
  }
  return result;
 }
 if(body.game=="pointblank"){
  result.base=exact("viewmodel_"+pointBlankIdentity(body.name)+"_hands");
 }else{
  const auto tokens=[](std::string n){std::set<std::string> out;std::string token;for(char c:identity(n)+"_"){if(c=='_'){if(token.size()>1&&token!="mp"&&token!="usa"&&token!="rus"&&token!="body"&&token!="head"&&token!="viewhands"&&token!="viewmodel"&&token!="player"&&token!="hands"&&token!="smg"&&token!="ar"&&token!="lmg"&&token!="shotgun"&&token!="sniper"&&token!="assault"&&token!="basic"&&token!="fullbody")out.insert(token);token.clear();}else token+=c;}return out;};
  const auto wanted=tokens(body.name);int best=0;bool tied=false;
  for(std::size_t i=0;i<entries.size();++i){const auto& a=entries[i];if(a.game!=body.game||a.role!=Role::ViewHands)continue;auto offered=tokens(a.name);int score=0;for(const auto& t:wanted)if(offered.contains(t))score+=int(t.size());
   // A palette match alone is not a character identity. Never pick generic hands.
   bool identityMatch=false;for(const auto& t:wanted)if(offered.contains(t)&&t!="arctic"&&t!="desert"&&t!="urban"&&t!="woodland"&&t!="black"&&t!="elite")identityMatch=true;
   if(!identityMatch)continue;
   if(score>best){best=score;result.base=i;tied=false;}else if(score==best)tied=true;
  }if(tied)result.base=std::size_t(-1);
 }
 if(result.base>=entries.size())result.error="No unambiguous matching viewhands are exported";
 return result;
}
}
