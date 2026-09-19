#include "scene/C2mxMap.h"
#include "scene/SpawnSelection.h"
#include <fstream>
#include <set>
#include <numbers>

namespace scene::c2mx {
namespace {
using codm::Json;
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
double number(const Json& j){require(j.is_number(),"Expected numeric collision value");const auto v=j.get<double>();require(std::isfinite(v)&&std::abs(v)<1e12,"Invalid collision number");return v;}
float scalar(const Json& j){return static_cast<float>(number(j));}
Vec3 vec(const Json& j){require(j.is_array()&&j.size()==3,"Expected three-element vector");return {scalar(j[0]),scalar(j[1]),scalar(j[2])};}
bool flag(const Json& j,const char* key,bool fallback){if(!j.contains(key))return fallback;require(j[key].is_boolean(),"Expected boolean collider field");return j[key].get<bool>();}
std::string string(const Json& j,const char* key){require(j.contains(key)&&j[key].is_string(),"Missing/string schema field");const auto s=j[key].get<std::string>();require(s.size()<1024*1024,"Oversized metadata string");return s;}
std::string readText(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary|std::ios::ate);require(bool(f),"Cannot open collision metadata");const auto n=f.tellg();require(n>=0&&n<=64*1024*1024,"Metadata file exceeds size bound");std::string s(static_cast<std::size_t>(n),'\0');f.seekg(0);require(bool(f.read(s.data(),n)),"Short metadata read");return s;}
void grow(Bounds& b,Vec3 p){require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<1e8f&&std::abs(p.y)<1e8f&&std::abs(p.z)<1e8f,"Transformed collider position outside safe world bounds");if(!b.valid){b.minimum=b.maximum=p;b.valid=true;}else{b.minimum={std::min(b.minimum.x,p.x),std::min(b.minimum.y,p.y),std::min(b.minimum.z,p.z)};b.maximum={std::max(b.maximum.x,p.x),std::max(b.maximum.y,p.y),std::max(b.maximum.z,p.z)};}}
std::uint64_t little(std::span<const std::uint8_t> b,std::size_t p,unsigned n){require(p<=b.size()&&n<=b.size()-p,"Truncated C2MX integer");std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(b[p+i])<<(i*8);return v;}
void triangle(std::vector<glb::CollisionTriangle>& out,Vec3 a,Vec3 b,Vec3 c,Bounds& bounds){
    grow(bounds,a);grow(bounds,b);grow(bounds,c);auto normal=cross(b-a,c-a);if(length(normal)<1e-7f)return;
    glb::CollisionTriangle t;t.a=a;t.b=b;t.c=c;t.normal=normalize(normal);
    t.minimum={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};
    t.maximum={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
    t.walkable=t.normal.z>=.5f;t.blocking=!t.walkable;out.push_back(t);
    require(out.size()<=4000000,"Authored collision triangle bound exceeded");
}
}

