#include "scene/AuthoredGameplay.h"
#include "scene/C2MMap.h"
#include "scene/BoundedJson.h"
#include "gameplay/AuthoredTraversal.h"
#include "gameplay/BotActor.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace scene;
void check(bool b,const char* what){if(!b)throw std::runtime_error(what);}
authored::Volume box(authored::Kind kind,Vec3 low,Vec3 high){
    authored::Volume v;v.kind=kind;v.origin=v.minimum=low;v.maximum=high;v.center=(low+high)*.5f;
    v.edge[0]={high.x-low.x,0,0};v.edge[1]={0,high.y-low.y,0};v.edge[2]={0,0,high.z-low.z};
    v.reciprocal[0]={1/v.edge[0].x,0,0};v.reciprocal[1]={0,1/v.edge[1].y,0};v.reciprocal[2]={0,0,1/v.edge[2].z};v.facing={1,0,0};return v;
}
int main(int argc,char** argv){try{
    using Json=nlohmann::json;
    const auto temp=std::filesystem::temp_directory_path()/("cadence-v168-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp);
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{temp};
    Json corners=Json::array();for(int x:{0,1})for(int y:{0,1})for(int z:{0,1})corners.push_back({-10*x,20*y,50*z});
    Json item={{"id","fixture"},{"kind","LadderVolume"},{"enabled",true},{"hierarchyActive",true},{"forward",{0,0,1}},{"up",{1,0,0}},{"targetLadderId",nullptr},
        {"shapes",Json::array({{{"kind","BoxCollider"},{"enabled",true},{"hierarchyActive",true},{"worldCorners",corners}}})}};
    auto inactive=item;inactive["hierarchyActive"]=false;
    auto death=item;death["kind"]="DeathZoneVolume";
    Json document={{"schema","codm.gameplay-volumes/1"},{"units","inches"},{"coordinateSystem","RH_Z_UP"},{"sets",Json::array({{{"items",Json::array({item,inactive,death})}}})}};
    const auto save=[&](const Json& j){std::ofstream out(temp/"gameplay_volumes.json");out<<j.dump();};save(document);
    auto parsed=authored::load(temp);check(parsed.volumes.size()==1&&parsed.warnings.empty(),"active filtering / null linkage");check(parsed.volumes[0].overlaps({-12.7f,25.4f,63.5f},0,0),"mirrored affine box");
    check(std::abs(parsed.volumes[0].maximum.z-127)<.001f,"inches converted incorrectly");
    document["units"]="metres";save(document);parsed=authored::load(temp);check(parsed.volumes.empty()&&!parsed.warnings.empty(),"contradictory units accepted");
    {std::ofstream broken(temp/"gameplay_volumes.json");broken<<"{";}parsed=authored::load(temp);check(parsed.volumes.empty()&&!parsed.warnings.empty(),"malformed sidecar accepted");
    authored::Data d;d.volumes.push_back(box(authored::Kind::Crouch,{-20,-20,0},{20,20,80}));d.index();
    check(d.crouch({0,0,0},10,180),"crouch volume missed");check(!d.crouch({100,0,0},10,180),"crouch outside");
    d.volumes[0].kind=authored::Kind::Door;d.index();auto w=d.assistedWish({0,15,0},{100,0,0},10,180);check(w.y<0&&std::abs(length(w)-100)<.001f,"door steer/speed");
    check(length(d.assistedWish({0,15,0},{0,100,0},10,180)-Vec3{0,100,0})<.001f,"door alters sideways input");
    d.assistance=false;check(length(d.assistedWish({0,15,0},{100,0,0},10,180)-Vec3{100,0,0})<.001f,"assistance off");
    d.volumes[0].kind=authored::Kind::Mantle;check(!d.mantleDirections({-30,0,0},{1,0,0},10,180,60).empty(),"mantle direction absent");check(d.mantleDirections({-30,0,0},{-1,0,0},10,180,60).empty(),"mantle behind player");
    d.mantles=false;check(d.mantleDirections({-30,0,0},{1,0,0},10,180,60).empty(),"mantle toggle");
    glb::Map ledge;
    const auto tri=[&](Vec3 a,Vec3 b,Vec3 c){glb::CollisionTriangle t;t.a=a;t.b=b;t.c=c;t.normal=normalize(cross(b-a,c-a));t.walkable=t.normal.z>.5f;t.blocking=!t.walkable;t.minimum={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};t.maximum={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};ledge.collision.push_back(t);};
    tri({-300,-300,0},{0,-300,0},{0,300,0});tri({-300,-300,0},{0,300,0},{-300,300,0});
    tri({0,-300,100},{300,-300,100},{300,300,100});tri({0,-300,100},{300,300,100},{0,300,100});
    tri({0,-300,0},{0,-300,100},{0,300,100});tri({0,-300,0},{0,300,100},{0,300,0});ledge.buildCollisionIndex();
    ledge.gameplay.climbs.push_back({{-65,0,0},{65,0,100}});
    check(gameplay::authored::climb(ledge,{-65,0,0},{1,0,0},38.1f,182.88f,45.72f,145,110,60,1,false).has_value(),"authored endpoint mantle missing");
    check(!gameplay::authored::climb(ledge,{-65,0,0},{-1,0,0},38.1f,182.88f,45.72f,145,110,60,1,false),"authored mantle behind view");
    ledge.gameplay.mantles=false;
    check(!gameplay::authored::climb(ledge,{-65,0,0},{1,0,0},38.1f,182.88f,45.72f,145,110,60,1,false),"authored mantle toggle ignored");
    check(gameplay::mantle::findClimb(ledge,{-65,0,0},{1,0,0},38.1f,182.88f,45.72f,145,110,60).has_value(),"automatic mantle disabled by authored toggle");
    glb::Map map;map.gameplay.volumes.push_back(box(authored::Kind::Ladder,{-15,-30,0},{15,30,300}));map.gameplay.index();
    gameplay::authored::LadderState s;Vec3 p{20,0,0},v{};bool grounded=true;
    check(gameplay::authored::ladder(s,map,p,v,grounded,{-1,0,0},1,false,.02f,10,100)&&s.active&&p.z>0,"ladder ascent");
    const float z=p.z;gameplay::authored::ladder(s,map,p,v,grounded,{-1,0,0},0,false,.02f,10,100);check(p.z==z,"ladder hold drifts");
    gameplay::authored::ladder(s,map,p,v,grounded,{-1,0,0},1,true,.02f,10,100);check(!s.active&&v.z>0&&v.x>0,"ladder jump off");
    check(!gameplay::authored::ladder(s,map,p,v,grounded,{-1,0,0},1,false,.01f,10,100),"ladder instantly regrabs");
    gameplay::bot::Actor a;gameplay::bot::BehaviorConfig c;std::vector<authored::Destination> goals={{{1000,0,0},1,"BOTNaviSpot"},{{0,1000,0},1,"CampSpot"}};
    check(!gameplay::bot::mapPatrolDestination(a,c),"destinations default on");c.mapDestinations=&goals;auto goal=gameplay::bot::mapPatrolDestination(a,c);check(goal.has_value(),"no bot destination");check(length(*goal-*gameplay::bot::mapPatrolDestination(a,c))<.001f,"goal rerolled each frame");a.position=*goal;check(length(*goal-*gameplay::bot::mapPatrolDestination(a,c))>100,"arrival does not advance");
    if(argc>=2){std::size_t maps=0,ladders=0;for(const auto& f:std::filesystem::directory_iterator(argv[1]))if(f.is_directory()){
        auto x=authored::load(f.path());std::size_t n=0;for(const auto& volume:x.volumes){if(volume.kind==authored::Kind::Ladder)++n;check(volume.overlaps(volume.center,0,0),"actual volume center outside");}
        std::cout<<f.path().filename().string()<<": volumes="<<x.volumes.size()<<" ladders="<<n<<" links="<<x.climbs.size()<<" destinations="<<x.destinations.size()<<" warnings="<<x.warnings.size()<<"\n";
        for(const auto& warning:x.warnings)std::cout<<warning<<"\n";
        auto twice=authored::load(f.path(),2);check(twice.volumes.size()==x.volumes.size(),"scale changes record count");if(!x.volumes.empty())check(length(twice.volumes[0].center-x.volumes[0].center*2)<.01f,"scale not once");
        ladders+=n;++maps;
    }check(maps>=9&&ladders>=9,"installed active ladder inventory missing");}
    if(argc>=3){glb::Map real;std::string error;check(c2m::load(argv[2],real,error),error.c_str());real.gameplay=authored::load(std::filesystem::path(argv[2]).parent_path());
        std::cout<<"Collision triangles="<<real.collision.size()<<" authored="<<real.authoredCollision.present<<"\n";
        unsigned passed=0,total=0;
        for(const auto& volume:real.gameplay.volumes)if(volume.kind==authored::Kind::Ladder){++total;bool success=false;
            for(float sign:{1.f,-1.f})for(float offset:{20.f,35.f,50.f,65.f}){
                const auto normal=volume.facing*sign;Vec3 start=volume.center+normal*offset;start.z=volume.minimum.z;
                const float ground=real.navigationGroundHeight(start.x,start.y,start.z+60-45,-1e9f,15);
                std::cout<<volume.id<<" side="<<sign<<" offset="<<offset<<" bottom="<<start.z<<" ground="<<ground;
                if(std::abs(ground-start.z)>250){std::cout<<" ground rejected\n";continue;}start.z=ground+.05f;
                if(gameplay::mantle::sweepRounded(real,start,start,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72)).startSolid){std::cout<<" solid\n";continue;}
                gameplay::authored::LadderState state;Vec3 pos=start,vel{};bool onGround=true,entered=false,exited=false;
                for(int tick=0;tick<1000;++tick){bool own=gameplay::authored::ladder(state,real,pos,vel,onGround,-normal,1,false,.008f,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72));entered|=state.active;if(entered&&!state.active&&onGround){exited=true;break;}if(!own)break;}
                std::cout<<" entered="<<entered<<" climbed="<<pos.z-start.z<<" exit="<<exited<<"\n";
                if(entered&&!exited){for(float distance:{88.f,126.f,164.f,202.f}){auto target=pos-normal*distance;target.z=real.navigationGroundHeight(target.x,target.y,volume.maximum.z+40-45.f,-1e9f,scene::course::kPlayerRadius*.7f);auto path=gameplay::mantle::findClimbTo(real,pos,target,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight,gameplay::iw::worldUnits(72)*.8f,240,-scene::course::kStepHeight-8);std::cout<<"  exit dist="<<distance<<" feet="<<pos.z<<" ground="<<target.z<<" path="<<bool(path)<<"\n"; if(distance==126){for(float lift:{0.f,24.f,48.f,80.f}){auto high=pos;high.z=std::max(pos.z,target.z)+lift;auto over=target;over.z=high.z;auto up=gameplay::mantle::sweepRounded(real,pos,high,38.1f,182.88f);auto across=gameplay::mantle::sweepRounded(real,high,over,38.1f,182.88f);auto down=gameplay::mantle::sweepRounded(real,over,target+Vec3{0,0,.04f},38.1f,182.88f);std::cout<<"    lift="<<lift<<" up="<<up.fraction<<"/"<<up.startSolid<<" across="<<across.fraction<<"/"<<across.startSolid<<" down="<<down.fraction<<"/"<<down.startSolid<<"\n";}}}}
                if(entered&&pos.z>start.z+50&&exited){std::cout<<volume.id<<" climbed "<<pos.z-start.z<<" cm; exit="<<exited<<" from "<<start.x<<","<<start.y<<","<<start.z<<"\n";success=true;break;}
            }
            if(success)++passed;else std::cout<<volume.id<<" no valid climbing approach found\n";
        }std::cout<<"Actual collision ladder ascent "<<passed<<"/"<<total<<"\n";check(total>0&&passed==total,"actual map ladder ascent failed");
    }
    std::cout<<"Authored gameplay checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
