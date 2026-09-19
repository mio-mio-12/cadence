#include "scene/AuthoredGameplay.h"
#include "scene/BoundedJson.h"
#include <fstream>
#include <set>
#include <stdexcept>

namespace scene::authored {
namespace {
using Json=nlohmann::json;
constexpr float cellSize=256.f;
std::int64_t key(int x,int y){return static_cast<std::int64_t>((std::uint64_t(std::uint32_t(x))<<32)|std::uint32_t(y));}
Vec3 vector(const Json& j,float scale=1){
    if(!j.is_array()||j.size()!=3)throw std::runtime_error("invalid vector");
    Vec3 p{j.at(0).get<float>()*scale,j.at(1).get<float>()*scale,j.at(2).get<float>()*scale};
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>1e8f)throw std::runtime_error("non-finite or excessive coordinate");
    return p;
}
bool active(const Json& j){return j.value("enabled",false)&&j.value("hierarchyActive",false);}
float number(const Json& j,const char* key,float fallback){auto i=j.find(key);if(i==j.end()||!i->is_number())return fallback;const float f=i->get<float>();return std::isfinite(f)?f:fallback;}
Volume shape(const Json& item,const Json& s,Kind kind,float scale){
    Volume v;v.kind=kind;v.id=item.value("id","");
    if(auto target=item.find("targetLadderId");target!=item.end()&&target->is_string())v.target=target->get<std::string>();
    const auto& corners=s.at("worldCorners");if(corners.size()!=8)throw std::runtime_error("box requires eight corners");
    Vec3 p[8];for(int i=0;i<8;++i)p[i]=vector(corners[i],scale);
    v.origin=p[0];v.edge[0]=p[4]-p[0];v.edge[1]=p[2]-p[0];v.edge[2]=p[1]-p[0];
    const float det=dot(v.edge[0],cross(v.edge[1],v.edge[2]));
    if(!std::isfinite(det)||std::abs(det)<.001f)throw std::runtime_error("degenerate volume");
    v.reciprocal[0]=cross(v.edge[1],v.edge[2])/det;v.reciprocal[1]=cross(v.edge[2],v.edge[0])/det;v.reciprocal[2]=cross(v.edge[0],v.edge[1])/det;
    v.minimum=v.maximum=p[0];for(auto q:p){v.minimum={std::min(v.minimum.x,q.x),std::min(v.minimum.y,q.y),std::min(v.minimum.z,q.z)};v.maximum={std::max(v.maximum.x,q.x),std::max(v.maximum.y,q.y),std::max(v.maximum.z,q.z)};}
    v.center=(v.minimum+v.maximum)*.5f;
    v.facing=vector(item.at(kind==Kind::Ladder||kind==Kind::Entrance?"up":"forward"));v.facing.z=0;
    // CODM ladders use source forward as climb axis and source up as rung
    // direction. Their cross product is the contact normal; nonuniform scale
    // means the narrowest box dimension is NOT a reliable substitute.
    if(kind==Kind::Ladder){v.facing=cross(vector(item.at("forward")),vector(item.at("up")));v.facing.z=0;}
    if(length(v.facing)>.01f)v.facing=normalize(v.facing);
    const auto props=item.value("sourceProperties",Json::object());
    v.angle=std::clamp(number(props,kind==Kind::Mantle?"angle":"AffectAngle",45),5.f,80.f);
    v.strength=std::clamp(number(props,"AffectForce",50),0.f,100.f);
    v.forbidDown=number(props,"IsForbidDownToLadder",0)!=0;
    return v;
}
}
bool Volume::overlaps(Vec3 feet,float radius,float height) const {
    if(feet.x+radius<minimum.x||feet.x-radius>maximum.x||feet.y+radius<minimum.y||feet.y-radius>maximum.y||feet.z+height<minimum.z||feet.z>maximum.z)return false;
    // Affine box tests also support mirrored/scaled transforms. Test the body
    // segment against the expanded box, not merely three discrete sample points.
    const auto local=feet-origin;float enter=0,leave=1;
    for(auto n:reciprocal){const float p=dot(local,n),d=height*n.z,pad=radius*std::hypot(n.x,n.y);
        if(std::abs(d)<1e-8f){if(p < -pad||p>1+pad)return false;continue;}
        float a=(-pad-p)/d,b=(1+pad-p)/d;if(a>b)std::swap(a,b);enter=std::max(enter,a);leave=std::min(leave,b);if(enter>leave)return false;
    }return true;
}
void Data::index(){cells.clear();global.clear();for(std::size_t i=0;i<volumes.size();++i){const auto& v=volumes[i];const int x0=int(std::floor(v.minimum.x/cellSize)),x1=int(std::floor(v.maximum.x/cellSize)),y0=int(std::floor(v.minimum.y/cellSize)),y1=int(std::floor(v.maximum.y/cellSize));if(std::int64_t(x1-x0+1)*(y1-y0+1)>256){global.push_back(i);continue;}for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)cells[key(x,y)].push_back(i);}}
std::vector<std::size_t> Data::nearby(Vec3 p,float radius) const {
    if(!enabled||volumes.empty())return {};auto result=global;
    for(int y=int(std::floor((p.y-radius)/cellSize));y<=int(std::floor((p.y+radius)/cellSize));++y)for(int x=int(std::floor((p.x-radius)/cellSize));x<=int(std::floor((p.x+radius)/cellSize));++x)if(auto i=cells.find(key(x,y));i!=cells.end())result.insert(result.end(),i->second.begin(),i->second.end());
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
const Volume* Data::ladder(Vec3 p,float radius,float height) const {
    if(!ladders)return nullptr;const Volume* best=nullptr;float distance=1e30f;
    for(auto i:nearby(p,radius)){const auto& v=volumes[i];if((v.kind!=Kind::Ladder&&v.kind!=Kind::Entrance)||!v.overlaps(p,radius,height))continue;
        const Volume* candidate=&v;if(v.kind==Kind::Entrance){candidate=nullptr;for(const auto& target:volumes)if(target.kind==Kind::Ladder&&target.id==v.target){candidate=&target;break;}}
        if(candidate&&length(candidate->center-p)<distance){best=candidate;distance=length(candidate->center-p);}
    }return best;
}
bool Data::crouch(Vec3 p,float radius,float height) const {if(!assistance)return false;for(auto i:nearby(p,radius))if(volumes[i].kind==Kind::Crouch&&volumes[i].overlaps(p,radius,height))return true;return false;}
Vec3 Data::assistedWish(Vec3 p,Vec3 wish,float radius,float height) const {
    const float speed=std::hypot(wish.x,wish.y);if(!assistance||speed<1)return wish;
    for(auto i:nearby(p,radius)){const auto& v=volumes[i];if(v.kind!=Kind::Door||!v.overlaps(p,radius,height)||length(v.facing)<.5f)continue;
        const auto dir=wish/speed;if(std::abs(dot(dir,v.facing))<std::cos(v.angle*kPi/180.f))continue;
        const Vec3 side{-v.facing.y,v.facing.x,0};const float error=dot(v.center-p,side);
        // Steering only: preserve input speed, aim and collision. Never teleport.
        const auto adjusted=dir+side*std::clamp(error/100.f,-.22f,.22f)*(v.strength/100.f);
        wish=normalize(adjusted)*speed;break;
    }return wish;
}
std::vector<Vec3> Data::mantleDirections(Vec3 p,Vec3 looking,float radius,float height,float reach) const {
    std::vector<Vec3> out;if(!mantles)return out;looking.z=0;if(length(looking)<.5f)return out;looking=normalize(looking);
    for(auto i:nearby(p,radius+reach)){const auto& v=volumes[i];if(v.kind!=Kind::Mantle||!v.overlaps(p,radius+reach,height)||length(v.facing)<.5f)continue;
        // Both approaches are permitted, but never turn the player away from
        // their input. The actual ledge and complete motion remain collision-tested.
        Vec3 dir=dot(looking,v.facing)<0?-v.facing:v.facing;
        if(dot(looking,dir)>=std::cos(std::min(45.f,v.angle)*kPi/180.f)&&dot(v.center-p,looking)>=-radius)out.push_back(dir);
        if(out.size()==4)break;
    }return out;
}
Data load(const std::filesystem::path& folder,float mapScale){
    Data out;if(!std::isfinite(mapScale)||mapScale<=0){out.warnings.push_back("Invalid gameplay volume scale");return out;}const float scale=2.54f*mapScale;
    for(const auto file:{"gameplay_volumes.json","tactical_markers.json"}){
        const auto path=folder/file;std::error_code ec;if(!std::filesystem::exists(path,ec))continue;
        try{const auto size=std::filesystem::file_size(path);if(size>64u*1024u*1024u)throw std::runtime_error("sidecar exceeds 64 MiB");std::ifstream in(path);if(!in)throw std::runtime_error("cannot open sidecar");auto root=codm::parseJson(std::string(std::istreambuf_iterator<char>(in),{}));
            const std::string schema=std::string(file)=="gameplay_volumes.json"?"codm.gameplay-volumes/1":"codm.tactical-markers/1";
            if(root.value("schema","")!=schema||root.value("units","")!="inches"||root.value("coordinateSystem","")!="RH_Z_UP")throw std::runtime_error("unsupported schema or units");
            std::size_t count=0;for(const auto& set:root.at("sets"))for(const auto& item:set.at("items")){
                if(++count>100000)throw std::runtime_error("too many records");if(!active(item))continue;
                try{const auto kind=item.value("kind","");
                    std::optional<Kind> k;if(kind=="LadderVolume")k=Kind::Ladder;else if(kind=="LadderEnterVolume")k=Kind::Entrance;else if(kind=="ClimbUpTriggerVolume")k=Kind::Mantle;else if(kind=="CrouchVolume")k=Kind::Crouch;else if(kind=="DoorAssistantVolume")k=Kind::Door;
                    if(k){for(const auto& s:item.at("shapes"))if(active(s)&&s.value("kind","")=="BoxCollider")out.volumes.push_back(shape(item,s,*k,scale));}
                    else if(kind=="ClimbSpot"&&item.value("endpointStatus","")=="complete"){
                        const auto& e=item.at("endpoints");if(e.at("startPoint").value("hierarchyActive",false)&&e.at("endPoint").value("hierarchyActive",false))out.climbs.push_back({vector(e.at("startPoint").at("position"),scale),vector(e.at("endPoint").at("position"),scale)});
                    }else if(kind=="BOTNaviSpot"||kind=="CampSpot"||kind=="DOMObjectiveVolume"||kind=="HPObjectiveVolume"||kind=="ControlObjectiveVolume"||kind=="BombPlacingPointVolume"||kind=="GFObjectiveVolume"){
                        const auto p=vector(item.at("position"),scale);bool duplicate=false;for(const auto& d:out.destinations)if(length(d.position-p)<30){duplicate=true;break;}
                        if(!duplicate)out.destinations.push_back({p,kind=="CampSpot"?.3f:1.f,kind});
                    }
                }catch(const std::exception& e){out.warnings.push_back(item.value("id","record")+": "+e.what());}
            }
        }catch(const std::exception& e){out.warnings.push_back(std::string(file)+": "+e.what());}
    }out.index();return out;
}
}