Extension readExtension(std::span<const std::uint8_t> bytes){
    Extension e;e.baseEnd=bytes.size();if(bytes.size()<32)return e;
    const auto footer=bytes.size()-32;
    if(std::memcmp(bytes.data()+footer,"C2MX",4)!=0){
        for(std::size_t i=footer+1;i+4<=bytes.size();++i)require(std::memcmp(bytes.data()+i,"C2MX",4)!=0,"Truncated C2MX footer");return e;
    }
    const auto version=little(bytes,footer+4,4),offset=little(bytes,footer+8,8),len=little(bytes,footer+16,8),count=little(bytes,footer+24,4),flags=little(bytes,footer+28,4);
    require(version==1&&!flags&&count<=4096&&len==count*24&&offset<=footer&&len==footer-offset&&offset%8==0,"Invalid C2MX directory");
    std::set<std::string> tags;std::vector<std::pair<std::size_t,std::size_t>> spans;
    for(std::size_t i=0;i<count;++i){const auto p=static_cast<std::size_t>(offset)+i*24;const std::string tag(reinterpret_cast<const char*>(bytes.data()+p),4);
        const auto ver=little(bytes,p+4,4),start=little(bytes,p+8,8),size=little(bytes,p+16,8);
        require(tags.insert(tag).second,"Duplicate C2MX chunk");require(start>=8&&start%8==0&&start<=offset&&size<=offset-start&&size<=64*1024*1024,"Invalid C2MX chunk bounds");
        for(const auto& span:spans)require(start>=span.second||start+size<=span.first,"Overlapping C2MX chunks");
        spans.emplace_back(static_cast<std::size_t>(start),static_cast<std::size_t>(start+size));e.baseEnd=std::min(e.baseEnd,static_cast<std::size_t>(start));
        if(tag=="META"||tag=="COLL"||tag=="NAVM"){
            require(ver==1,"Unsupported C2MX known chunk version");auto j=codm::parseJson(std::string_view(reinterpret_cast<const char*>(bytes.data()+start),static_cast<std::size_t>(size)));
            if(tag=="META")e.meta=std::move(j);if(tag=="COLL")e.collision=std::move(j);if(tag=="NAVM")e.navigation=std::move(j);
        }
    }
    require(tags.contains("META")&&tags.contains("COLL")&&tags.contains("NAVM"),"C2MX requires META/COLL/NAVM");
    require(e.meta.is_object()&&string(e.meta,"schema")=="codm.c2mx/1"&&string(e.meta,"collisionPolicy")=="authored_only","Unsupported C2MX META schema/policy");
    require(e.meta.contains("collisionComplete")&&e.meta["collisionComplete"].is_boolean(),"META completeness is required");e.present=true;return e;
}

