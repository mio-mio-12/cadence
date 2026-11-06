// Drives the production input decision and timer paths without OS key injection.
int iw4SwapRegression(){
    int failures=0;
    const auto check=[&](bool ok,const char* label){if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}};
    for(float drop:{0.f,.001f,.15f})for(float raise:{0.f,.001f,.25f})for(bool third:{false,true}){
        auto state=std::make_unique<AppState>();auto& app=*state;
        app.gameplayLogic=true;app.weaponSwitchAlgorithm=5;app.classGpuResident=true;
        app.experimentalThirdWeapon=third;app.assetFirstPerson=true;
        for(int slot=0;slot<2;++slot){
            assets::Asset asset;asset.game=slot==0?"cs16":"mw";asset.name=slot==0?"ak47":"desert_eagle";
            asset.role=assets::Role::ViewWeapon;app.assetCatalog.entries.push_back(asset);
            CachedClassRig rig;rig.weaponAsset=slot;rig.scene.skeleton.bones.resize(1);
            rig.scene.skeleton.bones[0].name="root";
            rig.profile.name=asset.name;
            rig.profile.stats.dropTime=rig.profile.stats.quickDropTime=drop;
            rig.profile.stats.raiseTime=rig.profile.stats.quickRaiseTime=raise;
            rig.profile.stats.iw4YySmoothingTime=.391f;
            for(const auto* role:{"idle","pullout","putaway"}){
                scene::Animation clip;clip.sourceName=std::string(role)+".cast";
                clip.domain=scene::AnimationDomain::ViewModel;clip.durationFrames=30;clip.framerate=30;
                clip.tracks.resize(1);clip.tracks[0].frames={0};clip.tracks[0].scalarValues={0};
                rig.scene.animations.push_back(clip);rig.profile.animations[role]=clip.sourceName;
            }
            rig.idleAnimation=0;app.classSlotRigs[slot]=std::move(rig);
        }
        activateClassSlot(app,0);app.lastClassSlot=1;
        const auto tick=[&](bool swap){
            const bool wheelSwap=false;int thirdRequest=-1;bool wantsShot=false;
            if(app.experimentalThirdWeapon&&swap&&!app.previousSwap)
                thirdRequest=app.weaponSwitchStage==1?app.activeClassSlot:app.lastClassSlot;
            const auto trigger=[&](scene::ActionRole action){stopGameplayAction(app);app.gameplayAction=action;triggerGameplayAction(app);};
#include "Iw4Switch.inc"
            app.previousSwap=swap;app.gameplayClock+=.01;updateGameplay(app,.01f);
        };
        tick(true);for(int frame=0;frame<100;++frame)tick(true);
        check(app.activeClassSlot==1&&app.weaponSwitchStage==0,"one held swap returned to outgoing slot");
        tick(false);tick(true);for(int frame=0;frame<100;++frame)tick(false);
        check(app.activeClassSlot==0&&app.weaponSwitchStage==0,"fresh swap failed to return");
    }
    std::cout<<"IW4 swap regression failures: "<<failures<<'\n';return failures?1:0;
}

