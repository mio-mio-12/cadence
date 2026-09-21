// Benchmark-only loader. Never borrows or mutates the live Play/Replay scene.
void prepareBenchmarkWorld(AppState& state,const AppState& owner,const std::string& sourceGame,std::size_t bodyAsset){
    state.scene={};state.animationSources.clear();state.animationDocuments.clear();
    state.loadedRigModelPaths.clear();state.loadedAttachmentPaths.clear();
    if(bodyAsset>=owner.assetCatalog.entries.size()||owner.assetCatalog.entries[bodyAsset].role!=assets::Role::PlayerModel){state.status="Missing explicitly selected player model";return;}
    const auto& body=owner.assetCatalog.entries[bodyAsset];
    state.loadedBaseModelPath=body.path;state.selectedBaseAsset=bodyAsset;
    const auto document=cast::Document::load(body.path);
    if(!document.valid()){state.status="Could not parse player model: "+body.path.string();return;}
    state.scene=scene::buildScene(document);
    std::unordered_set<std::string> parts{body.path.lexically_normal().string()};
    const auto appendPart=[&](std::size_t index){
        if(index>=owner.assetCatalog.entries.size())return;
        const auto& part=owner.assetCatalog.entries[index];
        if(lowerText(part.game)!=lowerText(body.game)||!parts.insert(part.path.lexically_normal().string()).second)return;
        const auto partDoc=cast::Document::load(part.path);
        if(partDoc.valid()){scene::appendRigModel(partDoc,state.scene,part.name);state.loadedRigModelPaths.push_back(part.path);}
    };
    for(auto part:characterAssemblyParts(owner,body))appendPart(part);
    if(body.game=="aw")scene::refreshCharacterBounds(state.scene);
    // Embedded clips from the body/parts are not the requested animation source.
    state.scene.animations.clear();state.scene.runtimePoseAdapters.clear();
    std::unordered_set<std::string> loaded;
    const auto appendDocument=[&](const cast::Document& animation){
        if(!animation.valid()||!loaded.insert(animation.sourceName()).second)return;
        const auto before=state.scene.animations.size();
        scene::appendAnimations(animation,state.scene);
        for(std::size_t i=before;i<state.scene.animations.size();++i)state.scene.animations[i].sourceGame=sourceGame;
        if(state.scene.animations.size()>before)state.animationSources.emplace_back(animation.sourceName());
    };
    const auto cached=owner.botAnimationCache.find(lowerText(sourceGame));
    if(cached!=owner.botAnimationCache.end())for(const auto& animation:cached->second)appendDocument(animation);
    // Also scan torso clips: the gameplay bot cache intentionally only scans pb_/mp_.
    std::optional<std::filesystem::path> folder;
    for(const auto& asset:owner.assetCatalog.entries)if(lowerText(asset.game)==lowerText(sourceGame)){folder=animationFolderFor(asset);if(folder)break;}
    std::vector<std::filesystem::path> paths;
    if(folder){
        std::error_code ec;
        for(std::filesystem::recursive_directory_iterator it(*folder,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
            if(ec){ec.clear();continue;}
            if(!it->is_regular_file(ec)||lowerText(it->path().extension().string())!=".cast")continue;
            const auto name=lowerText(it->path().filename().string());
            if(name.find("dog")!=std::string::npos||name.find("shepherd")!=std::string::npos||name.find("vehicle")!=std::string::npos||name.find("turret")!=std::string::npos)continue;
            scene::Animation metadata;scene::classifyAnimationName(name,metadata);
            if(metadata.domain==scene::AnimationDomain::PlayerBody||metadata.domain==scene::AnimationDomain::PlayerTorso)paths.push_back(it->path());
        }
    }
    std::sort(paths.begin(),paths.end());
    for(const auto& path:paths)if(!loaded.contains(path.string()))appendDocument(cast::Document::load(path));
    bool missingSourceSkeleton=false;
    if(lowerText(sourceGame)!=lowerText(body.game)){
        auto sourceSkeleton=playerSkeletonForGame(owner,sourceGame);
        const auto sourceSetup=owner.gameReferenceSetups.find(sourceGame);
        if(sourceSetup!=owner.gameReferenceSetups.end()&&sourceSetup->second.playermodelAsset<owner.assetCatalog.entries.size()){
            const auto& sourceAsset=owner.assetCatalog.entries[sourceSetup->second.playermodelAsset];
            if(sourceAsset.role==assets::Role::PlayerModel&&lowerText(sourceAsset.game)==lowerText(sourceGame)){
                const auto sourceDocument=cast::Document::load(sourceAsset.path);
                if(sourceDocument.valid())sourceSkeleton=scene::buildScene(sourceDocument).skeleton;
            }
        }
        if(sourceSkeleton)scene::retargetAnimationRange(state.scene,0,*sourceSkeleton);
        else missingSourceSkeleton=true;
    }
    state.animationIndex=state.previousAnimationIndex=0;state.animationFrame=state.previousAnimationFrame=0;
    state.viewmodelCamera=false;state.actorMode=false;state.gameplayLogic=false;
    focusCamera(state);
    if(!uploadMainScene(state)){state.status="Benchmark player upload failed: "+state.rendererError;return;}
    state.status=state.scene.animations.empty()?"No classified player locomotion clips found for "+sourceGame:
        "Loaded "+std::to_string(state.scene.animations.size())+" player clips from "+sourceGame+" on "+body.game+" body";
    if(missingSourceSkeleton)state.status+="; missing source skeleton: cross-game mapping is unverified";
}
