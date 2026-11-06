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
        return scene::imported::fitPoseOnlyViewSkin(output,skin,error);
    auto referenceScene=scene::buildScene(reference);
    scene::codm::normalizeToCentimetres(referenceScene,scene::codm::presentationScale);
    return scene::codm::fitLegacyHands(output,referenceScene,hands,error);
}
bool importedViewPair(const AppState& app,const std::filesystem::path& handsPath,const std::string& driverGame){
    if(driverGame.empty())return false;
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
    // Foreign imported skins need the native hand driver, not a raw name
    // merge. This fits skin only; native weapon/animation handling stays intact.
    if(driverGame=="cs2")return (!handGame.empty()&&handGame!="cs2")||
        (handGame=="cs2"&&lowerText(handsPath.stem().string())!="viewhands_model");
    return !handGame.empty()&&(handGame!=lowerText(driverGame)||assets::imported::sourceFamily(handGame)||assets::imported::additionalFamily(handGame))&&
        (assets::imported::supported(handGame)||handGame=="cs2"||handGame=="bocw_sp"||assets::imported::supported(lowerText(driverGame))||lowerText(driverGame)=="bocw_sp"||lowerText(driverGame)=="iw_sp");
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
        for(const auto& a:catalog.entries)if(lowerText(a.game)==driverGame&&a.role==assets::Role::ViewHands){const auto name=lowerText(a.name);int score=1;if(name.starts_with("viewmodel_base_viewhands")||name.starts_with("c_usa_mp_seal6_longsleeve_viewhands"))score=10;if(name=="hands_shared"||name=="hand_707")score=50;if(!nativeHandId.empty()&&name.ends_with(nativeHandId))score=80;if(!exact.empty()&&name==exact)score=100;if(preferredIwReferenceHands(driverGame,name))score=200;if(score>bestScore){best=&a;bestScore=score;}}
        return best?best->path:std::filesystem::path{};
    };
    auto nativePath=choose(app.assetCatalog);
    if(nativePath.empty()&&!app.defaultSalukiDirectory.empty()){assets::Catalog catalog;if(!assets::appendScan(salukiExportRoot(app.defaultSalukiDirectory)/driverGame,driverGame,catalog,error))return false;nativePath=choose(catalog);}
    if(nativePath.empty()&&(driverGame=="bo"||driverGame=="t5"))nativePath=boCompatibleReferenceHands(app,error);
    if(nativePath.empty()){error="Missing native "+driverGame+" reference viewhands";return false;}
    const auto nativeDoc=cast::Document::load(nativePath);if(!nativeDoc.valid()){error="Cannot read native reference viewhands";return false;}
    auto native=scene::buildScene(nativeDoc),skin=scene::buildScene(hands);
    if(driverGame=="cs2"&&weapon){
        const auto document=cast::Document::load(weapon->path);if(!document.valid()){error="Cannot read CS2 weapon driver";return false;}
        auto weaponDriver=scene::buildScene(document);
        if(weaponDriver.skeleton.boneByName.contains("armUpperShoulder_L"))native=std::move(weaponDriver);
        else {
            // Prefer the hierarchy embedded in this weapon's own clips. The
            // visible agent/glove model need not contain the animation rig.
            auto root=weapon->path.parent_path();
            while(!root.empty()&&lowerText(root.filename().string())!="models"){const auto parent=root.parent_path();if(parent==root)break;root=parent;}
            if(lowerText(root.filename().string())=="models"){
                auto family=lowerText(weapon->name);if(family.ends_with("_model"))family.resize(family.size()-6);
                const auto folder=root.parent_path()/"animations"/"viewmodel"/family;std::error_code ec;
                for(std::filesystem::directory_iterator it(folder,ec),end;it!=end;it.increment(ec)){
                    if(ec)break;if(it->path().extension()!=".cast")continue;
                    const auto clip=cast::Document::load(it->path());if(!clip.valid())continue;
                    auto candidate=scene::buildScene(clip);
                    if(candidate.skeleton.boneByName.contains("armUpperShoulder_L")){candidate.animations.clear();native=std::move(candidate);break;}
                }
            }
            // Agent skins and standalone weapon models can contain a body rig.
            // Matching hand names are not sufficient to drive first-person clips.
            for(const auto& a:app.assetCatalog.entries)if(!native.skeleton.boneByName.contains("armUpperShoulder_L")&&lowerText(a.game)=="cs2"&&a.role==assets::Role::ViewHands&&lowerText(a.name)=="viewhands_model"){
                const auto reference=cast::Document::load(a.path);if(reference.valid()){
                    auto candidate=scene::buildScene(reference);
                    if(candidate.skeleton.boneByName.contains("armUpperShoulder_L")){native=std::move(candidate);break;}
                }
            }
            if(!native.skeleton.boneByName.contains("armUpperShoulder_L")){
                error="CS2 weapon clips have no usable embedded first-person hierarchy or compatible legacy reference";
                return false;
            }
        }
        native.meshes.clear();
    }
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
            if(driverGame=="gitsfa")for(const auto* name:{"tag_camera","tag_view"}){
                const auto from=native.skeleton.boneByName.find(name),to=weaponScene.skeleton.boneByName.find(name);
                if(from!=native.skeleton.boneByName.end()&&to!=weaponScene.skeleton.boneByName.end())weaponScene.skeleton.bones[to->second]=native.skeleton.bones[from->second];
            }
            scene::imported::parentSyntheticViewCamera(weaponScene,driverGame);
            weaponScene.importedViewGame=driverGame;native=std::move(weaponScene);
        }else{scene::CastScene composite;if(!scene::imported::assemblePrepared(std::move(weaponScene),std::move(native),composite,error))return false;native=std::move(composite);}
    }
    // Preserve authored helpers and all triangles when this is literally the
    // selected native reference and assembly has not changed its skin/bind.
    // Different files, weapon-authored palettes and CS2 retain existing fitting.
    std::error_code identityError;
    const bool identicalReference=driverGame!="cs2"&&std::filesystem::equivalent(nativePath,std::filesystem::path(hands.sourceName()),identityError)&&!identityError;
    if(!(identicalReference&&scene::imported::nativeViewSkinUnchanged(native,skin))){
        // The same preparation function reconstructs recorded hands. Keeping
        // this candidate here exercises both paths without post-load overrides.
        const bool supportCandidate=driverGame!="cs2"&&std::getenv("CADENCE_NATIVE_SUPPORT_ASSEMBLY")!=nullptr;
        if(!(supportCandidate?scene::imported::fitWithNativeSupport(native,skin,error,std::getenv("CADENCE_NATIVE_SUPPORT_ENVELOPE")!=nullptr):
             scene::imported::fitPoseOnlyViewSkin(native,skin,error)))return false;
    }
    if(driverGame=="css"&&scene::imported::gameForPath(std::filesystem::path(hands.sourceName()))=="css")
        scene::imported::bindMirroredSourceViewSkin(native,skin);
    // The normal equipment path mounts the weapon once after loading hands.
    // Retain its authoritative bind here, but not duplicate weapon geometry.
    std::erase_if(native.meshes,[](const auto& m){return m.viewmodelWeapon;});
    native.viewHandsDriverGame=driverGame;output=std::move(native);return true;
}
