#pragma once
#include <string_view>
#include <string>
#include <cctype>
namespace render::rain {
inline bool nonblockingSurface(std::string_view name){
    std::string token;
    const auto excluded=[&]{return token=="sky"||token=="skybox"||token=="skydome"||token=="invisible"||token=="nodraw";};
    for(unsigned char c:name){if(std::isalnum(c))token+=static_cast<char>(std::tolower(c));else{if(excluded())return true;token.clear();}}
    return excluded();
}
}
