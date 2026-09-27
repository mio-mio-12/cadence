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
        a.sunBlackbody=a.ambientBlackbody=true;
        a.sunColor={.13f,.57f,.91f};a.ambientColor={.73f,.21f,.42f};
        a.bloomIntensity=.37f;a.environmentExposure=1.23f;a.eeveeRoughness=.28f;
        a.cubemapSpecularIntensity=1.73f;a.cubemapSpecularBlur=.46f;a.weaponCubemapBlur=.29f;a.cubemapKawaseSamples=9;
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1600,2400};io.DeltaTime=1.f/60;
        unsigned char* pixels{};int w{},h{};io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        for(int page:{0,3,2,4,0}){
            ImGui::NewFrame();ImGui::SetNextWindowSize({1500,2300});ImGui::Begin("Visual tab regression");
            ImGui::GetStateStorage()->SetInt(ImGui::GetID("visual_category"),page);
            if(page==2){
                ImGui::GetStateStorage()->SetInt(ImGui::GetID("2 · Specular Stack A"),1);
                ImGui::GetStateStorage()->SetInt(ImGui::GetID("3 · Specular Stack B / Routing"),1);
                for(const auto* id:{"stack_a","stack_b"}){ImGui::PushID(id);ImGui::GetStateStorage()->SetInt(ImGui::GetID("Shared cubemap specular"),1);ImGui::PopID();}
            }
            drawRenderingControls(a);ImGui::End();ImGui::Render();
            CHECK(a.sunColor.x==.13f&&a.sunColor.y==.57f&&a.sunColor.z==.91f);
            CHECK(a.ambientColor.x==.73f&&a.ambientColor.y==.21f&&a.ambientColor.z==.42f);
            CHECK(a.bloomIntensity==.37f&&a.environmentExposure==1.23f&&a.eeveeRoughness==.28f);
            CHECK(a.cubemapSpecularIntensity==1.73f&&a.cubemapSpecularBlur==.46f&&a.weaponCubemapBlur==.29f&&a.cubemapKawaseSamples==9);
        }
        ImGui::DestroyContext();
        const auto path=std::filesystem::temp_directory_path()/"cadence_v236_visual_tab.castvisual";
        CHECK(saveVisualPreset(a,path));auto loaded=std::make_unique<AppState>();VisualPresetResources resources;std::ifstream in(path);
        CHECK(parseVisualPreset(*loaded,in,resources));in.close();std::filesystem::remove(path);
        CHECK(loaded->sunColor.x==a.sunColor.x&&loaded->ambientColor.z==a.ambientColor.z&&loaded->bloomIntensity==a.bloomIntensity);
        CHECK(loaded->cubemapSpecularIntensity==a.cubemapSpecularIntensity&&loaded->cubemapSpecularBlur==a.cubemapSpecularBlur&&loaded->weaponCubemapBlur==a.weaponCubemapBlur&&loaded->cubemapKawaseSamples==a.cubemapKawaseSamples);
    }
    for(const auto game:{"cs1.6","css","csnz","cso2","eldewrito"}){
        auto isolated=std::make_unique<AppState>();auto& a=*isolated;
        a.gameplayLogic=a.assetFirstPerson=true;a.scene.importedViewGame=game;
        for(const auto slot:{"idle","pullout","fire"}){scene::Animation c;c.sourceName=std::string(slot)+".cast";c.domain=scene::AnimationDomain::ViewModel;c.durationFrames=30;c.framerate=30;c.tracks.resize(1);a.weaponProfile.animations[slot]=c.sourceName;a.scene.animations.push_back(c);}
        a.animationIndex=1;a.animationFrame=12;a.actionActive=true;a.activeAction=scene::ActionRole::Equip;a.weaponSwitchStage=2;a.actionElapsed=.4f;
        CHECK(resolveGameplayAnimation(a));CHECK(a.animationFrame==12&&a.actionElapsed==.4f);
        a.actorMode=a.actorSprinting=a.actorGrounded=true;
        CHECK(resolveGameplayAnimation(a));CHECK(a.animationFrame==12);
        a.weaponSwitchStage=0;a.actionActive=false;a.activeAction=scene::ActionRole::None;
        CHECK(resolveGameplayAnimation(a));CHECK(a.animationIndex==0);
        a.actionActive=a.actionOverlay=true;a.activeAction=scene::ActionRole::Fire;a.actionAnimationIndex=2;a.actionFrame=9;a.actionElapsed=.3f;
        CHECK(resolveGameplayAnimation(a));CHECK(a.actionFrame==9&&a.actionElapsed==.3f);
    }
    {
        auto isolated=std::make_unique<AppState>();auto& a=*isolated;a.animationSetForBots=true;a.animationSet.curatedDeaths=true;
        scene::CastScene actor;scene::Animation death;death.action=scene::ActionRole::Death;death.domain=scene::AnimationDomain::PlayerBody;death.sourceGame="css";death.sourceName="pb_death.cast";death.tracks.resize(1);actor.animations.push_back(death);
        a.botSpPainClips.push_back(0);applyBotAnimationSet(a,actor,"css");CHECK(actor.animations[0].tracks.empty());CHECK(a.botSpPainClips.empty());CHECK(cadence::prepareBotDeaths(actor).empty());
        actor.animations[0]=death;a.animationSet.set("css",death.action,death.sourceName,true);applyBotAnimationSet(a,actor,"css");CHECK(!actor.animations[0].tracks.empty());
        a.botDeathChoices=cadence::prepareBotDeaths(actor);gameplay::bot::Actor bot;bot.id=1;CHECK(botDeathAnimation(actor,bot,scene::WeaponClass::Rifle,1,&a));CHECK(botDeathAnimation(actor,bot,scene::WeaponClass::Rifle,2,&a));
    }
    {
        auto isolated=std::make_unique<AppState>();auto& a=*isolated;
        assets::Asset asset;asset.game="csnz";a.assetCatalog.entries.push_back(asset);a.selectedWeaponAsset=0;
        a.weaponProfile.archetype=weapon::Archetype::SemiSniper;
        CHECK(importedCameraScope(a));
        a.actorMode=true;a.gameplayAds=a.viewmodelAdsEngaged=true;a.adsCameraBlend=1;a.weaponTiming.hideWeaponOnAds=true;
        CHECK((livePlayerVisibility(a)&take::ScopeOverlay)!=0);
        a.weaponTiming.hideWeaponOnAds=false;CHECK((livePlayerVisibility(a)&take::ScopeOverlay)==0);
        a.weaponProfile.archetype=weapon::Archetype::Rifle;CHECK(!importedCameraScope(a));
        a.weaponProfile.archetype=weapon::Archetype::SemiSniper;a.assetCatalog.entries[0].game="cs2";CHECK(!importedCameraScope(a));
        a.assetCatalog.entries[0].game="css";scene::Animation ads;ads.sourceName="scope_up.cast";ads.domain=scene::AnimationDomain::ViewModel;ads.tracks.resize(1);a.scene.animations.push_back(ads);a.weaponProfile.animations["ads_up"]=ads.sourceName;CHECK(!importedCameraScope(a));
    }
    {
        auto isolated=std::make_unique<AppState>();auto& a=*isolated;
        a.gameplayLogic=true;a.assetFirstPerson=true;
        a.weaponTiming.firstRaiseTime=1.7f;a.weaponTiming.raiseTime=.25f;
        CHECK(firstRaiseDuration(a)==1.7f);
        a.weaponTiming.firstRaiseTime=0;
        CHECK(firstRaiseDuration(a)==.6f); // Missing clip never borrows pullout time.
        {scene::Animation fallback;fallback.sourceName="shared_pullout.cast";fallback.domain=scene::AnimationDomain::ViewModel;fallback.durationFrames=300;fallback.framerate=30;fallback.tracks.resize(1);a.scene.animations.push_back(fallback);a.weaponProfile.animations["first_raise"]=fallback.sourceName;a.weaponProfile.animations["pullout"]=fallback.sourceName;CHECK(firstRaiseDuration(a)==.25f);a.scene.animations.clear();a.weaponProfile.animations.clear();}
        a.weaponTiming.firstRaiseTime=1.7f;
        scene::Animation draw;draw.sourceName="viewmodel_test_first_raise.cast";draw.durationFrames=51;draw.framerate=30;draw.action=scene::ActionRole::FirstRaise;
        a.scene.animations.push_back(draw);a.animationIndex=a.actionAnimationIndex=0;a.actionDurationOverride=1.7f;
        a.weaponSwitchStage=2;a.weaponSwitchAlgorithm=3;a.weaponSwitchFirstRaiseAnimation=true;
        a.actionActive=true;a.activeAction=scene::ActionRole::FirstRaise;
        updateGameplay(a,.3f);
        CHECK(a.weaponSwitchStage==2&&a.actionActive&&a.activeAction==scene::ActionRole::FirstRaise);
        updateGameplay(a,1.4f);
        CHECK(a.weaponSwitchStage==0&&!a.weaponSwitchFirstRaiseAnimation);
        a.actorMode=true;a.actorMantling=true;
        CHECK(desiredMotion(a)==scene::MotionRole::Climb);
        a.actorMantling=false;a.actorGrounded=true;
        CHECK(desiredMotion(a)!=scene::MotionRole::Climb);
    }
    {
        app.gameplayAds=app.viewmodelAdsEngaged=app.viewmodelAdsExiting=app.viewmodelAdsPoseHold=true;
        app.viewmodelAdsBaseAnimation=4;app.viewmodelAdsBaseFrame=20;app.adsCameraBlend=1;
        resetWeaponAimState(app);
        CHECK(!app.gameplayAds&&!app.viewmodelAdsEngaged&&!app.viewmodelAdsExiting&&!app.viewmodelAdsPoseHold);
        CHECK(app.viewmodelAdsBaseAnimation==static_cast<std::size_t>(-1)&&app.viewmodelAdsBaseFrame==0&&app.adsCameraBlend==0);
    }
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
    std::mt19937 rouletteRandom{237};
    const auto rolls=rollBotModels(expected,{17},18,rouletteRandom);
    CHECK(rolls.actors.size()==18);
    CHECK(rolls.variants.size()>1);
    CHECK(rolls.variants.size()<=expected.size());
    for(std::size_t start=0;start+expected.size()<=rolls.actors.size();start+=expected.size()){
        std::set<std::size_t> used;
        for(std::size_t i=0;i<expected.size();++i)used.insert(rolls.variants[rolls.actors[start+i]].first);
        CHECK(used.size()==expected.size());
    }
    for(auto index:rolls.actors){
        CHECK(index<rolls.variants.size());
        CHECK(std::find(expected.begin(),expected.end(),rolls.variants[index].first)!=expected.end());
        CHECK(rolls.variants[index].second==17);
    }
    const auto single=rollBotModels({ally},{17},18,rouletteRandom);
    CHECK(single.variants.size()==1);
    CHECK(single.actors==std::vector<std::size_t>(18,0));
    CHECK(rollBotModels({},{17},18,rouletteRandom).actors.empty());
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
    for(const auto game:{"cs1.6","cscz","css","csnz","cso2"}){
        const auto view=add(game,"viewmodel_rifle_v_ak47",assets::Role::ViewWeapon);
        CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[view])==view);
        const auto name=assets::imported::worldName(game,app.assetCatalog.entries[view].name);
        const auto world=add(game,name,assets::Role::WorldWeapon);
        CHECK(findWorldWeaponForViewWeapon(app,app.assetCatalog.entries[view])==world);
    }
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