bool applyCollision(const Json& collision,const Json& meta,const Json& navigation,glb::Map& map,std::string& error,const c2m::LoadOptions& options,bool rebuildIndex){
    try{
        require(std::isfinite(map.scaleMultiplier)&&map.scaleMultiplier>0&&map.scaleMultiplier<=10000,"Invalid map scale");
        require(options.primitiveSegments>=8&&options.primitiveSegments<=64,"Primitive tessellation must be 8..64 segments");
        require(collision.is_object()&&string(collision,"schema")=="codm.collision/1"&&string(collision,"coordinateSystem")=="RH_Z_UP"&&string(collision,"units")=="inches","Unsupported COLL schema/coordinates/units");
        require(collision.contains("colliders")&&collision["colliders"].is_array()&&collision["colliders"].size()<=100000,"Invalid collider list");
        require(meta.is_object()&&meta.contains("collisionComplete")&&meta["collisionComplete"].is_boolean(),"Collision completeness metadata required");
        require(navigation.is_object()&&string(navigation,"schema")=="codm.navigation/1","Unsupported NAVM schema");
        const auto navStatus=string(navigation,"status");require(navStatus=="settings_only"||navStatus=="native_payload_preserved"||navStatus=="not_found_in_selected_scenes","Unknown NAVM status");
        require(navigation.contains("runtimeReady")&&navigation["runtimeReady"].is_boolean()&&!navigation["runtimeReady"].get<bool>(),"NAVM is preservation, not a runtime graph");
        require(navigation.contains("assets")&&navigation["assets"].is_array()&&navigation["assets"].size()<=100000,"Invalid NAVM asset list");
        glb::AuthoredCollisionMetadata info;info.present=true;info.complete=meta["collisionComplete"].get<bool>();info.navigationStatus=navStatus;
        info.colliderCount=collision["colliders"].size();info.primitiveSegments=options.primitiveSegments;info.solidLayerMask=options.solidLayerMask;
        info.metaJson=meta.dump();info.collisionJson=collision.dump();info.navigationJson=navigation.dump();
        std::vector<glb::CollisionTriangle> solids,triggers;std::vector<glb::AuthoredCollider> descriptors;descriptors.reserve(collision["colliders"].size());std::set<std::string> ids;
        const float scale=2.54f*map.scaleMultiplier;bool unsupported=false;
        for(const auto& record:collision["colliders"]){
            require(record.is_object(),"Collider must be object");glb::AuthoredCollider d;d.id=string(record,"id");d.name=string(record,"name");d.kind=string(record,"kind");require(ids.insert(d.id).second,"Duplicate collider id");
            d.enabled=flag(record,"enabled",true);d.trigger=flag(record,"trigger",false);
            require(record.contains("layer")&&record["layer"].is_number_integer(),"Collider layer integer required");const auto layer=number(record["layer"]);require(layer>=0&&layer<=63,"Collider layer outside 0..63");d.layer=static_cast<unsigned>(layer);
            require(record.contains("matrix")&&record["matrix"].is_array()&&record["matrix"].size()==16,"Collider matrix must have 16 numbers");for(int i=0;i<16;++i)d.matrix.v[i]=scalar(record["matrix"][i]);
            require(std::abs(d.matrix.v[3])<1e-6f&&std::abs(d.matrix.v[7])<1e-6f&&std::abs(d.matrix.v[11])<1e-6f&&std::abs(d.matrix.v[15]-1)<1e-6f,"Collider matrix must be affine");d.center=vec(record.at("center"));
            if(record.contains("physicsMaterial"))d.physicsMaterialJson=record["physicsMaterial"].dump();if(record.contains("source"))d.sourceJson=record["source"].dump();
            const Vec3 basis[3]={{d.matrix.v[0],d.matrix.v[1],d.matrix.v[2]},{d.matrix.v[4],d.matrix.v[5],d.matrix.v[6]},{d.matrix.v[8],d.matrix.v[9],d.matrix.v[10]}};
            const float scales[3]={length(basis[0]),length(basis[1]),length(basis[2])};for(float s:scales)require(s>1e-8f,"Degenerate collider transform");
            const bool shear=std::abs(dot(basis[0],basis[1])/(scales[0]*scales[1]))>1e-4f||std::abs(dot(basis[0],basis[2])/(scales[0]*scales[2]))>1e-4f||std::abs(dot(basis[1],basis[2])/(scales[1]*scales[2]))>1e-4f;
            const bool mirrored=dot(cross(basis[0],basis[1]),basis[2])<0;
            std::vector<Vec3> vertices;std::vector<std::array<unsigned,3>> faces;bool supported=!shear;
            if(d.kind=="BoxCollider"){
                // Explicit affine-box policy: keep the exported eight corners,
                // including shear, rather than orthogonalizing the source shape.
                // Rounded primitives still require their separate scale policy.
                require(std::abs(dot(cross(basis[0]/scales[0],basis[1]/scales[1]),basis[2]/scales[2]))>1e-6f,"Singular box transform");
                supported=true;
                d.size=vec(record.at("size"));require(d.size.x>0&&d.size.y>0&&d.size.z>0,"Box extents must be positive");
                vertices.reserve(8);faces.reserve(12);
                for(unsigned i=0;i<8;++i)vertices.push_back(transformPoint(d.matrix,d.center+Vec3{(i&1?1.f:-1.f)*d.size.x*.5f,(i&2?1.f:-1.f)*d.size.y*.5f,(i&4?1.f:-1.f)*d.size.z*.5f})*scale);
                faces={{0,2,3},{0,3,1},{4,5,7},{4,7,6},{0,1,5},{0,5,4},{2,6,7},{2,7,3},{0,4,6},{0,6,2},{1,3,7},{1,7,5}};
                if(mirrored)for(auto& f:faces)std::swap(f[1],f[2]);
            }else if(d.kind=="MeshCollider"){
                require(length(d.center)<1e-5f,"World mesh collider center must be zero");for(int i=0;i<16;++i)require(std::abs(d.matrix.v[i]-(i%5==0?1.f:0.f))<1e-5f,"World mesh collider matrix must be identity");
                const auto& vs=record.at("vertices");const auto& fs=record.at("triangles");require(vs.is_array()&&fs.is_array()&&vs.size()<=4000000&&fs.size()<=4000000,"Mesh collider array bounds");
                vertices.reserve(vs.size());faces.reserve(fs.size());
                for(const auto& v:vs)vertices.push_back(vec(v)*scale);
                for(const auto& f:fs){require(f.is_array()&&f.size()==3,"Triangle must have three indices");std::array<unsigned,3> face{};for(int i=0;i<3;++i){require(f[i].is_number_integer(),"Triangle index must be integer");const auto idx=f[i].get<std::int64_t>();require(idx>=0&&static_cast<std::uint64_t>(idx)<vertices.size(),"Triangle index out of range");face[i]=static_cast<unsigned>(idx);}faces.push_back(face);}
                if(flag(record,"convex",false))supported=false; // No cooked convex hull: do not use source triangles as a hull.
            }else if(d.kind=="SphereCollider"||d.kind=="CapsuleCollider"){
                d.radius=scalar(record.at("radius"));require(d.radius>0,"Primitive radius must be positive");
                Vec3 axis{0,0,1};float radius=d.radius*std::max({scales[0],scales[1],scales[2]}),halfLine=0;
                if(d.kind=="SphereCollider")require(string(record,"scalePolicy")=="unity_max_axis","Unknown sphere scale policy");
                else{require(string(record,"scalePolicy")=="unity_axis_height_max_perpendicular_radius","Unknown capsule scale policy");require(record.at("axis").is_number_integer(),"Capsule axis integer required");d.axis=record["axis"].get<int>();require(d.axis>=0&&d.axis<3,"Capsule axis out of range");d.height=scalar(record.at("height"));require(d.height>0,"Capsule height must be positive");axis=basis[d.axis]/scales[d.axis];radius=d.radius*std::max(scales[(d.axis+1)%3],scales[(d.axis+2)%3]);halfLine=std::max(0.f,d.height*scales[d.axis]*.5f-radius);}
                const auto center=transformPoint(d.matrix,d.center);const auto u=normalize(cross(axis,std::abs(axis.z)<.9f?Vec3{0,0,1}:Vec3{0,1,0})),v=cross(axis,u);
                const unsigned segments=options.primitiveSegments,hemisphereRings=std::max(2u,segments/4);
                std::vector<std::pair<float,float>> rings;rings.reserve(2*(hemisphereRings+1));
                vertices.reserve(2*(hemisphereRings+1)*segments);faces.reserve(2*(2*hemisphereRings+1)*segments);
                for(unsigned row=0;row<=hemisphereRings;++row){const float theta=(kPi*.5f)*row/hemisphereRings;rings.emplace_back(radius*std::sin(theta),radius*std::cos(theta)+halfLine);}
                // Separate top/bottom equators retain an actual cylindrical
                // middle instead of stretching one hemisphere strip across it.
                for(unsigned row=halfLine>0?0:1;row<=hemisphereRings;++row){const float theta=kPi*.5f+(kPi*.5f)*row/hemisphereRings;rings.emplace_back(radius*std::sin(theta),radius*std::cos(theta)-halfLine);}
                for(const auto [radial,axial]:rings)for(unsigned col=0;col<segments;++col){const float phi=2*kPi*col/segments;vertices.push_back((center+(u*std::cos(phi)+v*std::sin(phi))*radial+axis*axial)*scale);}
                for(unsigned row=0;row+1<rings.size();++row)for(unsigned col=0;col<segments;++col){const unsigned next=(col+1)%segments,a=row*segments+col,b=row*segments+next,c=(row+1)*segments+col,z=(row+1)*segments+next;faces.push_back({a,c,b});faces.push_back({b,c,z});}
                d.approximate=true;if(d.enabled&&!d.trigger&&(options.solidLayerMask&(1ull<<d.layer)))info.approximate=true;
            }else supported=false;
            if(!supported&&d.enabled&&!d.trigger&&(options.solidLayerMask&(1ull<<d.layer)))unsupported=true;
            auto& target=d.trigger?triggers:solids;d.firstTriangle=target.size();
            if(d.enabled&&supported&&(d.trigger||(options.solidLayerMask&(1ull<<d.layer))))for(const auto& f:faces)triangle(target,vertices[f[0]],vertices[f[1]],vertices[f[2]],d.bounds);
            else for(auto p:vertices)grow(d.bounds,p);
            d.triangleCount=target.size()-d.firstTriangle;if(d.trigger)++info.triggerCount;descriptors.push_back(std::move(d));
        }
        if(unsupported)info.complete=false;
        info.physicsReady=info.complete&&(!info.approximate||options.allowApproximateCollision);
        info.status=!info.complete?"Authored collision incomplete/unsupported: preview only; no visual fallback":info.approximate?(info.physicsReady?"Authored collision with explicitly accepted primitive tessellation":"Authored primitive tessellation needs explicit approval: preview only"):"Complete authored collision (all enabled layers selected by explicit mask)";
        if(std::any_of(descriptors.begin(),descriptors.end(),[](const auto& d){return d.layer>31;}))info.status+="; exporter-specific layers 32..63 retained; game layer collision matrix unknown";
        // One atomic replacement, including valid empty authored lists.
        map.collision=std::move(solids);map.authoredTriggerTriangles=std::move(triggers);map.authoredColliders=std::move(descriptors);map.authoredCollision=std::move(info);map.hasDefaultSpawnPoint=false;
        if(rebuildIndex){
            map.buildCollisionIndex();
            // Sidecar loading occurs after GLB's old visual-derived spawn was
            // chosen. Replace it with a supported authored floor, never reuse
            // the obsolete collision or manufacture a height for an empty list.
            chooseAuthoredSpawn(map);
        }error.clear();return true;
    }catch(const std::exception& ex){error=std::string("Authored collision: ")+ex.what();return false;}
}

