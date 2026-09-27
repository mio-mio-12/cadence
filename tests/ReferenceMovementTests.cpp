#include "gameplay/ReferenceMovement.h"
#include "gameplay/Iw4Switch.h"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
using namespace gameplay::reference;
bool near(float a,float b,float tolerance=.001f){return std::abs(a-b)<tolerance;}
auto empty=[](Vec3,Vec3 b){return gameplay::movement::Trace{1,b,{},false};};
auto floorTrace=[](Vec3 a,Vec3 b){
    gameplay::movement::Trace hit{1,b,{},false};
    if(b.z<0&&a.z>=0){hit.fraction=a.z/(a.z-b.z);hit.end=scene::lerp(a,b,hit.fraction);hit.normal={0,0,1};}
    return hit;
};
void triangle(scene::glb::Map& map,Vec3 a,Vec3 b,Vec3 c){
    scene::glb::CollisionTriangle t;t.a=a;t.b=b;t.c=c;t.normal=scene::normalize(scene::cross(b-a,c-a));
    t.minimum={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};
    t.maximum={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
    t.walkable=t.normal.z>=.7f;t.blocking=!t.walkable;map.collision.push_back(t);
}
void quad(scene::glb::Map& map,Vec3 a,Vec3 b,Vec3 c,Vec3 d){triangle(map,a,b,c);triangle(map,a,c,d);}
int main(){
    // Independent scalar goldens for 10 ms split gravity and a 2000/unit
    // component bound. Include naturally reached terminal fall and both sides
    // of each boundary; no production clamp helper computes expectations.
    struct BoundCase{float input,travel,finalVelocity;};
    for(const auto row:std::array<BoundCase,6>{{
            {-2001,-20,-2000},{-2000,-20,-2000},{-1999,-20,-2000},
            {1999,19.95f,1991},{2000,19.96f,1992},{2005,20,1996}}}){
        for(bool swept:{false,true})for(float sign:{-1.f,1.f}){
            scene::glb::Map emptyMap;
            const auto trace=[&](Vec3 a,Vec3 b){return swept?
                gameplay::movement::sweep(emptyMap,a,b,16,72):empty(a,b);};
            State falling;falling.position={0,0,100};
            falling.velocity={2001*sign,-2001*sign,row.input};
            tick(falling,Command{},10,Rules::HalfLife,trace);
            CHECK(near(falling.position.x,20*sign));
            CHECK(near(falling.position.y,-20*sign));
            CHECK(near(falling.position.z,100+row.travel));
            CHECK(near(falling.velocity.z,row.finalVelocity));
            CHECK(near(falling.velocity.x,2000*sign));
            CHECK(near(falling.velocity.y,-2000*sign));
        }
    }
    // Actual swept wall: a far contact must not be reached using the unclamped
    // velocity, while a nearby wall still clips normally. Hull radius is 16.
    for(float wallX:{35.f,39.f}){
        scene::glb::Map wall;
        quad(wall,{wallX,-100,-100},{wallX,-100,300},{wallX,100,300},{wallX,100,-100});
        State moving;moving.position={0,0,100};moving.velocity={2500,0,0};
        tick(moving,Command{},10,Rules::HalfLife,[&](Vec3 a,Vec3 b){
            return gameplay::movement::sweep(wall,a,b,16,72);});
        if(wallX==39){CHECK(near(moving.position.x,20));CHECK(near(moving.velocity.x,2000));}
        else {CHECK(near(moving.position.x,19,.005f));CHECK(near(moving.velocity.x,0));}
    }
    // Independent scalar contact oracle: clip onto y/z ramp, then x/z ramp
    // without intervening travel. Golden values use ordinary plane projection,
    // not the production clip/slide helpers. Mirror horizontal axes to exercise
    // backward motion without a special movement branch.
    for(float sign:{-1.f,1.f})for(int zeroContacts:{1,2}){
        State contact;contact.velocity={100*sign,80*sign,-150};int calls=0;
        const auto scripted=[&](Vec3 a,Vec3 b){
            const int index=calls++;gameplay::movement::Trace hit{1,b,{},false};
            if(index==0){hit.fraction=.5f;hit.end=scene::lerp(a,b,.5f);hit.normal={0,.8f*sign,.6f};}
            else if(index<=zeroContacts){hit.fraction=0;hit.end=a;hit.normal={.8f*sign,0,.6f};}
            return hit;
        };
        slide(contact,.01f,Rules::HalfLife,scripted);
        CHECK(near(contact.velocity.x,100.512f*sign,.002f));
        CHECK(near(contact.velocity.y,100.8f*sign,.002f));
        CHECK(near(contact.velocity.z,-134.016f,.002f));
        CHECK(calls==zeroContacts+2);
        // Grounded behavior must not acquire the airborne rewrite.
        if(zeroContacts==1){
            calls=0;contact={};contact.grounded=true;contact.velocity={100*sign,80*sign,-150};
            slide(contact,.01f,Rules::HalfLife,scripted);
            CHECK(near(contact.velocity.x,87.552f*sign,.002f));
            CHECK(near(contact.velocity.y,87.552f*sign,.002f));
            CHECK(near(contact.velocity.z,-116.736f,.002f));
        }
    }
    // Scalar results from the pinned movement equations, in original units.
    Vec3 velocity{};accelerate(velocity,{320,0,0},.01f,Rules::HalfLife,true);CHECK(near(velocity.x,32));
    velocity={};accelerate(velocity,{320,0,0},.01f,Rules::HalfLife,false);CHECK(near(velocity.x,30));
    accelerate(velocity,{0,320,0},.01f,Rules::HalfLife,false);CHECK(near(velocity.x,30)&&near(velocity.y,30));
    velocity={};accelerate(velocity,{20,0,0},.008f,Rules::Cod,true);CHECK(near(velocity.x,7.2f));
    CHECK(snap(.5f)==0&&snap(1.5f)==2&&snap(2.5f)==2&&snap(-1.5f)==-2);
    Command cmd;cmd.forward=1;cmd.side=1;
    const auto diagonal=wish(cmd,Rules::Cod,true);CHECK(near(diagonal.x,-diagonal.y));CHECK(near(scene::length(diagonal),190));
    cmd.forward=-1;cmd.side=0;CHECK(near(scene::length(wish(cmd,Rules::Cod,true)),133));CHECK(near(scene::length(wish(cmd,Rules::Cod,false)),190));
    auto projected=projectGround({100,0,0},scene::normalize(Vec3{-1,0,1}));CHECK(near(projected.z,projected.x));CHECK(near(scene::length(projected),100));
    State state;cmd={};cmd.forward=1;
    tick(state,cmd,10,Rules::HalfLife,floorTrace);CHECK(near(state.velocity.x,32)&&near(state.position.x,.32f));
    for(int i=0;i<100;++i)tick(state,cmd,10,Rules::HalfLife,floorTrace);
    CHECK(near(state.velocity.x,320)&&state.grounded);
    cmd={};cmd.autoJump=false;cmd.jump=true;state={};tick(state,cmd,10,Rules::HalfLife,floorTrace);
    CHECK(near(state.position.z,(std::sqrt(72000.f)-4)*.01f));CHECK(near(state.velocity.z,std::sqrt(72000.f)-8));
    for(int i=0;i<150;++i)tick(state,cmd,10,Rules::HalfLife,floorTrace);
    CHECK(state.grounded&&state.velocity.z==0); // holding jump must not auto-hop
    cmd.jump=false;tick(state,cmd,10,Rules::HalfLife,floorTrace);cmd.jump=true;tick(state,cmd,10,Rules::HalfLife,floorTrace);CHECK(!state.grounded);
    state={};state.velocity={600,0,0};tick(state,cmd,10,Rules::HalfLife,floorTrace);
    CHECK(near(state.velocity.x,600*(544*.65f/std::sqrt(600.f*600+16.f)),.01f));
    // Space pressed in the air cannot silently become a buffered landing jump.
    state={};state.position.z=50;for(int i=0;i<100;++i)tick(state,cmd,10,Rules::HalfLife,floorTrace);CHECK(state.grounded);
    state={};cmd={};cmd.autoJump=false;cmd.jump=true;tick(state,cmd,8,Rules::Cod,floorTrace);CHECK(!state.grounded);CHECK(state.velocity.z==243);
    for(int i=0;i<180;++i)tick(state,cmd,8,Rules::Cod,floorTrace);CHECK(state.grounded);CHECK(state.lastJumpMs==8);
    // Held Space is an input extension, not a speed boost or an airborne jump.
    // Both native rule sets retain their own speed caps, gravity and COD gate.
    for(auto rules:{Rules::HalfLife,Rules::Cod}){
        const int interval=rules==Rules::HalfLife?10:8;
        State forward{},backward{};forward.velocity.x=250;backward.velocity.x=-250;
        cmd={};cmd.jump=true;CHECK(cmd.autoJump);
        int jumps=0;bool grounded=true;
        for(int i=0;i<400;++i){
            tick(forward,cmd,interval,rules,floorTrace);tick(backward,cmd,interval,rules,floorTrace);
            if(grounded&&!forward.grounded)++jumps;grounded=forward.grounded;
            CHECK(near(forward.position.x,-backward.position.x,.002f));
            CHECK(near(forward.velocity.x,-backward.velocity.x,.002f));
            CHECK(forward.position.z==backward.position.z);
            CHECK(std::abs(forward.velocity.x)<=250.001f);
        }
        CHECK(jumps>=4);
        // Holding jump throughout freefall must not reset vertical velocity.
        forward={};forward.position.z=1000;backward=forward;
        Command released;released.jump=false;
        for(int i=0;i<30;++i){tick(forward,cmd,interval,rules,empty);tick(backward,released,interval,rules,empty);}
        CHECK(forward.position.z==backward.position.z&&forward.velocity.z==backward.velocity.z);
        // A press begun airborne is queued by being held, then used on ground.
        forward={};forward.position.z=30;int landings=0;jumps=0;grounded=false;
        for(int i=0;i<160;++i){tick(forward,cmd,interval,rules,floorTrace);if(!grounded&&forward.grounded)++landings;if(grounded&&!forward.grounded)++jumps;grounded=forward.grounded;}
        CHECK(landings>=1&&jumps>=1);
    }
    // Unobstructed falling uses split gravity; no frame-size dependent Euler drift.
    state={};state.position.z=1000;cmd={};tick(state,cmd,10,Rules::HalfLife,empty);CHECK(near(state.position.z,999.96f)&&near(state.velocity.z,-8));
    // Same tick stream across render-frame groupings.
    const auto slope=[](Vec3 a,Vec3 b){
        gameplay::movement::Trace hit{1,b,{},false};const float da=a.z-.5f*a.x,db=b.z-.5f*b.x;
        if(db<0&&da>=0){hit.fraction=da/(da-db);hit.end=scene::lerp(a,b,hit.fraction);hit.normal=scene::normalize(Vec3{-.5f,0,1});}
        return hit;
    };
    state={};cmd={};cmd.forward=1;tick(state,cmd,8,Rules::Cod,slope);
    CHECK(state.grounded&&state.position.z>0&&state.velocity.z>0);
    CHECK(near(state.position.z,state.position.x*.5f));
    State a{},b{};cmd.forward=1;for(int i=0;i<100;++i)tick(a,cmd,8,Rules::Cod,floorTrace);
    for(int i=0;i<20;++i)for(int j=0;j<5;++j)tick(b,cmd,8,Rules::Cod,floorTrace);
    CHECK(a.position.x==b.position.x&&a.velocity.x==b.velocity.x);
    // Head hit: upward momentum cannot survive the collision and pierce a roof.
    const auto ceiling=[](Vec3 a,Vec3 b){auto hit=floorTrace(a,b);if(b.z>20&&a.z<=20){hit.fraction=(20-a.z)/(b.z-a.z);hit.end=scene::lerp(a,b,hit.fraction);hit.normal={0,0,-1};}return hit;};
    for(auto rules:{Rules::HalfLife,Rules::Cod}){
        state={};cmd={};cmd.jump=true;for(int i=0;i<100;++i){tick(state,cmd,8,rules,ceiling);CHECK(state.position.z<=20.001f);CHECK(std::isfinite(state.velocity.z));}
    }
    // Production mesh trace: finite wall sliding, hard head contact, and steps.
    for(auto rules:{Rules::HalfLife,Rules::Cod}){
        scene::glb::Map map;
        quad(map,{-1000,-1000,0},{1000,-1000,0},{1000,1000,0},{-1000,1000,0});
        quad(map,{100,-1000,0},{100,1000,0},{100,1000,200},{100,-1000,200});
        const auto meshTrace=[&](Vec3 a,Vec3 b){return gameplay::movement::sweep(map,a,b,16,72);};
        state={};state.position.z=.002f;cmd={};cmd.forward=1;cmd.side=1;
        for(int i=0;i<150;++i)tick(state,cmd,8,rules,meshTrace);
        CHECK(state.position.x<=84.01f&&state.position.x>75);CHECK(state.position.y<-50);CHECK(state.grounded);
        scene::glb::Map stairs;
        quad(stairs,{-1000,-1000,0},{1000,-1000,0},{1000,1000,0},{-1000,1000,0});
        quad(stairs,{80,-1000,0},{80,1000,0},{80,1000,12},{80,-1000,12});
        quad(stairs,{80,-1000,12},{1000,-1000,12},{1000,1000,12},{80,1000,12});
        const auto stepTrace=[&](Vec3 a,Vec3 b){return gameplay::movement::sweep(stairs,a,b,16,72);};
        state={};state.position.z=.002f;cmd={};cmd.forward=1;
        for(int i=0;i<150;++i)tick(state,cmd,8,rules,stepTrace);
        CHECK(state.position.x>100);CHECK(near(state.position.z,12,.01f));CHECK(state.grounded);
    }
    // GoldSrc surfing must emerge from ordinary gravity, air acceleration and
    // hull clipping. No surf flag, backward branch or injected boost is used.
    for(float offset:{0.f,20000.f})for(float steepness:{1.1f,2.f}){
        scene::glb::Map ramp;
        const Vec3 uphill{std::cos(.37f),std::sin(.37f),0},along{-uphill.y,uphill.x,0};
        const Vec3 origin{offset,offset,offset};
        const auto point=[&](float x,float y){return origin+uphill*x+along*y+Vec3{0,0,steepness*x};};
        quad(ramp,point(-5000,-5000),point(5000,-5000),point(5000,5000),point(-5000,5000));
        const auto trace=[&](Vec3 a,Vec3 b){return gameplay::movement::sweep(ramp,a,b,16,72);};
        const float support=steepness*16*(std::abs(uphill.x)+std::abs(uphill.y));
        State falling,steering,backward;falling.position=origin+Vec3{0,0,support+10};steering=backward=falling;
        Command neutral,steer;steer.forward=1;steer.yaw=std::atan2(along.y,along.x);
        Command reverse=steer;reverse.forward=-1;reverse.yaw+=scene::kPi;reverse.jump=true;
        for(int i=0;i<80;++i){
            tick(falling,neutral,10,Rules::HalfLife,trace);tick(steering,steer,10,Rules::HalfLife,trace);
            tick(backward,reverse,10,Rules::HalfLife,trace);
            CHECK(!falling.grounded&&!steering.grounded);
            CHECK(std::isfinite(scene::length(steering.velocity)));
            CHECK(scene::length(backward.position-steering.position)<.02f);
            CHECK(scene::length(backward.velocity-steering.velocity)<.02f);
        }
        CHECK(scene::dot(falling.velocity,uphill)<-100);
        CHECK(scene::dot(steering.position-falling.position,along)>10);
        CHECK(falling.position.z<origin.z+support-50);
    }
    // A rotated air wish retains the orthogonal velocity, rather than clamping
    // total speed. Forward and reverse trajectories use exactly the same math.
    for(float yaw:{0.f,.37f,2.1f}){
        Vec3 forward{250,0,0},backward=-forward;
        Command steering;steering.forward=1;steering.yaw=yaw;
        const auto velocityWish=wish(steering,Rules::HalfLife,false);
        for(int i=0;i<60;++i){
            accelerate(forward,velocityWish,.01f,Rules::HalfLife,false);
            accelerate(backward,-velocityWish,.01f,Rules::HalfLife,false);
            CHECK(scene::length(forward+backward)<.001f);
        }
    }
    using namespace gameplay::iw4switch;
    CHECK(raiseDurationSeconds(true,true,.12f,.4f)==.12f);
    for(float missing:{0.f,-1.f}){
        CHECK(raiseDurationSeconds(true,true,missing,.4f)==.4f);
        CHECK(raiseDurationSeconds(true,true,missing,missing)==.001f);
        CHECK(raiseDurationSeconds(false,true,missing,.4f)==missing);
        CHECK(raiseDurationSeconds(false,false,.12f,missing)==missing);
    }
    CHECK(raiseDurationSeconds(true,false,.12f,.4f)==.4f);
    CHECK(raiseDurationSeconds(true,false,.12f,0)==.001f);
    {
        FireCycle cycle;
        CHECK(!cycle.pending(10,0,12));
        cycle.shot(10,2,0,12);CHECK(cycle.pending(11,0,12));
        CHECK(!cycle.pending(12,0,12)); // Includes neither visual tail nor burst delay.
        cycle.shot(10,2,0,12);CHECK(!cycle.pending(11,1,12));
        cycle.shot(10,2,0,12);CHECK(!cycle.pending(11,0,13));
        cycle.shot(10,2,0,12);CHECK(!cycle.pending(9,0,12));
        cycle.shot(10,2,0,12);CHECK(!cycle.pending(11,0,12,false));
        cycle.shot(10,2,0,12);cycle.clear();CHECK(!cycle.pending(11,0,12));
        cycle.shot(10,-1,0,12);CHECK(!cycle.pending(10,0,12));
    }
    CHECK(request(0,1,0,true)==Action::BeginDrop);CHECK(request(0,0,1,true)==Action::CancelDrop);
    CHECK(request(0,2,1,true)==Action::RetargetDrop);CHECK(request(1,0,2,true)==Action::BeginDrop);
    CHECK(!admits(Busy::Fire,true));CHECK(admits(Busy::Fire,false));CHECK(admits(Busy::Reload,true));
    CHECK(admits(Busy::Rechamber,true));CHECK(!admits(Busy::Melee,false));CHECK(!admits(Busy::Offhand,false));
    // Full adapter-domain gate table from pinned iw4L check_for_change_admits:
    // weapon_time==0 bypasses delay, never melee/offhand; reload/rechamber
    // interrupt positive timers; firing cannot, even with zero delay.
    for(auto busy:{Busy::Ready,Busy::Fire,Busy::Reload,Busy::Rechamber,Busy::Melee,Busy::Offhand})
        for(bool time:{false,true})for(bool delay:{false,true}){
            const bool expected=busy==Busy::Melee||busy==Busy::Offhand?false:
                !time||busy==Busy::Reload||busy==Busy::Rechamber?true:busy!=Busy::Fire&&!delay;
            CHECK(admits(busy,time,delay)==expected);
        }
    for(int held=0;held<3;++held)for(int target=0;target<3;++target)for(int stage=0;stage<3;++stage){
        CHECK(request(held,target,stage,false)==Action::None);
        const auto expected=stage==1?(held==target?Action::CancelDrop:Action::RetargetDrop):
            held==target?Action::None:Action::BeginDrop;
        CHECK(request(held,target,stage,true)==expected);
    }
    std::cout<<"PASS reference movement: native latch/caps, held hops, reverse symmetry, ramp surfing, gravity, ceiling; IW4 switch gate matrix\n";
}
