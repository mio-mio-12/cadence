// Included only in the two new controllers' live-physics branch.
const bool halfLife=app.movementAlgorithm==4;
const auto rules=halfLife?gameplay::reference::Rules::HalfLife:gameplay::reference::Rules::Cod;
const double interval=gameplay::reference::interval(app.movementAlgorithm);
const float unit=gameplay::iw::worldUnits(1);
auto& state=app.referenceMovement;
// Keep native-unit state across render frames; repeated cm/unit round trips
// otherwise make the result depend on the number of presentation frames.
const auto publishedPosition=state.position*unit,publishedVelocity=state.velocity*unit;
if(app.actorPosition.x!=publishedPosition.x||app.actorPosition.y!=publishedPosition.y||app.actorPosition.z!=publishedPosition.z)
    state.position=app.actorPosition/unit;
if(app.actorVelocity.x!=publishedVelocity.x||app.actorVelocity.y!=publishedVelocity.y||app.actorVelocity.z!=publishedVelocity.z)
    state.velocity=app.actorVelocity/unit;
gameplay::reference::Command command;
command.forward=forward;command.side=side;command.yaw=app.actorYaw;command.jump=jump;
command.sprint=sprint;command.ads=ads;command.weaponScale=app.weaponTiming.moveSpeedScale;
command.stance=app.gameplayStance==scene::Stance::Prone?2:app.gameplayStance==scene::Stance::Crouch?1:0;
const float height=unit*(halfLife?(command.stance?36.f:72.f):(command.stance==2?30.f:command.stance==1?50.f:70.f));
const float radius=unit*(halfLife?16.f:15.f);
const auto trace=[&](scene::Vec3 a,scene::Vec3 b){
    gameplay::movement::Trace hit;const auto start=a*unit,end=b*unit;
    if(app.loadedMap)hit=gameplay::movement::sweep(*app.loadedMap,start,end,radius,height);
    else {
        hit.end=constrainActorMove(app,start,end,&hit.normal);
        const float floor=actorGroundHeight(app,end.x,end.y,start.z+18*unit);
        if(end.z<floor&&start.z>=floor){hit.fraction=(start.z-floor)/std::max(.0001f,start.z-end.z);hit.end=scene::lerp(start,end,hit.fraction);hit.normal={0,0,1};}
        else if(scene::length(hit.end-end)>.001f)hit.fraction=0;
    }
    hit.end=hit.end/unit;return hit;
};
app.sourceFixedAccumulator+=std::clamp(double(deltaSeconds),0.0,.25);
while(app.sourceFixedAccumulator+1e-10>=interval){
    app.sourceFixedAccumulator-=interval;app.actorPreviousPosition=state.position*unit;
    const bool grounded=state.grounded;const auto velocity=state.velocity;
    gameplay::reference::tick(state,command,halfLife?10:8,rules,trace);
    if(!grounded&&state.grounded)landingSpeed=std::max(landingSpeed,-velocity.z*unit);
    app.actorPosition=state.position*unit;app.actorVelocity=state.velocity*unit;app.actorGrounded=state.grounded;
    app.actorMantleApproach.contact(velocity*unit,app.actorVelocity,app.actorMantleIntentTime>0);
    if(app.actorFollowCamera)app.cameraTarget+=app.actorPosition-app.actorPreviousPosition;
}
app.actorGroundGrace=state.grounded?.1f:0.f;
app.actorRenderPosition=scene::lerp(app.actorPreviousPosition,app.actorPosition,float(app.sourceFixedAccumulator/interval));
app.actorSurfing=false;
