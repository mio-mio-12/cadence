#pragma once
#include "cast/CastDocument.h"
#include <algorithm>
#include <cctype>
#include <string>
namespace scene {
inline bool source2ContextualBody(std::string source,std::string name){
    const auto lower=[](std::string& s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));};
    auto sourceLower=source;lower(sourceLower);lower(name);
    if(sourceLower.find("hostage")!=std::string::npos||name.find("hostage")!=std::string::npos||name.find("death_lowviolence")!=std::string::npos||name.find("death_poses")!=std::string::npos)return true;
    // cs2cast's normalized filenames retain the FNV identity, not the native
    // action name. Recover only this contextual identity from its embedded
    // source, without hardcoding a particular character or clip hash.
    if(source.empty())return false;
    const auto key=source+"\n"+source+"\ndeath_lowviolence\n-1\n-1";
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned char c:key)hash=(hash^c)*1099511628211ull;
    return name.ends_with("_clip"+std::to_string(hash)+".cast");
}
inline bool source2ContextualBody(const cast::Document& document){
    std::string source;for(const auto& root:document.roots())if(const auto* p=root.findProperty("cs2cast_source");p&&p->stringValue){source=*p->stringValue;break;}
    return source2ContextualBody(source,document.sourceName());
}
}