bool loadCollisionSidecar(const std::filesystem::path& path,glb::Map& map,std::string& error,const c2m::LoadOptions& options){
    try{const auto collision=codm::parseJson(readText(path));const auto folder=path.parent_path();
        require(std::filesystem::is_regular_file(folder/"report.json"),"Explicit collision sidecar requires package report.json completeness");
        auto meta=codm::parseJson(readText(folder/"report.json"));Json nav={{"schema","codm.navigation/1"},{"status","not_found_in_selected_scenes"},{"runtimeReady",false},{"assets",Json::array()}};
        if(std::filesystem::is_regular_file(folder/"navigation.json"))nav=codm::parseJson(readText(folder/"navigation.json"));
        return applyCollision(collision,meta,nav,map,error,options,true);
    }catch(const std::exception& ex){error=std::string("Collision sidecar: ")+ex.what();return false;}
}

bool chooseAuthoredSpawn(glb::Map& map){
    map.hasDefaultSpawnPoint=false;
    map.collisionPreviewPoint=glb::findFallbackSpawn(map);
    if(!map.authoredCollision.physicsReady||!map.collisionPreviewPoint)return false;
    map.defaultSpawnPoint=*map.collisionPreviewPoint;
    map.hasDefaultSpawnPoint=true;
    return true;
}

