#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main

int main(int argc,char**argv){
    if(argc!=3||!glfwInit())return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* w=glfwCreateWindow(640,480,"Putaway/world audit",nullptr,nullptr);if(!w)return 2;
    glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;app.defaultSalukiDirectory=argv[1];app.deferSceneUpload=true;
    const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");std::string error;
    if(!app.renderer.initialize(error))return 3;
    for(const auto* game:{"mw","bo2"})if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))return 4;
    int failures=0;
    const auto check=[&](bool ok,const std::string& text){report<<(ok?"PASS ":"FAIL ")<<text<<std::endl;if(!ok)++failures;};
    const auto index=[&](const std::string& name){for(size_t i=0;i<app.assetCatalog.entries.size();++i)if(app.assetCatalog.entries[i].name==name)return i;return SIZE_MAX;};
    const auto difference=[](const auto& a,const auto& b){if(a.size()!=b.size())return 1e9f;float e=0;for(size_t i=0;i<a.size();++i)for(int j=0;j<16;++j)e=std::max(e,std::abs(a[i].v[j]-b[i].v[j]));return e;};
    for(const auto* name:{"viewmodel_ak47_mp_LOD0","t6_wpn_ar_an94_view_LOD0"}){
        const auto weapon=index(name);if(weapon==SIZE_MAX)return 5;const auto game=app.assetCatalog.entries[weapon].game;
        app.selectedBaseAsset=index(game=="mw"?"viewmodel_base_viewhands_LOD0":"c_usa_mp_isa_smg_viewhands_LOD0");
        equipViewWeapon(app,weapon);app.actorMode=false;app.gameplayLogic=true;app.playing=true;
        const auto idle=findViewmodelClip(app,"idle"),putaway=findViewmodelClip(app,"putaway");if(!idle||!putaway)return 6;
        for(const auto* source:{"idle","fire","reload","ads_up","sprint_loop"}){
            const auto clip=findViewmodelClip(app,source);if(!clip){report<<"SKIP "<<name<<" "<<source<<'\n';continue;}
            app.runtimeLayers.clear();app.actionActive=app.actionOverlay=app.transitioning=false;app.interruptPoseDuration=0;
            app.gameplayAds=app.viewmodelAdsEngaged=app.viewmodelAdsExiting=false;app.viewmodelAdsBaseAnimation=SIZE_MAX;
            app.animationIndex=*idle;app.animationFrame=0;app.jumpFeedbackAnimation=SIZE_MAX;app.weaponSwitchStage=0;
            if(std::string(source)=="fire"){
                app.actionActive=app.actionOverlay=true;app.activeAction=scene::ActionRole::Fire;app.actionAnimationIndex=*clip;
                app.actionFrame=app.scene.animations[*clip].durationFrames*.25f;app.actionElapsed=.02f;app.actionDurationOverride=authoredClipDuration(app,*clip);
            }else if(std::string(source)=="ads_up"){
                startViewmodelAimClip(app,*clip);app.interruptPoseDuration=0;app.transitioning=false;
                app.viewmodelAdsBaseFrame=app.scene.animations[*clip].durationFrames*.7f;app.viewmodelAdsTransitionElapsed=app.weaponTiming.adsIn*.7f;
            }else{
                app.animationIndex=*clip;app.animationFrame=app.scene.animations[*clip].durationFrames*.4f;
                if(std::string(source)=="reload"){app.actionActive=true;app.activeAction=scene::ActionRole::Reload;app.actionElapsed=.4f;}
            }
            const auto before=evaluateCurrentPose(app);
            app.gameplayAction=scene::ActionRole::Unequip;triggerGameplayAction(app);
            const auto after=evaluateCurrentPose(app);
            // Reload exit deliberately parks discarded T6 magazine dummies and
            // seats the installed magazine. Require all remaining pose channels
            // to be continuous, and require exactly that established repair.
            auto expected=before;
            if(std::string(source)=="reload")scene::restoreT6MagazineAfterReload(app.scene,expected,*putaway,app.animationFrame);
            const float entryError=difference(expected,after);
            if(difference(before,expected)>.01f)report<<"NOTE "<<name<<" preserved established reload-exit magazine repair\n";
            check(app.actionActive&&app.activeAction==scene::ActionRole::Unequip&&app.animationIndex==*putaway,std::string(name)+" "+source+" putaway selected");
            check(entryError<.01f,std::string(name)+" "+source+" entry error="+std::to_string(entryError));
            check(app.actionDurationOverride==app.weaponTiming.dropTime,std::string(name)+" drop timing unchanged");
            const auto incomingDuration=authoredClipDuration(app,*putaway);
            for(int frame=0;frame<=60;++frame){app.actionElapsed=incomingDuration*frame/60.f;app.animationFrame=app.scene.animations[*putaway].durationFrames*frame/60.f;app.interruptPoseElapsed=app.actionElapsed;const auto p=evaluateCurrentPose(app);for(const auto&m:p)for(float v:m.v)if(!std::isfinite(v))++failures;}
            const auto end=evaluateCurrentPose(app);app.interruptPoseDuration=0;check(difference(end,evaluateCurrentPose(app))<.001f,std::string(name)+" "+source+" blend exits to exact putaway pose");
        }
    }
    // Verification only: exercise the actual matcher/attachment path without
    // substituting a world model to make an unsuccessful match look correct.
    for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& asset=app.assetCatalog.entries[i];if(asset.game!="mw"||asset.role!=assets::Role::ViewWeapon)continue;
        const auto world=findWorldWeaponForViewWeapon(app,asset);
        report<<"WORLD "<<asset.name<<" -> "<<(world<app.assetCatalog.entries.size()?app.assetCatalog.entries[world].name:"UNMATCHED")<<'\n';
        if(world>=app.assetCatalog.entries.size())continue;
        app.autoPlayerModel=false;app.manualPlayerModelAsset=findGenericPlayermodelForGame(app,"mw");app.classPrimaryAsset=app.selectedWeaponAsset=i;app.playerWorldAnimationGame="mw";app.activeClassSlot=0;app.weaponProfile=generatedWeaponProfile(app,asset);app.gameplayWeapon=gameplayWeaponClass(app.weaponProfile,asset);configureClassActor(app);
        check(app.hiddenWorldActor&&!app.hiddenWorldActor->attachments.empty(),asset.name+" world attached");if(!app.hiddenWorldActor)continue;
        app.actorMode=true;app.actorGrounded=true;app.actorPosition=app.actorRenderPosition={};app.actorVelocity=app.actorWishVelocity={};app.actorMoveInputForward=app.actorMoveInputSide=0;app.actionActive=false;app.gameplayAds=false;app.gameplayStance=scene::Stance::Stand;
        app.actorCurrLocoClip=app.actorPrevLocoClip=SIZE_MAX;app.actorWorldFacingOffset=0;app.actorYaw=app.cameraPitch=0;
        const auto pose=evaluateHiddenWorldActorPose(app);auto& actor=*app.hiddenWorldActor;
        if(!app.renderer.loadScene(actor,error))return 7;app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.12f,.13f,.15f,1});
        const auto socket=actor.attachments.front().boneIndex;const auto center=scene::transformPoint(pose[socket],{});
        const auto vp=scene::perspective(45*scene::kPi/180,4.f/3,1,4000)*scene::lookAt(center+scene::Vec3{85,-100,35},center,{0,0,1});
        app.renderer.render(actor,pose,vp,800,600,false,false,false);if(!app.renderer.saveColorPng(out/(asset.name+"_world.png"),error))return 8;
    }
    report<<"MW2 available="<<std::filesystem::is_directory(app.defaultSalukiDirectory/"mw2")<<'\n';
    report<<"failures="<<failures<<'\n';return failures?9:0;
}
