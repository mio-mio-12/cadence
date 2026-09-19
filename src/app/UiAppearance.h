#pragma once
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>
namespace uiappearance {
inline bool save(const std::filesystem::path& path,int theme,int font,float scale){
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
    auto temp=path;temp += ".tmp";
    std::ofstream out(temp);if(!out)return false;
    out.precision(9);out<<"CADENCE_APPEARANCE 1\n"<<theme<<' '<<font<<' '<<scale<<' '<<ImGui::GetStyle().FrameRounding<<'\n';
    for(const auto& c:ImGui::GetStyle().Colors)out<<c.x<<' '<<c.y<<' '<<c.z<<' '<<c.w<<'\n';
    out.close();if(!out)return false;
    // Keep a recoverable previous settings file during the replacement on Windows.
    auto backup=path;backup += ".previous";
    if(std::filesystem::exists(path,ec)){std::filesystem::copy_file(path,backup,std::filesystem::copy_options::overwrite_existing,ec);if(ec)return false;}
    std::filesystem::copy_file(temp,path,std::filesystem::copy_options::overwrite_existing,ec);
    if(ec)return false;std::filesystem::remove(temp,ec);return true;
}
inline bool load(const std::filesystem::path& path,int& theme,int& font,float& scale){
    std::ifstream in(path);std::string magic;int version{},t{},f{};float s{},rounding{};
    if(!(in>>magic>>version>>t>>f>>s>>rounding)||magic!="CADENCE_APPEARANCE"||version!=1||t<0||t>=16||f<0||!std::isfinite(s)||s<.5f||s>3.f||!std::isfinite(rounding)||rounding<0||rounding>32)return false;
    auto candidate=ImGui::GetStyle();candidate.FrameRounding=rounding;
    for(auto& c:candidate.Colors){if(!(in>>c.x>>c.y>>c.z>>c.w))return false;for(float v:{c.x,c.y,c.z,c.w})if(!std::isfinite(v)||v<0||v>1)return false;}
    theme=t;font=f;scale=s;ImGui::GetStyle()=candidate;ImGui::GetIO().FontGlobalScale=s;return true;
}
}