std::vector<std::string> triggerIdsAtPoint(const glb::Map& map,Vec3 point){
    std::vector<std::string> ids;const Vec3 ray=normalize(Vec3{1,.371f,.529f});
    for(const auto& d:map.authoredColliders)if(d.enabled&&d.trigger&&d.bounds.valid){
        const auto& b=d.bounds;if(point.x<b.minimum.x||point.x>b.maximum.x||point.y<b.minimum.y||point.y>b.maximum.y||point.z<b.minimum.z||point.z>b.maximum.z)continue;
        std::vector<float> hits;for(std::size_t i=d.firstTriangle;i<d.firstTriangle+d.triangleCount;++i){const auto& t=map.authoredTriggerTriangles[i];const auto e1=t.b-t.a,e2=t.c-t.a,h=cross(ray,e2);const float det=dot(e1,h);if(std::abs(det)<1e-7f)continue;const auto s=point-t.a;const float u=dot(s,h)/det;if(u<0||u>1)continue;const auto q=cross(s,e1);const float v=dot(ray,q)/det;if(v<0||u+v>1)continue;const float distance=dot(e2,q)/det;if(distance>=0)hits.push_back(distance);}
        std::sort(hits.begin(),hits.end());hits.erase(std::unique(hits.begin(),hits.end(),[](float a,float b){return std::abs(a-b)<.01f;}),hits.end());if(hits.size()%2)ids.push_back(d.id);
    }return ids;
}
}
