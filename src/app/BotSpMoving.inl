// Included inside main.cpp's application namespace. Root travel is sampled once
// at load; runtime follows its horizontal velocity through normal bot physics.
scene::Vec3 spMotionAt(const std::vector<scene::Vec3>& samples,float frame){
    frame=std::clamp(frame,0.f,static_cast<float>(samples.size()-1));
    const auto i=static_cast<std::size_t>(frame),j=std::min(i+1,samples.size()-1);
    return samples[i]+(samples[j]-samples[i])*(frame-static_cast<float>(i));
}

void prepareSpMovingScenario(AppState& app,gameplay::bot::Actor& bot,float delta){
    using namespace gameplay::bot;
    if(app.activeBotSystemMode!=1||!app.botActorScene||delta<=0)return;
    const auto& scene=*app.botActorScene;
    bot.spMotionCooldown=std::max(0.f,bot.spMotionCooldown-delta);
    const bool damage=bot.health<bot.spLastHealth-.01f;
    bot.spMotionDamagePending=damage?.35f:std::max(0.f,bot.spMotionDamagePending-delta);
    const bool eligible=bot.alive&&bot.grounded&&!bot.mantling&&!bot.spMovingScenario&&bot.spScenarioWeight<=.001f&&bot.spMotionCooldown<=0&&bot.reloadTime<=0&&bot.equipmentPoseTime<=0&&bot.stance==scene::Stance::Stand&&horizontalSpeed(bot)>40;
    if(eligible&&bot.spVarietyClock.advance(delta,1.0))bot.spMotionOpportunity=true;
    // At most one bot runs a full authored-path validation per frame. A ready
    // opportunity waits for its work turn rather than getting dropped/rerolled.
    const bool workTurn=static_cast<std::size_t>(&bot-app.bots.data())==app.botRouteWorkTurn;
    const auto actorIndex=static_cast<std::size_t>(&bot-app.bots.data());
    const bool granted=actorIndex<app.botSpContextStates.size()&&app.botSpContextStates[actorIndex].varietyGrant==1;
    if(eligible&&workTurn&&(granted||bot.spMotionDamagePending>0)){
        const bool periodic=granted;
        const bool pendingDamage=bot.spMotionDamagePending>0;
        bot.spMotionOpportunity=false;bot.spMotionDamagePending=0;
        SpMovingReactionContext context;
        context.gameDelta=delta;context.alive=bot.alive;context.grounded=bot.grounded;
        context.standing=bot.stance==scene::Stance::Stand;
        context.rifle=cadence::sp::supportsSpLongGun(bot.weaponClass);
        context.moving=horizontalSpeed(bot)>40;context.targetVisible=false; // Explicit stylized slip opportunities may reserve visible combat too.
        context.mantling=bot.mantling;context.reloading=bot.reloadTime>0;
        context.firing=false;context.equipment=bot.equipmentPoseTime>0; // Reservation stops new firing through the moving-scenario policy.
        context.scenarioActive=bot.spScenarioWeight>.001f;
        if(pendingDamage)context.actualDamageAge=0;
        auto choice=chooseSpMovingReaction(context,{app.botSpSlipChance,app.botSpMovingPainChance,0},bot.id,++bot.spMotionSequence);
        // An explicitly requested ambient category, not fabricated damage.
        const bool ambientRoll=!pendingDamage&&periodic&&context.rifle&&spOpportunityChance(app.botSpAmbientStumbleChance,bot.id,bot.spMotionSequence,0x414d4249);
        // Separate chances, fair arbitration if both succeed: one high slider
        // must not permanently suppress the other animation category.
        const bool ambient=ambientRoll&&(choice!=SpMovingReaction::Slip||(bot.spMotionSequence&1u));
        if(ambient)choice=SpMovingReaction::MovingPain;
        // Ambient stumbles share the already validated slip variants. Actual
        // hit reactions retain the dedicated pain pool and event semantics.
        const bool ambientSlip=ambient&&!app.botSpSlipClips.empty()&&
            bot.spMotionSequence%(app.botSpSlipClips.size()+app.botSpMovingPainClips.size())<app.botSpSlipClips.size();
        const auto* pool=(choice==SpMovingReaction::Slip||ambientSlip)?&app.botSpSlipClips:choice==SpMovingReaction::MovingPain?&app.botSpMovingPainClips:nullptr;
        if(pool&&!pool->empty()){
            const auto index=(*pool)[bot.spScenarioSequence++%pool->size()];
            const auto& samples=app.botSpMotionSamples.at(index);
            const float c=std::cos(bot.yaw),s=std::sin(bot.yaw);
            bool clear=true;
            // Validate the entire sampled horizontal route, not merely its end.
            // Collider still owns every actual step, including dynamic changes.
            if(app.loadedMap){
                auto previous=bot.position;
                for(std::size_t i=1;i<samples.size();++i){
                    const auto d=samples[i]-samples.front();
                    const auto next=bot.position+scene::Vec3{c*d.x-s*d.y,s*d.x+c*d.y,0};
                    const float missing=-1e8f;
                    const float ground=app.loadedMap->groundHeight(next.x,next.y,bot.position.z+scene::course::kStepHeight,missing);
                    const auto constrained=app.loadedMap->constrainMove(previous,next,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight);
                    if(ground==missing||std::abs(ground-bot.position.z)>scene::course::kStepHeight||horizontalDistance(constrained,next)>1.f||!app.loadedMap->navigationSegmentClear(previous,next,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight)){clear=false;break;}
                    previous=next;
                }
            }
            if(clear){
                bot.spMovingScenario=true;bot.spMovingPain=choice==SpMovingReaction::MovingPain&&!ambient;bot.spAmbientScenario=ambient||choice==SpMovingReaction::Slip;
                bot.spScenario=index;bot.spScenarioFrame=0;bot.spScenarioInterrupted=false;bot.spScenarioStance=bot.stance;
                bot.spMotionYaw=bot.yaw;bot.spMotionBlocked=0;
                bot.spMotionCooldown=.35f;bot.spPendingPain=0;
            }
        }
    }
    if(!bot.spMovingScenario)return;
    if(!bot.alive||!bot.grounded||bot.mantling||bot.reloadTime>0||bot.stance!=bot.spScenarioStance)bot.spScenarioInterrupted=true;
    const auto it=app.botSpMotionSamples.find(bot.spScenario);
    if(it==app.botSpMotionSamples.end()){bot.spScenarioInterrupted=true;return;}
    const auto& clip=scene.animations[bot.spScenario];
    const auto travel=spMotionAt(it->second,bot.spScenarioFrame+delta*clip.framerate)-spMotionAt(it->second,bot.spScenarioFrame);
    const float run=gameplay::iw::kRunSpeed*std::max(.1f,bot.movementThrottle);
    bot.spMotionForward=std::clamp(travel.x/(delta*run*(travel.x<0?gameplay::iw::kBackSpeedScale:1.f)),-1.f,1.f);
    // Bot movement defines positive right as local -Y.
    bot.spMotionRight=std::clamp(-travel.y/(delta*run*gameplay::iw::kStrafeSpeedScale),-.5f,.5f);
    if(bot.spScenarioFrame>clip.framerate*.3f&&bot.spMotionForward>.15f&&horizontalSpeed(bot)<5)bot.spMotionBlocked+=delta;
    else bot.spMotionBlocked=0;
    if(bot.spMotionBlocked>.2f)bot.spScenarioInterrupted=true;
}
