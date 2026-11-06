    if(!iw4Yy){
    // A direct slot command and Q can represent the same physical press.
    // Consuming both would immediately put away the just-selected weapon.
    const bool directRequest=thirdRequest>=0;
    const auto swapTarget=[&]{return app.experimentalThirdWeapon&&app.lastClassSlot>=0&&app.lastClassSlot<3&&app.lastClassSlot!=app.activeClassSlot&&app.classSlotRigs[app.lastClassSlot]?app.lastClassSlot:(app.activeClassSlot==0?1:0);};
    const auto immediateSwap=[&](int target){
        const int outgoing=app.activeClassSlot;app.yyReturning=app.yyTowardPutaway=app.yyReverse=false;app.yyCancelDuration=0;
        app.weaponSwitchTarget=target;activateClassSlot(app,target);app.weaponSwitchStage=2;app.weaponSwitchElapsed=0;app.weaponSwitchFirstRaiseAnimation=false;
        app.weaponSwitchQuickAnimation=app.weaponSwitchAlgorithm==2&&useQuickSwapAnimation(app,target,outgoing);
        app.gameplayAction=app.weaponSwitchAlgorithm==3?scene::ActionRole::FirstRaise:scene::ActionRole::Equip;triggerGameplayAction(app);
        app.weaponSwitchFirstRaiseAnimation=app.weaponSwitchAlgorithm==3&&app.actionActive;
        if(!app.actionActive){app.gameplayAction=scene::ActionRole::Equip;triggerGameplayAction(app);}
        if(!app.actionActive)app.weaponSwitchStage=0;
    };
    if(thirdRequest>=0&&thirdRequest<3&&thirdRequest!=app.activeClassSlot&&app.classSlotRigs[thirdRequest]&&(app.weaponSwitchAlgorithm==2||app.weaponSwitchAlgorithm==3)){
        immediateSwap(thirdRequest);thirdRequest=-1;wantsShot=false;
    }
    if(thirdRequest==app.activeClassSlot&&app.weaponSwitchStage==1){
        if(app.weaponSwitchAlgorithm==4)cancelYyV4Drop(app);
        else {app.weaponSwitchStage=2;app.weaponSwitchTarget=app.activeClassSlot;app.gameplayAction=scene::ActionRole::Equip;triggerGameplayAction(app);
        if(!app.actionActive)app.weaponSwitchStage=0;}
        wantsShot=false;
    }
    if(thirdRequest>=0&&thirdRequest<3&&thirdRequest!=app.activeClassSlot&&app.classSlotRigs[thirdRequest]){
        wantsShot=false;
        app.yyReturning=app.yyTowardPutaway=app.yyReverse=false;app.yyCancelDuration=0;app.queuedWeaponSwap=false;
        app.weaponSwitchTarget=thirdRequest;app.weaponSwitchQuickAnimation=useQuickSwapAnimation(app,app.activeClassSlot,thirdRequest);
        app.weaponSwitchStage=1;app.weaponSwitchElapsed=0;trigger(scene::ActionRole::Unequip);
        if(!app.actionActive){activateClassSlot(app,thirdRequest);app.weaponSwitchStage=2;app.gameplayAction=scene::ActionRole::Equip;triggerGameplayAction(app);if(!app.actionActive)app.weaponSwitchStage=0;}
    }
    const bool swapPressed=gameplay::practice::swapPressed(swap,app.previousSwap,wheelSwap,app.experimentalThirdWeapon,directRequest);
    if(swapPressed&&(app.weaponSwitchAlgorithm==2||app.weaponSwitchAlgorithm==3)&&!sprint&&app.classSlotRigs[swapTarget()]){immediateSwap(swapTarget());wantsShot=false;}
    else if(swapPressed&&app.weaponSwitchAlgorithm==4&&app.weaponSwitchStage==1){
        cancelYyV4Drop(app);wantsShot=false;
    }
    else if(swapPressed&&app.weaponSwitchStage!=0){
        if(app.weaponSwitchStage==2&&app.yyReverse){
            // A fresh swap never inherits the canceled attempt's phase. Blend
            // from the currently visible reverse pose into putaway frame zero.
            const bool smoothResume=app.weaponSwitchAlgorithm==6;
            const float resumeFrame=app.animationFrame,resumeVelocity=app.yyV5Velocity;
            const auto resumeClip=app.animationIndex;
            if(!smoothResume)beginInterruptPoseBlend(app,std::max(0.001f,app.actionBlendTime));
            app.weaponSwitchStage=0;app.yyReverse=false;app.yyReturning=false;app.yyTowardPutaway=false;app.yyCancelDuration=0;
            app.weaponSwitchTarget=swapTarget();app.weaponSwitchQuickAnimation=useQuickSwapAnimation(app,app.activeClassSlot,app.weaponSwitchTarget);app.weaponSwitchStage=1;app.weaponSwitchElapsed=0;trigger(scene::ActionRole::Unequip);app.animationFrame=0;app.transitioning=false;app.gameplayStatus="YY — fresh putaway blended from the current pose";
            if(smoothResume&&app.animationIndex==resumeClip&&app.animationIndex<app.scene.animations.size()){
                app.animationFrame=resumeFrame;app.yyV5Elapsed=0;app.yyV5Resuming=true;
                const auto& clip=app.scene.animations[app.animationIndex];
                const float duration=app.actionDurationOverride>0?app.actionDurationOverride:clip.durationFrames/std::max(1.f,clip.framerate);
                app.yyV5Resume.begin(resumeFrame,resumeVelocity,clip.durationFrames/std::max(.001f,duration));
                app.interruptPoseDuration=0;app.gameplayStatus="YY v5";
            }
        } else if(app.weaponSwitchStage==1){

            if(app.weaponSwitchAlgorithm==6&&app.animationIndex<app.scene.animations.size()){
                const auto& clip=app.scene.animations[app.animationIndex];
                const float duration=app.actionDurationOverride>0?app.actionDurationOverride:clip.durationFrames/std::max(1.f,clip.framerate);
                const float velocity=app.yyV5Resuming?app.yyV5Velocity:(app.animationFrame<clip.durationFrames?clip.durationFrames/std::max(.001f,duration):0.f);
                app.yyV5Return.begin(app.animationFrame,velocity,clip.durationFrames,yyReturnDuration(app));
                app.yyV5Elapsed=0;app.yyV5Resuming=false;app.yyV5Velocity=velocity;
            }
            app.yyPutawayAnimation=app.animationIndex;app.yyPutawayFrameLimit=app.animationFrame;app.queuedWeaponSwap=false;app.weaponSwitchTarget=app.activeClassSlot;app.weaponSwitchStage=2;app.weaponSwitchElapsed=0;app.yyCancelDuration=yyReturnDuration(app);app.nextFireTime=std::min(app.nextFireTime,app.gameplayClock);if(app.weaponSwitchAlgorithm==0)beginInterruptPoseBlend(app,app.yyCancelDuration);app.actionActive=false;app.actionOverlay=false;app.activeAction=scene::ActionRole::None;
            if(app.weaponSwitchAlgorithm==0){app.yyReturning=true;app.yyTowardPutaway=false;app.yyReverse=false;if(const auto idle=findViewmodelClip(app,"idle")){selectAnimation(app,*idle,true);app.activeTransitionDuration=app.yyCancelDuration;app.transitionElapsed=0;}else{app.weaponSwitchStage=0;resolveGameplayAnimation(app);}app.gameplayStatus="YY v1 cancel — interpolating current putaway pose back to idle";}
            else{app.yyReturning=false;app.yyTowardPutaway=false;app.yyReverse=true;app.transitioning=false;app.transitionElapsed=0;app.gameplayStatus="YY v2 cancel — returning putaway toward frame zero";}
        } else if(app.classSlotRigs[0]&&app.classSlotRigs[1]){
            app.yyReturning=false;app.yyTowardPutaway=false;app.yyReverse=false;app.yyCancelDuration=0;app.queuedWeaponSwap=false;
            app.weaponSwitchTarget=swapTarget();app.weaponSwitchQuickAnimation=useQuickSwapAnimation(app,app.activeClassSlot,app.weaponSwitchTarget);
            app.weaponSwitchStage=1;app.weaponSwitchElapsed=0;trigger(scene::ActionRole::Unequip);app.gameplayStatus="Pullout interrupted — putting the current weapon away";
        }
    }
    else if(swapPressed&&app.weaponSwitchStage==0&&!sprint&&app.classSlotRigs[swapTarget()]){const bool canceledReload=app.actionActive&&app.activeAction==scene::ActionRole::Reload;if(canceledReload)app.nextFireTime=std::min(app.nextFireTime,app.gameplayClock);app.yyReturning=false;app.yyTowardPutaway=false;app.yyPutawayAnimation=static_cast<std::size_t>(-1);app.yyPutawayFrameLimit=0;app.yyReverse=false;app.yyCancelDuration=0;app.queuedWeaponSwap=false;app.weaponSwitchTarget=swapTarget();app.weaponSwitchQuickAnimation=useQuickSwapAnimation(app,app.activeClassSlot,app.weaponSwitchTarget);app.weaponSwitchStage=1;app.weaponSwitchElapsed=0;trigger(scene::ActionRole::Unequip);app.gameplayStatus="Putting away for weapon swap";}
    }
