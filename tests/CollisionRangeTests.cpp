#include "scene/GlbMap.h"
#include "scene/C2MMap.h"
#include "gameplay/MovementTrace.h"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <random>

using scene::Vec3;
using scene::glb::Map;

static void require(bool value,const char* message){
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}
static bool same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
static bool same(Vec3 a,Vec3 b){return same(a.x,b.x)&&same(a.y,b.y)&&same(a.z,b.z);}
static bool same(const std::optional<Vec3>& a,const std::optional<Vec3>& b){return a.has_value()==b.has_value()&&(!a||same(*a,*b));}

static void add(Map& map,Vec3 a,Vec3 b,Vec3 c,bool walkable,bool blocking){
    scene::glb::CollisionTriangle t;
    t.a=a;t.b=b;t.c=c;t.normal=scene::normalize(scene::cross(b-a,c-a));
    t.minimum={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};
    t.maximum={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
    t.walkable=walkable;t.blocking=blocking;map.collision.push_back(t);
}

// Clear only the optional acceleration, retaining the exact pre-v91 grid and
// narrow-phase path. No approximate epsilon comparisons: require identical bits.
static void compare(Map accelerated,int count){
    auto linear=accelerated;
    linear.walkableRanges={};linear.blockingRanges={};
    // The sweep's unindexed path tests every triangle in original index order.
    linear.gridCollision.clear();
    std::mt19937 rng(910091);
    for(int i=0;i<count;++i){
        Vec3 p{float(int(rng()%10001)-5000),float(int(rng()%10001)-5000),float(int(rng()%300)-50)};
        if(!accelerated.collision.empty()){
            const auto& t=accelerated.collision[rng()%accelerated.collision.size()];
            if(i%3==0) p=(t.a+t.b+t.c)/3.0f;
            else if(i%3==1) p=t.a; // shared vertices, edges and exact-height ties
        }
        if(i%7==0){ // either side of a cell boundary / ground's padded bounds
            p.x=accelerated.boundsMinX+float(i%20)*accelerated.collisionCellSize+float(i%3-1);
        }
        p.z+=float(rng()%200);
        const Vec3 q=p+Vec3{float(int(rng()%501)-250),float(int(rng()%501)-250),float(int(rng()%101)-50)};
        if(i==count/2){accelerated.queryEpoch=linear.queryEpoch=std::numeric_limits<std::uint32_t>::max();}
        require(same(accelerated.groundHeight(p.x,p.y,p.z,-1e20f),linear.groundHeight(p.x,p.y,p.z,-1e20f)),"ground height mismatch");
        require(same(accelerated.navigationGroundHeight(p.x,p.y,p.z,-1e20f,22),linear.navigationGroundHeight(p.x,p.y,p.z,-1e20f,22)),"footprint support mismatch");
        require(accelerated.lineOfSight(p,q)==linear.lineOfSight(p,q),"line of sight mismatch");
        require(accelerated.navigationSegmentClear(p,q,38.1f,182.88f,45.72f)==linear.navigationSegmentClear(p,q,38.1f,182.88f,45.72f),"navigation link mismatch");
        Vec3 aNormal{3,4,5},bNormal=aNormal;
        require(same(accelerated.constrainMove(p,q,38.1f,182.88f,45.72f,&aNormal),linear.constrainMove(p,q,38.1f,182.88f,45.72f,&bNormal))&&same(aNormal,bNormal),"movement/contact order mismatch");
        require(same(accelerated.mantleTarget(p,q-p,38.1f,182.88f,45.72f,170,80,100),linear.mantleTarget(p,q-p,38.1f,182.88f,45.72f,170,80,100)),"mantle target mismatch");
        require(same(accelerated.raycastWalkable(p,q-p,600),linear.raycastWalkable(p,q-p,600)),"walkable ray mismatch");
        const auto a=accelerated.raycastSurface(p,q-p,600),b=linear.raycastSurface(p,q-p,600);
        require(a.has_value()==b.has_value()&&(!a||(same(a->position,b->position)&&same(a->normal,b->normal)&&same(a->distance,b->distance))),"surface ray mismatch");
        const auto sa=accelerated.findSurfContact(p,22,182.88f,4),sb=linear.findSurfContact(p,22,182.88f,4);
        require(sa.hit==sb.hit&&same(sa.normal,sb.normal)&&same(sa.distance,sb.distance)&&same(sa.separation,sb.separation),"surf contact mismatch");
        const auto sweepA=gameplay::movement::sweep(accelerated,p,q,22,182.88f),sweepB=gameplay::movement::sweep(linear,p,q,22,182.88f);
        require(sweepA.startSolid==sweepB.startSolid&&same(sweepA.fraction,sweepB.fraction)&&same(sweepA.end,sweepB.end)&&same(sweepA.normal,sweepB.normal),"swept hull/tie order mismatch");
    }
    std::cout<<count<<" points x 10 query types: bit-identical\n";
}

int main(int argc,char** argv){
    Map map;
    if(argc>1){
        std::string error;
        const auto path=std::filesystem::u8path(argv[1]);
        const bool loaded=path.extension()==".glb"?scene::glb::load(path,map,error):scene::c2m::load(path,map,error);
        require(loaded,error.c_str());
        compare(std::move(map),6000);
        return 0;
    }
    compare(map,40); // empty authored COLL
    add(map,{-20,-20,0},{20,-20,0},{20,20,0},true,false);
    map.buildCollisionIndex();compare(map,100); // linear small-list path
    map.collision.clear();map.collisionCellSize=128;
    for(int y=-15;y<=15;++y)for(int x=-15;x<=15;++x){
        const float px=float(x)*22,py=float(y)*22,z=float((x+y+30)%5)*12;
        add(map,{px,py,z},{px+24,py,z+2},{px+24,py+24,z+2},true,false);
        add(map,{px,py,z},{px+24,py+24,z+2},{px,py+24,z},true,false);
        if(x%3==0){
            add(map,{px,py,z},{px,py+24,z},{px+6,py+24,z+150},false,true);
            add(map,{px,py,z},{px+6,py+24,z+150},{px+6,py,z+150},false,true);
        }
    }
    // >1024-cell triangles must still be visited via global lists, including
    // outside the grid. Enough globals to exercise their own range hierarchy.
    for(int i=0;i<40;++i){
        const float z=float(i)*3-120;
        add(map,{-5000,-5000,z},{5000,-5000,z},{5000,5000,z},true,true);
    }
    map.buildCollisionIndex();
    require(!map.walkableRanges.nodes.empty()&&!map.blockingRanges.nodes.empty(),"dense ranges not built");
    require(map.walkableRanges.roots.back()!=std::numeric_limits<std::uint32_t>::max(),"global range not built");
    compare(map,5000);
    auto moved=std::move(map);compare(moved,100); // no cached pointers into an old Map
    moved.collision.erase(moved.collision.begin(),moved.collision.end()-1);
    moved.buildCollisionIndex();compare(moved,100);
    require(moved.walkableRanges.nodes.empty()&&moved.blockingRanges.nodes.empty(),"rebuild retained stale ranges");
    moved.collision.clear();moved.buildCollisionIndex();compare(moved,40);
    require(moved.walkableRanges.roots.empty()&&moved.blockingRanges.roots.empty(),"empty rebuild retained stale roots");
    std::cout<<"Collision range regression passed\n";
}