int swapAssetRegression(const std::filesystem::path& root,const std::filesystem::path& out,std::string_view mode={}){
    const bool nativeHands=mode=="native",knives=mode=="knives";
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(320,240,"Swap asset audit",nullptr,nullptr);if(!window)return 2;
    glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;app.deferSceneUpload=true;
    std::string error;for(const auto* game:{"mwr","cs1.6","pointblank","gitsfa"})
        if(!assets::appendScan(root/game,game,app.assetCatalog,error)){std::cerr<<error;return 3;}
    if((knives||mode=="flip-key")&&!assets::appendScan(root/"cs2","cs2",app.assetCatalog,error)){std::cerr<<error;return 3;}
    if(mode=="flip-key"&&!assets::appendScan(root/"mw","mw",app.assetCatalog,error)){std::cerr<<error;return 3;}
    app.defaultSalukiDirectory=root.parent_path().string();if(!app.renderer.initialize(error))return 4;
    size_t hands=SIZE_MAX;std::array<size_t,3> weapons{SIZE_MAX,SIZE_MAX,SIZE_MAX};
    for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];
        if(a.name=="viewhands_h1_usmc_desert_LOD0")hands=i;
        if(a.name=="viewmodel_rifle_v_ak47")weapons[0]=i;
        if(a.name=="viewmodel_pistol_DesertEagle")weapons[1]=i;
        if(a.name.find(mode=="m4"?"weapon_AR_M4A1_1st_M4A1_Body":"weapon_SR_M200_1st_M200_Body")==0)weapons[2]=i;
    }
    if(knives){weapons.fill(SIZE_MAX);for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];
        if(a.game!="cs2")continue;
        for(int slot=0;slot<3;++slot)if(a.name==std::array<std::string,3>{"knife_css_model","knife_karambit_model","knife_flip_model"}[slot])weapons[slot]=i;
    }}
    if(mode=="flip-key")weapons[0]=weapons[1]=SIZE_MAX;
    if(mode=="flip-key")for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];
        if(a.game=="cs2"&&a.name=="knife_flip_model")weapons[0]=i;
        if(a.game=="mw"&&a.name.starts_with("viewmodel_remington700_mp"))weapons[1]=i;
    }
    if(hands==SIZE_MAX||std::find(weapons.begin(),weapons.end(),SIZE_MAX)!=weapons.end()){std::cerr<<"Missing audit assets";return 5;}
    for(int slot=0;slot<3;++slot){
        app.classViewhandsOverride=nativeHands?SIZE_MAX:hands;
        app.selectedBaseAsset=nativeHands?classHandsForWeapon(app,app.assetCatalog.entries[weapons[slot]]):hands;
        app.selectedWeaponAsset=weapons[slot];app.assetFirstPerson=true;
        equipViewWeapon(app,weapons[slot]);app.actionActive=false;app.gameplayLogic=true;resolveGameplayAnimation(app);
        CachedClassRig rig;rig.scene=std::move(app.scene);rig.animationPrefix=app.viewmodelAnimationPrefix;rig.rigAssets=app.equippedRigAssets;
        rig.baseModelPath=app.loadedBaseModelPath;rig.rigModelPaths=app.loadedRigModelPaths;rig.attachmentPaths=app.loadedAttachmentPaths;
        rig.weaponAsset=weapons[slot];rig.idleAnimation=app.animationIndex;rig.adsDownAnimation=app.viewmodelAdsBaseAnimation;
        rig.profile=app.weaponProfile;rig.profilePath=app.weaponProfilePath;app.classSlotRigs[slot]=std::move(rig);
        if(mode=="zero-key"&&slot==0){auto& stats=app.classSlotRigs[slot]->profile.stats;stats.raiseTime=stats.quickRaiseTime=stats.dropTime=stats.quickDropTime=0;}
    }
    app.activeClassSlot=-1;app.classGpuResident=true;app.actorMode=true;app.weaponSwitchAlgorithm=5;app.experimentalThirdWeapon=true;
    app.cameraPitch=app.actorYaw=0;app.actorRenderPosition={};app.actorPresentationZValid=false;
    activateClassSlot(app,0);std::filesystem::create_directories(out);std::ofstream log(out/"swap.csv");
    log<<"target,frame,held,stage,clip,camera_x,camera_y,camera_z,forward_x,forward_y,forward_z,action,animation_frame\n";
    int failures=0;
    const int frames=knives?720:180;
    const bool keyMode=mode=="zero-key"||mode=="flip-key";
    const auto sequence=keyMode?std::array<int,6>{1,0,1,0,1,0}:std::array<int,6>{1,2,1,0,2,0};
    for(int target:sequence)for(int frame=0;frame<frames;++frame){
        int thirdRequest=keyMode?-1:frame==0?target:-1;const bool swap=keyMode&&frame<12,wheelSwap=false,sprint=false;bool wantsShot=false;
        if(keyMode&&swap&&!app.previousSwap)thirdRequest=app.weaponSwitchStage==1?app.activeClassSlot:app.lastClassSlot;
        const auto trigger=[&](scene::ActionRole action){stopGameplayAction(app);app.gameplayAction=action;triggerGameplayAction(app);};
#include "Iw4Switch.inc"
#include "LegacyWeaponSwitch.inc"
        app.previousSwap=swap;
        if(knives&&frame==0){activateClassSlot(app,target);app.weaponSwitchStage=2;app.weaponSwitchElapsed=0;
            app.weaponSwitchQuickAnimation=app.weaponSwitchFirstRaiseAnimation=false;
            app.gameplayAction=scene::ActionRole::Equip;triggerGameplayAction(app);}
        app.gameplayClock+=1./120;updateGameplay(app,1.f/120);advanceLiveAnimationPlayback(app,1.f/120);
        const auto pose=evaluateCurrentPose(app);const auto& c=app.actorViewCamera;
        if(mode=="m4"&&app.activeClassSlot==2&&(std::abs(c.v[12])>.01f||std::abs(c.v[13])>.01f||std::abs(c.v[14]-app.actorViewHeight)>.01f))++failures;
        if(knives&&frame==0){if(!app.renderer.loadScene(app.scene,error)){std::cerr<<error;return 6;}
            app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});}
        if(knives&&(frame==0||frame==30||frame==60||frame==90||frame==120||frame==240||frame==600)){
            const scene::Vec3 eye{c.v[12],c.v[13],c.v[14]},forward{c.v[0],c.v[1],c.v[2]},up{c.v[8],c.v[9],c.v[10]};
            app.renderer.setCameraPosition(eye);
            const auto vp=scene::perspective(75*scene::kPi/180,960.f/540,.02f,5000)*scene::lookAtDirection(eye,forward,up);
            app.renderer.render(app.scene,pose,vp,960,540,false,false,false);
            if(!app.renderer.saveColorPng(out/("knife_"+std::to_string(target)+"_"+std::to_string(frame)+".png"),error)){std::cerr<<error;return 7;}
        }
        for(float value:c.v)if(!std::isfinite(value))++failures;
        log<<target<<','<<frame<<','<<app.activeClassSlot<<','<<app.weaponSwitchStage<<','
           <<(app.animationIndex<app.scene.animations.size()?app.scene.animations[app.animationIndex].sourceName:"none")<<','
           <<c.v[12]<<','<<c.v[13]<<','<<c.v[14]<<','<<c.v[0]<<','<<c.v[1]<<','<<c.v[2]<<','<<int(app.activeAction)<<','<<app.animationFrame<<'\n';
        if(frame==frames-1&&app.activeClassSlot!=target)++failures;
    }
    app.renderer.shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"Real asset swap failures: "<<failures<<'\n';return failures?1:0;
}
