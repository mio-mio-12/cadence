#pragma once

#include "assets/AssetCatalog.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>

namespace cadence::loadout_defaults {

inline std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline std::size_t findExact(const assets::Catalog& catalog, assets::Role role, std::string_view game,
                             std::string_view name) {
    const auto wantedGame = lower(std::string(game));
    const auto wantedName = lower(std::string(name));
    for (std::size_t i = 0; i < catalog.entries.size(); ++i) {
        const auto& asset = catalog.entries[i];
        if (asset.role == role && lower(asset.game) == wantedGame && lower(asset.name) == wantedName)
            return i;
    }
    return std::numeric_limits<std::size_t>::max();
}

inline bool applyBo2TestingDefaults(const assets::Catalog& catalog, std::size_t& primary,
                                    std::size_t& secondary, std::size_t& viewhands, int& faction) {
    const bool bo2Loaded = std::any_of(catalog.entries.begin(), catalog.entries.end(), [](const auto& a) { return lower(a.game) == "bo2"; });
    const auto game = bo2Loaded ? "bo2" : "mw";
    const auto weapon = findExact(catalog, assets::Role::ViewWeapon, game, bo2Loaded ? "t6_wpn_ar_an94_view_LOD0" : "viewmodel_ak47_mp_LOD0");
    const auto pistol = findExact(catalog, assets::Role::ViewWeapon, game, bo2Loaded ? "t6_wpn_pistol_fnp45_view_LOD0" : "viewmodel_desert_eagle_LOD0");
    const auto hands = findExact(catalog, assets::Role::ViewHands, game, bo2Loaded ? "c_usa_mp_isa_smg_viewhands_LOD0" : "viewhands_usmc_LOD0");
    if (weapon >= catalog.entries.size() || pistol >= catalog.entries.size() || hands >= catalog.entries.size()) return false;
    primary = weapon;
    secondary = pistol;
    viewhands = hands;
    faction = 0; // SEAL Team Six
    return true;
}

} // namespace cadence::loadout_defaults
