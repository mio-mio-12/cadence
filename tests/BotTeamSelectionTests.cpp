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
    app.gameReferenceSetups["aw"].helmetAsset=static_cast<std::size_t>(-2);
    const auto noHelmet=characterAssemblyParts(app,app.assetCatalog.entries[awBody]);
    CHECK(noHelmet.size()==7);
    for(auto i:noHelmet)CHECK(assets::character::awPart(app.assetCatalog.entries[i].name)!=assets::character::Part::Headgear);
    app.gameReferenceSetups["aw"].helmetAsset=static_cast<std::size_t>(-1);
    CHECK(characterAssemblyParts(app,app.assetCatalog.entries[awBody]).size()==8);
    CHECK(assets::character::displayName("mp_headgear_01a_cormack_LOD0")=="01a cormack");
    // Complete PB bodies need no arbitrary head attached, including unknown teams.
    const auto pb = add("pointblank", "playermode_bella_fb", assets::Role::PlayerModel);
    app.botGame = "pointblank";
    CHECK(botTeamModels(app) == std::vector<std::size_t>{pb});
    CHECK(!botHeadForBody(app, app.assetCatalog.entries[pb]));
    const auto pbHands=add("pointblank","viewmodel_bella_hands",assets::Role::ViewHands);
    CHECK(assets::character::matchHands(app.assetCatalog.entries,app.assetCatalog.entries[pb],{}).base==pbHands);
    const auto matchBody=add("aw","mp_top_m_c_04d_LOD0",assets::Role::PlayerModel);
    const auto sleeves=add("aw","mp_view_top_04d_LOD0",assets::Role::OtherModel);
    const auto glove=add("aw","mp_glove_03i_LOD0",assets::Role::OtherModel);
    const auto exo=add("aw","mp_exo_08a_LOD0",assets::Role::OtherModel);
    CHECK(assets::character::matchHands(app.assetCatalog.entries,app.assetCatalog.entries[matchBody],{glove,exo}).base==std::size_t(-1));
    const auto viewGlove=add("aw","mp_view_gloves_03i_LOD0",assets::Role::OtherModel);
    const auto viewExo=add("aw","mp_view_exo_08a_LOD0",assets::Role::OtherModel);
    const auto matched=assets::character::matchHands(app.assetCatalog.entries,app.assetCatalog.entries[matchBody],{glove,exo});
    CHECK(matched.base==sleeves);CHECK(matched.parts==std::vector<std::size_t>({viewGlove,viewExo}));
    const auto unknownGhost=add("ghosts","mp_body_devgru_arctic_LOD0",assets::Role::PlayerModel);
    add("ghosts","viewhands_elite_pmc_arctic_LOD0",assets::Role::ViewHands);
    CHECK(assets::character::matchHands(app.assetCatalog.entries,app.assetCatalog.entries[unknownGhost],{}).base==std::size_t(-1));
    scene::CastScene assembly;scene::Mesh torso,boots,weaponMesh;
    torso.vertices.resize(1);torso.vertices[0].position={0,0,90};boots.vertices.resize(1);boots.vertices[0].position={0,0,0};weaponMesh.vertices.resize(1);weaponMesh.vertices[0].position={0,0,-200};weaponMesh.attachmentIndex=0;
    assembly.meshes={torso,boots,weaponMesh};scene::refreshCharacterBounds(assembly);
    CHECK(assembly.bounds.minimum.z==0);CHECK(assembly.bounds.maximum.z==90);
    std::cout << "Team assembly and character-to-hands exact/missing/variant matching passed\n";
}
