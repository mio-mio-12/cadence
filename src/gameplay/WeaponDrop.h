#pragma once
#include "scene/GlbMap.h"
#include "scene/TestCourse.h"
#include <box3d/box3d.h>
#include <unordered_set>

namespace gameplay::ragdoll {
struct WeaponDropSettings {
    int trigger{};
    float bounce{},friction{.65f},spin{1.f},carry{.65f};
};

class DropWorld {
public:
    explicit DropWorld(const scene::glb::Map* map) : map_(map) {
        auto world=b3DefaultWorldDef();
        world.gravity={0,0,-9.81f};
        world.restitutionThreshold=.25f;
        id_=b3CreateWorld(&world);
        auto ground=b3DefaultBodyDef();
        ground_=b3CreateBody(id_,&ground);
        if(!map_){
            addBox({0,0,-50},{scene::course::kHalfExtent,scene::course::kHalfExtent,50});
            for(const auto& b:scene::course::kBoxes)
                addBox({(b.minX+b.maxX)*.5f,(b.minY+b.maxY)*.5f,b.height*.5f},
                       {(b.maxX-b.minX)*.5f,(b.maxY-b.minY)*.5f,b.height*.5f});
        }
    }
    ~DropWorld() {
        b3DestroyWorld(id_);
        for(auto* mesh:meshes_)b3DestroyMesh(mesh);
    }
    DropWorld(const DropWorld&)=delete;
    DropWorld& operator=(const DropWorld&)=delete;

    void cover(scene::Vec3 point) {
        if(!map_)return;
        const auto& map=map_->shotGeometry?*map_->shotGeometry:*map_;
        std::vector<b3Vec3> vertices;
        const auto append=[&](std::uint32_t index){
            if(index>=map.collision.size()||!triangles_.insert(index).second)return;
            const auto& t=map.collision[index];
            if(!(t.walkable||t.blocking)||scene::length(scene::cross(t.b-t.a,t.c-t.a))<.05f)return;
            vertices.insert(vertices.end(),{metres(t.a),metres(t.b),metres(t.c)});
        };
        if(!coveredGlobal_){
            coveredGlobal_=true;
            if(map.gridCollision.empty())for(std::uint32_t i=0;i<map.collision.size();++i)append(i);
            else for(const auto i:map.globalCollision)append(i);
        }
        const int cx=map.gridCoordX(point.x),cy=map.gridCoordY(point.y);
        for(int y=cy-1;y<=cy+1;++y)for(int x=cx-1;x<=cx+1;++x){
            const int cell=map.cellIndex(x,y);
            if(cell<0||!cells_.insert(cell).second)continue;
            for(const auto i:map.gridCollision[cell])append(i);
        }
        if(vertices.empty())return;
        std::vector<int32_t> indices;
        indices.reserve(vertices.size()*2);
        for(int32_t i=0;i<static_cast<int32_t>(vertices.size());i+=3)
            indices.insert(indices.end(),{i,i+1,i+2,i,i+2,i+1});
        b3MeshDef def{};
        def.vertices=vertices.data();
        def.indices=indices.data();
        def.vertexCount=static_cast<int>(vertices.size());
        def.triangleCount=static_cast<int>(indices.size()/3);
        def.useMedianSplit=true;
        if(auto* mesh=b3CreateMesh(&def,nullptr,0)){
            meshes_.push_back(mesh);
            auto shape=b3DefaultShapeDef();
            b3CreateMeshShape(ground_,&shape,mesh,{1,1,1});
        }
    }
    b3BodyId add(scene::Vec3 position,scene::Quat rotation,scene::Vec3 half,scene::Vec3 velocity,scene::Vec3 spin,const WeaponDropSettings& settings){
        cover(position);
        auto body=b3DefaultBodyDef();
        body.type=b3_dynamicBody;
        const auto p=metres(position);
        body.position={p.x,p.y,p.z};
        body.rotation={{rotation.x,rotation.y,rotation.z},rotation.w};
        body.linearVelocity=metres(velocity);
        body.angularVelocity={spin.x,spin.y,spin.z};
        body.linearDamping=.25f;
        body.angularDamping=.5f;
        body.isBullet=true;
        const auto id=b3CreateBody(id_,&body);
        auto box=b3MakeBoxHull(std::max(.01f,half.x*.01f),std::max(.01f,half.y*.01f),std::max(.01f,half.z*.01f));
        auto shape=b3DefaultShapeDef();
        shape.baseMaterial.restitution=std::clamp(settings.bounce,0.f,1.f);
        shape.baseMaterial.friction=std::clamp(settings.friction,0.f,2.f);
        b3CreateHullShape(id,&shape,&box.base);
        bodies_.push_back(id);
        return id;
    }
    void remove(b3BodyId id) {
        std::erase_if(bodies_,[&](auto body){return B3_ID_EQUALS(id,body);});
        b3DestroyBody(id);
    }
    void step(float dt) {
        accumulator_=std::min(accumulator_+std::max(dt,0.f),.1f);
        constexpr float tick=1.f/120.f;
        while(accumulator_>=tick){
            if(map_)for(const auto body:bodies_)if(b3Body_IsAwake(body)){
                const auto p=b3Body_GetPosition(body);
                cover({float(p.x)*100.f,float(p.y)*100.f,float(p.z)*100.f});
            }
            b3World_Step(id_,tick,2);
            accumulator_-=tick;
        }
    }
private:
    static b3Vec3 metres(scene::Vec3 p){return {p.x*.01f,p.y*.01f,p.z*.01f};}
    void addBox(scene::Vec3 center,scene::Vec3 half){
        const auto c=metres(center),h=metres(half);
        auto box=b3MakeOffsetBoxHull(h.x,h.y,h.z,c);
        auto shape=b3DefaultShapeDef();
        b3CreateHullShape(ground_,&shape,&box.base);
    }
    const scene::glb::Map* map_;
    b3WorldId id_{};
    b3BodyId ground_{};
    float accumulator_{};
    std::vector<b3BodyId> bodies_;
    std::unordered_set<int> cells_;
    std::unordered_set<std::uint32_t> triangles_;
    std::vector<b3MeshData*> meshes_;
    bool coveredGlobal_{};
};

struct DroppedWeapon {
    std::shared_ptr<DropWorld> world;
    b3BodyId id{};
    std::vector<scene::DetachedAttachment> local;
    ~DroppedWeapon(){if(world)world->remove(id);}
    std::vector<scene::DetachedAttachment> pose()const{
        auto result=local;
        const auto p=b3Body_GetTransform(id);
        const scene::Vec3 center{float(p.p.x)*100.f,float(p.p.y)*100.f,float(p.p.z)*100.f};
        const auto model=scene::trs(center,{p.q.v.x,p.q.v.y,p.q.v.z,p.q.s},{1,1,1});
        for(auto& part:result)part.mount=model*part.mount;
        return result;
    }
};
}
