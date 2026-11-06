#pragma once
#include "assets/AssetCatalog.h"
#include <algorithm>
#include <cctype>
#include <optional>

namespace cadence::reference_hands {
inline std::string lower(std::string text){
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return text;
}
inline std::optional<std::filesystem::path> select(const assets::Catalog& catalog,std::string_view game){
    const std::pair<std::string_view,std::string_view> preferred[]={
        {"mw","viewmodel_base_viewhands"},{"mw2","viewmodel_base_viewhands"},{"mw3","viewmodel_base_viewhands"},{"aw","viewmodel_base_viewhands"},
        {"iw_sp","viewmodel_base_viewhands_iw7"},{"iw7","viewmodel_base_viewhands_iw7"},{"ghosts","viewmodel_base_viewhands"},{"bo","viewhands_usmc"},
        {"bo2","c_usa_mp_seal6_longsleeve_viewhands"},{"cs2","viewhands_model"}
    };
    for(const auto& [preferredGame,name]:preferred)if(game==preferredGame)for(const auto& asset:catalog.entries){
        auto model=lower(asset.name);if(model.ends_with("_lod0"))model.resize(model.size()-5);
        if(asset.role==assets::Role::ViewHands&&lower(asset.game)==game&&model==name)return asset.path;
    }
    for(const auto& asset:catalog.entries)if(asset.role==assets::Role::ViewHands&&lower(asset.game)==game)return asset.path;
    return std::nullopt;
}
// Discovery is local to the requested reference game and must not enable it in
// the visible catalog. A stale on-disk catalog gets one fresh discovery pass.
template<class Discover,class Exists>
std::optional<std::filesystem::path> resolve(const assets::Catalog& enabled,std::string_view requested,Discover&& discover,Exists&& exists){
    const auto game=lower(std::string(requested));
    if(const auto path=select(enabled,game))return path;
    for(const bool fresh:{false,true}){
        const auto catalog=discover(game,fresh);
        if(const auto path=select(catalog,game);path&&exists(*path))return path;
    }
    return std::nullopt;
}
}
