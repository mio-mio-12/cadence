// Melee uses the same death/respawn pipeline as hitscan, but never produces a shot.
void processPlayerKnifeAttack(AppState& app){
    if(app.botProcessedKnifeAttack==app.playerKnifeAttack)return;
    app.botProcessedKnifeAttack=app.playerKnifeAttack;
    if(!app.actorMode||app.takePreview||!isKnifeWeapon(app)||!app.botActorScene)return;
    const scene::Vec3 origin=app.actorPosition+scene::Vec3{0,0,app.actorViewHeight};
    const float cp=std::cos(app.cameraPitch);
    const scene::Vec3 forward{cp*std::cos(app.actorYaw),cp*std::sin(app.actorYaw),std::sin(app.cameraPitch)};
    for(auto& bot:app.bots){
        if(!bot.alive)continue;
        const float height=app.botHeadHeight*(bot.stance==scene::Stance::Prone?.3f:bot.stance==scene::Stance::Crouch?.7f:1.f);
        const auto hit=gameplay::knifeTarget(origin,forward,bot.position,height,app.weaponProfile.knifeRadiusCm,app.weaponProfile.knifeConeDegrees,[&](scene::Vec3 point){
            const auto offset=point-origin;const float distance=scene::length(offset);
            if(app.loadedMap){
                const auto obstruction=app.loadedMap->raycastSurface(origin,offset/distance,distance);
                return !obstruction||scene::length(obstruction->position-origin)>=distance-.1f;
            }
            return gameplay::bot::lineOfSight(app.actorPosition,bot.position);
        });
        if(!hit)continue;
        const auto clip=botDeathAnimation(*app.botActorScene,bot,bot.weaponClass,app.playerKnifeAttack,&app);
        bot.health=0;bot.alive=false;bot.behavior=gameplay::bot::BehaviorState::Dead;
        bot.respawnTime=app.botRespawnTime;bot.velocity.x*=.35f;bot.velocity.y*=.35f;
        bot.input={};bot.animationFrame=0;bot.previousAnimation=bot.animation;bot.previousAnimationFrame=0;bot.animationBlendElapsed=0;
        bot.actionBlendWeight=bot.spScenarioWeight=bot.firePoseTime=bot.equipmentPoseTime=0;bot.spMovingScenario=false;bot.mantling=false;
        if(clip)bot.animation=*clip;
        app.hitmarkerTime=.24f;app.hitmarkerKind=3;app.renderer.addImpactCube(*hit);
        app.gameplayStatus="Knife kill";
    }
}
