#pragma once
#include <array>
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
struct ImFont;
namespace cadence::fonts {
inline bool matches(std::string_view text,std::string_view query){
    return std::search(text.begin(),text.end(),query.begin(),query.end(),[](unsigned char a,unsigned char b){return std::tolower(a)==std::tolower(b);})!=text.end()||query.empty();
}
struct Browser {
    std::array<char,128> search{};
    ImFont* preview{};std::string previewName,pendingPreview,hoverName;double hoverSince{};
    void hover(const std::string& name,double now){if(hoverName!=name){hoverName=name;hoverSince=now;}if(now-hoverSince>=.3&&previewName!=name)pendingPreview=name;}
};
}
