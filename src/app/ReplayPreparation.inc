// Included after the replay reconstruction helpers. This is CPU-only: callers
// must not publish the incoming take or change the global pack preference until
// preparation succeeds. The reconstruction helpers honor deferSceneUpload.
struct PreparedReplay {
    std::array<std::optional<CachedClassRig>,3> rigs;
    std::optional<scene::CastScene> botActor;
    std::optional<cast::Document> firstDocument;
    std::set<std::filesystem::path> constructionDocuments;
    int firstSlot{};
};

struct PreparedTakeOpen {
    std::filesystem::path path;
    take::Take take;
    cadence::content::Metadata metadata;
    std::optional<PreparedReplay> replay;
};

void refreshPreparedReplayProfiles(const AppState& source,PreparedReplay& replay){
    for(auto& rig:replay.rigs){
        if(!rig)continue;
        const auto matches=[&](const auto& paths,const auto& base){return rig->rigModelPaths==paths&&(!paths.empty()||rig->baseModelPath==base);};
        if(matches(source.loadedRigModelPaths,source.loadedBaseModelPath)){
            rig->profile=source.weaponProfile;rig->profile.stats=source.weaponTiming;rig->profilePath=source.weaponProfilePath;continue;
        }
        bool found=false;
        const auto match=[&](const auto& slots){for(const auto& live:slots)if(live&&matches(live->rigModelPaths,live->baseModelPath)){rig->profile=live->profile;rig->profilePath=live->profilePath;return true;}return false;};
        if(source.liveStandaloneBeforeTake){const auto& live=source.liveStandaloneBeforeTake->rig;if(matches(live.rigModelPaths,live.baseModelPath)){rig->profile=live.profile;rig->profilePath=live.profilePath;found=true;}}
        if(!found&&source.liveClassBeforeTake)found=match(*source.liveClassBeforeTake);
        if(!found)match(source.classSlotRigs);
    }
}

bool prepareReplay(const AppState& source,const take::Take& incoming,const std::string& incomingPack,
                   PreparedReplay& output,std::string& error){
    cadence::content::ResolutionScope resolution(incomingPack);
    error.clear();
    if(incoming.samples.empty()){
        error="Recording has no samples";
        return false;
    }
    PreparedReplay prepared;
    cadence::content::ScopedDocumentCollection observations(prepared.constructionDocuments);
    prepared.firstSlot=static_cast<int>(incoming.samples.front().weaponSlot);
    std::array<bool,3> required{};
    for(const auto& sample:incoming.samples){
        if(sample.weaponSlot>=required.size()){
            error="Recording refers to an invalid weapon slot";
            return false;
        }
        required[sample.weaponSlot]=true;
    }
    // Unused declarations are not extra requirements for trimmed recordings.
    // Every sampled slot is still prepared, so seeking cannot defer a failure.
    auto workerOwner=std::make_unique<AppState>();
    auto& worker=*workerOwner;
    worker.deferSceneUpload=true;
    worker.assetCatalog=source.assetCatalog;
    worker.defaultSalukiDirectory=source.defaultSalukiDirectory;
    worker.t5WeaponRoot=source.t5WeaponRoot;
    worker.iw5WeaponRoot=source.iw5WeaponRoot;
    worker.gameReferenceSetups=source.gameReferenceSetups;
    worker.viewmodelFov=source.viewmodelFov;
    worker.viewmodelFovScale=source.viewmodelFovScale;
    // Manifest-bearing slots reconstruct once. A legacy slot without a model
    // manifest retains a CPU copy of its supplied compatible rig instead; no
    // CAST file can safely be inferred for such a slot.
    for(std::size_t slot=0;slot<required.size();++slot){
        if(!required[slot])continue;
        const auto& actor=incoming.actorSlots[slot].empty()?incoming.actor:incoming.actorSlots[slot];
        const auto& world=incoming.worldActorSlots[slot].empty()?incoming.worldActor:incoming.worldActorSlots[slot];
        CachedClassRig rig;
        worker.hiddenWorldActor.reset();
        if(actor.empty()){
            const auto expected=incoming.bonesForSlot(static_cast<std::uint8_t>(slot));
            const auto& cached=source.classSlotRigs[slot];
            if(static_cast<int>(slot)!=source.activeClassSlot&&cached&&cached->scene.skeleton.bones.size()==expected){
                rig=*cached;rig.worldActor.reset();
                rig.replayViewmodelCamera=cached->replayViewmodelCamera.value_or(source.viewmodelCamera);
            }else if((static_cast<int>(slot)==source.activeClassSlot||!cached)&&source.scene.skeleton.bones.size()==expected){
                rig.scene=source.scene;rig.baseModelPath=source.loadedBaseModelPath;
                rig.rigModelPaths=source.loadedRigModelPaths;rig.attachmentPaths=source.loadedAttachmentPaths;
                rig.profile=source.weaponProfile;rig.profile.stats=source.weaponTiming;rig.profilePath=source.weaponProfilePath;
                rig.weaponAsset=source.selectedWeaponAsset;rig.worldModelAsset=source.classWorldModelAsset;
                rig.worldWeaponAsset=source.classWorldWeaponAsset;rig.animationPrefix=source.viewmodelAnimationPrefix;
                rig.rigAssets=source.equippedRigAssets;rig.meleeKnifeBone=source.meleeKnifeBone;
                rig.idleAnimation=source.animationIndex;rig.adsDownAnimation=source.viewmodelAdsBaseAnimation;
                rig.replayViewmodelCamera=source.viewmodelCamera;
                if(static_cast<int>(slot)==prepared.firstSlot)prepared.firstDocument=source.document;
            }else{error="Legacy weapon slot "+std::to_string(slot+1)+" needs its original compatible rig";return false;}
        }else{
        std::vector<std::filesystem::path> models;
        for(const auto& stored:actor.rigModels){
            const auto resolved=resolveTakeModelPath(worker,stored);
            if(!resolved){error="Take rig model is missing: "+stored;return false;}
            models.push_back(*resolved);
        }
        worker.loadedRigModelPaths=models;
        worker.weaponProfile=replayWeaponProfile(source,models);
        worker.selectedWeaponAsset=source.selectedWeaponAsset;
        if(!restoreTakeActorInternal(worker,actor,incoming.bonesForSlot(static_cast<std::uint8_t>(slot)),false,error)){
            error="Weapon slot "+std::to_string(slot+1)+": "+error;
            return false;
        }
        rig.scene=std::move(worker.scene);
        rig.baseModelPath=worker.loadedBaseModelPath;
        rig.rigModelPaths=worker.loadedRigModelPaths;
        rig.attachmentPaths=worker.loadedAttachmentPaths;
        rig.profile=worker.weaponProfile;
        rig.profilePath=worker.weaponProfilePath;
        rig.weaponAsset=worker.selectedWeaponAsset;
        rig.animationPrefix=worker.viewmodelAnimationPrefix;
        rig.rigAssets=worker.equippedRigAssets;
        rig.meleeKnifeBone=worker.meleeKnifeBone;
        rig.replayViewmodelCamera=actor.viewmodelCamera;
        if(static_cast<int>(slot)==prepared.firstSlot)prepared.firstDocument=std::move(worker.document);
        }
        const auto worldBones=incoming.bonesForWorldSlot(static_cast<std::uint8_t>(slot));
        if(worldBones>0){
            if(world.empty()){error="Weapon slot "+std::to_string(slot+1)+" has no recorded world actor";return false;}
            if(!restoreTakeWorldActor(worker,world,worldBones,error)){
                error="World actor slot "+std::to_string(slot+1)+": "+error;
                return false;
            }
        }
        rig.worldActor=std::move(worker.hiddenWorldActor);
        prepared.rigs[slot]=std::move(rig);
    }
    if(incoming.botCount>0){
        if(incoming.botActor.empty()||incoming.botBoneCount==0){error="Recording has no usable bot actor manifest";return false;}
        if(!restoreTakeBotActor(worker,incoming.botActor,incoming.botBoneCount,error))return false;
        prepared.botActor=std::move(worker.botActorScene);
    }
    output=std::move(prepared);
    return true;
}

