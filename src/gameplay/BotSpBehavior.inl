// Included by BotActor.h inside gameplay::bot after Actor is defined.
// Policy chooses committed intentions; map routing validates their next waypoint.
inline void steerSpMovement(Actor& a,scene::Vec3 waypoint,float dt){
    if(!a.spWantsMove)return;
    const auto d=waypoint-a.position;
    const float travelYaw=std::atan2(d.y,d.x);
    const auto aim=a.lastKnownPlayer-a.position;
    const bool movingAim=a.spCombatMoving&&a.targetVisible&&a.reloadTime<=0;
    const float desired=movingAim?std::atan2(aim.y,aim.x):travelYaw,turn=wrapAngle(desired-a.yaw);
    a.yaw=wrapAngle(a.yaw+std::clamp(turn*(1.0f-std::exp(-6.0f*dt)),-5.0f*dt,5.0f*dt));
    const float travelError=wrapAngle(travelYaw-a.yaw),remaining=std::abs(travelError);
    const bool canMove=horizontalDistance(waypoint,a.position)>iw::worldUnits(8.0f)&&(movingAim||remaining<1.1f);
    const float arrivalScale=horizontalDistance(waypoint,a.spMoveGoal)<30.f?std::clamp(horizontalDistance(waypoint,a.position)/140.f,.15f,1.f):1.f;
    const float throttle=canMove?a.spMoveThrottle*arrivalScale:0;
    // Smooth facing independently, but keep translation aimed at the validated
    // segment instead of drawing a wide forward-only arc off its support.
    a.input.forward=throttle*std::cos(travelError);
    a.input.right=-throttle*std::sin(travelError);
    a.input.ads=movingAim;
    // Short navigation segments must not permanently suppress longer sprints.
    a.input.sprint=!movingAim&&!a.spPatrolWalking&&a.spSprintWanted&&a.input.forward>0.65f&&remaining<0.35f&&a.reloadTime<=0&&horizontalDistance(a.spMoveGoal,a.position)>iw::worldUnits(160.0f);
    if(a.input.forward>0){a.movementIntentTime=0.16f;a.sprintIntent=a.input.sprint;}
}

// Caller provides a collision-tested jump/corner opportunity; never synthesize
// an obstacle from a random roll. A successful roll is consumed by a verified
// arc; a rejected roll waits one game second instead of rerolling each frame.
inline bool applySpTacticalJump(Actor& a,const BehaviorConfig& c,float dt,bool geometryApproved){
    if(!c.spPlayerPolicy||dt<=0)return false;
    a.spJumpApprovalHeld=geometryApproved;
    const bool combatMove=(a.targetVisible&&a.spCombatMoving)||(a.awarenessMemory>0&&a.spPhase==SpPhase::Reposition);
    if(!combatMove||std::hypot(a.velocity.x,a.velocity.y)<iw::worldUnits(50))return false;
    if(!geometryApproved||!a.alive||!a.grounded||a.mantling||a.reloadTime>0||a.spMovingScenario||a.spReactionWindow||a.equipmentPoseTime>0||a.spJumpCooldown>0||!a.spWantsMove||std::abs(a.input.forward)+std::abs(a.input.right)<0.2f)return false;
    ++a.spJumpSequence;
    if(!spOpportunityChance(c.spJumpChance,a.id,a.spJumpSequence,0x4a554d50u)){a.spJumpCooldown=1.f;return false;}
    a.input.jump=true;a.spJumpCooldown=.15f;a.stance=scene::Stance::Stand;
    return true;
}

