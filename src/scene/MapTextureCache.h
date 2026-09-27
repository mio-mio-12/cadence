#pragma once
#include "content/AssetPaths.h"
#include <fstream>
#include <functional>
#include <iterator>

namespace scene::glb {
// A map's name alone is not an identity: installed packs routinely contain
// identically named maps. Keep each source revision in its own namespace.
inline std::filesystem::path textureCacheDirectory(const std::filesystem::path& source){
    namespace fs=std::filesystem;
    const auto canonical=fs::canonical(source);
    const auto owner=cadence::content::utf8(canonical)+"\n"+
        std::to_string(fs::file_size(canonical))+"\n"+
        std::to_string(fs::last_write_time(canonical).time_since_epoch().count());
    const auto root=fs::temp_directory_path()/"CastStage"/"glb_cache_v2";
    fs::create_directories(root);
    const auto bucket=std::to_string(std::hash<std::string>{}(owner));
    for(unsigned collision=0;collision<64;++collision){
        const auto candidate=root/(bucket+"_"+std::to_string(collision));
        const auto identity=candidate/"source.txt";
        if(fs::create_directory(candidate)){
            std::ofstream out(identity,std::ios::binary);out.write(owner.data(),static_cast<std::streamsize>(owner.size()));out.close();
            if(!out)throw std::runtime_error("Cannot write map texture cache identity");
            return candidate;
        }
        // Verify identity rather than relying on a hash being collision-free.
        // A concurrent initializer or incomplete old entry gets another slot.
        if(fs::is_symlink(fs::symlink_status(candidate)))continue;
        std::error_code ec;
        if(fs::file_size(identity,ec)!=owner.size()||ec)continue;
        std::ifstream in(identity,std::ios::binary);
        const std::string existing{std::istreambuf_iterator<char>(in),{}};
        if(existing==owner)return candidate;
    }
    throw std::runtime_error("Cannot allocate a map-specific texture cache");
}
}
