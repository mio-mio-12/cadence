    if(!app.scene.pointBlankWeaponStem.empty()||app.scene.dualWield){
        std::vector<scene::PoseSlot> slots;
        if(hasBaseTransition)slots.push_back({{{app.animationIndex,app.animationFrame,classicBlend,scene::LayerMode::Override,false}}});
        scene::PoseSlot actions;
        for(const auto& layer:app.runtimeLayers){
            // PB dual shots begin at their authored kick peak. Blend only the
            // firing side in over one source frame, keeping the other side live.
            const bool dualShot=app.scene.dualWield&&!app.scene.pointBlankWeaponStem.empty()&&layer.action==scene::ActionRole::Fire;
            const float entry=dualShot?std::clamp(layer.elapsed*app.scene.animations[layer.animation].framerate,0.f,1.f):1.f;
            actions.nodes.push_back({layer.animation,layer.frame,layer.weight*entry*entry*(3-2*entry),layer.mode,false});
        }
        if(app.actionActive&&app.actionOverlay&&app.actionAnimationIndex<app.scene.animations.size()){
            const auto& clip=app.scene.animations[app.actionAnimationIndex];const float duration=app.actionDurationOverride>0?app.actionDurationOverride:clip.durationFrames/std::max(1.f,clip.framerate);
            const float fade=std::max(.001f,std::min(app.activeAction==scene::ActionRole::Reload?.16f:app.activeAction==scene::ActionRole::Melee?.12f:.08f,duration*.2f));
            // Rechamber entry already crossfades from the captured fire pose.
            // A second idle-to-bolt ramp bends that path back through idle.
            const float enter=app.activeAction==scene::ActionRole::Fire||app.gameplayRechamber||app.activeAction==scene::ActionRole::Equip||app.activeAction==scene::ActionRole::FirstRaise?1.f:std::clamp(app.actionElapsed/fade,0.f,1.f);
            const float leave=actionRequiresHold(app.activeAction)?1.f:std::clamp((duration-app.actionElapsed)/fade,0.f,1.f);
            const float t=std::min(enter,leave);actions.nodes.push_back({app.actionAnimationIndex,app.actionFrame,t*t*(3-2*t),scene::LayerMode::Override,false});
        }
        if(!actions.nodes.empty())slots.push_back(std::move(actions));
        if(!(app.actionActive&&(app.activeAction==scene::ActionRole::Equip||app.activeAction==scene::ActionRole::FirstRaise))&&app.jumpFeedbackAnimation<app.scene.animations.size())if(auto idle=findViewmodelClip(app,"idle")){
            const auto& clip=app.scene.animations[app.jumpFeedbackAnimation];const float duration=clip.durationFrames/std::max(1.f,clip.framerate),fade=std::min(.09f,duration*.25f);
            if(duration>0&&app.jumpFeedbackElapsed<duration){const float w=std::clamp(std::min(app.jumpFeedbackElapsed,duration-app.jumpFeedbackElapsed)/std::max(.001f,fade),0.f,1.f);
                scene::PoseLayer layer{app.jumpFeedbackAnimation,app.jumpFeedbackElapsed*clip.framerate,w*w*(3-2*w),scene::LayerMode::Additive,false};layer.referenceAnimation=*idle;layer.referenceFrame=0;slots.push_back({{layer}});
            }
        }
        pose=app.scene.samplePoseSlots(layeredBaseAnimation,layeredBaseFrame,slots);
    }else
