#include "gameplay/BotSpTraversal.h"
#include <iostream>

struct TestMap {
    scene::Vec3 target{100,0,120};
    bool targetAvailable{true},blocked{},missingFloor{},invalidConstraint{};
    float floor{},ledgeBegin{-1e9f},ledgeEnd{1e9f};
    std::optional<scene::Vec3> mantleTarget(scene::Vec3,scene::Vec3,float,float,float,float,float,float)const{return targetAvailable?std::optional{target}:std::nullopt;}
    scene::Vec3 constrainMove(scene::Vec3 previous,scene::Vec3 next,float,float,float)const{return invalidConstraint?scene::Vec3{std::numeric_limits<float>::quiet_NaN(),0,0}:blocked?previous:next;}
    float navigationGroundHeight(float x,float,float,float,float)const{return missingFloor||x<ledgeBegin||x>ledgeEnd?-1e9f:floor;}
};
int main(){
    using namespace gameplay::bot;
    int failures=0;const auto check=[&](bool v,const char* msg){if(!v){++failures;std::cerr<<msg<<'\n';}};
    Actor a;a.spWantsMove=true;a.spMoveGoal={500,0,0};a.spMoveThrottle=1;a.velocity={300,0,0};
    struct DropMap {
        bool wall{},voidBelow{};
        scene::Vec3 constrainMove(scene::Vec3 old,scene::Vec3 p,float,float,float,scene::Vec3* normal)const{if(wall&&p.x>40){p.x=old.x;*normal={-1,0,0};}return p;}
        float navigationGroundHeight(float x,float,float reference,float fallback,float)const{const float floor=x<40?300.f:0.f;return voidBelow&&x>=40?fallback:floor<=reference+45?floor:fallback;}
    } drop;
    Actor ledge;ledge.position={0,0,300};
    const auto exit=findSpLedgeExit(drop,ledge,{500,0,0});
    check(exit.has_value(),"clear descending ledge jump rejected");
    if(exit){ledge.spLedgeTravel=exit->travel;ledge.spLedgeTime=exit->duration+.3f;ledge.input.jump=true;
        for(int i=0;i<180;++i){stepOnMap(ledge,1.f/60,1,drop);ledge.input.jump=false;if(ledge.grounded)break;}
        check(ledge.grounded&&ledge.position.z==0&&ledge.position.x>70,"validated ledge jump did not land through runtime physics");}
    ledge={};ledge.position={0,0,300};drop.wall=true;check(!findSpLedgeExit(drop,ledge,{500,0,0}),"wall blocked ledge exit accepted");
    drop.wall=false;drop.voidBelow=true;check(!findSpLedgeExit(drop,ledge,{500,0,0}),"unsupported ledge exit accepted");
    drop.voidBelow=false;check(!findSpLedgeExit(drop,ledge,{500,0,300}),"same level goal triggered descending jump");
    TestMap map;map.floor=120;
    check(obviousSpMantle(map,a).has_value(),"obvious forward grounded mantle rejected");
    map.target={-4,0,120};map.ledgeBegin=0;
    const auto supportedLip=obviousSpMantle(map,a);
    check(supportedLip&&supportedLip->x>=scene::course::kPlayerRadius,"mantle accepted centre/footprint outside landing lip");
    map.ledgeEnd=30;check(!obviousSpMantle(map,a),"mantle accepted narrow unsupported landing");
    map.ledgeBegin=-1e9f;map.ledgeEnd=1e9f;map.target={100,0,120};
    map.blocked=true;check(!obviousSpMantle(map,a),"blocked mantle arc accepted");map.blocked=false;
    map.missingFloor=true;check(!obviousSpMantle(map,a),"mantle without landing floor accepted");map.missingFloor=false;
    a.yaw=scene::kPi;check(!obviousSpMantle(map,a),"back-facing mantle accepted");a.yaw=0;
    a.spMoveGoal={-500,0,0};check(!obviousSpMantle(map,a),"off-route mantle accepted");a.spMoveGoal={500,0,0};
    a.reloadTime=1;check(!obviousSpMantle(map,a),"mantle overrides reload");a.reloadTime=0;
    a.input.fire=true;check(!obviousSpMantle(map,a),"mantle overrides firing");a.input.fire=false;
    map.target={200,0,120};check(!obviousSpMantle(map,a),"long speculative mantle accepted");map.target={100,0,120};
    const auto start=spMantlePosition({0,0,0},map.target,0),end=spMantlePosition({0,0,0},map.target,1),mid=spMantlePosition({0,0,0},map.target,.4f);
    check(scene::length(start)<.001f&&scene::length(end-map.target)<.001f,"mantle path endpoint incorrect");
    check(mid.x==0&&mid.z>100,"mantle moved forward before clearing lip");
    map.floor=0;check(clearSpJumpArc(map,a),"clear same-floor jump rejected");
    map.blocked=true;check(!clearSpJumpArc(map,a),"blocked jump accepted");map.blocked=false;
    map.missingFloor=true;check(!clearSpJumpArc(map,a),"jump into unsupported gap accepted");map.missingFloor=false;
    map.floor=100;check(!clearSpJumpArc(map,a),"jump speculative high landing accepted");map.floor=0;
    a.grounded=false;check(!clearSpJumpArc(map,a),"airborne jump approval retriggered");a.grounded=true;
    a.velocity={50,0,0};check(!clearSpJumpArc(map,a),"slow launch jump approved");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    a.velocity={300,0,0};map.floor=nan;check(!clearSpJumpArc(map,a)&&!obviousSpMantle(map,a),"nonfinite landing accepted");
    map.floor=120;map.target.x=nan;check(!obviousSpMantle(map,a),"nonfinite mantle target accepted");map.target={100,0,120};
    map.invalidConstraint=true;check(!obviousSpMantle(map,a)&&!clearSpJumpArc(map,a),"nonfinite collision response accepted");map.invalidConstraint=false;
    a.velocity.x=nan;check(!clearSpJumpArc(map,a),"nonfinite launch velocity accepted");a.velocity={300,0,0};
    a.spMoveGoal.x=nan;check(!obviousSpMantle(map,a),"nonfinite mantle intention accepted");a.spMoveGoal={500,0,0};
    check(scene::length(spMantlePosition({1,2,3},map.target,nan)-scene::Vec3{1,2,3})<.001f,"nonfinite mantle time poisoned position");
    if(!failures)std::cout<<"SP traversal facing/path/landing/action/launch safety PASS.\n";
    return failures?1:0;
}
