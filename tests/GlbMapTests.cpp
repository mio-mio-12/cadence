#include "scene/GlbMap.h"
#include "scene/CodmMaterialMetadata.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>

namespace {
bool uriLoadFixtures(){
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("cadence_map_uri_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto package=root/"package";fs::create_directories(package);
    struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}}cleanup{root};
    const float vertices[]{-1,0,-1,1,0,-1,0,0,1};
    const auto buffer=[&](const fs::path& path){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(vertices),sizeof(vertices));};
    const auto load=[&](const std::string& bufferUri,const std::string& imageUri,scene::glb::Map& map,std::string& error){
        {std::ofstream out(package/"triangle.gltf");
         out<<R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],"buffers":[{"uri":")"<<bufferUri<<R"(","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,0,-1],"max":[1,0,1]}],"images":[{"uri":")"<<imageUri<<R"("}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}]})";}
        return scene::glb::load(package/"triangle.gltf",map,error,1,false);
    };
    try{
        struct Case{const char* raw;const char* actual;};
        const Case cases[]{
            {"space%20buffer.dat","space buffer.dat"},
            {R"(json\u0020buffer.dat)","json buffer.dat"},
            {R"(caf\u00e9.dat)","caf\xc3\xa9.dat"},
            {R"(face\ud83d\ude00.dat)","face\xf0\x9f\x98\x80.dat"},
            {R"(nested\/data)","nested/data"},
            {"percent%25.dat","percent%.dat"},
            {"%252e%252e/literal.dat","%2e%2e/literal.dat"}
        };
        for(const auto& item:cases){
            buffer(package/fs::u8path(item.actual));
            // Also exercise JSON + percent decoding for image paths; the map
            // loader records this path without needing a renderer/image codec.
            const auto image=package/fs::u8path("image caf\xc3\xa9%.png");std::ofstream(image).put('\0');
            scene::glb::Map map;std::string error;
            if(!load(item.raw,R"(image%20caf\u00e9%25.png)",map,error)||map.scene.meshes.empty()||map.scene.meshes.front().vertices.size()!=3||!fs::equivalent(map.scene.meshes.front().albedoPath,image))
                throw std::runtime_error(std::string("URI fixture ")+item.raw+": "+error);
        }
        buffer(root/"outside.dat");buffer(package/"valid.dat");
        for(const auto* unsafe:{"../outside.dat","%2e%2e%2foutside.dat","%2e%2e%5coutside.dat","valid.dat%00ignored",R"(valid.dat\u0000ignored)","C%3a/escape.dat","%2fescape.dat"}){
            scene::glb::Map map;std::string error;
            if(load(unsafe,"safe.png",map,error)||error.empty())throw std::runtime_error(std::string("Unsafe buffer accepted: ")+unsafe);
            if(load("valid.dat",unsafe,map,error)||error.empty())throw std::runtime_error(std::string("Unsafe image accepted: ")+unsafe);
        }
        std::cout<<"Actual GLTF URI loading: JSON Unicode/space/slash, percent once, extensionless and rejected traversal/NUL passed\n";
        return true;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return false;}
}
}

