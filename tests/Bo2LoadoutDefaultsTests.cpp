#include "app/Bo2LoadoutDefaults.h"

#include <stdexcept>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("BO2 loadout default regression"); }
assets::Asset asset(std::string game, std::string name, assets::Role role) {
    assets::Asset value;
    value.game = std::move(game);
    value.name = std::move(name);
    value.role = role;
    return value;
}
}

int main() {
    constexpr auto invalid = std::numeric_limits<std::size_t>::max();
    assets::Catalog catalog;
    catalog.entries.push_back(asset("aw", "t6_wpn_ar_an94_view_LOD0", assets::Role::ViewWeapon));
    catalog.entries.push_back(asset("BO2", "C_USA_MP_ISA_SMG_VIEWHANDS_LOD0", assets::Role::ViewHands));
    catalog.entries.push_back(asset("bo2", "t6_wpn_ar_an94_view_LOD0", assets::Role::ViewWeapon));
    std::size_t primary = invalid, secondary = invalid, hands = invalid;
    int faction = 1;
    require(cadence::loadout_defaults::applyBo2TestingDefaults(catalog, primary, secondary, hands, faction));
    require(primary == 2 && secondary == 2 && hands == 1 && faction == 0);

    assets::Catalog incomplete;
    incomplete.entries.push_back(catalog.entries[2]);
    primary = 7; secondary = 8; hands = 9; faction = 1;
    require(!cadence::loadout_defaults::applyBo2TestingDefaults(incomplete, primary, secondary, hands, faction));
    require(primary == 7 && secondary == 8 && hands == 9 && faction == 1);
    return 0;
}
