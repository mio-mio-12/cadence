#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>

int main(int argc,char** argv){
    try{
        if(argc!=2||!glfwInit())return 2;
        const auto output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output);
        glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto* window=glfwCreateWindow(640,360,"Standalone replay return audit",nullptr,nullptr);
        if(!window)throw std::runtime_error("No audit window");
        glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
        auto owner=std::make_unique<AppState>();auto& app=*owner;app.window=window;
        std::string error;if(!app.renderer.initialize(error))throw std::runtime_error(error);
        app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
        for(const auto* game:{"bo2","csnz"})if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))throw std::runtime_error(error);
        const auto select=[&](std::string_view game,assets::Role role,std::string_view name){
            for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& asset=app.assetCatalog.entries[i];if(asset.game==game&&asset.role==role&&asset.name.find(name)!=std::string::npos)return i;}
            throw std::runtime_error("Missing standalone fixture asset: "+std::string(name));
        };
        app.selectedBaseAsset=select("bo2",assets::Role::ViewHands,"seal6");
        app.selectedWeaponAsset=select("csnz",assets::Role::ViewWeapon,"as50");
        equipViewWeapon(app,app.selectedWeaponAsset);
        const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
        check(app.activeClassSlot==-1&&!app.classSlotRigs[0]&&!app.classSlotRigs[1]&&!app.classSlotRigs[2],"Fixture unexpectedly loaded a class");
        check(!app.scene.animations.empty(),"Standalone fixture has no live animations");
        app.actorMode=true;app.gameplayLogic=true;
        const auto clips=[&]{std::vector<std::string> names;for(const auto& clip:app.scene.animations)names.push_back(clip.sourceName);return names;};
        const auto liveClips=clips();const auto liveSources=app.animationSources;
        const auto liveDocumentCount=app.animationDocuments.size();
        const auto liveDocument=app.document?app.document->sourceName():std::string{};
        const auto liveTokens=app.viewmodelAttachmentTokens;const auto liveBaseAsset=app.selectedBaseAsset;
        const auto liveBase=app.loadedBaseModelPath;const auto liveModels=app.loadedRigModelPaths;
        const auto liveWeapon=app.selectedWeaponAsset;
        const auto fire=[&]{
            stopGameplayAction(app);app.gameplayRechamber=false;app.gameplayReloadEmpty=false;app.gameplayAction=scene::ActionRole::Fire;
            triggerGameplayAction(app);
            const auto index=app.actionOverlay?app.actionAnimationIndex:app.animationIndex;
            const bool active=app.actionActive&&app.activeAction==scene::ActionRole::Fire&&index<app.scene.animations.size();
            std::cout<<"fire active="<<active<<" clip="<<(index<app.scene.animations.size()?app.scene.animations[index].sourceName:"<none>")<<'\n';
            stopGameplayAction(app);app.gameplayAction=scene::ActionRole::None;return active;
        };
        check(fire(),"Live standalone fire probe is not meaningful");
        app.recordedTake.boneCount=app.scene.skeleton.bones.size();captureTakeActorManifest(app);
        for(int i=0;i<2;++i){take::Sample sample;sample.time=i*.1f;sample.pose=app.scene.samplePose(app.animationIndex,0);app.recordedTake.samples.push_back(std::move(sample));}
        const auto path=output/"own-standalone-take.c_dm";
        check(take::save(app.recordedTake,path,error),error.c_str());
        toggleTakePlayback(app);check(app.takePreview,"Immediate replay did not start");returnToLiveGameplay(app);
        const bool immediateClips=clips()==liveClips,immediateFire=fire();
        std::cout<<"immediate: clips="<<app.scene.animations.size()<<" same_clips="<<immediateClips<<" slot="<<app.activeClassSlot<<" backup="<<app.liveClassBeforeTake.has_value()<<std::endl;
        check(immediateClips&&immediateFire,"Immediate baseline already failed");
        // An immediate replay still owns the original animated standalone rig.
        toggleTakePlayback(app);check(app.takePreview,"Second immediate replay did not start");
        check(openRecordedTake(app,path),app.status.c_str());
        check(app.liveStandaloneBeforeTake.has_value(),"Standalone snapshot was not retained");
        app.weaponProfile.gunPosition={7,8,9};app.weaponProfile.adsGunPosition={10,11,12};app.weaponProfile.separateAdsPosition=true;app.weaponProfile.viewmodelFovMultiplier=1.3f;app.weaponProfile.flipViewmodel=true;syncSharedGunPosition(app);
        check(openRecordedTake(app,path),app.status.c_str());
        check(app.liveStandaloneBeforeTake&&app.liveStandaloneBeforeTake->rig.scene.animations.size()==liveClips.size(),"Replay-to-replay opening replaced original standalone snapshot");
        std::cout<<"saved preview: clips="<<app.scene.animations.size()<<" slot="<<app.activeClassSlot<<" backup="<<app.liveClassBeforeTake.has_value()<<std::endl;
        app.nextFireTime=100000;app.burstShotsRemaining=3;app.gameplayRechamber=true;app.pendingWeaponRechamber=true;app.viewmodelSprintEntering=true;app.viewmodelSprintExiting=true;
        returnToLiveGameplay(app);
        check(app.nextFireTime==0&&app.burstShotsRemaining==0&&!app.gameplayRechamber&&!app.pendingWeaponRechamber&&!app.viewmodelSprintEntering&&!app.viewmodelSprintExiting,"Standalone return retained replay action timing");
        const bool savedClips=clips()==liveClips,savedSources=app.animationSources==liveSources,savedFire=fire();
        const bool sameModel=app.loadedBaseModelPath==liveBase&&app.loadedRigModelPaths==liveModels&&app.selectedWeaponAsset==liveWeapon;
        std::cout<<"saved return: clips="<<app.scene.animations.size()<<" same_clips="<<savedClips<<" same_sources="<<savedSources<<" same_models="<<sameModel<<" slot="<<app.activeClassSlot<<" backup="<<app.liveClassBeforeTake.has_value()<<std::endl;
        const bool documents=app.document&&app.document->sourceName()==liveDocument&&app.animationDocuments.size()==liveDocumentCount&&app.viewmodelAttachmentTokens==liveTokens&&app.selectedBaseAsset==liveBaseAsset;
        const bool edits=app.weaponProfile.gunPosition.x==7&&app.weaponProfile.adsGunPosition.y==11&&app.weaponProfile.separateAdsPosition&&app.weaponProfile.viewmodelFovMultiplier==1.3f&&app.weaponProfile.flipViewmodel;
        const bool passed=savedClips&&savedSources&&savedFire&&sameModel&&app.activeClassSlot==-1&&documents&&edits&&!app.liveStandaloneBeforeTake;
        check(passed,"Standalone saved return lost rig, source documents, or shared presentation edits");
        // Explicit new equipment must retire the old saved-live snapshot.
        check(openRecordedTake(app,path),app.status.c_str());check(app.liveStandaloneBeforeTake.has_value(),"Second lifecycle did not capture standalone rig");
        app.selectedBaseAsset=liveBaseAsset;equipViewWeapon(app,liveWeapon);
        check(!app.liveStandaloneBeforeTake,"Explicit equipment kept a stale standalone snapshot");
        returnToLiveGameplay(app);check(fire(),"Explicit equipment became unplayable after Live");
        owner.reset();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
        if(!passed){std::cerr<<"Standalone saved replay did not restore its original playable rig\n";return 1;}
        std::cout<<"Standalone immediate and saved return parity passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
