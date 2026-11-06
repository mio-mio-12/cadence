#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>

// Run with an existing, unpacked real take and a new diagnostic output folder.
// The source take and its models are never modified.
int main(int argc,char** argv){
    try{
        if(argc!=3)return 2;
        if(std::string_view(argv[2])=="--italy"){
            auto owner=std::make_unique<AppState>();auto& app=*owner;
            app.defaultSalukiDirectory="exported_files";
            take::Take incoming;std::string error;
            if(!take::load(argv[1],incoming,error))throw std::runtime_error(error);
            for(const auto* game:{"css","bo","bo2","blacksquad","cs2"})
                if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))throw std::runtime_error(error);
            PreparedReplay replay;
            bool ready=prepareReplay(app,incoming,{},replay,error);
            if(!ready)std::cerr<<error<<'\n';
            if(!glfwInit())throw std::runtime_error("GLFW initialization failed");
            glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(800,600,"Italy rig audit",nullptr,nullptr);
            if(!window)throw std::runtime_error("Audit window failed");
            glfwMakeContextCurrent(window);
            render::StageRenderer renderer;if(!renderer.initialize(error))throw std::runtime_error(error);
            const auto captures=std::filesystem::current_path()/"diagnostics/v494/italy";std::filesystem::create_directories(captures);
            bool mounts=true;
            ImportedWorldGripBatch batch;
            for(std::uint8_t slot=0;slot<3;++slot){
                const auto& actor=incoming.actorSlots[slot];
                const auto& world=incoming.worldActorSlots[slot];
                std::cout<<"slot "<<int(slot+1)<<": ";
                if(ready)std::cout<<replay.rigs[slot]->scene.skeleton.bones.size()<<" bones ";
                std::cout<<'\n';
                if(ready&&replay.rigs[slot]->worldActor){
                    for(const auto& mount:replay.rigs[slot]->worldActor->attachments)
                        std::cout<<"restored world "<<mount.name<<'\n';
                }
                const auto view=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[&](const auto& a){return a.path==std::filesystem::path(actor.rigModels.at(0));});
                if(view==app.assetCatalog.entries.end())throw std::runtime_error("Missing view catalog entry");
                const auto index=std::size_t(view-app.assetCatalog.entries.begin());
                const auto worldIndex=findWorldWeaponForViewWeapon(app,*view);
                auto body=scene::buildScene(cast::Document::load(world.baseModel));
                const auto doc=cast::Document::load(app.assetCatalog.entries.at(worldIndex).path);
                if(!attachImportedWorldWeaponAndParts(app,doc,index,worldIndex,body,"css")||body.attachments.empty()){
                    for(const auto& warning:body.warnings)std::cerr<<warning<<'\n';
                    mounts=false;continue;
                }
                std::cout<<"mount "<<body.skeleton.bones.at(body.attachments.front().boneIndex).name
                         <<" model "<<body.attachments.front().name<<'\n';
                const std::string family=slot==0?"rifle":slot==1?"pistol":"knife";
                const auto folder=app.defaultSalukiDirectory/"css"/"animations";
                assets::AnimationFileIndex files;
                for(const auto& file:files.files(folder))if(file.lowerFilename.starts_with("pb_"+family+"_stand_idle_clip")){
                    scene::appendAnimations(cast::Document::load(file.path),body);if(!body.animations.empty())break;
                }
                const auto pose=body.samplePose(0,0);
                const auto wrist=scene::transformPoint(pose.at(body.attachments.front().boneIndex),{});
                const auto shoulder=scene::imported::world_grip::bone(body.skeleton,{"ValveBiped.Bip01_R_UpperArm"});
                const auto focus=shoulder>=0?(wrist*2+scene::transformPoint(pose[shoulder],{}))/3.f:wrist;
                if(!renderer.loadScene(body,error))throw std::runtime_error(error);
                for(int angle=0;angle<2;++angle){
                    const auto eye=focus+scene::Vec3{angle?80.f:-70.f,-110,38};
                    renderer.setCameraPosition(eye);
                    const auto vp=scene::perspective(55*scene::kPi/180,800.f/600,.1f,2000)*scene::lookAt(eye,focus,{0,0,1});
                    renderer.render(body,pose,vp,800,600,false,false,false);
                    if(!renderer.saveColorPng(captures/("slot_"+std::to_string(slot+1)+"_"+std::to_string(angle)+".png"),error))throw std::runtime_error(error);
                }
            }
            renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();
            if(!ready||!mounts)return 1;
            if(!replay.rigs[0]->worldActor||replay.rigs[0]->worldActor->attachments.empty()||
                replay.rigs[0]->worldActor->attachments.front().name!="w_snip_awp"||
                !replay.rigs[1]->worldActor||replay.rigs[1]->worldActor->attachments.empty())
                throw std::runtime_error("The original replay did not pick up the corrected default world mounts");
            const auto originalCount=incoming.samples.size();
            const auto* originalPose=incoming.samples.front().pose.data();
            take::TrimHistory trims;
            if(!trims.apply(incoming,10,originalCount-11)||!trims.apply(incoming,4,incoming.samples.size()-5)||
               !trims.undo(incoming)||!trims.undo(incoming)||incoming.samples.size()!=originalCount||incoming.samples.front().pose.data()!=originalPose)
                throw std::runtime_error("Real replay tick trim/undo did not preserve pose storage");
            app.recordedTake=std::move(incoming);app.scene=std::move(replay.rigs[0]->scene);
            app.loadedAttachmentPaths=replay.rigs[0]->attachmentPaths;app.takePreview=true;app.takePlaybackSlot=0;
            const auto before=replayAttachmentSnapshot(app);
            const auto prop=std::filesystem::path(app.recordedTake.actorSlots[0].rigModels.back());
            if(!scene::appendAttachment(cast::Document::load(prop),app.scene,0,"audit prop"))throw std::runtime_error("Could not add test attachment");
            app.loadedAttachmentPaths.push_back(prop);app.scene.attachments.back().position={4,5,6};
            commitReplayAttachmentEdits(app,before);
            if(!app.takeDirty||app.recordedTake.actorSlots[0].attachedModels.back().position.z!=6||!app.recordedTake.actorSlots[1].attachedModels.empty())
                throw std::runtime_error("Attachment edit did not stay in the selected slot");
            const auto beforeTags=app.scene.hiddenBones;
            const auto tag=std::find_if(app.scene.skeleton.bones.begin(),app.scene.skeleton.bones.end(),[&](const auto& b){
                return scene::canonicalName(b.name).starts_with("tag_")&&!beforeTags.contains(std::size_t(&b-app.scene.skeleton.bones.data()));
            });
            if(tag!=app.scene.skeleton.bones.end()){
                const auto index=std::uint32_t(tag-app.scene.skeleton.bones.begin());
                app.scene.hiddenBones.insert(index);commitReplayTagEdits(app,beforeTags);
                const auto& sample=app.recordedTake.samples.front();
                if(std::find(sample.hiddenBones.begin(),sample.hiddenBones.end(),index)==sample.hiddenBones.end())throw std::runtime_error("Tag edit did not reach replay ticks");
            }
            auto restored=std::make_unique<AppState>();restored->deferSceneUpload=true;restored->assetCatalog=app.assetCatalog;restored->defaultSalukiDirectory=app.defaultSalukiDirectory;
            if(!restoreTakeActorInternal(*restored,app.recordedTake.actorSlots[0],app.recordedTake.bonesForSlot(0),false,error)||
               restored->scene.attachments.empty()||restored->scene.attachments.back().position.z!=6)
                throw std::runtime_error("Edited attachment could not be restored: "+error);
            std::cout<<"PASS exact original slot counts, tick undo, isolated attachment edit and rebuild\n";
            return 0;
        }
        if(!glfwInit())throw std::runtime_error("GLFW initialization failed");
        glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto* window=glfwCreateWindow(640,360,"Replay preparation audit",nullptr,nullptr);
        if(!window)throw std::runtime_error("Audit window failed");
        glfwMakeContextCurrent(window);
        ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
        const auto root=std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(root);
        auto owner=std::make_unique<AppState>();auto& app=*owner;
        app.window=window;
        app.defaultSalukiDirectory="exported_files";
        std::string error;
        if(!app.renderer.initialize(error))throw std::runtime_error(error);
        for(const auto* game:{"bo2","csnz"})
            if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))throw std::runtime_error(error);
        take::Take incoming;
        if(!take::load(argv[1],incoming,error))throw std::runtime_error(error);
        const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
        {
            const auto global=cadence::content::constructionDocuments();
            std::set<std::filesystem::path> local,child,paused;
            {cadence::content::ScopedDocumentCollection collection(local);
                cadence::content::observeDocument(root/"local-only.cast");
                {cadence::content::ObservationPause nested;cadence::content::observeDocument(root/"probe-must-not-be-packed.cast");}
                {cadence::content::ScopedDocumentCollection nested(child);cadence::content::observeDocument(root/"child-only.cast");}
                cadence::content::observeDocument(root/"restored-parent.cast");}
            {cadence::content::ObservationPause pause;cadence::content::ScopedDocumentCollection collection(paused);cadence::content::observeDocument(root/"outer-paused.cast");}
            check(local.size()==2&&child.size()==1&&paused.empty()&&cadence::content::constructionDocuments()==global,"Scoped construction collection leaked, captured probes, or failed nested restoration");
        }
        check(!incoming.actor.empty()&&!incoming.samples.empty(),"Need a manifest-bearing real take");
        const auto first=incoming.samples.front().weaponSlot;
        const auto& actor=incoming.actorSlots[first].empty()?incoming.actor:incoming.actorSlots[first];
        for(const auto& path:actor.rigModels){const auto resolved=resolveTakeModelPath(app,path);check(resolved.has_value(),"Real rig dependency missing");app.loadedRigModelPaths.push_back(*resolved);}
        app.weaponProfile.gunPosition={12,23,34};app.weaponProfile.adsGunPosition={45,56,67};
        app.weaponProfile.separateAdsPosition=true;app.weaponProfile.viewmodelFovMultiplier=1.37f;
        app.weaponProfile.sprintBobMultiplier=.23f;app.weaponProfile.flipViewmodel=true;
        check(openRecordedTake(app,argv[1]),app.status.c_str());
        app.takePreview=true;app.takePlaying=true;app.takeTime=3.25f;
        app.cameraYaw=.72f;app.cameraPitch=-.23f;
        const auto observations=cadence::content::constructionDocuments();
        const auto* sceneBones=app.scene.skeleton.bones.data();const auto* sampleData=app.recordedTake.samples.data();
        const auto metadataPack=app.demoLibrary.recording.pack;const auto context=cadence::content::resolverContextKey();
        const auto unchanged=[&]{check(app.takePreview&&app.takePlaying&&app.takeTime==3.25f&&app.cameraYaw==.72f&&app.cameraPitch==-.23f,"Preparation mutated active playback");check(cadence::content::constructionDocuments()==observations,"Preparation leaked construction dependencies");check(app.scene.skeleton.bones.data()==sceneBones&&app.recordedTake.samples.data()==sampleData&&app.demoLibrary.recording.pack==metadataPack&&cadence::content::resolverContextKey()==context,"Failed open replaced the active scene, take or resolver");};
        PreparedReplay good;
        check(prepareReplay(app,incoming,{},good,error),error.c_str());unchanged();
        check(good.rigs[first].has_value(),"Initial slot not prepared");
        check(!good.constructionDocuments.empty(),"Real preparation did not retain construction dependencies");
        const auto& profile=good.rigs[first]->profile;
        check(profile.gunPosition.x==12&&profile.gunPosition.y==23&&profile.gunPosition.z==34&&profile.adsGunPosition.z==67&&profile.separateAdsPosition&&profile.viewmodelFovMultiplier==1.37f&&profile.sprintBobMultiplier==.23f&&profile.flipViewmodel,"Current presentation profile was not preserved");
        const auto malformed=root/"existing-invalid.cast";
        {std::ofstream out(malformed,std::ios::binary);out<<"not a cast document";}
        const auto emptyCast=root/"existing-empty.cast";
        {std::ofstream out(emptyCast,std::ios::binary);const std::uint32_t header[]{cast::Document::kMagic,1,0,0};out.write(reinterpret_cast<const char*>(header),sizeof(header));}
        {cadence::content::ObservationPause pause;check(cast::Document::load(emptyCast).valid(),"Empty CAST parse fixture invalid");}
        const auto failure=[&](take::Take broken,const char* name){
            // Save/load ensures this is a valid recording with bad existing
            // model content, not merely a structurally invalid Take object.
            const auto path=root/(std::string(name)+".c_dm");
            check(take::save(broken,path,error),error.c_str());take::Take readable;
            check(take::load(path,readable,error),error.c_str());
            PreparedReplay preserved;preserved.firstSlot=2;
            check(!prepareReplay(app,readable,{},preserved,error),"Invalid reconstruction unexpectedly succeeded");
            check(preserved.firstSlot==2&&!preserved.rigs[0],"Failed preparation published a partial output");unchanged();
            check(!openRecordedTake(app,path),"Direct open accepted invalid reconstruction");
            if(readable.actor.empty())std::cout<<name<<" rejected; preview="<<app.takePreview<<" playing="<<app.takePlaying<<" time="<<app.takeTime<<" status="<<app.status<<std::endl;
            unchanged();
            std::cout<<name<<": "<<error<<'\n';
        };
        for(const auto& model:{malformed,emptyCast}){
            auto broken=incoming;broken.actorSlots[first]=actor;broken.actorSlots[first].baseModel=model.string();broken.actorSlots[first].rigModels.clear();broken.actorSlots[first].attachedModels.clear();
            failure(std::move(broken),model==malformed?"invalid-cast":"incompatible-cast");
        }
        const auto other=static_cast<std::uint8_t>((first+1)%3);
        auto late=incoming;late.actorSlots[other]=actor;late.actorSlots[other].baseModel=malformed.string();late.actorSlots[other].rigModels.clear();late.actorSlots[other].attachedModels.clear();
        late.actorSlotBoneCounts[other]=incoming.bonesForSlot(first);late.worldActorSlotBoneCounts[other]=incoming.bonesForWorldSlot(first);
        auto last=incoming.samples.front();last.time=incoming.samples.back().time+1.f;last.weaponSlot=other;late.samples.push_back(std::move(last));failure(std::move(late),"later-slot");
        auto legacyWorld=incoming;
        legacyWorld.actor={};legacyWorld.actorSlots={};legacyWorld.worldActorSlots={};
        legacyWorld.worldActor={};legacyWorld.worldActor.baseModel=malformed.string();legacyWorld.worldBoneCount=1;legacyWorld.worldActorSlotBoneCounts={};
        legacyWorld.botActor={};legacyWorld.botCount=legacyWorld.botBoneCount=0;
        for(auto& sample:legacyWorld.samples){sample.weaponSlot=0;sample.worldActor.pose={scene::Mat4::identity()};sample.bots.clear();}
        failure(legacyWorld,"legacy-invalid-world");
        auto legacyBot=legacyWorld;legacyBot.worldActor={};legacyBot.worldBoneCount=0;
        legacyBot.botActor.baseModel=malformed.string();legacyBot.botCount=legacyBot.botBoneCount=1;
        for(auto& sample:legacyBot.samples){sample.worldActor.pose.clear();take::RecordedActorState bot;bot.pose={scene::Mat4::identity()};sample.bots={std::move(bot)};}
        failure(std::move(legacyBot),"legacy-invalid-bot");
        const auto snapshotPath=root/"retained-input.c_dm";
        check(take::save(incoming,snapshotPath,error),error.c_str());
        cadence::content::Metadata snapshotMetadata;snapshotMetadata.mapScale=7;
        cadence::content::writeMetadata(snapshotPath,snapshotMetadata);
        auto retained=prepareRecordedTakeOpen(app,snapshotPath);
        check(static_cast<bool>(retained),app.status.c_str());unchanged();
        const auto retainedDependencies=retained->replay->constructionDocuments;
        retained->metadata.pack="unmounted-audit-pack";
        check(!publishRecordedTakeOpen(app,*retained),"Publication accepted a now-unavailable pack");unchanged();
        retained->metadata.pack.clear();
        {std::ofstream out(snapshotPath,std::ios::binary|std::ios::trunc);out<<"replaced after preparation";}
        snapshotMetadata.mapScale=9;cadence::content::writeMetadata(snapshotPath,snapshotMetadata);
        app.weaponProfile.gunPosition.x=98.f;app.weaponProfile.adsGunPosition.y=76.f;
        check(publishRecordedTakeOpen(app,*retained),app.status.c_str());
        check(app.recordedTake.samples.size()==incoming.samples.size()&&app.demoLibrary.recording.mapScale==7,"Publication reread changed input take/metadata");
        check(app.weaponProfile.gunPosition.x==98.f&&app.weaponProfile.adsGunPosition.y==76.f,"Pending publication lost current matching presentation edits");
        const auto publishedDependencies=cadence::content::constructionDocuments();
        for(const auto& dependency:retainedDependencies){
            check(std::find(publishedDependencies.begin(),publishedDependencies.end(),dependency)!=publishedDependencies.end(),"Published construction dependency missing from later capture inventory");
            check(std::find(app.demoLibrary.recording.construction.begin(),app.demoLibrary.recording.construction.end(),cadence::content::utf8(dependency))!=app.demoLibrary.recording.construction.end(),"Published construction dependency missing from saved metadata");
        }
        std::cout<<"Retained input snapshot, pack recheck and pending profile refresh passed\n";
        const auto body=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[](const auto& asset){return asset.game=="bo2"&&lowerText(asset.name)=="c_chn_mp_pla_assault_fb_lod0";});
        check(body!=app.assetCatalog.entries.end(),"Real BO2 auxiliary body fixture is missing");
        scene::CastScene bodyScene;
        {cadence::content::ObservationPause pause;const auto document=cast::Document::load(body->path);check(document.valid(),"BO2 body fixture did not parse");bodyScene=scene::buildScene(document);}
        check(!bodyScene.skeleton.bones.empty(),"BO2 body fixture has no skeleton");
        auto legacyGood=incoming;legacyGood.actor={};legacyGood.actorSlots={};legacyGood.worldActorSlots={};
        legacyGood.worldActor={};legacyGood.worldActor.baseModel=body->path.string();legacyGood.worldBoneCount=bodyScene.skeleton.bones.size();legacyGood.worldActorSlotBoneCounts={};
        legacyGood.botActor=legacyGood.worldActor;legacyGood.botCount=1;legacyGood.botBoneCount=legacyGood.worldBoneCount;
        const auto bodyPose=bodyScene.samplePose(0,0);
        for(auto& sample:legacyGood.samples){sample.weaponSlot=0;sample.worldActor.pose=bodyPose;take::RecordedActorState bot;bot.pose=bodyPose;sample.bots={std::move(bot)};}
        const auto liveBones=app.scene.skeleton.bones.size(),liveMeshes=app.scene.meshes.size(),liveClips=app.scene.animations.size();
        const auto liveBase=app.loadedBaseModelPath;const bool liveCamera=app.viewmodelCamera;
        std::vector<std::array<float,16>> liveInverseBinds;for(const auto& bone:app.scene.skeleton.bones)liveInverseBinds.push_back(bone.inverseBind.v);
        const auto positive=root/"legacy-valid-auxiliaries.c_dm";
        check(take::save(legacyGood,positive,error),error.c_str());check(openRecordedTake(app,positive),app.status.c_str());
        check(app.scene.skeleton.bones.size()==liveBones&&app.scene.meshes.size()==liveMeshes&&app.scene.animations.size()==liveClips&&app.loadedBaseModelPath==liveBase,"Legacy auxiliary publication replaced its supplied main rig");
        for(std::size_t b=0;b<liveBones;++b)check(app.scene.skeleton.bones[b].inverseBind.v==liveInverseBinds[b],"Legacy auxiliary publication changed main bind transforms");
        check(app.hiddenWorldActor&&app.botActorScene&&app.hiddenWorldActor->skeleton.bones.size()==legacyGood.worldBoneCount&&app.botActorScene->skeleton.bones.size()==legacyGood.botBoneCount,"Valid legacy world/bot actors missing");
        check(app.viewmodelCamera==liveCamera&&app.weaponProfile.gunPosition.x==98.f,"Legacy auxiliary publication lost presentation state");
        auto mixed=legacyGood;mixed.actorSlots[1]=actor;mixed.actorSlotBoneCounts[1]=incoming.bonesForSlot(first);
        auto mixedLast=mixed.samples.front();mixedLast.time=mixed.samples.back().time+1.f;mixedLast.weaponSlot=1;mixed.samples.push_back(std::move(mixedLast));
        const auto mixedPath=root/"rootless-mixed-view-manifests.c_dm";
        check(take::save(mixed,mixedPath,error),error.c_str());check(openRecordedTake(app,mixedPath),app.status.c_str());
        check(ensureTakeWeaponSlot(app,1)&&ensureTakeWeaponSlot(app,0),"Rootless mixed recording could not switch prepared slots");
        check(app.viewmodelCamera==liveCamera,"Missing-manifest cached slot lost its camera mode");
        std::cout<<"Valid legacy world/bot actors and mixed manifest slot switching passed\n";
        auto unrelated=std::make_unique<AppState>();unrelated->assetCatalog=app.assetCatalog;unrelated->defaultSalukiDirectory=app.defaultSalukiDirectory;
        auto fullyManifested=incoming;fullyManifested.actor={};fullyManifested.actorSlots={};fullyManifested.actorSlots[0]=actor;
        for(auto& sample:fullyManifested.samples)sample.weaponSlot=0;
        const auto explicitPath=root/"rootless-all-explicit.c_dm";
        check(take::save(fullyManifested,explicitPath,error),error.c_str());
        check(!fullyManifested.compatible(unrelated->scene.skeleton.bones.size()),"Explicit legacy gate test needs a mismatching current rig");
        check(static_cast<bool>(prepareRecordedTakeOpen(*unrelated,explicitPath)),"Fully manifested sampled slots unnecessarily required a matching current rig");
        fullyManifested.actorSlots={};
        const auto noManifestPath=root/"legacy-needs-current-rig.c_dm";
        check(take::save(fullyManifested,noManifestPath,error),error.c_str());
        check(!prepareRecordedTakeOpen(*unrelated,noManifestPath),"Missing-manifest legacy slot bypassed original-rig prerequisite");
        unrelated->scene=bodyScene;unrelated->activeClassSlot=0;
        CachedClassRig supplied;supplied.scene=app.scene;supplied.baseModelPath=app.loadedBaseModelPath;supplied.rigModelPaths=app.loadedRigModelPaths;supplied.profile=app.weaponProfile;
        unrelated->classSlotRigs[1]=std::move(supplied);
        take::Take unequal;unequal.boneCount=app.scene.skeleton.bones.size();
        unequal.actorSlots[0].baseModel=body->path.string();
        unequal.actorSlotBoneCounts={bodyScene.skeleton.bones.size(),unequal.boneCount,unequal.boneCount};
        take::Sample cachedSample;cachedSample.weaponSlot=1;cachedSample.pose=app.scene.samplePose(0,0);unequal.samples.push_back(cachedSample);
        take::Sample explicitSample;explicitSample.weaponSlot=0;explicitSample.time=1;explicitSample.pose=bodyPose;unequal.samples.push_back(explicitSample);
        check(unequal.boneCount!=unrelated->scene.skeleton.bones.size(),"Unequal cached/current fixture accidentally has equal rigs");
        const auto unequalPath=root/"rootless-mixed-unequal-cached.c_dm";
        check(take::save(unequal,unequalPath,error),error.c_str());
        auto unequalRequest=prepareRecordedTakeOpen(*unrelated,unequalPath);
        check(static_cast<bool>(unequalRequest),"Matching cached legacy slot was rejected because current explicit rig differs");
        check(unequalRequest->replay->rigs[1]->scene.skeleton.bones.size()==unequal.boneCount&&unequalRequest->replay->rigs[0]->scene.skeleton.bones.size()==bodyScene.skeleton.bones.size(),"Mixed current/cached slot preparation selected the wrong rigs");
        unrelated->classSlotRigs[1].reset();
        check(!prepareRecordedTakeOpen(*unrelated,unequalPath),"Missing matching cached legacy rig was not rejected");
        unrelated->loadedBaseModelPath="base-a.cast";unrelated->loadedRigModelPaths.clear();
        unrelated->weaponProfile.flipViewmodel=false;unrelated->weaponProfile.viewmodelFovMultiplier=1.1f;
        CachedClassRig baseOnly;baseOnly.baseModelPath="base-b.cast";baseOnly.profile.flipViewmodel=true;baseOnly.profile.viewmodelFovMultiplier=1.9f;baseOnly.profile.gunPosition={4,5,6};
        unrelated->classSlotRigs[1]=baseOnly;
        PreparedReplay baseProfiles;baseProfiles.rigs[1]=baseOnly;
        refreshPreparedReplayProfiles(*unrelated,baseProfiles);
        check(baseProfiles.rigs[1]->profile.flipViewmodel&&baseProfiles.rigs[1]->profile.viewmodelFovMultiplier==1.9f&&baseProfiles.rigs[1]->profile.gunPosition.y==5,"Base-only legacy profile was replaced by a different base model");
        std::cout<<"Rootless explicit-manifest eligibility and base-only profile isolation passed\n";
        std::cout<<"CPU-only preparation/profile preservation and existing-model failures passed\n";
        unrelated.reset();owner.reset();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
