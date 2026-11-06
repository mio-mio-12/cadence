const float unit=gameplay::iw::worldUnits(1);
auto& state=app.cssMovement;
const auto publishedPosition=state.position*unit,publishedVelocity=state.velocity*unit;
if(app.actorPosition.x!=publishedPosition.x||app.actorPosition.y!=publishedPosition.y||app.actorPosition.z!=publishedPosition.z){state.position=app.actorPosition/unit;state.initialized=false;}
if(app.actorVelocity.x!=publishedVelocity.x||app.actorVelocity.y!=publishedVelocity.y||app.actorVelocity.z!=publishedVelocity.z)state.velocity=app.actorVelocity/unit;
gameplay::css::Command command;
command.forward=forward;command.side=side;command.yaw=app.actorYaw;command.jump=jump;
command.duck=app.gameplayStance!=scene::Stance::Stand;
command.walk=actorKey(app,GLFW_KEY_LEFT_SHIFT)||actorKey(app,GLFW_KEY_RIGHT_SHIFT);
gameplay::css::Settings settings;
settings.maxSpeed=250.f*std::max(.01f,app.weaponTiming.moveSpeedScale);
settings.airAccelerate=app.sourceAirAccelerate;
settings.unlimitedBunnyhop=app.sourceUnlimitedBunnyhop;
const float height=actorCollisionHeight(app,app.gameplayStance,72.f,36.f,36.f);
const auto trace=[&](scene::Vec3 a,scene::Vec3 b,float radius=16.f,scene::Vec3 offset=scene::Vec3{}){
    const bool custom=customCollisionFootprint(app);
    const auto probe=gameplay::collision::scaleFootprintProbe(actorCollisionHalfExtents(app,16.f*unit),16.f,radius,offset);
    if(custom)offset=probe.offset/unit;
    gameplay::movement::Trace hit;const auto start=(a+offset)*unit,end=(b+offset)*unit;
    if(app.loadedMap)hit=custom?gameplay::movement::sweep(*app.loadedMap,start,end,probe.halfExtents,height):gameplay::movement::sweep(*app.loadedMap,start,end,radius*unit,height);
    else {
        hit.end=custom?scene::course::constrainMove(start,end,probe.halfExtents,height):constrainActorMove(app,start,end,&hit.normal);
        const float floor=actorGroundHeight(app,end.x,end.y,start.z+18*unit);
        if(end.z<floor&&start.z>=floor){hit.fraction=(start.z-floor)/std::max(.0001f,start.z-end.z);hit.end=scene::lerp(start,end,hit.fraction);hit.normal={0,0,1};}
        else if(scene::length(hit.end-end)>.001f)hit.fraction=0;
    }
    hit.end=hit.end/unit-offset;return hit;
};
app.sourceFixedAccumulator+=std::clamp(double(deltaSeconds),0.0,.25);
while(app.sourceFixedAccumulator+1e-10>=gameplay::css::interval){
    app.sourceFixedAccumulator-=gameplay::css::interval;app.actorPreviousPosition=state.position*unit;
    const bool grounded=state.grounded;const auto velocity=state.velocity;
    gameplay::css::tick(state,command,settings,trace);
    if(!grounded&&state.grounded)landingSpeed=std::max(landingSpeed,-velocity.z*unit);
    app.actorPosition=state.position*unit;app.actorVelocity=state.velocity*unit;app.actorGrounded=state.grounded;
    if(app.actorFollowCamera)app.cameraTarget+=app.actorPosition-app.actorPreviousPosition;
}
app.actorGroundGrace=state.grounded?.1f:0.f;
app.actorRenderPosition=scene::lerp(app.actorPreviousPosition,app.actorPosition,float(app.sourceFixedAccumulator/gameplay::css::interval));
app.actorSurfing=false;