inline void updateSpPlayerBehavior(Actor& a,scene::Vec3 player,const BehaviorConfig& c,float dt,bool sceneVisibility){
    dt=std::max(0.0f,dt);
    a.input={};a.spWantsMove=a.spIdleWindow=a.spReactionWindow=false;
    if(dt<=0)return;
    const auto decay=[dt](float& t){t=std::max(0.0f,t-dt);};
    decay(a.movementIntentTime);decay(a.groundGrace);decay(a.fireCooldown);decay(a.firePoseTime);decay(a.muzzleFlashTime);decay(a.equipmentPoseTime);decay(a.awarenessMemory);decay(a.spPhaseTime);decay(a.repathTime);decay(a.spJumpCooldown);
    a.behaviorClock+=dt;a.reloadStarted=false;
    if(!a.alive){a.behavior=BehaviorState::Dead;return;}
    if(!a.spPolicyInitialized){a.spPolicyInitialized=true;a.spPolicyLastHealth=a.health;a.lastKnownPlayer=a.position;a.spPhaseTime=0;a.spQuietTime=2+3*personalityValue(a.id*3917u);}
    // Arena pursuit is a coarse navigation hint, not visibility or aim data.
    // All firing still requires actual sight. Do not orbit the spawn forever.
    a.spHuntRefresh=std::max(0.f,a.spHuntRefresh-dt);
    if(a.spHuntRefresh<=0){a.spHuntGoal=player;a.spHuntRefresh=1.5f+.25f*float(a.id%3);}
    if(a.reloadTime>0){decay(a.reloadTime);a.input.reload=true;if(a.reloadTime<=0)a.ammo=std::max(1,c.magazineSize);}
    if(a.ammo<=0&&a.reloadTime<=0){a.reloadTime=std::max(0.2f,c.reloadDuration);a.firePoseTime=a.reloadTime;a.reloadStarted=true;a.input.reload=true;}
    const auto toPlayer=player-a.position;
    const float distance=scene::length(toPlayer);
    const float dot=scene::dot({std::cos(a.yaw),std::sin(a.yaw),0},scene::normalize(scene::Vec3{toPlayer.x,toPlayer.y,0}));
    const bool wasVisible=a.targetVisible;
    const bool attentive=wasVisible||a.spPhase==SpPhase::Aim||a.spPhase==SpPhase::Burst||a.spPhase==SpPhase::React;
    a.targetVisible=sceneVisibility&&(c.spUseSceneVisibilityOnly||lineOfSight(a.position,player))&&distance<c.detectionRange&&(attentive||dot>0.15f||distance<iw::worldUnits(90.0f));
    if(a.targetVisible){if(wasVisible&&horizontalDistance(player,a.lastKnownPlayer)>.01f)a.spLastSeenDirection=scene::normalize(scene::Vec3{player.x-a.lastKnownPlayer.x,player.y-a.lastKnownPlayer.y,0});a.lastKnownPlayer=player;a.awarenessMemory=5;a.awareness=1;a.targetVisibleTime+=dt;}
    else {a.targetVisibleTime=0;decay(a.awareness);}
    const bool damage=a.health<a.spPolicyLastHealth-0.01f;
    a.spPolicyLastHealth=a.health;
    // Movement-aware SP clips own their short, collision-constrained step.
    // Sense threats normally, but never spend ammunition under a reaction pose.
    if(a.spMovingScenario&&a.grounded&&!a.mantling&&a.reloadTime<=0){
        if(a.targetVisible&&!a.spMovingPain&&!a.spAmbientScenario)a.spScenarioInterrupted=true;
        if(!a.spScenarioInterrupted){a.yaw=a.spMotionYaw;a.input.forward=a.spMotionForward;a.input.right=a.spMotionRight;}
        a.spReactionWindow=true;return; // The blend-out owns the pose too.
    }
    const auto roll=[&](unsigned salt){return personalityValue(a.id*17389u+a.spDecisionSequence*7919u+salt);};
    const auto enter=[&](SpPhase phase,float time){
        a.spPhase=phase;a.spPhaseTime=time;++a.spDecisionSequence;
        a.spPatrolWalking=phase==SpPhase::Patrol&&spOpportunityChance(c.spPatrolWalkChance,a.id,a.spDecisionSequence,11);
        a.spSprintWanted=spOpportunityChance(c.spSprintChance,a.id,a.spDecisionSequence,13);
        if(phase!=SpPhase::Burst)a.spCombatMoving=false;
        if(phase==SpPhase::Aim&&spOpportunityChance(c.spCombatMobilityChance,a.id,a.spDecisionSequence,17)){
            const auto toward=scene::normalize(scene::Vec3{a.lastKnownPlayer.x-a.position.x,a.lastKnownPlayer.y-a.position.y,0});
            const float side=roll(31)<0.5f?-1.f:1.f;
            a.spMoveGoal=a.position+scene::Vec3{-toward.y,toward.x,0}*(side*iw::worldUnits(130+90*roll(37)));
            if(distance<c.preferredRange*.55f)a.spMoveGoal=a.spMoveGoal-toward*iw::worldUnits(80);
            a.spCombatMoving=true;
        }
        if(phase==SpPhase::Search){a.spSearchGoal=a.lastKnownPlayer;a.spSearchExtended=false;}
    };
    const auto reposition=[&](){
        const auto toward=scene::normalize(scene::Vec3{a.lastKnownPlayer.x-a.position.x,a.lastKnownPlayer.y-a.position.y,0});
        const float side=roll(31)<0.5f?-1.0f:1.0f;
        a.spMoveGoal=a.position+scene::Vec3{-toward.y,toward.x,0}*(side*iw::worldUnits(65+75*roll(47)));
        if(distance<c.preferredRange*0.55f)a.spMoveGoal=a.spMoveGoal-toward*iw::worldUnits(85);
        enter(SpPhase::Reposition,1.25f+0.9f*roll(59));
        if(roll(61)<.35f){a.spSprintWanted=true;a.spPatrolWalking=false;}
    };
    if(damage&&a.grounded&&a.reloadTime<=0){enter(SpPhase::React,0.25f);a.spQuietTime=0;}
    else if(a.targetVisible&&!wasVisible&&(a.spPhase==SpPhase::Patrol||a.spPhase==SpPhase::Pause||a.spPhase==SpPhase::Search))enter(SpPhase::React,std::max(0.18f,c.reactionDelay));
    else if(!a.targetVisible&&(a.spPhase==SpPhase::Aim||a.spPhase==SpPhase::Burst||a.spPhase==SpPhase::Approach))enter(SpPhase::Search,18.f);
    if(a.spPhase==SpPhase::React&&a.spPhaseTime<=0){
        if(a.targetVisible)enter(distance>c.preferredRange*1.3f?SpPhase::Approach:SpPhase::Aim,0.15f);
        else enter(SpPhase::Search,18.f);
    }
    if(a.spPhase==SpPhase::Approach){
        a.spMoveGoal=a.lastKnownPlayer;
        if(distance<=c.preferredRange*1.05f)enter(SpPhase::Aim,0.12f+0.15f*roll(73));
    }
    if(a.spPhase==SpPhase::Aim&&a.spPhaseTime<=0&&a.targetVisible&&a.reloadTime<=0){a.spBurstShots=3+static_cast<int>(roll(89)*3.99f);enter(SpPhase::Burst,std::max(0.8f,c.fireInterval*static_cast<float>(a.spBurstShots)+0.5f));}
    if(a.spPhase==SpPhase::Burst&&(a.spBurstShots<=0||a.spPhaseTime<=0||a.reloadTime>0)){
        if(roll(101)<std::clamp(c.tacticalReposition,0.0f,0.9f))reposition();
        else enter(SpPhase::Aim,0.25f+0.25f*roll(103));
    }
    if(a.spPhase==SpPhase::Reposition&&(a.spPhaseTime<=0||horizontalDistance(a.position,a.spMoveGoal)<iw::worldUnits(22)))enter(a.targetVisible?SpPhase::Aim:SpPhase::Search,a.targetVisible?0.15f:18.f);
    if(a.spPhase==SpPhase::Search){
        a.spMoveGoal=a.spSearchGoal;
        if(a.targetVisible)enter(SpPhase::Aim,0.3f);
        else if(a.spPhaseTime<=0)enter(SpPhase::Patrol,0);
        else if(horizontalDistance(a.position,a.spMoveGoal)<iw::worldUnits(36)){
            // Investigate a short distance along the last observed motion,
            // never the hidden player's live position; map routing validates it.
            if(!a.spSearchExtended&&scene::length(a.spLastSeenDirection)>.5f){a.spSearchExtended=true;a.spSearchGoal=a.lastKnownPlayer+a.spLastSeenDirection*250.f;a.spMoveGoal=a.spSearchGoal;++a.spDecisionSequence;}
            else enter(SpPhase::Patrol,0);
        }
    }
    if(a.spPhase==SpPhase::Pause){
        if(a.targetVisible)enter(SpPhase::Aim,0.25f);
        else if(a.spPhaseTime<=0){enter(SpPhase::Patrol,0);a.spQuietTime=0;}
    }
    if(a.spPhase==SpPhase::Patrol){
        if(a.targetVisible)enter(SpPhase::Approach,0);
        else {
            a.spQuietTime+=dt;
            if(a.spQuietTime>9+5*roll(151)){
                const bool camper=a.id%5==0;
                if(spOpportunityChance(c.spIdlePauseChance*(camper?1.f:.10f),a.id,a.spDecisionSequence,153))enter(SpPhase::Pause,camper?3.f+2.f*roll(157):.6f);
                a.spQuietTime=0;
            }
            else if(a.spPhaseTime<=0||horizontalDistance(a.position,a.spMoveGoal)<iw::worldUnits(35)){
                // Route to known supported player ground, not an arbitrary
                // lateral point or a floating same-height midpoint in a wall.
                // The route planner's soft lane penalties supply diversity.
                a.spMoveGoal=mapPatrolDestination(a,c).value_or(a.spHuntGoal);
                enter(SpPhase::Patrol,20);
            }
        }
    }
    a.behavior=a.spPhase==SpPhase::Patrol||a.spPhase==SpPhase::Pause?BehaviorState::Patrol:(a.targetVisible?BehaviorState::Engage:BehaviorState::Chase);
    if(a.spCombatMoving&&horizontalDistance(a.position,a.spMoveGoal)<iw::worldUnits(20))a.spCombatMoving=false;
    a.spWantsMove=a.spPhase==SpPhase::Patrol||a.spPhase==SpPhase::Approach||a.spPhase==SpPhase::Search||a.spPhase==SpPhase::Reposition||a.spCombatMoving;
    a.spIdleWindow=a.spPhase==SpPhase::Pause&&!a.targetVisible;
    a.spReactionWindow=a.spPhase==SpPhase::React;
    const float desiredThrottle=a.spWantsMove?(a.spPatrolWalking?0.42f:(a.spCombatMoving?0.78f:1.f)):0.f;
    a.spMoveThrottle+=(desiredThrottle-a.spMoveThrottle)*(1-std::exp(-4.f*dt));
    if(a.spWantsMove)steerSpMovement(a,a.spMoveGoal,dt);
    if(a.targetVisible&&(!a.spWantsMove||a.spCombatMoving)){
        const float desired=std::atan2(toPlayer.y,toPlayer.x)+std::sin(a.behaviorClock*1.71f+a.id*2.17f)*c.aimErrorDegrees*scene::kPi/180;
        if(!a.spWantsMove)a.yaw=wrapAngle(a.yaw+std::clamp(wrapAngle(desired-a.yaw)*(1-std::exp(-std::max(1.0f,c.aimResponse)*dt)),-8.5f*dt,8.5f*dt));
        a.input.ads=a.spPhase!=SpPhase::React&&a.reloadTime<=0;
        if(a.spPhase==SpPhase::Burst&&a.reloadTime<=0&&a.ammo>0&&a.fireCooldown<=0&&a.targetVisibleTime>=c.reactionDelay&&std::abs(wrapAngle(desired-a.yaw))<0.085f){
            a.input.fire=true;--a.ammo;--a.spBurstShots;a.fireCooldown=std::max(0.02f,c.fireInterval);a.firePoseTime=std::max(0.1f,c.fireInterval);a.muzzleFlashTime=0.055f;
        }
    }else if(a.spIdleWindow)a.yaw=wrapAngle(a.yaw+(a.id%2?-0.55f:0.55f)*dt);
    // Stance is not randomly rerolled, and reaction windows do not cancel reload.
    if(a.grounded)a.stance=scene::Stance::Stand;
    // SP policy returns before Classic's equipment opportunity code. Give it
    // its own bounded timer, using the same user-facing settings.
    decay(a.equipmentOpportunityTime);
    if(c.allowEquipment&&a.equipmentOpportunityTime<=0&&a.grounded&&!a.mantling&&!a.targetVisible&&a.reloadTime<=0&&a.firePoseTime<=0&&!a.spReactionWindow&&!a.spMovingScenario&&a.spScenarioWeight<=.01f&&a.equipmentPoseTime<=0){
        const auto seed=a.id*17401u+static_cast<std::uint32_t>(++a.equipmentSequence);
        const float lo=std::max(.1f,c.equipmentMinInterval),hi=std::max(lo,c.equipmentMaxInterval);
        a.equipmentOpportunityTime=lo+(hi-lo)*personalityValue(seed+11003u);
        a.input.equipment=c.equipmentChance>=1.f||(c.equipmentChance>0&&personalityValue(seed)<c.equipmentChance);
    }
    if(a.input.equipment||a.equipmentPoseTime>0){a.input.forward=a.input.right=0;a.input.sprint=a.input.fire=a.input.ads=a.input.jump=false;a.spWantsMove=false;}
}
