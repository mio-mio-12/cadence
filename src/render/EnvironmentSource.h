#pragma once
#include "scene/BoundedJson.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace render::environment {
enum class Layout { Panorama, T6Cube, FirstAssaultDome, UnsupportedNative };
struct Source { std::filesystem::path texture; Layout layout{Layout::Panorama}; };
inline std::string lower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;
}
inline bool relativeAsset(const std::string& value,std::filesystem::path& result) {
    result=std::filesystem::path(value).lexically_normal();
    if(result.empty()||result.is_absolute()||result.has_root_name())return false;
    for(const auto& part:result)if(part=="..")return false;
    return true;
}
// Native texture dimensions do not identify a projection. Only exporter metadata
// may select the native dome mapping; unrelated DDS images retain panorama behavior.
inline bool resolve(const std::filesystem::path& requested,Source& result,std::string& error) {
    error.clear();result={requested,Layout::Panorama};
    const auto ext=lower(requested.extension().string());
    if(ext==".iwi"){result.layout=Layout::T6Cube;return true;}
    std::error_code ec;
    std::filesystem::path manifest;
    const bool explicitManifest=ext==".json";
    if(explicitManifest)manifest=requested;
    else if(ext==".dds") {
        for(auto parent=requested.parent_path(),stop=parent.parent_path().parent_path();!parent.empty()&&parent!=stop;parent=parent.parent_path()) {
            if(std::filesystem::is_regular_file(parent/"skybox.json",ec)){manifest=parent/"skybox.json";break;}
        }
    }
    if(manifest.empty())return true;
    try {
        const auto size=std::filesystem::file_size(manifest);
        if(size>1024*1024)throw std::runtime_error("Sky manifest exceeds 1 MiB");
        std::ifstream input(manifest,std::ios::binary);
        if(!input)throw std::runtime_error("Cannot open sky manifest");
        const std::string text((std::istreambuf_iterator<char>(input)),{});
        const auto json=scene::codm::parseJson(text);
        if(json.value("schema",std::string{})!="firstassault.skybox/1") {
            if(explicitManifest)throw std::runtime_error("Unsupported sky manifest schema");
            return true;
        }
        const auto& textures=json.at("textures");
        if(!textures.is_array()||textures.size()>128)throw std::runtime_error("Invalid sky texture list");
        const auto requestedKey=lower(std::filesystem::absolute(requested).lexically_normal().generic_string());
        for(const auto& texture:textures) {
            std::filesystem::path relative;
            if(!relativeAsset(texture.value("exportedPath",std::string{}),relative))throw std::runtime_error("Invalid sky texture path");
            const auto candidate=(manifest.parent_path()/relative).lexically_normal();
            const auto layout=texture.value("layout",std::string{});
            const bool selected=explicitManifest ? layout=="native_skydome_uv" : lower(std::filesystem::absolute(candidate).lexically_normal().generic_string())==requestedKey;
            if(!selected)continue;
            result={candidate,layout=="native_skydome_uv"?Layout::FirstAssaultDome:Layout::UnsupportedNative};
            return true;
        }
        throw std::runtime_error("Selected texture has no supported sky manifest entry");
    }catch(const std::exception& exception){error=exception.what();return false;}
}
}
