#pragma once
#include "assets/AssetCatalog.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_map>

namespace cadence {
// Catalog positions are not asset identities. Build once when replacing a
// catalog, never from the render/update loop.
class CatalogRemap {
    std::unordered_map<std::string,std::size_t> byPath_;
    std::vector<std::size_t> indices_;
    static std::string key(const std::filesystem::path& path){
        auto text=path.lexically_normal().generic_string();
        std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        return text;
    }
public:
    static constexpr auto missing=std::numeric_limits<std::size_t>::max();
    CatalogRemap(const assets::Catalog& before,const assets::Catalog& after){
        byPath_.reserve(after.entries.size());
        for(std::size_t i=0;i<after.entries.size();++i)byPath_.try_emplace(key(after.entries[i].path),i);
        indices_.reserve(before.entries.size());
        for(const auto& asset:before.entries)indices_.push_back(find(asset.path));
    }
    std::size_t find(const std::filesystem::path& path) const{
        const auto it=byPath_.find(key(path));return it==byPath_.end()?missing:it->second;
    }
    std::size_t operator()(std::size_t old) const{return old<indices_.size()?indices_[old]:missing;}
};
}
