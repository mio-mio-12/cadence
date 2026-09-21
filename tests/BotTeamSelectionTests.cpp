#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " << #x << "\n"; return 1; } } while (false)

int main() {
    auto state = std::make_unique<AppState>();
    auto& app = *state;
    {
        auto isolated=std::make_unique<AppState>();auto& a=*isolated;
        a.assetFirstPerson=true;
        for(bool exiting:{false,true})for(bool overlay:{false,true})for(auto action:{scene::ActionRole::Reload,scene::ActionRole::Fire}){
            a.actionActive=true;a.activeAction=action;a.actionOverlay=overlay;
            a.gameplayRechamber=false;a.viewmodelAdsEngaged=true;a.viewmodelAdsExiting=exiting;
            a.animationIndex=3;a.actionAnimationIndex=4;a.animationFrame=7;a.actionFrame=8;
            a.actionElapsed=.25f;a.actionDurationOverride=2.5f;
            releaseViewmodelAim(a);
            CHECK(a.actionActive&&a.activeAction==action&&a.actionOverlay==overlay);
            CHECK(a.animationIndex==3&&a.actionAnimationIndex==4&&a.animationFrame==7&&a.actionFrame==8);
            CHECK(a.actionElapsed==.25f&&a.actionDurationOverride==2.5f);
        }
    }
    {
        scene::CastScene fixture;fixture.skeleton.bones.emplace_back();
        int stage=0;
        for(const auto suffix:{"reload_intro.cast","reload_loop.cast","reload_out.cast"}){
            scene::Animation clip;clip.sourceName=std::string("viewmodel_test_")+suffix;clip.durationFrames=1;clip.framerate=30;clip.looping=stage==1;
            scene::Track track;track.property=scene::TrackProperty::TranslationX;track.frames={0,1};track.scalarValues={float(stage),float(stage+1)};clip.tracks.push_back(track);fixture.animations.push_back(clip);++stage;
        }
        CHECK(cadence::iw_reload::prepare(fixture,"viewmodel_test_"));
        CHECK(fixture.animations.back().durationFrames==3&&fixture.animations[1].looping);
        for(float frame:{0.f,.5f,1.f,1.5f,2.f,2.5f,3.f})CHECK(std::abs(fixture.sampleLocalPose(3,frame)[0].position.x-frame)<.0001f);
        CHECK(!cadence::iw_reload::prepare(fixture,"viewmodel_test_"));
        CHECK(!cadence::iw_reload::prepare(fixture,"viewmodel_missing_"));
    }
    for(const auto& [game,model,prefix]:std::vector<std::tuple<std::string,std::string,std::string>>{
        {"mw","viewmodel_remington700_mp_LOD0","viewmodel_remington_"},
        {"mwr","wpn_h1_pst_m9_vm_wet_camo_LOD0","h1_wpn_pst_m9_"},
        {"mwr","viewmodel_ak47_LOD0","h1_wpn_asl_ak47_"},
        {"mwr","wpn_h1_melee_staff_vm_LOD0","h1_wpn_melee_tribal_staff_"},
        {"mwr","wpn_h1_lau_rpg7_vm_LOD0","h1_wpn_lau_rpg_"}}){
        assets::Asset asset;asset.game=game;asset.name=model;
        CHECK(animationPrefixForWeapon(asset)==prefix);
        CHECK(animationSourceMatchesWeapon(prefix+"idle.cast",asset));
        if(game=="mwr")CHECK(!animationSourceMatchesWeapon("h1_wpn_pst_usp_idle.cast",asset));
    }
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
    // Missing weapon-family locomotion must never fall back to clip zero's
    // mantle (the visible frozen-pose/gliding regression).
    scene::CastScene locomotion;
    const auto addClip=[&](const char* name){scene::Animation c;c.sourceName=name;c.sourceGame="bo2";scene::classifyAnimationName(name,c);scene::Track t;t.frames={0,20};t.scalarValues={0,10};c.tracks.push_back(t);c.durationFrames=20;locomotion.animations.push_back(c);};
    addClip("mp_mantle_over_high.cast");addClip("pb_stand_alert.cast");addClip("pb_combatrun_forward_loop.cast");
    for(int weapon=0;weapon<=int(scene::WeaponClass::RC);++weapon)for(auto motion:{scene::MotionRole::Idle,scene::MotionRole::Walk,scene::MotionRole::Run,scene::MotionRole::Sprint}){
        resetBotLocomotionCache();scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=motion;q.stance=scene::Stance::Stand;q.weapon=scene::WeaponClass(weapon);q.direction=scene::Direction::Forward;
        const auto clip=botLocomotionAnimation(locomotion,q,&app);
        CHECK(clip);CHECK(*clip==(motion==scene::MotionRole::Idle?1:2));
    }
    scene::AnimationQuery missing;missing.domain=scene::AnimationDomain::PlayerBody;missing.motion=scene::MotionRole::Run;missing.stance=scene::Stance::Prone;
    CHECK(!botLocomotionAnimation(locomotion,missing,&app));
    app.botLocomotionOverrides[{"bo2",scene::MotionRole::Run,scene::Stance::Stand,scene::Direction::Forward,scene::WeaponClass::Knife}]="mp_mantle_over_high.cast";++app.botLocomotionOverrideRevision;
    missing.stance=scene::Stance::Stand;missing.weapon=scene::WeaponClass::Knife;missing.direction=scene::Direction::Forward;
    CHECK(botLocomotionAnimation(locomotion,missing,&app)==2);
    app.assetCatalog.entries.clear();
    const auto mwView=add("mw","viewmodel_m40a3_mp_LOD0",assets::Role::ViewWeapon);
    add("mw3","weapon_m40a3_LOD0",assets::Role::WorldWeapon);
    const auto mwWorld=add("mw","weapon_m40a3_LOD0",assets::Role::WorldWeapon);
    CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[mwView])==mwWorld);
    const auto gold=add("mw","viewmodel_desert_eagle_gold_mp_LOD0",assets::Role::ViewWeapon);
    add("mw","weapon_desert_eagle_silver_LOD0",assets::Role::WorldWeapon);
    CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[gold])==std::size_t(-1));
    const auto goldWorld=add("mw","weapon_desert_eagle_gold_LOD0",assets::Role::WorldWeapon);
    CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[gold])==goldWorld);
    const auto wet=add("mwr","wpn_h1_pst_m9_vm_wet_camo_LOD0",assets::Role::ViewWeapon);
    const auto legacyAk=add("mwr","viewmodel_ak47_LOD0",assets::Role::ViewWeapon);
    CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[legacyAk])==std::size_t(-1));
    add("mwr","wpn_h1_pst_m9_npc_camo_LOD0",assets::Role::WorldWeapon);
    CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[wet])==std::size_t(-1));
    const auto wetWorld=add("mwr","wpn_h1_pst_m9_npc_wet_camo_LOD0",assets::Role::WorldWeapon);
    CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[wet])==wetWorld);
    for(const auto name:{"concussion","flash","frag","smoke"}){
        const auto view=add("mwr",std::string("wpn_h1_grenade_")+name+"_vm_LOD0",assets::Role::ViewWeapon);
        const auto world=add("mwr",std::string("wpn_h1_grenade_")+name+(std::string(name)=="flash"?"_mp_npc_LOD0":"_npc_mp_LOD0"),assets::Role::WorldWeapon);
        CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[view])==world);
    }
    std::cout << "Team assembly, character matching, safe locomotion and scoped world names passed\n";
    for(const auto& names:std::vector<std::pair<std::string,std::string>>{{"kriss","kriss_v"},{"lsat","lsat_iw6"},{"mk14_ebr","mk14_ebr_iw6"},{"mk14","mk14_iw6"},{"rm_22_ar","rm_22"},{"magum_iw6","magnum_iw6"},{"vbr_pdw","vbr"}}){
        app.assetCatalog.entries.clear();
        const auto view=add("ghosts","viewmodel_"+names.first+"_gold_LOD0",assets::Role::ViewWeapon);
        add("ghosts","weapon_"+names.second+"_camo_LOD0",assets::Role::WorldWeapon);
        add("mw3","weapon_"+names.second+"_gold_LOD0",assets::Role::WorldWeapon);
        CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[view])==std::size_t(-1));
        const auto world=add("ghosts","weapon_"+names.second+"_gold_LOD0",assets::Role::WorldWeapon);
        CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[view])==world);
        app.assetCatalog.entries[view].name="viewmodel_"+names.first+"_special_gold_LOD0";
        CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[view])==std::size_t(-1));
    }
}
