std::filesystem::path nativePointBlankHandsPath(const AppState& app){
    const auto find=[](const assets::Catalog& catalog){for(const auto& a:catalog.entries)if(lowerText(a.game)=="pointblank"&&lowerText(a.name)=="viewmodel_swat_male_hands")return a.path;return std::filesystem::path{};};
    if(auto path=find(app.assetCatalog);!path.empty())return path;
    assets::Catalog catalog;std::string error;
    if(!app.defaultSalukiDirectory.empty()&&assets::appendScan(salukiExportRoot(app.defaultSalukiDirectory)/"pointblank","pointblank",catalog,error))return find(catalog);
    return {};
}
bool assembleCodmViewHands(AppState& app,const cast::Document& weapon,const cast::Document& hands,scene::CastScene& output,std::string& error){
    const auto skin=scene::buildScene(hands);
    const bool native=skin.skeleton.boneByName.contains("b_LeftHand");
    auto reference=hands;
    if(!native){
        assets::Catalog catalog;
        auto ref=cadence::codm::referenceHands(app.assetCatalog);
        if(ref)reference=cast::Document::load(app.assetCatalog.entries[*ref].path);
        else{
            if(app.defaultSalukiDirectory.empty()||!assets::appendScan(salukiExportRoot(app.defaultSalukiDirectory)/"codm","codm",catalog,error))return false;
            ref=cadence::codm::referenceHands(catalog);
            if(!ref){error="Missing native CODM reference viewhands";return false;}
            reference=cast::Document::load(catalog.entries[*ref].path);
        }
    }
    if(!scene::codm::assemble(weapon,reference,output,error))return false;
    if(native)return true;
    // Use the same native driver for live equipment and replay reconstruction.
    if(assets::imported::supported(scene::imported::gameForPath(std::filesystem::path(hands.sourceName())))||
       gameFromExportPath(std::filesystem::path(hands.sourceName()))=="bocw_sp"||gameFromExportPath(std::filesystem::path(hands.sourceName()))=="cs2"||scene::pointblank::exportedByPb2cast(hands))
        return scene::imported::fitForeignViewSkin(output,skin,error);
    auto referenceScene=scene::buildScene(reference);
    scene::codm::normalizeToCentimetres(referenceScene,scene::codm::presentationScale);
    return scene::codm::fitLegacyHands(output,referenceScene,hands,error);
}
bool importedViewPair(const AppState& app,const std::filesystem::path& handsPath,const std::string& driverGame){
    if(driverGame.empty()||driverGame=="cs2")return false;
    std::string handGame=scene::imported::gameForPath(handsPath);
    for(const auto& a:app.assetCatalog.entries)if(a.path==handsPath){handGame=lowerText(a.game);break;}
    // Relocated/canonical path spellings need not equal a catalog entry.
    // Keep the imported-only classifier narrow; recover established CoD games
    // here so foreign-hand preparation does not silently become a raw merge.
    if(handGame.empty()){
        const auto logical=cadence::content::exportKey(handsPath);
        const auto separator=logical.find('/');
        handGame=separator!=std::string::npos?logical.substr(0,separator):gameFromExportPath(handsPath);
        // Legacy saluki/exported/_files trees contain wrapper directories,
        // not games. In particular, never mistake that wrapper for CS2's game.
        if(handGame=="exported"||handGame=="_files")handGame.clear();
    }
    return !handGame.empty()&&(handGame!=lowerText(driverGame)||assets::imported::sourceFamily(handGame))&&
        (assets::imported::supported(handGame)||handGame=="cs2"||handGame=="bocw_sp"||assets::imported::supported(lowerText(driverGame)));
}
bool prepareImportedViewHands(AppState& app,const cast::Document& hands,std::string driverGame,scene::CastScene& output,std::string& error,const std::filesystem::path& recordedWeapon={}){
    driverGame=lowerText(std::move(driverGame));
    if(!importedViewPair(app,std::filesystem::path(hands.sourceName()),driverGame))return false;
    const assets::Asset* weapon=nullptr;
    if(app.selectedWeaponAsset<app.assetCatalog.entries.size()&&lowerText(app.assetCatalog.entries[app.selectedWeaponAsset].game)==driverGame)weapon=&app.assetCatalog.entries[app.selectedWeaponAsset];
    // A take identifies its native weapon even when that library is disabled.
    // Keep this descriptor local; do not enable or mutate the user's catalog.
    assets::Asset replayWeapon;
    if(!recordedWeapon.empty()){
        replayWeapon.path=recordedWeapon;replayWeapon.game=driverGame;replayWeapon.name=recordedWeapon.stem().string();replayWeapon.role=assets::Role::ViewWeapon;
        weapon=&replayWeapon;
    }
    std::string exact;
    if(weapon&&assets::imported::supported(driverGame))exact=assets::imported::weaponStem(driverGame,weapon->name)+(driverGame=="cso2"?"_hand_707":"_hands");
    std::string nativeHandId;
    if(weapon&&driverGame=="eldewrito"){auto stem=assets::imported::weaponStem(driverGame,weapon->name);if(stem.ends_with("_dual"))stem.resize(stem.size()-5);if(stem.find("_fp_")!=std::string::npos)nativeHandId=stem.substr(stem.rfind('_'));}
    const auto choose=[&](const assets::Catalog& catalog)->std::filesystem::path{
        const assets::Asset* best=nullptr;int bestScore=-1;
        for(const auto& a:catalog.entries)if(lowerText(a.game)==driverGame&&a.role==assets::Role::ViewHands){const auto name=lowerText(a.name);int score=1;if(name.starts_with("viewmodel_base_viewhands")||name.starts_with("c_usa_mp_seal6_longsleeve_viewhands"))score=10;if(name=="hands_shared"||name=="hand_707")score=50;if(!nativeHandId.empty()&&name.ends_with(nativeHandId))score=80;if(!exact.empty()&&name==exact)score=100;if(score>bestScore){best=&a;bestScore=score;}}
        return best?best->path:std::filesystem::path{};
    };
    auto nativePath=choose(app.assetCatalog);
    if(nativePath.empty()&&!app.defaultSalukiDirectory.empty()){assets::Catalog catalog;if(!assets::appendScan(salukiExportRoot(app.defaultSalukiDirectory)/driverGame,driverGame,catalog,error))return false;nativePath=choose(catalog);}
    if(nativePath.empty()){error="Missing native "+driverGame+" reference viewhands";return false;}
    const auto nativeDoc=cast::Document::load(nativePath);if(!nativeDoc.valid()){error="Cannot read native reference viewhands";return false;}
    auto native=scene::buildScene(nativeDoc),skin=scene::buildScene(hands);
    if(assets::imported::supported(driverGame)){
        if(!weapon){error="Select the native weapon before fitting foreign viewhands";return false;}
        const auto weaponDoc=cast::Document::load(weapon->path);if(!weaponDoc.valid()){error="Cannot read native weapon driver";return false;}
        auto weaponScene=scene::buildScene(weaponDoc);
        const auto authoredArms=scene::imported::viewLayout(weaponScene);
        if(!authoredArms.usable()&&assets::imported::sourceFamily(driverGame)&&scene::imported::gameForPath(std::filesystem::path(hands.sourceName()))==driverGame&&weaponScene.skeleton.boneByName.contains("Left Hand")&&weaponScene.skeleton.boneByName.contains("Right Hand")&&weaponScene.meshes.size()>1){
            // Old combined exports do not contain the finger joints required
            // by universal skins. Mount their own geometry once, unchanged.
            weaponScene.meshes.clear();scene::imported::addViewCamera(nativeDoc,weaponScene);
            weaponScene.importedViewGame.clear();weaponScene.viewHandsDriverGame=driverGame;
            weaponScene.warnings.push_back("Legacy combined rig: using embedded native hands; shared-hand conversion requires a complete exported arm skeleton");
            output=std::move(weaponScene);error.clear();return true;
        }
        bool matchingReference=true;for(const auto& mesh:native.meshes)for(const auto& vertex:mesh.vertices)for(int k=0;k<4;++k)if(vertex.weights[k]>0&&vertex.bones[k]<native.skeleton.bones.size())matchingReference&=weaponScene.skeleton.boneByName.contains(native.skeleton.bones[vertex.bones[k]].name);
        const bool useAuthoredDriver=authoredArms.usable()&&(!authoredArms.valid()||!matchingReference);
        if(useAuthoredDriver){
            // Some native props author just one arm. Do not graft an unrelated
            // shared two-arm rig onto that driver: only its authored arm moves.
            for(auto& mesh:weaponScene.meshes)mesh.viewmodelWeapon=true;
            scene::imported::addViewCamera(nativeDoc,weaponScene);
            weaponScene.importedViewGame=driverGame;native=std::move(weaponScene);
        }else{scene::CastScene composite;if(!scene::imported::assemblePrepared(std::move(weaponScene),std::move(native),composite,error))return false;native=std::move(composite);}
    }
    if(!scene::imported::fitForeignViewSkin(native,skin,error))return false;
    // The normal equipment path mounts the weapon once after loading hands.
    // Retain its authoritative bind here, but not duplicate weapon geometry.
    std::erase_if(native.meshes,[](const auto& m){return m.viewmodelWeapon;});
    native.viewHandsDriverGame=driverGame;output=std::move(native);return true;
}