// GPU publication is a separate phase. Absent slots only receive GPU placeholder
// ranges; they do not become valid CPU replay slots. This accommodates a trimmed
// recording containing slot 1 alone or slots 0 and 2 without reloading models.
// The renderer replacement itself is not transactional; the caller retains the
// old CPU state and restores its GPU residency if this fails.
bool uploadPreparedReplay(AppState& app,const PreparedReplay& prepared,std::string& error){
    if(prepared.firstSlot<0||prepared.firstSlot>=3||!prepared.rigs[prepared.firstSlot]){
        error="Prepared replay has no initial weapon rig";
        return false;
    }
    const auto& fallback=*prepared.rigs[prepared.firstSlot];
    const auto& primary=prepared.rigs[0]?*prepared.rigs[0]:fallback;
    const auto& secondary=prepared.rigs[1]?*prepared.rigs[1]:fallback;
    const auto* third=prepared.rigs[2]?&*prepared.rigs[2]:nullptr;
    app.renderer.setMapTextureResolution(app.mapTextureResolution);
    app.renderer.setHardwareTextureCompression(app.mapTextureCompression);
    const auto* bot=prepared.botActor?&*prepared.botActor:nullptr;
    const auto* primaryWorld=primary.worldActor?&*primary.worldActor:nullptr;
    const auto* secondaryWorld=secondary.worldActor?&*secondary.worldActor:nullptr;
    const auto* thirdWorld=third&&third->worldActor?&*third->worldActor:nullptr;
    const bool hasMap=app.mapGpuReady&&app.loadedMap&&app.renderer.hasMapMeshes();
    return hasMap
        ?app.renderer.replaceClassScenes(primary.scene,secondary.scene,primaryWorld,secondaryWorld,bot,error,third?&third->scene:nullptr,thirdWorld)
        :app.renderer.loadClassScenes(primary.scene,secondary.scene,primaryWorld,secondaryWorld,app.loadedMap?&app.loadedMap->scene:nullptr,bot,error,third?&third->scene:nullptr,thirdWorld);
}
