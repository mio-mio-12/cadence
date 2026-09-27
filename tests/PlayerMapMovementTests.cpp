#include "gameplay/PlayerMapMovement.h"
#include "gameplay/PlayerTraversal.h"
#include "scene/C2MMap.h"
#include <iostream>
#include <stdexcept>
#include <sstream>
using scene::Vec3;
namespace movement=gameplay::movement;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void quad(scene::glb::Map& map,Vec3 a,Vec3 b,Vec3 c,Vec3 d){
    for(auto points:{std::array{a,b,c},std::array{a,c,d}}){scene::glb::CollisionTriangle t;t.a=points[0];t.b=points[1];t.c=points[2];t.normal=scene::normalize(scene::cross(t.b-t.a,t.c-t.a));t.minimum=t.maximum=t.a;
        for(auto p:points){t.minimum={std::min(t.minimum.x,p.x),std::min(t.minimum.y,p.y),std::min(t.minimum.z,p.z)};t.maximum={std::max(t.maximum.x,p.x),std::max(t.maximum.y,p.y),std::max(t.maximum.z,p.z)};}
        t.walkable=t.normal.z>=.7f;t.blocking=!t.walkable;map.collision.push_back(t);
    }
}
constexpr float radius=38.1f,height=182.88f,stepHeight=45.72f,dt=.008f;
void floor(scene::glb::Map& m,float z){quad(m,{-1000,-1000,z},{1000,-1000,z},{1000,1000,z},{-1000,1000,z});}
int main(int argc,char**argv)try{
    {
        namespace traversal=gameplay::traversal;
        scene::glb::Map m;floor(m,200);m.buildCollisionIndex();
        for(bool reverse:{false,true}){
            if(reverse){for(auto& t:m.collision){std::swap(t.b,t.c);t.normal=-t.normal;}m.buildCollisionIndex();}
            for(float h:{height,101.6f,50.8f})for(float travel:{50.f,500.f,1500.f}){
                const auto hit=traversal::ceiling(m,{0,0,200-h-10},{80,0,200-h-10+travel},radius,h);
                check(hit.fraction<1&&!hit.startSolid&&hit.end.z<=200-h+.01f,"head cap missed thin ceiling");
            }
            for(float dz:{0.f,-100.f}){
                const auto hit=traversal::ceiling(m,{0,0,200-height},{80,0,200-height+dz},radius,height);
                check(hit.fraction==1&&!hit.startSolid&&hit.end.x==80,"ceiling check changed walking/descent");
            }
        }
        check(!traversal::landingFromAbove(false,0,10),"airborne actor snapped onto overhead floor");
        check(traversal::landingFromAbove(false,11,10),"normal landing rejected");
        check(traversal::landingFromAbove(true,0,30),"grounded step support changed");
        check(traversal::lowAssist(50,150)&&!traversal::lowAssist(120,150),"low traversal threshold");
        scene::glb::Map ledge;quad(ledge,{70,-200,0},{70,-200,140},{70,200,140},{70,200,0});ledge.buildCollisionIndex();
        check(traversal::canPresentMantle(ledge,{0,0,0},{110,0,140},{1,0,0},150,radius,50),"near chest/eye grab rejected");
        check(!traversal::canPresentMantle(ledge,{-100,0,0},{110,0,140},{1,0,0},150,radius,50),"distant mantle accepted");
        check(!traversal::canPresentMantle(ledge,{0,0,0},{110,0,40},{1,0,0},150,radius,50),"low step got full mantle");
        check(!traversal::canPresentMantle(ledge,{0,0,0},{110,0,180},{1,0,0},150,radius,50),"above-eye mantle accepted");
        traversal::Settings tuning;
        check(tuning.minimumEyePercent==21.1f&&tuning.maximumEyePercent==122.8f&&tuning.reachIw==16.8f&&!tuning.airEdgeEnabled&&tuning.airEdgeIw==3,"approved traversal defaults changed");
        tuning.minimumEyePercent=72;tuning.maximumEyePercent=100;tuning.reachIw=20;
        for(float top:{1.f,30.f,60.f,100.f,107.9f})check(traversal::classify(ledge,{0,0,0},{110,0,top},{1,0,0},150,radius,tuning)==traversal::Decision::Below,"below-chest candidate entered mantle path");
        check(traversal::classify(ledge,{0,0,0},{110,0,140},{1,0,0},150,radius,tuning)==traversal::Decision::Eligible,"tunable nearby mantle rejected");
        tuning.reachIw=0;
        check(traversal::classify(ledge,{0,0,0},{110,0,140},{1,0,0},150,radius,tuning)==traversal::Decision::NoFace,"reach setting ignored");
        tuning.reachIw=20;tuning.minimumEyePercent=50;tuning.maximumEyePercent=80;
        check(traversal::classify(ledge,{0,0,0},{110,0,130},{1,0,0},150,radius,tuning)==traversal::Decision::Above,"maximum setting ignored");
        check(traversal::classify(ledge,{0,0,0},{110,0,100},{1,0,0},150,radius,tuning)==traversal::Decision::Eligible,"minimum setting ignored");
        std::stringstream serialized;serialized<<tuning;traversal::Settings copy;serialized>>copy;
        check(bool(serialized)&&copy==tuning,"traversal settings round trip");
        std::stringstream invalid("100 50 20 0 3");invalid>>copy;check(!invalid,"inverted traversal range accepted");
        scene::glb::Map lip;floor(lip,0);
        quad(lip,{50,-200,5},{200,-200,5},{200,200,5},{50,200,5});
        quad(lip,{50,-200,0},{50,-200,5},{50,200,5},{50,200,0});lip.buildCollisionIndex();
        const Vec3 start{10,0,0},wanted{20,0,1},blocked{10,0,1};
        check(!traversal::airEdge(lip,start,wanted,blocked,radius,height,0),"disabled/zero edge budget changed position");
        check(!traversal::airEdge(lip,start,{20,0,-1},blocked,radius,height,8),"descending edge lift");
        check(!traversal::airEdge(lip,start,wanted,wanted,radius,height,8),"unblocked jump got edge lift");
        const auto edge=traversal::airEdge(lip,start,wanted,blocked,radius,height,8);
        check(edge.has_value()&&edge->x==wanted.x&&edge->y==wanted.y&&edge->z-wanted.z<=8,"small edge clearance failed or changed horizontal travel");
        floor(lip,height+2);lip.buildCollisionIndex();
        check(!traversal::airEdge(lip,start,wanted,blocked,radius,height,8),"edge assist passed through ceiling");
    }
    check(movement::shouldCheckSupport(true,-16,100),"uphill floor clipping must retain support");
    check(!movement::shouldCheckSupport(false,300,300),"jump must not snap down");
    {
        scene::glb::Map m;quad(m,{-2000,-1000,-400},{2000,-1000,400},{2000,1000,400},{-2000,1000,-400});m.buildCollisionIndex();
        Vec3 p{0,0,10};bool grounded=false;float vertical=0;int lostGround=0;
        for(int tick=0;tick<500;++tick){
            Vec3 v{tick<20?0.f:200.f,0,vertical-2032*dt};const auto moved=movement::movePlayer(m,p,v,dt,radius,height,stepHeight,grounded);
            p=moved.position;vertical=moved.velocity.z;
            const auto support=movement::shouldCheckSupport(grounded,v.z,vertical)?movement::playerSupport(m,p,radius,height,grounded?stepHeight:.2f):std::optional<float>{};
            grounded=support.has_value();if(grounded){p.z=*support;vertical=0;}else if(tick>30)++lostGround;
        }
        check(lostGround==0,"walking uphill lost ground friction/support");
    }
    {
        scene::glb::Map m;floor(m,0);floor(m,200);m.buildCollisionIndex();
        std::vector<std::uint32_t> scratch{999999};
        for(float z:{0.f,19.f,201.f,500.f})for(float travel:{-600.f,0.f,600.f}){
            const Vec3 start{0,0,z},end{100,20,z+travel};
            const auto a=gameplay::mantle::sweepRounded(m,start,end,radius,height);
            const auto b=gameplay::mantle::sweepRounded(m,start,end,radius,height,&scratch);
            check(a.fraction==b.fraction&&a.startSolid==b.startSolid&&scene::length(a.end-b.end)==0&&scene::length(a.normal-b.normal)==0,"sweep scratch changed collision result");
        }
        for(float velocity:{500.f,1500.f,5000.f}){
            const auto hit=movement::movePlayer(m,{0,0,0},{0,0,velocity},.2f,radius,height,stepHeight,false);
            check(hit.position.z<=200-height+.01f&&hit.velocity.z<=.01f,"boost passed through ceiling");
        }
        // Both windings must catch a descending hull; no ground-height query
        // may choose the base terrain beneath the truck's thin floor.
        for(bool reverse:{false,true}){
            if(reverse){for(auto&t:m.collision){std::swap(t.b,t.c);t.normal=-t.normal;}m.buildCollisionIndex();}
            auto p=Vec3{0,0,201};bool grounded=false;Vec3 velocity{};
            for(int f=0;f<1000;++f){velocity.z-=2032*dt;auto moved=movement::movePlayer(m,p,velocity,dt,radius,height,stepHeight,grounded);p=moved.position;velocity=moved.velocity;const auto support=movement::playerSupport(m,p,radius,height,.2f);grounded=support.has_value();if(support){p.z=*support;velocity.z=0;}check(p.z>=199.99f,"fell through thin floor");}
        }
    }
    {
        scene::glb::Map m;floor(m,0);
        quad(m,{-500,-500,50},{500,-500,-50},{500,500,-50},{-500,500,50});m.buildCollisionIndex();
        auto hit=gameplay::mantle::sweepRounded(m,{0,0,50},{0,0,-10},radius,height);
        auto p=hit.end;
        for(int f=0;f<100;++f){auto moved=movement::movePlayer(m,p,{100,0,-11.5f},dt,radius,height,0,false);check(!moved.startSolid,"shallow slope overlap accumulated");p=moved.position;}
    }
    {
        scene::glb::Map m;floor(m,0);
        quad(m,{100,-500,0},{40,-500,180},{40,500,180},{100,500,0});m.buildCollisionIndex();
        Vec3 p{0,0,0},v{};
        for(int f=0;f<1000;++f){v.x=500;v.z-=2032*dt;auto moved=movement::movePlayer(m,p,v,dt,radius,height,stepHeight,true);p=moved.position;v=moved.velocity;check(p.z>=-.01f,"slanted wall pushed through floor");check(p.x<100,"passed through slanted wall");}
    }
    {
        scene::glb::Map m;floor(m,0);quad(m,{100,-500,0},{100,-500,30},{100,500,30},{100,500,0});quad(m,{100,-500,30},{500,-500,30},{500,500,30},{100,500,30});m.buildCollisionIndex();
        Vec3 p{0,0,0},v{};bool grounded=true;
        for(int f=0;f<90;++f){v={350,0,-2032*dt};auto moved=movement::movePlayer(m,p,v,dt,radius,height,stepHeight,grounded);p=moved.position;const auto support=movement::playerSupport(m,p,radius,height,stepHeight);grounded=support.has_value();if(support)p.z=*support;}
        if(!(p.x>180&&std::abs(p.z-30)<.1f))std::cerr<<"step end "<<p.x<<","<<p.y<<","<<p.z<<"\n";
        check(p.x>180&&std::abs(p.z-30)<.1f,"swept movement broke ordinary steps");
    }
    if(argc>1){
        scene::glb::Map map;std::string error;check(scene::c2m::load(argv[1],map,error),error.c_str());
        std::cout<<"Map triangles "<<map.collision.size()<<"\n";
        if(argc>2&&std::string(argv[2])=="--head-cap"){
            int checked=0;
            for(const auto marker:{Vec3{-875.3651f,532.3766f,-875.8204f},Vec3{-978.4861f,833.8252f,-497.6695f},Vec3{918.1605f,270.4745f,400.2932f},Vec3{-3071.9080f,949.4807f,89.3559f},Vec3{-3378.2847f,200.4255f,-333.4476f}}){
                const auto surface=map.raycastSurface(marker-Vec3{0,0,200},{0,0,1},500);
                if(!surface||std::abs(surface->normal.z)<.7f)continue;
                for(float h:{height,101.6f,50.8f}){
                    const Vec3 p{marker.x,marker.y,surface->position.z-h-10};
                    const auto hit=gameplay::traversal::ceiling(map,p,p+Vec3{0,0,400},radius,h);
                    check(hit.fraction<1&&hit.end.z+h<=surface->position.z+.02f,"actual map upward probe passed through surface");
                    ++checked;
                }
                const auto walking=gameplay::traversal::ceiling(map,marker,marker+Vec3{70,20,0},radius,height);
                const auto descending=gameplay::traversal::ceiling(map,marker,marker+Vec3{70,20,-15},radius,height);
                check(walking.fraction==1&&descending.fraction==1&&!walking.startSolid&&!descending.startSolid,"actual map walking/descent altered");
            }
            check(checked>=6,"insufficient actual-map ceiling probes");
            std::cout<<checked<<" actual-map upward head-cap probes passed; walking/descent bypass unchanged\n";
            return 0;
        }
        for(const auto marker:{Vec3{-1056.6576f,643.6118f,-954.0086f},Vec3{-1142.4418f,-1476.0251f,-969.8646f}}){
            const float ground=map.groundHeight(marker.x,marker.y,marker.z+200-45,-1.e20f);
            std::cout<<"Marker "<<marker.x<<","<<marker.y<<" floor="<<ground<<"\n";
            for(float h:{height,101.6f,50.8f}){
                const Vec3 p{marker.x,marker.y,ground+.04f};
                std::cout<<" hull height "<<h<<" solid="<<gameplay::mantle::sweepRounded(map,p,p,radius,h).startSolid<<"\n";
                if(h==height)for(const auto&t:map.collision)for(float z:{radius,h*.5f,h-radius}){const auto c=p+Vec3{0,0,z};const auto near=gameplay::mantle::closestOnTriangle(c,t.a,t.b,t.c);const float distance=scene::length(c-near);if(distance<radius-.025f)std::cout<<" overlap sample="<<z<<" distance="<<distance<<" nearest="<<near.x<<","<<near.y<<","<<near.z<<" normal="<<t.normal.x<<","<<t.normal.y<<","<<t.normal.z<<"\n";}
            }
            int valid=0;
            for(int direction=0;direction<8;++direction){
                Vec3 p{marker.x,marker.y,ground+.04f};Vec3 v{};bool grounded=true;
                if(gameplay::mantle::sweepRounded(map,p,p,radius,height).startSolid){
                    const auto support=gameplay::mantle::sweepRounded(map,p+Vec3{0,0,stepHeight},p,radius,height);
                    if(support.startSolid||support.fraction>=1||support.normal.z<.7f)continue;
                    p=support.end;
                }
                ++valid;
                int solid=0;
                for(int f=0;f<125;++f){const float angle=direction*scene::kPi/4;v={std::cos(angle)*100,std::sin(angle)*100,v.z-2032*dt};auto moved=movement::movePlayer(map,p,v,dt,radius,height,stepHeight,grounded);solid+=moved.startSolid;
                    if(direction==0&&f<5)std::cout<<" tick "<<f<<" from="<<p.x<<","<<p.y<<","<<p.z<<" to="<<moved.position.x<<","<<moved.position.y<<","<<moved.position.z<<" solid="<<moved.startSolid<<"\n";
                    p=moved.position;v=moved.velocity;auto support=movement::playerSupport(map,p,radius,height,grounded?stepHeight:.2f);grounded=support.has_value();if(support){p.z=*support;v.z=0;}}
                std::cout<<" direction "<<direction<<" end="<<p.x<<","<<p.y<<","<<p.z<<" grounded="<<grounded<<" solid="<<solid<<" velocityZ="<<v.z<<"\n";
                check(solid==0&&grounded,"Nuketown directional support probe failed");
            }
            std::cout<<" valid starts "<<valid<<"\n";
            check(valid==8,"Nuketown fixture could not place all directional probes");
        }
    }
    std::cout<<"Player sweep fixtures passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
