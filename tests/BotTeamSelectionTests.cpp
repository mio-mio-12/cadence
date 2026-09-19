#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " << #x << "\n"; return 1; } } while (false)

int main() {
    auto state = std::make_unique<AppState>();
    auto& app = *state;
    const auto add = [&](const std::string& game, const std::string& name, assets::Role role) {
        assets::Asset a;
        a.game = game; a.name = name; a.role = role;
        a.path = std::filesystem::path(game) / (name + ".cast");
        app.assetCatalog.entries.push_back(std::move(a));
        return app.assetCatalog.entries.size() - 1;
    };
    const auto ally = add("bo2", "body_seal_assault", assets::Role::PlayerModel);
    const auto axis = add("bo2", "body_pmc_assault", assets::Role::PlayerModel);
    const auto unknown = add("bo2", "body_unaffiliated", assets::Role::PlayerModel);
    const auto secondAlly = add("bo2", "body_seal_smg", assets::Role::PlayerModel);
    const auto allyHead = add("bo2", "head_seal_assault", assets::Role::OtherModel);
    const auto axisHead = add("bo2", "head_pmc_assault", assets::Role::OtherModel);
    add("bo2", "viewhands_seal", assets::Role::ViewHands);
    add("bo2", "weapon_seal", assets::Role::ViewWeapon);
    const auto otherGame = add("mw3", "body_delta_assault", assets::Role::PlayerModel);
    app.botTeamSide = 3;
    app.botGame = "BO2";
    app.botFaction = "Stale faction must not restrict Any";
    app.botModelAsset = otherGame;
    app.botFactionModelVariety = false;
    const std::vector<std::size_t> expected{ally, axis, unknown, secondAlly};
    CHECK(botTeamModels(app) == expected);
    CHECK(botHeadForBody(app, app.assetCatalog.entries[ally]) == allyHead);
    CHECK(botHeadForBody(app, app.assetCatalog.entries[axis]) == axisHead);
    app.botGame = "mw3";
    CHECK(botTeamModels(app) == std::vector<std::size_t>{otherGame});
    app.botGame = "missing";
    CHECK(botTeamModels(app).empty());
    app.botGame.clear();
    CHECK(botTeamModels(app).size() == 5);

    // Existing team and manual modes must keep their original semantics.
    app.botGame = "bo2"; app.botFaction.clear(); app.botTeamSide = 1;
    CHECK(botTeamModels(app) == (std::vector<std::size_t>{ally, secondAlly}));
    app.botTeamSide = 2;
    CHECK(botTeamModels(app) == std::vector<std::size_t>{axis});
    app.botFaction = "SEAL Team Six";
    CHECK(botTeamModels(app).empty());
    app.botTeamSide = 0; app.botModelAsset = ally;
    CHECK(botTeamModels(app) == std::vector<std::size_t>{ally});
    app.botFactionModelVariety = true;
    CHECK(botTeamModels(app) == (std::vector<std::size_t>{ally, secondAlly}));

    // Any selects AW torso bases, never loose parts; assembly remains complete
    // and identical to the manual-model path for each selected base.
    const auto awBody = add("aw", "mp_top_m_01a_lod0", assets::Role::PlayerModel);
    const auto awFemale = add("aw", "mp_top_f_01a_lod0", assets::Role::PlayerModel);
    for (const char* name : {"mp_head_cormack_lod0", "mp_pants_m_01b_k_lod0",
         "mp_pants_f_01b_k_lod0", "mp_glove_03a_lod0", "mp_boot_01a_lod0",
         "mp_loadouts_m_01a_lod0", "mp_loadouts_f_01a_lod0", "mp_exo_01a_lod0",
         "mp_kneepad_03a_lod0", "mp_headgear_01a_cormack_lod0", "fso_vest_lod0"})
        add("aw", name, assets::Role::OtherModel);
    app.botGame = "aw"; app.botTeamSide = 3;
    CHECK(botTeamModels(app) == (std::vector<std::size_t>{awBody, awFemale}));
    for (auto body : botTeamModels(app)) {
        const auto& asset = app.assetCatalog.entries[body];
        const auto parts = characterAssemblyParts(app, asset);
        CHECK(parts.size() == 8);
        for (auto p : parts) {
            const auto& part = app.assetCatalog.entries[p];
            CHECK(part.game == asset.game);
            CHECK(assets::character::genderCompatible(asset.name, part.name));
            CHECK(assets::character::awPart(part.name) != assets::character::Part::None);
        }
        app.botTeamSide = 0;
        CHECK(characterAssemblyParts(app, asset) == parts);
        app.botTeamSide = 3;
    }
    // Complete PB bodies need no arbitrary head attached, including unknown teams.
    const auto pb = add("pointblank", "playermode_bella_fb", assets::Role::PlayerModel);
    app.botGame = "pointblank";
    CHECK(botTeamModels(app) == std::vector<std::size_t>{pb});
    CHECK(!botHeadForBody(app, app.assetCatalog.entries[pb]));
    std::cout << "Any team: selected-game filtering, unknown factions, legacy modes, and multipart assembly passed\n";
}
