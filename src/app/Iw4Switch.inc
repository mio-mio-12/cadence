const bool iw4Yy=app.weaponSwitchAlgorithm==5&&!activeWeaponIsCs2(app);
if(iw4Yy){
    using namespace gameplay::iw4switch;
    const bool firing=app.iw4FireCycle.pending(app.gameplayClock,app.activeClassSlot,app.selectedWeaponAsset,!app.scene.dualWield&&!app.takePreview);
    Busy busy=firing?Busy::Fire:Busy::Ready;
    if(app.actionActive){
        // Dual-hand combat state remains on its existing path; one shared
        // timer must not incorrectly lock out an unrelated hand.
        if(app.scene.dualWield&&app.activeAction==scene::ActionRole::Fire)busy=Busy::Fire;
        if(app.activeAction==scene::ActionRole::Reload)busy=app.gameplayRechamber?Busy::Rechamber:Busy::Reload;
        if(app.activeAction==scene::ActionRole::Melee)busy=Busy::Melee;
        if(app.activeAction==scene::ActionRole::GrenadePrep||app.activeAction==scene::ActionRole::Throw||app.activeAction==scene::ActionRole::Deploy)busy=Busy::Offhand;
    }
    int target=thirdRequest;
    if(!app.experimentalThirdWeapon&&((swap&&!app.previousSwap)||wheelSwap))
        target=app.weaponSwitchStage==1||app.iw4PendingSlot>=0?app.activeClassSlot:(app.activeClassSlot==0?1:0);
    if(app.experimentalThirdWeapon&&swap&&!app.previousSwap&&app.weaponSwitchStage==1)target=app.activeClassSlot;
    if(target>=0)app.iw4PendingSlot=target;
    else target=app.iw4PendingSlot;
    const bool allowed=admits(busy,busy==Busy::Fire&&(firing||(app.scene.dualWield&&app.gameplayClock<app.nextFireTime)));
    if(target>=0&&target<3&&app.classSlotRigs[target]){
        const auto decision=request(app.activeClassSlot,target,app.weaponSwitchStage,allowed);
        if(decision==Action::None&&allowed&&target==app.activeClassSlot)app.iw4PendingSlot=-1;
        if(decision!=Action::None){
            app.iw4PendingSlot=-1;
            wantsShot=false;app.queuedWeaponSwap=false;
            if(decision==Action::RetargetDrop)app.weaponSwitchTarget=target;
            else {
                // The animation keeps advancing during the outgoing crossfade.
                beginYyV4LiveBlend(app,std::max(.001f,app.actionBlendTime),false);
                app.yyReturning=app.yyTowardPutaway=app.yyReverse=false;app.yyCancelDuration=0;
                app.yyPutawayAnimation=SIZE_MAX;app.yyPutawayFrameLimit=0;
                app.pendingWeaponRechamber=false;app.gameplayRechamber=false;app.burstShotsRemaining=0;
                app.weaponSwitchTarget=target;app.weaponSwitchElapsed=0;app.weaponSwitchFirstRaiseAnimation=false;
                if(decision==Action::CancelDrop){
                    app.weaponSwitchStage=0;app.weaponSwitchQuickAnimation=false;
                    app.actionActive=app.actionOverlay=false;app.activeAction=scene::ActionRole::None;
                    app.actionElapsed=app.actionFrame=app.actionDurationOverride=0;app.runtimeLayers.clear();
                    app.viewmodelAdsEngaged=app.viewmodelAdsExiting=app.viewmodelAdsPoseHold=false;app.gameplayAds=false;
                    app.viewmodelAdsBaseAnimation=SIZE_MAX;app.nextFireTime=std::min(app.nextFireTime,app.gameplayClock);
                    resolveGameplayAnimation(app);app.transitioning=false;
                    app.gameplayStatus="IW4 YY";
                }else{
                    app.weaponSwitchQuickAnimation=useQuickSwapAnimation(app,app.activeClassSlot,target);
                    app.weaponSwitchStage=1;trigger(scene::ActionRole::Unequip);
                }
            }
        }
    }else if(target>=0)app.iw4PendingSlot=-1;
}else {app.iw4PendingSlot=-1;app.iw4FireCycle.clear();}
