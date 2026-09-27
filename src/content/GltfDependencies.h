#pragma once
#include "scene/BoundedJson.h"
#include <filesystem>
#include <fstream>
#include <optional>
#include <cstdint>
#include <cctype>

namespace cadence::content::gltf {
namespace fs=std::filesystem;
inline std::optional<fs::path> decodedResourcePath(const fs::path& directory,std::string_view uri){
    if(uri.starts_with("data:"))return std::nullopt;
    if(uri.empty())throw std::runtime_error("Empty glTF resource URI");
    const auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
    std::string decoded;decoded.reserve(uri.size());
    for(std::size_t i=0;i<uri.size();++i){
        char c=uri[i];
        if(c=='%'){
            if(i+2>=uri.size()||hex(uri[i+1])<0||hex(uri[i+2])<0)throw std::runtime_error("Malformed glTF URI escape");
            c=static_cast<char>(hex(uri[i+1])*16+hex(uri[i+2]));i+=2;
        }
        if(c=='\0'||c==':'||c=='\\')throw std::runtime_error("glTF resource must be package-relative");
        decoded.push_back(c);
    }
    const auto relative=fs::u8path(decoded);
    if(relative.is_absolute()||relative.has_root_name()||relative.has_root_directory())throw std::runtime_error("Absolute glTF resource rejected");
    for(const auto& part:relative)if(part=="..")throw std::runtime_error("glTF resource traversal rejected");
    const auto root=fs::canonical(directory);auto result=root;bool missing=false;
    for(const auto& part:relative){
        if(part==".")continue;
        result/=part;
        if(!missing){
            std::error_code ec;const auto status=fs::symlink_status(result,ec);
            if(status.type()==fs::file_type::not_found||ec==std::errc::no_such_file_or_directory)missing=true;
            else {
                if(ec)throw fs::filesystem_error("Cannot inspect glTF resource",result,ec);
                result=fs::canonical(result);
            }
        }
        // Once a component is absent, the remaining validated relative suffix
        // cannot contain a symlink. Avoid probing nonexistent descendants.
        const auto remainder=result.lexically_relative(root);
        if(remainder.empty()||remainder.is_absolute())throw std::runtime_error("glTF resource outside package");
        for(const auto& component:remainder)if(component=="..")throw std::runtime_error("glTF resource symlink escape rejected");
    }
    if(result==root)throw std::runtime_error("glTF resource must name a file");
    // Physical paths are for containment validation only. Pack destinations and
    // cgltf's relative opens must retain the owning map's spelling/root.
    return (directory/relative).lexically_normal();
}

inline std::vector<fs::path> metadataDependencies(const fs::path& map){
    constexpr std::uint64_t limit=64u*1024u*1024u;
    auto extension=map.extension().string();for(auto& c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(extension!=".gltf"&&extension!=".glb")return {};
    std::ifstream input(map,std::ios::binary|std::ios::ate);
    if(!input)throw std::runtime_error("Cannot read glTF dependency metadata");
    const auto end=input.tellg();if(end<0)throw std::runtime_error("Cannot size glTF metadata");
    const auto size=static_cast<std::uint64_t>(end);input.seekg(0);
    const auto word=[&](){unsigned char b[4]{};input.read(reinterpret_cast<char*>(b),4);if(!input)throw std::runtime_error("Truncated GLB header");return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);};
    std::uint64_t jsonSize=size;
    if(extension==".glb"){
        if(size<20||word()!=0x46546c67||word()!=2)throw std::runtime_error("Invalid GLB header");
        const auto total=word();jsonSize=word();const auto chunkType=word();
        if(total!=size||chunkType!=0x4e4f534a||jsonSize>size-20||jsonSize%4)throw std::runtime_error("Invalid GLB JSON chunk");
    }
    if(jsonSize>limit)throw std::runtime_error("glTF JSON exceeds 64 MiB bound");
    std::string text(static_cast<std::size_t>(jsonSize),'\0');input.read(text.data(),static_cast<std::streamsize>(text.size()));
    if(!input)throw std::runtime_error("Truncated glTF JSON metadata");
    const auto json=scene::codm::parseJson(text);
    if(!json.is_object())throw std::runtime_error("glTF metadata must be an object");
    std::vector<fs::path> result;
    for(const char* collection:{"buffers","images"})if(json.contains(collection)){
        const auto& values=json.at(collection);if(!values.is_array())throw std::runtime_error("glTF resource collection must be an array");
        for(const auto& value:values){
            if(!value.is_object())throw std::runtime_error("glTF resource must be an object");
            if(value.contains("uri")){
                if(!value.at("uri").is_string())throw std::runtime_error("glTF resource URI must be text");
                if(auto path=decodedResourcePath(map.parent_path(),value.at("uri").get<std::string>()))result.push_back(*path);
            }
        }
    }
    return result;
}
}
