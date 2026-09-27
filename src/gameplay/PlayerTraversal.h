#pragma once
#include "gameplay/MovementTrace.h"
#include "gameplay/MantleAcquisition.h"
#include <istream>
#include <ostream>

namespace gameplay::traversal {
struct Settings {
    float minimumEyePercent{21.1f},maximumEyePercent{122.8f},reachIw{16.8f};
    bool airEdgeEnabled{};
    float airEdgeIw{3};
    bool operator==(const Settings&) const = default;
    bool valid() const {return std::isfinite(minimumEyePercent)&&std::isfinite(maximumEyePercent)&&std::isfinite(reachIw)&&std::isfinite(airEdgeIw)&&minimumEyePercent>=0&&maximumEyePercent>=minimumEyePercent&&reachIw>=0&&airEdgeIw>=0;}
};
inline std::ostream& operator<<(std::ostream& out,const Settings& s){return out<<s.minimumEyePercent<<' '<<s.maximumEyePercent<<' '<<s.reachIw<<' '<<s.airEdgeEnabled<<' '<<s.airEdgeIw;}
inline std::istream& operator>>(std::istream& in,Settings& s){Settings v;if(in>>v.minimumEyePercent>>v.maximumEyePercent>>v.reachIw>>v.airEdgeEnabled>>v.airEdgeIw){if(v.valid())s=v;else in.setstate(std::ios::failbit);}return in;}
enum class Decision { Below, Above, NoFace, Eligible, PathBlocked, Started };
inline const char* label(Decision d){switch(d){case Decision::Below:return "Below minimum - normal movement";case Decision::Above:return "Above maximum";case Decision::NoFace:return "No front face within reach";case Decision::Eligible:return "Height/reach eligible";case Decision::PathBlocked:return "Traversal path blocked";case Decision::Started:return "Mantle started";}return "";}
inline Decision classify(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 target,scene::Vec3 facing,float eye,float radius,const Settings& s){
    const float rise=target.z-start.z,minimum=eye*s.minimumEyePercent*.01f,maximum=eye*s.maximumEyePercent*.01f;
    if(rise<minimum)return Decision::Below;
    if(rise>maximum)return Decision::Above;
    facing.z=0;if(scene::length(facing)<.5f)return Decision::NoFace;
    const auto hit=map.raycastSurface(start+scene::Vec3{0,0,std::max(1.f,minimum-2.f)},scene::normalize(facing),radius+gameplay::iw::worldUnits(s.reachIw));
    return hit&&std::abs(hit->normal.z)<.7f?Decision::Eligible:Decision::NoFace;
}
struct Debug {
    scene::Vec3 start{},target{};
    Decision decision{Decision::NoFace};
    double time{-100};
    float edgeLift{};
    bool attempted{true};
};
// A thin swept head cap, not a replacement movement hull. Never participates
// in walking, descending, slope friction or ordinary step resolution.
inline movement::Trace ceiling(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 end,float radius,float height){
    movement::Trace result;result.end=end;
    if(end.z<=start.z)return result;
    const scene::Vec3 offset{0,0,height-.5f};
    result=movement::sweep(map,start+offset,end+offset,radius,.5f,true);
    result.end=result.end-offset;
    return result;
}
inline bool lowAssist(float rise,float eyeHeight){return rise<eyeHeight*.72f;}
inline bool canPresentMantle(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 target,scene::Vec3 facing,float eyeHeight,float radius,float reach){
    const float rise=target.z-start.z;
    if(lowAssist(rise,eyeHeight)||rise>eyeHeight)return false;
    facing.z=0;if(scene::length(facing)<.5f)return false;
    // Require a nearby front surface just below the minimum hand-grab height.
    // Destination centers may be deeper on a platform than the ledge itself.
    return map.raycastSurface(start+scene::Vec3{0,0,eyeHeight*.70f},scene::normalize(facing),radius+reach).has_value();
}
inline bool landingFromAbove(bool grounded,float oldFeet,float floor){return grounded||floor<=oldFeet+.0254f;}

// Optional small lip clearance. No trajectory, timer, velocity replacement,
// attraction to a target, or downward/grounded movement participates here.
inline std::optional<scene::Vec3> airEdge(const scene::glb::Map& map,scene::Vec3 start,scene::Vec3 wanted,scene::Vec3 resolved,float radius,float height,float budget){
    auto direction=wanted-start;direction.z=0;const float distance=scene::length(direction);
    if(budget<=0||wanted.z<=start.z||distance<.01f)return {};
    direction=direction/distance;
    if(scene::dot(resolved-start,direction)>=distance-.05f)return {};
    const auto probe=wanted+direction*radius;
    const auto top=map.raycastSurface({probe.x,probe.y,start.z+budget+.05f},{0,0,-1},budget+.1f);
    if(!top||top->normal.z<.7f)return {};
    const float lift=top->position.z+.04f-start.z;
    if(lift<=0||lift>budget)return {};
    const auto lifted=start+scene::Vec3{0,0,lift},over=wanted+scene::Vec3{0,0,lift};
    const auto headUp=ceiling(map,start,lifted,radius,height),headAcross=ceiling(map,lifted,over,radius,height);
    if(headUp.startSolid||headUp.fraction<.9999f||headAcross.startSolid||headAcross.fraction<.9999f)return {};
    const auto up=mantle::sweepRounded(map,start,lifted,radius,height),across=mantle::sweepRounded(map,lifted,over,radius,height);
    if(up.startSolid||up.fraction<.9999f||across.startSolid||across.fraction<.9999f)return {};
    return over;
}
}
