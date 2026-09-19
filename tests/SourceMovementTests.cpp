#include "gameplay/SourceMovement.h"
#include "gameplay/AutoStrafe.h"
#include "gameplay/MantleTrajectory.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
using scene::Vec3;
using namespace gameplay::source_v2;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void near(float a,float b,float tolerance,const char* message){require(std::abs(a-b)<=tolerance,message);}
void triangle(scene::glb::Map& m,Vec3 a,Vec3 b,Vec3 c) {
    scene::glb::CollisionTriangle t;t.a=a;t.b=b;t.c=c;t.normal=scene::normalize(scene::cross(b-a,c-a));
    t.minimum={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};
    t.maximum={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
    t.walkable=t.normal.z>=0.7f;t.blocking=!t.walkable;m.collision.push_back(t);
}
void quad(scene::glb::Map& m,Vec3 a,Vec3 b,Vec3 c,Vec3 d){triangle(m,a,b,c);triangle(m,a,c,d);}
void floor(scene::glb::Map& m){quad(m,{-10000,-10000,0},{10000,-10000,0},{10000,10000,0},{-10000,10000,0});}
void box(scene::glb::Map& m,float x,float depth,float z,float width=1000) {
    quad(m,{x,-width,0},{x,-width,z},{x,width,z},{x,width,0});
    quad(m,{x,-width,z},{x+depth,-width,z},{x+depth,width,z},{x,width,z});
    quad(m,{x+depth,-width,z},{x+depth,-width,0},{x+depth,width,0},{x+depth,width,z});
}
int main(int argc,char** argv) try {
    for(float speed:{units(30),units(250)}){
        const auto normal=gameplay::mantle::plan({}, {units(100),0,units(40)}, {speed,0,0},{1,0,0});
        for(float rate:{.25f,1.0f,3.0f}){
            const auto scaled=gameplay::mantle::plan({}, {units(100),0,units(40)}, {speed,0,0},{1,0,0},rate);
            near(scaled.duration,normal.duration/rate,1e-5f,"mantle speed scales traversal duration");
            for(float phase:{0.0f,.25f,.5f,1.0f}){
                const auto a=normal.sample(normal.duration*phase),b=scaled.sample(scaled.duration*phase);
                near(scene::length(a.position-b.position),0,.002f,"mantle speed preserves path and endpoint");
                near(scene::length(a.velocity*rate-b.velocity),0,.01f,"mantle speed scales derivative coherently");
            }
        }
    }
    Settings cfg;scene::glb::Map world;floor(world);
    if(argc>2&&std::string(argv[1])=="--map-surf") {
        std::string error;require(scene::glb::load(argv[2],world,error),error.c_str());
        auto mapTrace=[&](Vec3 a,Vec3 b){return gameplay::movement::sweep(world,a,b,cfg.radius,cfg.height);};
        int sampled=0;
        for(std::size_t index=0;index<world.collision.size()&&sampled<30;++index){
            const auto& t=world.collision[index];
            if(t.normal.z<0.1f||t.normal.z>=0.7f||scene::length(scene::cross(t.b-t.a,t.c-t.a))<10000)continue;
            const Vec3 center=(t.a+t.b+t.c)/3.0f;
            const float support=cfg.radius*(std::abs(t.normal.x)+std::abs(t.normal.y))/t.normal.z;
            const auto entry=mapTrace(center+Vec3{0,0,support+units(10)},center+Vec3{0,0,support-units(2)});
            if(entry.startSolid||entry.fraction>=1||scene::dot(entry.normal,t.normal)<0.99f)continue;
            State state{entry.end,scene::normalize(scene::cross(t.normal,Vec3{0,0,1}))*units(250),false};
            const auto start=state.position;
            const auto capsule=world.findSurfContact(start,cfg.radius,cfg.height,4);
            int grounded=0,stopped=0,mantles=0,clearMantles=0;
            const auto facing=scene::normalize(Vec3{-t.normal.x,-t.normal.y,0});
            for(int i=0;i<45;++i){
                if(const auto target=world.mantleTarget(state.position,facing,cfg.radius,cfg.height,units(18),units(57),units(42))){
                    ++mantles;
                    if(gameplay::mantle::clear(gameplay::mantle::plan(state.position,*target,state.velocity,facing),mapTrace))++clearMantles;
                }
                tick(state,{},true,cfg,mapTrace);grounded+=state.grounded;
                if(scene::length(state.velocity)<units(1))++stopped;
            }
            std::cout<<"ramp="<<index<<" normal="<<t.normal.x<<','<<t.normal.y<<','<<t.normal.z
                <<" start="<<start.x<<','<<start.y<<','<<start.z<<" moved="<<scene::length(state.position-start)
                <<" speed="<<scene::length(state.velocity)/scale()<<" grounded="<<grounded<<" stopped="<<stopped
                <<" capsule="<<capsule.hit<<" mantleCandidates="<<mantles<<" clearMantles="<<clearMantles<<'\n';++sampled;
        }
        require(sampled>0,"actual map contains accessible surf ramps");return 0;
    }
    require(cfg.autoJump,"Source v2 defaults to hold-to-bhop");
    auto trace=[&](Vec3 a,Vec3 b){return gameplay::movement::sweep(world,a,b,cfg.radius,cfg.height);};
    near(scale(),2.54f,0.0001f,"uniform geometry/velocity scale");
    Vec3 v{units(250),0,0};accelerate(v,{0,1,0},units(250),0.015f,true);
    near(v.x,units(250),0.001f,"air strafe preserves forward momentum");
    near(v.y,units(30),0.001f,"air wish cap");
    v={};accelerate(v,{1,0,0},units(250),0.001f,true);
    near(v.x,units(2.5f),0.001f,"uncapped wishspeed acceleration term");
    State stockAir{{0,0,units(1000)},{-units(100),0,0},false},surfAir=stockAir;
    Settings surfSettings=cfg;surfSettings.airAccelerate=150;
    tick(stockAir,{cfg.playerSpeed,0,0},false,cfg,trace);
    tick(surfAir,{cfg.playerSpeed,0,0},false,surfSettings,trace);
    near(stockAir.velocity.x,-units(100)+kCssAirAccelerate*cfg.playerSpeed*static_cast<float>(tickSeconds),0.001f,"stock air acceleration remains 10");
    near(surfAir.velocity.x,units(30),0.001f,"surf air acceleration increases control but retains projection cap");
    near(surfAir.velocity.z,stockAir.velocity.z,0.001f,"surf air setting leaves gravity unchanged");
    {float sign=1;const auto left=autoStrafeWish({units(250),0,0},{},0,cfg.playerSpeed,sign,kCssAirAccelerate,.01f);
     const auto right=autoStrafeWish({units(250),0,0},{},0,cfg.playerSpeed,sign,kCssAirAccelerate,-.01f);
     require(left.y>0&&right.y<0,"mouse view movement steers autostrafe without directional keys");}
    // The assist supplies wish input, never a velocity assignment. Compare
    // forward/backward trajectories through the actual air accelerator.
    for(bool directionalIntent:{false,true}) {
        Vec3 forwardVelocity{units(250),0,0},backwardVelocity{-units(250),0,0};
        float forwardSign=1,backwardSign=1;
        for(int i=0;i<240;++i) {
            const float previousSpeed=scene::length(forwardVelocity);
            const Vec3 desired=directionalIntent?Vec3{cfg.playerSpeed,0,0}:Vec3{};
            const auto fw=autoStrafeWish(forwardVelocity,desired,0,cfg.playerSpeed,forwardSign);
            const auto bw=autoStrafeWish(backwardVelocity,-desired,0,cfg.playerSpeed,backwardSign);
            near(scene::length(fw),cfg.playerSpeed,0.001f,"autostrafe stays within configured wish speed");
            accelerate(forwardVelocity,fw,scene::length(fw),static_cast<float>(tickSeconds),true);
            accelerate(backwardVelocity,bw,scene::length(bw),static_cast<float>(tickSeconds),true);
            require(scene::length(forwardVelocity)>=previousSpeed-0.001f,"autostrafe never brakes air speed");
            near(scene::length(forwardVelocity+backwardVelocity),0,0.04f,"forward/backhop assist symmetry");
            require(std::abs(std::atan2(forwardVelocity.y,forwardVelocity.x))<0.15f,
                    "autostrafe heading remains stable without a turn request");
        }
        require(scene::length(forwardVelocity)>units(350),"autostrafe gains speed through air acceleration");
    }
    // Independent double-precision recurrence in Source units, converted only
    // at comparison. This checks equations, not a running CSS executable.
    Settings referenceSettings=cfg;referenceSettings.playerSpeed=units(250);
    State a;a.grounded=true;double rv=0,rp=0;
    for(int i=0;i<200;++i){
        const double wish=i<100?250:0;
        rv=std::max(0.0,rv-std::max(rv,75.0)*4*0.015);
        rv+=std::max(0.0,std::min(wish-rv,5*wish*0.015));rp+=rv*0.015;
        tick(a,{units(static_cast<float>(wish)),0,0},false,referenceSettings,trace);
        near(a.velocity.x,units(static_cast<float>(rv)),0.02f,"reference friction/acceleration recurrence");
        near(a.position.x,units(static_cast<float>(rp)),0.08f,"reference position recurrence");
    }
    near(a.velocity.x,0,0.001f,"ground stop");
    State f{{0,0,0},{units(250),0,0},true},b{{0,1000,0},{-units(250),0,0},true};
    int forwardJumps=0;bool lastGround=true;
    for(int i=0;i<200;++i){
        const bool jump=(i%48)==0;
        tick(f,{},jump,cfg,trace);tick(b,{},jump,cfg,trace);
        near(f.velocity.x,-b.velocity.x,0.001f,"backhop has no special boost");
        near(f.position.x,-b.position.x,0.005f,"forward/backward trajectory symmetry");
        if(lastGround&&!f.grounded)++forwardJumps;lastGround=f.grounded;
    }
    require(forwardJumps>1,"repeated timed hops");
    cfg.autoJump=false;State held;held.grounded=true;int jumps=0;lastGround=true;
    for(int i=0;i<150;++i){tick(held,{},true,cfg,trace);if(lastGround&&!held.grounded)++jumps;lastGround=held.grounded;}
    require(jumps==1,"ordinary mode requires jump release");
    cfg.autoJump=true;held={};held.grounded=true;jumps=0;lastGround=true;
    for(int i=0;i<150;++i){tick(held,{},true,cfg,trace);if(lastGround&&!held.grounded)++jumps;lastGround=held.grounded;}
    require(jumps>=3,"auto-jump extension");cfg.autoJump=false;
    auto frames=[&](int fps){State state;double accumulator=0;int ticks=0;
        for(int frame=0;frame<fps*3;++frame){accumulator+=1.0/fps;while(accumulator+1e-10>=tickSeconds){
            accumulator-=tickSeconds;tick(state,wishVelocity(ticks<100?1.0f:-1.0f,0.5f,0.3f,cfg.playerSpeed),ticks%50==0,cfg,trace);++ticks;}}
        require(ticks==200,"fixed tick count");return state;};
    const auto low=frames(30),high=frames(144);near(scene::length(low.position-high.position),0,0.001f,"render FPS independence");
    // Thin geometry and ceiling: a long sweep must not tunnel.
    box(world,300,1,500);
    auto hit=trace({0,0,0},{1000,0,0});near(hit.end.x,300-cfg.radius,0.01f,"thin wall sweep");
    auto clipped=gameplay::movement::clip({100,50,0},hit.normal);near(clipped.x,0,0.001f,"wall clip");near(clipped.y,50,0.001f,"wall tangential motion");
    world.collision.clear();floor(world);box(world,150,150,units(16));
    State stepper;for(int i=0;i<75;++i)tick(stepper,{cfg.playerSpeed,0,0},false,cfg,trace);
    require(stepper.position.x>300,"step traversal");
    world.collision.clear();floor(world);box(world,150,150,units(24));
    stepper={};for(int i=0;i<50;++i)tick(stepper,{cfg.playerSpeed,0,0},false,cfg,trace);
    require(stepper.position.x<=150-cfg.radius+0.1f,"tall step blocks");
    world.collision.clear();floor(world);
    quad(world,{0,-1000,0},{500,-1000,250},{500,1000,250},{0,1000,0});
    State climber{{-100,0,0},{},true};
    for(int i=0;i<50;++i)tick(climber,{cfg.playerSpeed,0,0},false,cfg,trace);
    require(climber.position.z>100&&climber.grounded,"walkable slope follows ground");
    world.collision.clear();floor(world);
    quad(world,{100,-1000,0},{100,-1000,500},{100,1000,500},{100,1000,0});
    quad(world,{-1000,100,0},{1000,100,0},{1000,100,500},{-1000,100,500});
    State corner{{0,0,0},{1000,1000,0},true};slide(corner,0.2f,trace);
    require(corner.position.x<=100-cfg.radius+0.1f&&corner.position.y<=100-cfg.radius+0.1f,"two-plane corner collision");
    near(scene::length(corner.velocity),0,0.001f,"two-plane corner stops inward velocity");
    // Steep plane remains airborne; clipping retains downslope motion.
    const auto steep=scene::normalize(Vec3{-1,0,0.5f});
    const auto surf=gameplay::movement::clip({0,0,-100},steep);
    near(scene::dot(surf,steep),0,0.001f,"surf plane clipping");require(surf.z<0,"surf retains downhill velocity");
    // Exercise the actual swept hull repeatedly on a triangulated surf ramp,
    // including exported maps well away from the coordinate origin.
    for(float offset:{0.0f,20000.0f}) {
        world.collision.clear();
        quad(world,{offset-2000,-2000,-4000},{offset+2000,-2000,4000},
                   {offset+2000,2000,4000},{offset-2000,2000,-4000});
        State surfer{{offset, -400, cfg.radius*2+0.01f},{0,units(250),-units(100)},false};
        const auto start=surfer.position;
        for(int i=0;i<60;++i) {
            tick(surfer,{},false,cfg,trace);
            require(!surfer.grounded,"steep ramp never becomes walking ground");
            require(std::hypot(surfer.velocity.x,surfer.velocity.y)>units(200),"surf ramp preserves tangential speed");
        }
        require(surfer.position.y>start.y+units(200),"surf traverses triangle seam");
        require(surfer.position.z<start.z-units(100),"surf travels downslope");
    }
    // Mid-air entry onto a non-axis-aligned ramp: gravity must accelerate
    // downhill and air input must steer along the ramp without grounding.
    for(float offset:{0.0f,20000.0f})for(float slope:{1.1f,2.0f}) {
        world.collision.clear();
        const Vec3 uphill{std::cos(0.37f),std::sin(0.37f),0};
        const Vec3 along{-uphill.y,uphill.x,0},origin{offset,offset,offset};
        const auto point=[&](float x,float y){return origin+uphill*x+along*y+Vec3{0,0,slope*x};};
        quad(world,point(-5000,-5000),point(5000,-5000),point(5000,5000),point(-5000,5000));
        const float support=slope*cfg.radius*(std::abs(uphill.x)+std::abs(uphill.y));
        State falling{origin+Vec3{0,0,support+units(10)}, {},false},steering=falling;
        const Vec3 normal=scene::normalize(Vec3{-slope*uphill.x,-slope*uphill.y,1});
        for(int i=0;i<45;++i) {
            tick(falling,{},false,cfg,trace);
            tick(steering,along*cfg.playerSpeed,false,cfg,trace);
            require(!falling.grounded&&!steering.grounded,"midair surf stays off walkable ground");
        }
        require(scene::dot(falling.velocity,uphill)<-units(100),"gravity accelerates real ramp downhill");
        require(scene::dot(steering.position-falling.position,along)>units(10),"air input steers on real surf ramp");
        near(scene::dot(falling.velocity,normal),-gravity()*static_cast<float>(tickSeconds)*0.5f*normal.z,
             0.15f,"surf clips incoming velocity before final half gravity");
        require(surfContact(falling.position,falling.velocity,trace),"runtime surf predicate recognizes actual steep hull contact");
        require(!surfContact(falling.position,normal*units(50),trace),"leaving ramp releases surf priority immediately");
        require(!surfContact(falling.position+Vec3{0,0,units(10)},falling.velocity,trace),"off-ramp air movement retains mantle and assist eligibility");
    }
    // A jump brushing the near edge must keep its upward velocity, then move
    // across the top once the feet clear it. Test exported-map coordinates too.
    for(float offset:{0.0f,20000.0f}) {
        world.collision.clear();
        quad(world,{offset-2000,-2000,0},{offset+2000,-2000,0},
                   {offset+2000,2000,0},{offset-2000,2000,0});
        box(world,offset+units(40),units(30),units(24));
        State jumper{{offset+units(40)-cfg.radius-units(1),0,0},{cfg.playerSpeed,0,0},true};
        tick(jumper,{cfg.playerSpeed,0,0},true,cfg,trace);
        require(jumper.velocity.z>units(200),"jumping edge contact preserves upward velocity");
        for(int i=0;i<35;++i)tick(jumper,{cfg.playerSpeed,0,0},false,cfg,trace);
        require(jumper.position.x>offset+units(30)&&jumper.position.z>=units(24),
                "jump clears ledge without sticking to triangle edge");
    }
    world.collision.clear();floor(world);box(world,units(60),units(80),units(40));
    world.buildCollisionIndex();
    const auto selected=world.mantleTarget({}, {1,0,0},cfg.radius,cfg.height,cfg.stepHeight,units(57),units(75));
    require(selected.has_value(),"runtime ledge selection");
    const auto selectedPath=gameplay::mantle::plan({},*selected,{units(250),0,0},{1,0,0});
    require(gameplay::mantle::clear(selectedPath,trace),"runtime selected mantle clearance");
    const Vec3 airborneLedgeApproach{units(10),0,units(10)};
    require(!surfContact(airborneLedgeApproach,{units(150),0,0},trace),"airborne ledge approach is not misclassified as surfing");
    const auto airborneTarget=world.mantleTarget(airborneLedgeApproach,{1,0,0},cfg.radius,cfg.height,cfg.stepHeight,units(57),units(75));
    require(airborneTarget.has_value(),"intentional airborne ledge mantle remains available");
    const auto minimumTarget=[&](Vec3 position,float minimum){return world.mantleTarget(position,{1,0,0},cfg.radius,cfg.height,cfg.stepHeight,units(57),units(75),minimum);};
    require(!minimumTarget({},units(41)),"grounded mantle below configured minimum is rejected");
    require(minimumTarget({},units(40)).has_value(),"grounded mantle at configured minimum is accepted");
    require(minimumTarget({},units(39)).has_value(),"grounded mantle above configured minimum is accepted");
    require(!minimumTarget(airborneLedgeApproach,units(31)),"airborne mantle below configured minimum is rejected");
    require(minimumTarget(airborneLedgeApproach,units(30)).has_value(),"airborne mantle at configured minimum is accepted");
    require(minimumTarget(airborneLedgeApproach,units(29)).has_value(),"airborne mantle above configured minimum is accepted");
    require(!world.mantleTarget({}, {1,0,0},cfg.radius,cfg.height,cfg.stepHeight,units(39),units(75),units(20)),
            "minimum mantle setting never overrides maximum height");
    require(gameplay::mantle::clear(gameplay::mantle::plan(airborneLedgeApproach,*airborneTarget,{units(150),0,0},{1,0,0}),trace),
            "airborne ledge mantle has clear hull trajectory");
    // Disable the index before constructing the next independent fixture.
    world.gridWidth=world.gridHeight=0;
    std::ofstream csv;if(argc>1){csv.open(argv[1]);csv<<"case,time,x,y,z,vx,vy,vz\n";}
    for(float height:{units(24),units(40),units(56)})for(float speed:{units(25),units(250)})for(float side:{0.0f,units(60)}) {
        world.collision.clear();floor(world);box(world,units(80),units(20),height);
        const auto path=gameplay::mantle::plan({0,0,0},{units(100),0,height},{speed,side,0},{1,0,0});
        require(gameplay::mantle::clear(path,trace),"planned mantle obstacle clearance");
        const auto first=path.sample(0),last=path.sample(path.duration);
        near(first.position.z,0,0.001f,"no mantle entry snap");
        near(first.velocity.x,speed,0.001f,"no horizontal entry velocity discontinuity");
        if(speed>units(200)){near(last.velocity.x,speed,0.001f,"vault exit speed");near(last.velocity.y,side,0.001f,"oblique momentum");}
        for(int i=0;i<=100;++i){const float t=path.duration*i/100;const auto p=path.sample(t);
            if(csv)csv<<(speed>units(200)?"fast":"slow")<<'_'<<height/scale()<<'_'<<side/scale()<<','<<t<<','<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','<<p.velocity.x<<','<<p.velocity.y<<','<<p.velocity.z<<'\n';}
        // Full-hull ceiling intersects the vertical lift, not just a foot ray.
        quad(world,{-1000,-1000,cfg.height+10},{1000,-1000,cfg.height+10},{1000,1000,cfg.height+10},{-1000,1000,cfg.height+10});
        require(!gameplay::mantle::clear(path,trace),"blocked overhead rejected");
    }
    world.collision.clear();floor(world);box(world,units(80),units(20),units(40));
    const auto fastPath=gameplay::mantle::plan({}, {units(100),0,units(40)}, {units(250),0,0},{1,0,0});
    const auto handoff=fastPath.sample(fastPath.duration);
    State exit{handoff.position,handoff.velocity,false};
    box(world,units(150),units(1),units(200));
    for(int i=0;i<20;++i)tick(exit,{cfg.playerSpeed,0,0},false,cfg,trace);
    require(exit.position.x<=units(150)-cfg.radius+0.1f,"post-vault obstacle blocks without tunneling");
    std::cout<<"Source movement and mantle checks passed (active in Release).\n";return 0;
} catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
