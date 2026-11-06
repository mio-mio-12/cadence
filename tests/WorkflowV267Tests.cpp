#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(){
    {assets::Asset a;a.game="bo";a.name="t5_weapon_1911_viewmodel_LOD0";CHECK(animationPrefixForWeapon(a)=="viewmodel_colt1911_sw_");
     a.game="iw_sp";a.name="weapon_ump_vm_camo_LOD0";CHECK(animationPrefixForWeapon(a)=="vm_ump45_");
     a.name="weapon_m200_vm_camo_LOD0";CHECK(animationPrefixForWeapon(a)=="vm_m200_");}
    CHECK(preferredIwReferenceHands("iw_sp","viewmodel_base_viewhands_iw7_lod0"));
    CHECK(preferredIwReferenceHands("iw7","viewmodel_base_viewhands_iw7"));
    CHECK(!preferredIwReferenceHands("iw_sp","viewmodel_base_viewhands_lod0"));
    CHECK(!preferredIwReferenceHands("bo2","viewmodel_base_viewhands_iw7"));
    CHECK(!preferredIwReferenceHands("iw_sp","viewmodel_base_viewhands_iw7_unrelated"));
    ImGui::CreateContext();
    auto state=std::make_unique<AppState>();auto& app=*state;
    {
        auto references=std::make_unique<AppState>();assets::Asset hands;hands.game="bo2";hands.role=assets::Role::ViewHands;hands.name="c_usa_mp_seal6_longsleeve_viewhands_LOD0";hands.path="D:/test/bo2/reference.cast";references->assetCatalog.entries.push_back(hands);
        std::string error;CHECK(boCompatibleReferenceHands(*references,error)==hands.path);
        for(const char* game:{"cs2","cs1.6","css","csnz","cso2"}){assets::Asset skin;skin.game=game;skin.path=std::filesystem::path("D:/test")/game/"hands.cast";references->assetCatalog.entries.push_back(skin);CHECK(importedViewPair(*references,skin.path,"bo"));}
    }
    {
        app.metalShader.mode=2;app.metalShader.reflection=2.25f;app.metalShader.tint={.6f,.7f,.8f};
        app.nativeReShade.enabled=true;app.nativeReShade.preset="Test.ini";
        const auto path=std::filesystem::temp_directory_path()/"cadence_metal491.castvisual";CHECK(saveVisualPreset(app,path));
        auto loaded=std::make_unique<AppState>();VisualPresetResources resources;std::ifstream input(path);
        CHECK(parseVisualPreset(*loaded,input,resources));CHECK(render::metal::values(loaded->metalShader)==render::metal::values(app.metalShader));
        CHECK(loaded->nativeReShade.enabled&&loaded->nativeReShade.preset=="Test.ini");
        auto copied=std::make_unique<AppState>();copyVisualPresetSettings(*copied,*loaded);CHECK(copied->metalShader.mode==2&&copied->metalShader.reflection==2.25f);
        std::istringstream legacy("CASTVISUAL 5\n");CHECK(parseVisualPreset(*loaded,legacy,resources));CHECK(loaded->metalShader.mode==0);
        CHECK(!loaded->nativeReShade.enabled);app.nativeReShade={};
        input.close();std::filesystem::remove(path);app.metalShader={};
    }
    app.pendingMapFuture.emplace(std::async(std::launch::deferred,[]{return AppState::MapLoadResult{};}));
    CHECK(loadMapFile(app,"replacement.c2m")&&app.queuedMapPath==std::filesystem::path("replacement.c2m"));
    CHECK(loadMapFile(app,"newest.c2m")&&app.queuedMapPath==std::filesystem::path("newest.c2m"));
    app.pendingMapFuture.reset();app.queuedMapPath.reset();
    app.playing=true;app.takePlaying=true;app.takeTime=3;app.gameplayClock=7;
    app.actorInputCaptured=true;app.actorCaptureRequested=true;app.actorCapturePending=true;app.cameraInputCaptured=true;
    app.actorWheelDelta=2;app.freeCameraScrollDelta=4;
    setRuntimePaused(app,true);CHECK(app.runtimePaused&&!app.runtimeResumePending);
    CHECK(!app.actorInputCaptured&&!app.actorCaptureRequested&&!app.actorCapturePending&&!app.cameraInputCaptured);
    CHECK(app.actorWheelDelta==0&&app.freeCameraScrollDelta==0);
    CHECK(app.playing&&app.takePlaying&&app.takeTime==3&&app.gameplayClock==7);
    setRuntimePaused(app,false);CHECK(!app.runtimePaused&&app.runtimeResumePending);
    app.runtimeResumePending=false;app.takeRecording=true;setRuntimePaused(app,true);CHECK(!app.runtimePaused);app.takeRecording=false;
    app.exportActive=true;setRuntimePaused(app,true);CHECK(!app.runtimePaused);app.exportActive=false;
    app.playerRecorder.recording=true;setRuntimePaused(app,true);CHECK(!app.runtimePaused);app.playerRecorder.recording=false;
    app.takePlaying=false;app.takeTime=0;app.gameplayClock=0;
    assets::Asset blade;blade.name="viewmodel_nataknifed";blade.path="knife-fixture";CHECK(classCategoryForWeapon(app,blade)==9);
    blade.name="generic";blade.category="melee";CHECK(classCategoryForWeapon(app,blade)==9);
    for(const char* name:{"knife_m9_model","viewmodel_m9_dual_knife","viewmodel_nataknifed","viewmodel_karambit","v_bayonet"}){blade.name=name;blade.category.clear();CHECK(cadence::WeaponPickerCache::knifeCategory(blade));}
    for(const char* name:{"viewmodel_m9","viewmodel_ak47","viewmodel_zombibomb"}){blade.name=name;blade.category.clear();CHECK(!cadence::WeaponPickerCache::knifeCategory(blade));}
    CHECK(importedViewPair(app,"D:/exported_files/pointblank/models/viewmodel_SWAT_Male_hands.cast","cs2"));
    CHECK(importedViewPair(app,"D:/exported_files/gitsfa/models/viewhands.cast","cs2"));
    CHECK(importedViewPair(app,"D:/exported_files/dirtybomb/models/hands.cast","cs2"));
    CHECK(importedViewPair(app,"D:/exported_files/blacksquad/models/hands.cast","cs2"));
    for(const char* game:{"cs1.6","cscz","css","csnz","cso2","eldewrito"}){
        CHECK(importedViewPair(app,std::filesystem::path("D:/exported_files")/game/"models/hands_shared.cast","cs2"));
        assets::Asset hand;hand.path=std::filesystem::path("D:/relocated")/game/"hands.cast";hand.game=game;hand.role=assets::Role::ViewHands;
        app.assetCatalog.entries.push_back(hand);
        CHECK(importedViewPair(app,hand.path,"cs2"));
    }
    CHECK(importedViewPair(app,"D:/exported_files/pointblank/models/viewmodel_SWAT_Male_hands.cast","cso2"));
    CHECK(importedViewPair(app,"D:/exported_files/bo2/models/hands.cast","cs2"));
    CHECK(importedViewPair(app,"D:/exported_files/cs2/models/gloves/viewhands_ctm_sas_variantf_hands_model.cast","cs2"));
    CHECK(!importedViewPair(app,"D:/exported_files/cs2/models/viewhands/viewhands_model.cast","cs2"));
    CHECK(!importedViewPair(app,"D:/exported_files/pointblank/models/viewmodel_SWAT_Male_hands.cast",""));
    // Cold War hashed tracks require its native driver even for named CoD hands.
    // A direct name merge otherwise transfers native translations/proportions.
    for(const auto* game:{"aw","bo2","mw","mw3","mwr","iw_sp"})
        CHECK(importedViewPair(app,std::filesystem::path("D:/exported_files")/game/"models/hands.cast","bocw_sp"));
    CHECK(!importedViewPair(app,"D:/exported_files/bocw_sp/models/hands.cast","bocw_sp"));
    for(const auto* game:{"aw","bo2","mw","mw3","mwr"})
        CHECK(importedViewPair(app,std::filesystem::path("D:/exported_files")/game/"models/hands.cast","iw_sp"));
    CHECK(!importedViewPair(app,"D:/exported_files/iw_sp/models/hands.cast","iw_sp"));
    app.scene.animations.resize(1);app.scene.animations[0].durationFrames=30;app.scene.animations[0].framerate=30;
    app.scene.animations[0].action=scene::ActionRole::Unequip;
    app.gameplayLogic=true;app.playing=true;app.weaponSwitchAlgorithm=6;
    app.weaponSwitchStage=2;app.yyReverse=true;app.animationFrame=0;
    app.yyV5Return.begin(0,75,30,.444f);
    updateGameplay(app,0);
    CHECK(app.yyReverse&&app.weaponSwitchStage==2); // frame zero does not prematurely cancel the smooth turn
    CHECK(advanceYyV5(app,.02f));CHECK(app.animationFrame>0);
    updateGameplay(app,.02f);CHECK(app.yyReverse);
    CHECK(advanceYyV5(app,1));updateGameplay(app,0);CHECK(!app.yyReverse&&app.weaponSwitchStage==0);
    app.weaponSwitchStage=1;app.actionActive=true;app.activeAction=scene::ActionRole::Unequip;
    app.actionElapsed=100;app.animationFrame=10;app.yyV5Resuming=true;
    app.yyV5Resume.begin(10,-25,75);app.yyV5Elapsed=0;
    updateGameplay(app,.01f);CHECK(app.actionActive&&app.weaponSwitchStage==1);
    const float before=app.animationFrame;CHECK(advanceYyV5(app,.001f));CHECK(app.animationFrame<before);
    app.weaponSwitchAlgorithm=1;CHECK(!advanceYyV5(app,.1f)); // v2 untouched
    app.recordedTake.boneCount=1;
    take::Sample sample;sample.pose.push_back(scene::Mat4::identity());app.recordedTake.samples.push_back(sample);
    app.recordedTake.actor.baseModel="fixture.cast";app.takeDirty=true;app.takeRecording=true;
    bool called=false;CHECK(!guardTakeReplacement(app,[&]{called=true;}));CHECK(!called&&!app.takeRecording&&app.pendingTakeAction);
    CHECK(app.recordedTake.samples.size()==1);
    app.pendingTakeAction={};CHECK(!called); // cancel preserves take
    app.allowTakeReplace=true;CHECK(guardTakeReplacement(app,[]{}));app.allowTakeReplace=false;
    app.takeFilePath=std::filesystem::temp_directory_path()/("cadence_v267_workflow_"+std::to_string(GetCurrentProcessId())+".c_dm");
    const auto savedPath=app.takeFilePath;
    CHECK(saveUiTake(app,false));CHECK(!app.takeDirty&&std::filesystem::is_regular_file(savedPath));
    app.takeDirty=true;app.takeFilePath=savedPath/"invalid.c_dm";
    CHECK(!saveUiTake(app,false));CHECK(app.takeDirty&&!app.uiOperationError.empty());
    app.takeFilePath=savedPath;app.takeDirty=false;clearUiTake(app);
    CHECK(app.recordedTake.samples.empty()&&app.takeFilePath.empty()&&!app.takeDirty);
    const auto presetPath=savedPath.string()+".cadencegame";
    app.weaponSwitchAlgorithm=6;CHECK(saveGameplayPreset(app,presetPath));
    auto roundtrip=std::make_unique<AppState>();CHECK(loadGameplayPreset(*roundtrip,presetPath));CHECK(roundtrip->weaponSwitchAlgorithm==6);
    std::error_code ec;std::filesystem::remove(savedPath,ec);std::filesystem::remove(cadence::content::metadataPath(savedPath),ec);std::filesystem::remove(presetPath,ec);
    ImGui::DestroyContext();std::cout<<"PASS production YY v5, unchanged v2 routing, take guard/save/failure, preset roundtrip\n";
}
