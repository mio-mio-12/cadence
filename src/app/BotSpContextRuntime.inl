// Included after bot helpers and AppState. Contexts never create vehicle,
// grenade-physics, paired-scene, or unvalidated traversal actions.
AppState::SpContextState& spContextState(AppState& app,const gameplay::bot::Actor& bot){
    if(app.botSpContextStates.size()!=app.bots.size())app.botSpContextStates.resize(app.bots.size());
    return app.botSpContextStates[static_cast<std::size_t>(&bot-app.bots.data())];
}
bool spContextsEnabled(const AppState& app){return app.activeBotSystemMode==1&&app.botActorScene&&!app.botSpContextClips.empty();}
void reserveSpContextPolicy(AppState& app,const gameplay::bot::Actor& bot,gameplay::bot::BehaviorConfig& config){
    if(app.activeBotSystemMode!=1)return;
    const auto& state=spContextState(app,bot);
    // The policy runs after context acquisition. Do not create a new
    // equipment event underneath a committed cover approach or cover pose.
    if(state.coverSeek>0||state.cover>0)config.allowEquipment=false;
}
void restoreSpContextYaw(AppState& app,gameplay::bot::Actor& bot,float beforePolicyYaw,float dt){
    if(app.activeBotSystemMode!=1)return;
    const auto& state=spContextState(app,bot);
    if(state.coverSeek<=0&&state.cover<=0)return;
    // Policy still owns visibility/fire decisions, but must not apply a second
    // competing turn underneath the precise cover approach or anchored pose.
    bot.yaw=beforePolicyYaw;
    if(state.cover>0){
        const auto to=app.actorPosition-bot.position;
        const float desired=std::atan2(to.y,to.x);
        bot.yaw=gameplay::bot::wrapAngle(bot.yaw+std::clamp(gameplay::bot::wrapAngle(desired-bot.yaw)*(1-std::exp(-9.f*dt)),-8.f*dt,8.f*dt));
    }
}
cadence::sp::ContextRequest spContextRequest(const gameplay::bot::Actor& bot){
    cadence::sp::ContextRequest r;r.alive=bot.alive;r.grounded=bot.grounded;r.moving=gameplay::bot::horizontalSpeed(bot)>12;
    r.rifle=cadence::sp::supportsSpLongGun(bot.weaponClass);
    r.stance=bot.stance==scene::Stance::Crouch?cadence::sp::ContextStance::Crouch:cadence::sp::ContextStance::Stand;
    r.reloading=bot.reloadTime>0;r.mantling=bot.mantling;r.scenarioActive=bot.spScenarioWeight>.01f||bot.spMovingScenario;r.firing=bot.input.fire||bot.firePoseTime>0;
    if(bot.stance==scene::Stance::Prone)r.rifle=false;
    return r;
}
std::optional<float> spCoverHeightAt(const AppState& app,scene::Vec3 position){
    if(!app.loadedMap)return {};
    auto direction=app.actorPosition-position;direction.z=0;
    if(scene::length(direction)<90.f)return {};
    direction=scene::normalize(direction);
    const auto face=app.loadedMap->raycastSurface(position+scene::Vec3{0,0,60.f},direction,90.f);
    if(face&&std::abs(face->normal.z)>.35f)return {};
    // Thin walls and window ledges may end before a fixed-distance top probe.
    // Sample just inside the actual blocking face, not an arbitrary 65 cm out.
    const auto probe=face?face->position+direction*2.f:position+direction*65.f;
    const float top=app.loadedMap->navigationGroundHeight(probe.x,probe.y,position.z+155.f,-1e8f,face?1.f:6.f);
    const float height=top-position.z;
    if(!std::isfinite(height)||height<70.f||height>155.f)return {};
    const auto low=position+scene::Vec3{0,0,height*.65f};
    const auto high=position+scene::Vec3{0,0,height+15.f};
    const float distance=face?face->distance+4.f:70.f;
    if(app.loadedMap->lineOfSight(low,low+direction*distance)||!app.loadedMap->lineOfSight(high,high+direction*distance))return {};
    return height/gameplay::iw::kWorldUnitsPerIwUnit;
}
std::optional<float> spLocalCoverHeight(const AppState& app,const gameplay::bot::Actor& bot){
    auto direction=app.actorPosition-bot.position;direction.z=0;direction=scene::normalize(direction);
    if(scene::dot(direction,scene::Vec3{std::cos(bot.yaw),std::sin(bot.yaw),0})<.85f)return {};
    return spCoverHeightAt(app,bot.position);
}
void stopSpCoverSeek(AppState& app,gameplay::bot::Actor& bot){
    auto& state=spContextState(app,bot);
    if(state.coverSeek>0){
        bot.spWantsMove=bot.spCombatMoving=false;bot.input.forward=bot.input.right=0;
        bot.input.sprint=false;bot.spPhaseTime=0;
        bot.navigationRoute.clear();bot.repathTime=0;
        const auto index=static_cast<std::size_t>(&bot-app.bots.data());
        if(index<app.botSpRoutes.size())app.botSpRoutes[index].reset();
    }
    state.coverSeek=0;state.coverStall=0;state.cooldown=std::max(state.cooldown,1.f);
}
bool beginSpCoverSeek(AppState& app,gameplay::bot::Actor& bot){
    if(!app.loadedMap||app.botSpCoverChance<=0)return false;
    auto& state=spContextState(app,bot);
    auto request=spContextRequest(bot);request.kind=cadence::sp::ContextKind::CoverIdle;
    request.moving=false;request.firing=false;request.coverValidated=true;
    // A fixed local budget, only on the staggered variety opportunity. No
    // navigation mesh enumeration or growing-radius world search is involved.
    const float offset=gameplay::bot::personalityValue(bot.id*31337u+state.sequence)*scene::kPi;
    std::array<scene::Vec3,17> candidates;std::size_t count=0;
    // Surface-anchored candidates find the actual near face instead of
    // depending on a random ground point landing one capsule-width from it.
    for(unsigned i=0;i<8;++i){
        const float angle=offset+static_cast<float>(i)*scene::kPi*.25f;
        const scene::Vec3 direction{std::cos(angle),std::sin(angle),0};
        const auto hit=app.loadedMap->raycastSurface(bot.position+scene::Vec3{0,0,80.f},direction,260.f);
        if(!hit||std::abs(hit->normal.z)>.35f)continue;
        auto normal=scene::normalize(scene::Vec3{hit->normal.x,hit->normal.y,0});
        if(scene::dot(normal,bot.position-hit->position)<0)normal=normal*-1.f;
        if(scene::dot(normal,app.actorPosition-hit->position)>0)continue;
        auto candidate=hit->position+normal*(scene::course::kPlayerRadius+20.f);candidate.z=bot.position.z;
        if(gameplay::bot::horizontalDistance(candidate,bot.position)<=280.f)candidates[count++]=candidate;
    }
    for(unsigned i=0;i<9;++i){
        const float angle=offset+static_cast<float>(i)*scene::kPi*.5f;
        const float radius=i==0?0.f:(i<=4?130.f:260.f);
        candidates[count++]=bot.position+scene::Vec3{std::cos(angle),std::sin(angle),0}*radius;
    }
    for(std::size_t i=0;i<count;++i){
        auto candidate=candidates[i];
        candidate.z=app.loadedMap->navigationGroundHeight(candidate.x,candidate.y,bot.position.z+scene::course::kStepHeight+1,-1e8f,scene::course::kPlayerRadius*.55f);
        if(!std::isfinite(candidate.z)||std::abs(candidate.z-bot.position.z)>scene::course::kStepHeight||gameplay::bot::navigationBlocked(activeBotNavigation(app),candidate))continue;
        const auto& boundary=activeBotNavigation(app).boundary;
        if(boundary.enabled&&(candidate.z<boundary.floorZ||candidate.z>boundary.ceilingZ||(!boundary.points.empty()&&!gameplay::bot::pointInPolygon({candidate.x,candidate.y},boundary.points))))continue;
        const auto height=spCoverHeightAt(app,candidate);if(!height)continue;
        const auto constrained=app.loadedMap->constrainMove(candidate,candidate,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight/1.25f);
        if(scene::length(constrained-candidate)>1.f)continue;
        request.coverHeightIw=*height;request.stance=*height<44.f?cadence::sp::ContextStance::Crouch:cadence::sp::ContextStance::Stand;
        const auto coverClip=cadence::sp::selectContextClip(app.botSpContextClips,request,bot.id,state.sequence);
        if(!coverClip||*coverClip>=app.botActorScene->animations.size())continue;
        state.coverClip=*coverClip;
        state.coverGoal=candidate;state.coverHeightIw=*height;state.coverSeek=5.f;
        state.coverBestDistance=gameplay::bot::horizontalDistance(bot.position,candidate);
        state.coverCheck=state.coverStall=0;state.cooldown=1;
        bot.navigationRoute.clear();bot.repathTime=0;++bot.spDecisionSequence;
        const auto index=static_cast<std::size_t>(&bot-app.bots.data());
        if(index<app.botSpRoutes.size())app.botSpRoutes[index].reset();
        return true;
    }
    return false;
}
void applySpCoverIntent(AppState& app,gameplay::bot::Actor& bot,float dt){
    if(!spContextsEnabled(app))return;
    auto& state=spContextState(app,bot);if(state.coverSeek<=0)return;
    const float distance=gameplay::bot::horizontalDistance(bot.position,state.coverGoal);
    state.coverCheck-=dt;state.coverStall+=dt;
    if(distance<state.coverBestDistance-12.f){state.coverBestDistance=distance;state.coverStall=0;}
    if(state.coverStall>1.5f){stopSpCoverSeek(app,bot);return;}
    if(state.coverCheck<=0){
        state.coverCheck=.3f;
        if(!spCoverHeightAt(app,state.coverGoal)||(distance<80.f&&!app.loadedMap->navigationSegmentClear(bot.position,state.coverGoal,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight))){stopSpCoverSeek(app,bot);return;}
    }
    if(distance<6.f&&std::abs(bot.position.z-state.coverGoal.z)<12.f){
        bot.input.forward=bot.input.right=0;bot.input.sprint=false;bot.spWantsMove=bot.spCombatMoving=false;
        const auto toPlayer=app.actorPosition-bot.position;
        bot.yaw=gameplay::bot::wrapAngle(bot.yaw+gameplay::bot::wrapAngle(std::atan2(toPlayer.y,toPlayer.x)-bot.yaw)*(1-std::exp(-9.f*dt)));
        // Physics must settle at the actual supported position; arrival never
        // snaps position or velocity and turning remains the normal aim policy.
        if(gameplay::bot::horizontalSpeed(bot)<15.f){
            const auto height=spLocalCoverHeight(app,bot);
            if(height&&state.coverClip<app.botActorScene->animations.size()){
                stopSpCoverSeek(app,bot);state.cover=2.4f;state.coverElapsed=0;state.cooldown=4.4f;state.coverHeightIw=*height;state.coverCheck=.3f;
                state.overlay=state.coverClip;state.frame=0;state.weight=0;
            }
            else if(!spCoverHeightAt(app,bot.position))stopSpCoverSeek(app,bot);
        }
        return;
    }
    bot.spMoveGoal=state.coverGoal;bot.spWantsMove=bot.spCombatMoving=true;
    bot.spIdleWindow=bot.spReactionWindow=false;bot.spMoveThrottle=.65f;
    bot.stance=scene::Stance::Stand;bot.input.fire=false;bot.input.sprint=false;
    if(distance<80.f){
        // General routing intentionally accepts loose endpoints. The final
        // cover step instead approaches the exact collision-validated anchor.
        const auto to=state.coverGoal-bot.position;
        const float desired=std::atan2(to.y,to.x);
        bot.yaw=gameplay::bot::wrapAngle(bot.yaw+std::clamp(gameplay::bot::wrapAngle(desired-bot.yaw)*(1-std::exp(-9.f*dt)),-8.f*dt,8.f*dt));
        const float turn=gameplay::bot::wrapAngle(desired-bot.yaw);
        bot.input.forward=std::abs(turn)<.4f?.30f:0.f;bot.input.right=0;bot.input.ads=false;
        bot.spCombatMoving=false;
    }
}
bool prepareSpContext(AppState& app,gameplay::bot::Actor& bot,float dt,bool visible){
    if(app.activeBotSystemMode!=1||!app.botActorScene||dt<=0)return false;
    auto& state=spContextState(app,bot);state.varietyGrant=-1;
    const bool hurt=bot.health<state.lastHealth-.01f;state.lastHealth=bot.health;
    if(!bot.alive){state={};return false;}
    state.cooldown=std::max(0.f,state.cooldown-dt);
    const bool stationaryReserved=!bot.spMovingScenario&&!bot.spScenarioInterrupted&&bot.spScenario<app.botActorScene->animations.size()&&bot.spScenarioFrame<app.botActorScene->animations[bot.spScenario].durationFrames;
    const auto request=spContextRequest(bot);
    const bool safe=bot.grounded&&!bot.mantling&&bot.reloadTime<=0&&!bot.spMovingScenario&&!stationaryReserved&&bot.spScenarioWeight<.01f&&bot.equipmentPoseTime<=0;
    if(!safe){state.throwTime=state.wounded=0;}
    if(state.coverSeek>0){
        if(state.coverSeek<=dt||app.botSpCoverChance<=0||!app.loadedMap||!request.rifle||!safe)stopSpCoverSeek(app,bot);
        else state.coverSeek-=dt;
    }
    if(state.cover>0){
        state.cover=std::max(0.f,state.cover-dt);state.coverElapsed+=dt;state.coverCheck-=dt;
        if(state.cover<=0)++state.actionsCompleted;
        if(!safe||app.botSpCoverChance<=0)state.cover=0;
        else if(state.coverCheck<=0){state.coverCheck=.3f;if(!spLocalCoverHeight(app,bot))state.cover=0;}
    }
    if(state.throwTime>0){
        // Reserve normal deceleration before starting the authored clock.
        if(gameplay::bot::horizontalSpeed(bot)>=15.f){
            state.throwSettle+=dt;if(state.throwSettle>2.f)state.throwTime=0;
        }else{
            state.throwTime=std::max(0.f,state.throwTime-dt);
            if(state.throwTime<=0){++state.actionsCompleted;state.cooldown=1.f;}
        }
    }
    if(state.wounded>0){state.wounded=std::max(0.f,state.wounded-dt);if(state.wounded<=0){++state.actionsCompleted;state.cooldown=1.f;}}
    const bool eligible=safe&&state.cooldown<=0&&state.throwTime<=0&&state.wounded<=0&&state.coverSeek<=0&&state.cover<=0;
    if(eligible)state.clock.elapsed=std::min(1.0,state.clock.elapsed+dt);
    const bool periodic=eligible&&state.clock.elapsed>=1.0&&static_cast<std::size_t>(&bot-app.bots.data())==app.botRouteWorkTurn;
    if(periodic){
        state.clock.elapsed=0;state.varietyGrant=static_cast<int>(state.clock.opportunitySequence++%3);
        // Three bounded families share opportunities: contextual acting,
        // root-motion reactions, and stationary gestures.
        if(state.varietyGrant==0&&request.rifle){
            const unsigned first=state.contextCursor;
            for(unsigned attempt=0;attempt<3;++attempt){
                const unsigned kind=(first+attempt)%3;auto r=request;r.moving=false;r.firing=false;
                bool started=false;
                if(kind==0&&gameplay::bot::spOpportunityChance(app.botSpGrenadeChance,bot.id,++state.sequence,0x544852)){
                    r.kind=cadence::sp::ContextKind::Throw;
                    if(const auto clip=cadence::sp::selectContextClip(app.botSpContextClips,r,bot.id,state.sequence)){
                        state.throwClip=*clip;const auto& animation=app.botActorScene->animations[*clip];
                        state.throwTime=animation.durationFrames/std::max(1.f,animation.framerate);
                        state.throwSettle=0;state.frame=0;state.overlay=*clip;started=state.throwTime>0;
                    }
                }else if(kind==1&&gameplay::bot::spOpportunityChance(app.botSpWoundedChance,bot.id,++state.sequence,0x574e44)){
                    r.kind=cadence::sp::ContextKind::WoundedWalk;r.moving=true;r.temporaryWounded=true;
                    if(cadence::sp::selectContextClip(app.botSpContextClips,r,bot.id,state.woundedSequence+1)){
                        state.wounded=3.f;state.woundedSequence++;started=true;
                    }
                }else if(kind==2&&gameplay::bot::spOpportunityChance(app.botSpCoverChance,bot.id,++state.sequence,0x434f56))started=beginSpCoverSeek(app,bot);
                if(started){if(kind<2)bot.firePoseTime=0;state.contextCursor=(kind+1)%3;++state.actionsStarted;break;}
            }
        }
    }
    if(state.throwTime<=0&&state.wounded<=0)return false;
    // A cosmetic reservation owns inputs, never health or grenade gameplay.
    bot.input={};bot.spWantsMove=false;bot.spIdleWindow=false;bot.spReactionWindow=false;
    bot.firePoseTime=std::max(0.f,bot.firePoseTime-dt);bot.fireCooldown=std::max(0.f,bot.fireCooldown-dt);
    bot.behaviorClock+=dt;bot.movementIntentTime=0;
    if(state.wounded>0){bot.stance=scene::Stance::Stand;bot.input.forward=.65f;bot.movementIntentTime=.16f;}
    return true;
}
std::optional<std::size_t> spContextBaseAnimation(AppState& app,gameplay::bot::Actor& bot){
    if(!spContextsEnabled(app))return {};
    auto& state=spContextState(app,bot);auto r=spContextRequest(bot);
    if(bot.mantling){
        r.kind=cadence::sp::ContextKind::TraversalMantle;r.traversalValidated=true;r.mantleOver=false;r.traversalHeightIw=(bot.mantleEnd.z-bot.mantleStart.z)/gameplay::iw::kWorldUnitsPerIwUnit;
        return cadence::sp::selectContextClip(app.botSpContextClips,r,bot.id,0);
    }
    if(state.wounded>0&&state.throwTime<=0&&state.cover<=0&&state.coverSeek<=0&&!r.scenarioActive&&!r.firing&&!r.reloading&&(!r.moving||(bot.input.forward>.15f&&std::abs(bot.input.right)<.25f))){
        r.kind=r.moving?cadence::sp::ContextKind::WoundedWalk:cadence::sp::ContextKind::WoundedIdle;r.temporaryWounded=true;
        return cadence::sp::selectContextClip(app.botSpContextClips,r,bot.id,state.woundedSequence);
    }
    return {};
}
void limitSpWoundedMovement(AppState& app,gameplay::bot::Actor& bot){
    // Limit intent, not velocity: ordinary physics supplies smooth deceleration.
    // Reuse base selection so combat, traversal and reactions are never slowed
    // merely because an ambient wounded episode is still on its timer.
    const auto selected=spContextBaseAnimation(app,bot);
    if(!selected||*selected>=app.botClipAuthoredSpeeds.size())return;
    const auto entry=std::find_if(app.botSpContextClips.begin(),app.botSpContextClips.end(),[&](const auto& clip){return clip.animation==*selected;});
    if(entry==app.botSpContextClips.end()||entry->descriptor.kind!=cadence::sp::ContextKind::WoundedWalk)return;
    const float authored=app.botClipAuthoredSpeeds[*selected];
    if(!std::isfinite(authored)||authored<10.f)return;
    const float cap=std::min(1.f,authored/gameplay::iw::kRunSpeed);
    const float magnitude=std::hypot(bot.input.forward,bot.input.right);
    if(magnitude>cap){bot.input.forward*=cap/magnitude;bot.input.right*=cap/magnitude;}
    bot.input.sprint=false;bot.sprintIntent=false;
}
void updateSpContextPose(AppState& app,gameplay::bot::Actor& bot,float dt){
    if(!spContextsEnabled(app)||dt<=0)return;
    auto& state=spContextState(app,bot);auto r=spContextRequest(bot);std::optional<std::size_t> selected;
    if(bot.alive&&state.throwTime>0&&gameplay::bot::horizontalSpeed(bot)<15.f)selected=state.throwClip;
    else if(bot.alive&&state.cover>0){
        r.moving=false;r.scenarioActive=false;r.reloading=false;
        r.stance=state.coverHeightIw<44.f?cadence::sp::ContextStance::Crouch:cadence::sp::ContextStance::Stand;
        r.kind=r.firing&&state.coverElapsed>.35f?cadence::sp::ContextKind::CoverFire:cadence::sp::ContextKind::CoverIdle;
        if(r.kind==cadence::sp::ContextKind::CoverIdle)r.firing=false;
        r.coverValidated=true;r.coverHeightIw=state.coverHeightIw;
        selected=cadence::sp::selectContextClip(app.botSpContextClips,r,bot.id,state.sequence);
        // Commitment includes an actual BO2_SP cover clip, never an MP idle.
        if(!selected&&state.coverClip<app.botActorScene->animations.size())selected=state.coverClip;
    }
    else if(bot.alive&&bot.targetVisible&&bot.input.ads&&!r.moving&&!r.scenarioActive&&state.wounded<=0){
        const auto difference=app.actorPosition+scene::Vec3{0,0,app.actorViewHeight}-bot.position-scene::Vec3{0,0,gameplay::iw::worldUnits(60)};
        r.aimPitchDegrees=std::atan2(difference.z,std::sqrt(difference.x*difference.x+difference.y*difference.y))*180/scene::kPi;
        // Ordinary horizontal aim keeps the native MP action layer.
        if(std::abs(r.aimPitchDegrees)>25){r.kind=r.firing?cadence::sp::ContextKind::StandFire:cadence::sp::ContextKind::StandAim;selected=cadence::sp::selectContextClip(app.botSpContextClips,r,bot.id,0);}
    }
    if(selected&&*selected!=state.overlay){
        state.previousOverlay=state.overlay;state.previousFrame=state.frame;state.transitionElapsed=0;
        state.overlay=*selected;state.frame=0;
    }
    state.transitionElapsed=std::min(.16f,state.transitionElapsed+dt);
    state.weight=gameplay::sprintEnvelope(state.weight,selected.has_value(),dt,.16f);
    if(state.overlay<app.botActorScene->animations.size()){
        const auto& clip=app.botActorScene->animations[state.overlay];
        if(bot.input.fire&&clip.action==scene::ActionRole::Fire&&state.frame>=clip.durationFrames)state.frame=0;
        if(state.throwTime<=0||gameplay::bot::horizontalSpeed(bot)<15.f)state.frame+=dt*clip.framerate;
        if(clip.looping&&clip.durationFrames)state.frame=std::fmod(state.frame,static_cast<float>(clip.durationFrames));else state.frame=std::min(state.frame,static_cast<float>(clip.durationFrames));
    }
    if(!bot.alive){state={};return;}
    if(state.weight<=.001f&&!selected)state.overlay=static_cast<std::size_t>(-1);
}
bool spContextShotHits(AppState& app,const gameplay::bot::Actor& bot){
    if(app.activeBotSystemMode!=1)return true;
    const auto& state=spContextState(app,bot);
    const float eye=gameplay::iw::viewHeight(bot.stance);
    const float height=state.cover>0?std::max(eye,state.coverHeightIw*gameplay::iw::kWorldUnitsPerIwUnit+15.f):eye;
    if(app.loadedMap&&!app.loadedMap->lineOfSight(bot.position+scene::Vec3{0,0,height},app.actorPosition+scene::Vec3{0,0,app.actorViewHeight}))return false;
    return state.cover<=0||gameplay::bot::spOpportunityChance(.30f,bot.id,static_cast<unsigned>(bot.behaviorClock*1000),0x434f56);
}
