#pragma once
#include "gameplay/BotActor.h"
#include <sstream>
#include <istream>

namespace gameplay::bot {
// CASTNAV is written by Cadence in scene centimetres. Unlike CODM JSON,
// coordinates here must not receive an inches conversion or import scale.
struct CastNavigationSidecar { NavigationGraph graph;std::vector<scene::Vec3> spawns; };
inline bool readCastNavigation(std::istream& input,CastNavigationSidecar& output,std::string& error){
    CastNavigationSidecar parsed;std::string line;int version{};std::size_t lines{},bytes{},pointsExpected{};bool boundarySeen=false;
    auto fail=[&](const char* why){error="CASTNAV line "+std::to_string(lines)+": "+why;return false;};
    if(!std::getline(input,line))return fail("missing header");++lines;
    {std::istringstream row(line);std::string signature,extra;if(!(row>>signature>>version)||signature!="CASTNAV"||version<1||version>5||(row>>extra))return fail("unsupported header");}
    const auto finite=[](float value){return std::isfinite(value)&&std::abs(value)<=1e8f;};
    const auto vectorValid=[&](scene::Vec3 p){return finite(p.x)&&finite(p.y)&&finite(p.z);};
    while(std::getline(input,line)){
        ++lines;bytes+=line.size();if(lines>100000||bytes>16*1024*1024||line.size()>8192)return fail("size bound exceeded");
        std::istringstream row(line);std::string kind,extra;if(!(row>>kind))continue;
        if(kind=="boundary_poly"){
            auto& b=parsed.graph.boundary;
            if(boundarySeen||!(row>>b.floorZ>>b.ceilingZ>>b.enabled>>pointsExpected)||pointsExpected>10000||!finite(b.floorZ)||!finite(b.ceilingZ)||b.floorZ>b.ceilingZ)return fail("invalid polygon boundary");
            boundarySeen=true;
        }else if(kind=="bpoint"){
            scene::Vec2 p;if(!boundarySeen||parsed.graph.boundary.points.size()>=pointsExpected||!(row>>p.x>>p.y)||!finite(p.x)||!finite(p.y))return fail("invalid boundary point");parsed.graph.boundary.points.push_back(p);
        }else if(kind=="boundary"){
            scene::Vec3 lo,hi;auto& b=parsed.graph.boundary;
            if(boundarySeen||!(row>>lo.x>>lo.y>>lo.z>>hi.x>>hi.y>>hi.z>>b.enabled)||!vectorValid(lo)||!vectorValid(hi)||lo.x>hi.x||lo.y>hi.y||lo.z>hi.z)return fail("invalid box boundary");
            b.floorZ=lo.z;b.ceilingZ=hi.z;b.points={{lo.x,lo.y},{hi.x,lo.y},{hi.x,hi.y},{lo.x,hi.y}};boundarySeen=true;pointsExpected=4;
        }else if(kind=="node"){
            NavigationNode n;if(!(row>>n.position.x>>n.position.y>>n.position.z>>n.weight>>n.cost>>n.radius))return fail("truncated node");
            if(version>=2){if(!(row>>n.lane>>n.priority>>n.disabled))return fail("truncated node flags");}else if(!(row>>n.priority>>n.disabled))return fail("truncated node flags");
            if(version>=3&&!(row>>n.attraction>>n.attractionRadius>>n.mantlePriority>>n.mantleOver))return fail("truncated attractor");
            if(!vectorValid(n.position)||!finite(n.weight)||n.weight<0||!finite(n.cost)||n.cost<0||!finite(n.radius)||n.radius<0||!finite(n.attraction)||n.attraction<0||!finite(n.attractionRadius)||n.attractionRadius<0)return fail("invalid node values");parsed.graph.nodes.push_back(n);
        }else if(kind=="link"){
            NavigationLink link;if(!(row>>link.from>>link.to>>link.cost>>link.bidirectional)||!finite(link.cost)||link.cost<0)return fail("invalid link");parsed.graph.links.push_back(link);
        }else if(kind=="block"){
            NavigationBlock b;if(!(row>>b.minimum.x>>b.minimum.y>>b.minimum.z>>b.maximum.x>>b.maximum.y>>b.maximum.z)||!vectorValid(b.minimum)||!vectorValid(b.maximum)||b.minimum.x>b.maximum.x||b.minimum.y>b.maximum.y||b.minimum.z>b.maximum.z)return fail("invalid blocked box");parsed.graph.blocks.push_back(b);
        }else if(kind=="goal_area"){
            NavigationGoalArea g;if(!(row>>g.position.x>>g.position.y>>g.position.z>>g.radius>>g.weight>>g.enabled)||!vectorValid(g.position)||!finite(g.radius)||g.radius<=0||!finite(g.weight)||g.weight<0)return fail("invalid goal area");parsed.graph.goalAreas.push_back(g);
        }else if(kind=="spawn"){
            scene::Vec3 p;if(!(row>>p.x>>p.y>>p.z)||!vectorValid(p))return fail("invalid spawn");parsed.spawns.push_back(p);
        }else return fail("unknown record");
        if(row>>extra)return fail("extra record fields");
    }
    if(input.bad())return fail("read failed");
    if(parsed.graph.boundary.points.size()!=pointsExpected||(parsed.graph.boundary.enabled&&pointsExpected<3))return fail("boundary point count mismatch");
    for(const auto& link:parsed.graph.links)if(link.from>=parsed.graph.nodes.size()||link.to>=parsed.graph.nodes.size())return fail("link references absent node");
    output=std::move(parsed);error.clear();return true;
}
}