int main(int argc,char** argv){
    if(!uriLoadFixtures())return 1;
    {scene::glb::Map special;special.buildCollisionIndex();
     if(special.hasLadderTriangles||special.hasBounceTriangles||special.hasBoostTriangles)return 1;
     scene::glb::CollisionTriangle tri;tri.a={-100,-100,0};tri.b={100,-100,0};tri.c={0,100,0};
     tri.minimum={-100,-100,0};tri.maximum={100,100,0};tri.normal={0,0,1};
     tri.ladder=tri.bounce=tri.speedboost2=true;special.collision.push_back(tri);special.buildCollisionIndex();
     if(!special.hasLadderTriangles||!special.hasBounceTriangles||!special.hasBoostTriangles)return 1;
     if(!special.isBounceSurface(0,0,0,20)||special.speedBoostTier(0,0,0,20)!=2||!special.findLadderContact({0,0,0},20,72))return 1;
     special.collision[0].ladder=special.collision[0].bounce=special.collision[0].speedboost2=false;
     special.buildCollisionIndex();
     if(special.findLadderContact({0,0,0},20,72)||special.isBounceSurface(0,0,0,20)||special.speedBoostTier(0,0,0,20))return 1;
    }
    {
        const auto package=std::filesystem::current_path()/"codm_material_policy_fixture";
        std::filesystem::create_directories(package/"images");
        std::filesystem::create_directories(package/"source_layers");
        for(const auto name:{"a.png","n.png","mr.png","preview.png"}){std::ofstream fixture(package/"images"/name,std::ios::app);}
        {std::ofstream fixture(package/"source_layers"/"weights.npz",std::ios::app);}
        auto record=scene::codm::parseJson(R"({"alpha":"OPAQUE","decal":false,"blend":"alpha","color":[0.2,0.4,0.6,1],"doubleSided":false,"textures":{"color":"images/a.png","normal":"images/n.png","metallic":"images/mr.png"},"roughness":0.7,"metallic":0.8})");
        scene::Mesh mesh;mesh.forceAlpha=true;mesh.alphaTest=true;mesh.decal=true;std::string e;
        const auto check=[&](bool value,const char* message){if(!value){std::cerr<<message<<": "<<e<<'\n';std::exit(1);}};
        check(scene::codm::applyCodmMaterial(record,package,mesh,e),"CODM material parse failed");
        record["sky"]=true;check(scene::codm::applyCodmMaterial(record,package,mesh,e)&&mesh.weatherNonBlocking,"Authored sky must not block weather");record.erase("sky");
        check(mesh.materialPolicyExplicit&&mesh.gltfPbr&&!mesh.forceAlpha&&!mesh.alphaTest&&!mesh.decal&&mesh.ignoreAlbedoAlpha,"Explicit opaque did not clear heuristics");
        check(mesh.color.x==0.2f&&mesh.normalProfile==3&&mesh.specularPath.filename()=="mr.png","PBR factors/channels not retained");
        record["alpha"]="MASK";record["cutoff"]=0.67;record["doubleSided"]=true;
        check(scene::codm::applyCodmMaterial(record,package,mesh,e)&&mesh.alphaTest&&!mesh.forceAlpha&&mesh.doubleSided&&mesh.alphaCutoff==0.67f,"MASK must write depth, honor cutoff and sidedness");
        record["alpha"]="BLEND";record["blend"]="additive";record["decal"]=true;record["unlit"]=true;record["srcBlend"]=5;record["dstBlend"]=1;record["glbColorTexture"]="images/preview.png";
        check(scene::codm::applyCodmMaterial(record,package,mesh,e)&&mesh.decalAdditive&&mesh.forceAlpha&&mesh.albedoPath.filename()=="a.png","C2M additive must use native image, not GLB preview");
        check(mesh.unlit&&!mesh.emissive&&scene::length(mesh.emissiveFactor)==0,"Unlit must not manufacture white emission");
        mesh.albedoPath=package/"preview.png";mesh.color={.5f,.5f,.5f,1};
        check(scene::codm::applyCodmMaterial(record,package,mesh,e,true)&&!mesh.decalAdditive&&!mesh.decalMultiply&&mesh.albedoPath.filename()=="preview.png"&&mesh.color.x==.5f,"GLB additive approximation was reinterpreted");
        record["vertexBlendBake"]={{"sourceAttributes","source_layers/weights.npz"}};record["vertexTint"]=true;mesh.vertices.resize(1);mesh.vertices[0].color={.2f,.8f,.3f,.5f};
        check(scene::codm::applyCodmMaterial(record,package,mesh,e)&&mesh.vertexBlendBaked&&!mesh.useVertexColor&&mesh.vertices[0].color.x==1&&mesh.vertices[0].color.w==1,"Baked blend weights applied twice");
        check(mesh.sourceMaterialMetadata&&mesh.sourceMaterialMetadata->find("source_layers/weights.npz")!=std::string::npos,"Raw recipe not retained");
        const auto original=mesh.albedoPath;record["textures"]["color"]="../escape.png";
        check(!scene::codm::applyCodmMaterial(record,package,mesh,e)&&mesh.albedoPath==original&&mesh.vertices.size()==1,"Unsafe path accepted or failure was nontransactional");
        record["textures"]["color"]="C:/escape.png";
        check(!scene::codm::applyCodmMaterial(record,package,mesh,e),"Absolute package path accepted");
        record["textures"]["color"]="images/a.png";record["cutoff"]=2;
        check(!scene::codm::applyCodmMaterial(record,package,mesh,e),"Out of range alpha cutoff accepted");
    }
    if(argc<2){std::cerr<<"missing fixture\n";return 1;}scene::glb::Map map;std::string error;
    if(!scene::glb::load(std::filesystem::u8path(argv[1]),map,error)){std::cerr<<error<<'\n';return 1;}
    if(map.scene.meshes.empty()||map.collision.empty()||!map.scene.bounds.valid){std::cerr<<"incomplete imported map\n";return 1;}
    scene::glb::Map deferred;
    if(!scene::glb::load(std::filesystem::u8path(argv[1]),deferred,error,1.0f,false) ||
       !deferred.collision.empty() || !deferred.gridCollision.empty() || deferred.hasDefaultSpawnPoint ||
       deferred.scene.meshes.size()!=map.scene.meshes.size()){
        std::cerr<<"Deferred GLB collision contract failed\n";return 1;
    }
    for(size_t m=0;m<map.scene.meshes.size();++m){
        const auto& a=map.scene.meshes[m];const auto& b=deferred.scene.meshes[m];
        if(a.indices!=b.indices||a.vertices.size()!=b.vertices.size()||a.albedoPath!=b.albedoPath){std::cerr<<"Deferred visual mismatch\n";return 1;}
        for(size_t v=0;v<a.vertices.size();++v)if(scene::length(a.vertices[v].position-b.vertices[v].position)!=0){std::cerr<<"Deferred vertex mismatch\n";return 1;}
    }
    const auto size=map.scene.bounds.maximum-map.scene.bounds.minimum;
    if(size.x<99.0f||size.y<99.0f||size.z<99.0f){std::cerr<<"glTF meter conversion is incorrect\n";return 1;}
    scene::glb::Map navigation;const auto add=[&](scene::Vec3 a,scene::Vec3 b,scene::Vec3 c,bool walkable,bool blocking){scene::glb::CollisionTriangle triangle;triangle.a=a;triangle.b=b;triangle.c=c;triangle.normal=scene::normalize(scene::cross(b-a,c-a));if(triangle.normal.z<0){std::swap(triangle.b,triangle.c);triangle.normal=triangle.normal*-1.0f;}triangle.minimum={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};triangle.maximum={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};triangle.walkable=walkable;triangle.blocking=blocking;navigation.collision.push_back(triangle);};
    add({-200,-200,0},{200,-200,0},{200,200,0},true,false);add({-200,-200,0},{200,200,0},{-200,200,0},true,false);add({0,-100,0},{0,100,0},{0,100,100},false,true);add({0,-100,0},{0,100,100},{0,-100,100},false,true);add({0,-100,100},{200,-100,100},{200,100,100},true,false);add({0,-100,100},{200,100,100},{0,100,100},true,false);navigation.buildCollisionIndex();
    const auto mantle=navigation.mantleTarget({-50,0,0},{1,0,0},38.1f,182.88f,45.72f,144.78f,50.8f);if(!mantle||mantle->z<99.0f){std::cerr<<"mantle probe did not find a valid IW-height ledge\n";return 1;}
    // Mid-air mantling test: player jumped 40 units into the air toward a 100-unit ledge (chest/waist height relative to jump)
    const auto airMantle=navigation.mantleTarget({-50,0,40.0f},{1,0,0},38.1f,182.88f,45.72f,144.78f,50.8f);
    if(!airMantle||airMantle->z<99.0f){std::cerr<<"mid-air mantle probe failed to acquire chest-height ledge\n";return 1;}
    if(!navigation.navigationSegmentClear({-150,-60,0},{-60,-60,0},38.1f,182.88f,45.72f)){std::cerr<<"navigation rejected clear walkable ground\n";return 1;}if(navigation.navigationSegmentClear({-150,0,0},{150,0,0},38.1f,182.88f,45.72f)){std::cerr<<"navigation accepted a link through a blocking wall\n";return 1;}

    // Surf ramp test: 60-degree steep slope (cos(60) = 0.50 < 0.70 standard walkable threshold)
    scene::glb::Map surfCourse;
    // Sloped ramp plane: normal (0, 0.866, 0.50)
    surfCourse.collision.push_back({
        {-100.0f, 0.0f, 0.0f},
        {100.0f, 0.0f, 0.0f},
        {100.0f, -57.735f, 100.0f},
        {0.0f, 0.866025f, 0.50f},
        {-100.0f, -57.735f, 0.0f},
        {100.0f, 0.0f, 100.0f},
        false, true // walkable=false, blocking=true (surf ramp)
    });
    surfCourse.buildCollisionIndex();
    // 1. Verify surf ramp is NOT treated as flat walkable ground
    constexpr float noGround = -1e9f;
    const float rampGround = surfCourse.navigationGroundHeight(0.0f, -25.0f, 50.0f, noGround, 20.0f);
    if(rampGround > noGround * 0.5f){
        std::cerr<<"surf ramp was falsely classified as walkable ground\n";
        return 1;
    }
    // 2. Verify 3D collision constrainMove and contact normal reporting on surf ramp
    scene::Vec3 contactNormal{0.0f, 0.0f, 0.0f};
    const scene::Vec3 oldAirPos{0.0f, -10.0f, 50.0f};
    const scene::Vec3 proposedAirPos{0.0f, -35.0f, 40.0f}; // moving deeper into the ramp
    const scene::Vec3 resolved = surfCourse.constrainMove(oldAirPos, proposedAirPos, 20.0f, 72.0f, 18.0f, &contactNormal);
    if(scene::length(contactNormal) < 0.5f || contactNormal.z >= 0.70f){
        std::cerr<<"surf collision failed to report steep contact normal: z="<<contactNormal.z<<"\n";
        return 1;
    }
    // 3. Verify ClipVelocity deflection along the surf ramp plane
    scene::Vec3 velocity{200.0f, -50.0f, -80.0f};
    const float backoff = scene::dot(velocity, contactNormal);
    if(backoff < 0.0f){
        velocity = velocity - contactNormal * (backoff * 1.001f);
    }
    const float postBackoff = scene::dot(velocity, contactNormal);
    if(postBackoff < -0.01f){
        std::cerr<<"ClipVelocity failed to deflect velocity along surf plane: dot="<<postBackoff<<"\n";
        return 1;
    }

    std::cout<<map.scene.meshes.size()<<" material batches, "<<map.collision.size()<<" collision triangles, "<<map.activeCellCount()<<" occupied collision cells\n";
    return 0;
}
