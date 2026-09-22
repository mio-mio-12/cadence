#include "scene/CodmMaterialMetadata.h"
#include <cmath>
#include <stdexcept>

namespace scene::codm {
namespace {
float number(const Json& j,const char* key,float fallback,float low,float high){
    if(!j.contains(key))return fallback;
    const auto& value=j.at(key);
    if(!value.is_number())throw std::runtime_error(std::string("Material numeric field: ")+key);
    const double n=value.get<double>();
    if(!std::isfinite(n)||n<low||n>high)throw std::runtime_error(std::string("Material number out of range: ")+key);
    return static_cast<float>(n);
}
bool flag(const Json& j,const char* key,bool fallback){
    if(!j.contains(key))return fallback;
    if(!j.at(key).is_boolean())throw std::runtime_error(std::string("Material boolean field: ")+key);
    return j.at(key).get<bool>();
}
std::vector<float> vector(const Json& j,const char* key,std::size_t size,std::vector<float> fallback){
    if(!j.contains(key))return fallback;
    const auto& a=j.at(key);if(!a.is_array()||a.size()!=size)throw std::runtime_error(std::string("Material vector: ")+key);
    for(std::size_t i=0;i<size;++i){if(!a[i].is_number())throw std::runtime_error("Invalid material vector number");const double n=a[i].get<double>();if(!std::isfinite(n)||n<0||n>100000)throw std::runtime_error("Material vector outside finite range");fallback[i]=static_cast<float>(n);}
    return fallback;
}
}
std::filesystem::path packageTexturePath(const std::filesystem::path& directory,const std::string& relative){
    const auto p=std::filesystem::u8path(relative);
    if(relative.find('\0')!=std::string::npos||relative.find(':')!=std::string::npos||p.is_absolute()||p.has_root_name()||p.has_root_directory())throw std::runtime_error("Material texture must be package-relative");
    for(const auto& part:p)if(part=="..")throw std::runtime_error("Material texture traversal rejected");
    const auto root=std::filesystem::weakly_canonical(directory), result=std::filesystem::weakly_canonical(root/p);
    const auto remainder=result.lexically_relative(root);
    if(remainder.empty()||remainder.is_absolute())throw std::runtime_error("Material texture outside package");
    for(const auto& part:remainder)if(part=="..")throw std::runtime_error("Material texture symlink escapes package");
    return result;
}
bool applyCodmMaterial(const Json& j,const std::filesystem::path& directory,Mesh& mesh,std::string& error,bool glb){
    try{
        if(!j.is_object())throw std::runtime_error("visualMaterials entry must be object");
        // Validate on a material-only temporary; do not duplicate large geometry arrays.
        Mesh out;
        out.materialPolicyExplicit=true;out.gltfPbr=true;out.normalProfile=3;
        out.weatherNonBlocking=flag(j,"sky",false);
        const auto alpha=j.value("alpha",std::string("OPAQUE"));
        if(alpha!="OPAQUE"&&alpha!="MASK"&&alpha!="BLEND")throw std::runtime_error("Unknown material alpha mode");
        const auto blend=j.value("blend",std::string("alpha"));
        if(blend!="alpha"&&blend!="multiply"&&blend!="additive")throw std::runtime_error("Unknown material blend mode");
        out.decal=flag(j,"decal",false);
        out.alphaTest=flag(j,"alphaTest",alpha=="MASK");
        out.forceAlpha=flag(j,"forceAlpha",alpha=="BLEND");
        out.ignoreAlbedoAlpha=alpha=="OPAQUE"&&!out.forceAlpha&&!out.alphaTest;
        out.decalMultiply=!glb&&flag(j,"decalMultiply",blend=="multiply");
        out.decalAdditive=!glb&&flag(j,"decalAdditive",blend=="additive");
        out.doubleSided=flag(j,"doubleSided",false);out.unlit=flag(j,"unlit",false);
        out.vertexBlendBaked=j.contains("vertexBlendBake");
        out.useVertexColor=!out.vertexBlendBaked&&flag(j,"vertexTint",false);
        out.alphaCutoff=number(j,"cutoff",0.5f,0,1);
        out.materialDepthBias=number(j,"depthBias",0,-1000,1000);
        out.renderQueue=static_cast<int>(number(j,"renderQueue",-1,-1,10000));
        out.sourceBlend=static_cast<int>(number(j,"srcBlend",-1,-1,10));
        out.destinationBlend=static_cast<int>(number(j,"dstBlend",-1,-1,10));
        const auto color=vector(j,"color",4,{1,1,1,1});out.color={color[0],color[1],color[2],color[3]};
        const auto emission=vector(j,"emissive",3,{0,0,0});out.emissiveFactor={emission[0],emission[1],emission[2]};
        out.metallicFactor=number(j,"metallic",0,0,1);out.roughnessFactor=number(j,"roughness",1,0,1);
        if(j.contains("textures")){
            if(!j.at("textures").is_object())throw std::runtime_error("Material textures must be object");
            for(const auto& [role,value]:j.at("textures").items()){
                if(!value.is_string())throw std::runtime_error("Material texture path must be string");
                const auto path=packageTexturePath(directory,value.get<std::string>());
                if(role=="color")out.albedoPath=path;else if(role=="normal")out.normalPath=path;else if(role=="metallic")out.specularPath=path;else if(role=="emissive")out.emissivePath=path;
            }
        }
        if(j.contains("glbColorTexture"))packageTexturePath(directory,j.at("glbColorTexture").get<std::string>());
        if(j.contains("vertexBlendBake")&&j.at("vertexBlendBake").contains("sourceAttributes"))packageTexturePath(directory,j.at("vertexBlendBake").at("sourceAttributes").get<std::string>());
        out.sourceMaterialMetadata=std::make_shared<const std::string>(j.dump());
        if(glb){out.albedoPath=mesh.albedoPath;out.normalPath=mesh.normalPath;out.specularPath=mesh.specularPath;out.emissivePath=mesh.emissivePath;out.color=mesh.color;}
        out.name=std::move(mesh.name);out.materialName=std::move(mesh.materialName);out.vertices=std::move(mesh.vertices);out.indices=std::move(mesh.indices);out.bounds=mesh.bounds;out.modelTransform=mesh.modelTransform;
        if(out.vertexBlendBaked)for(auto& vertex:out.vertices)vertex.color={1,1,1,1};
        mesh=std::move(out);error.clear();return true;
    }catch(const std::exception& e){error=std::string("CODM material: ")+e.what();return false;}
}
bool validateCodmMaterials(const Json& records,const std::filesystem::path& directory,std::string& error){
    if(!records.is_array()||records.size()>65535){error="visualMaterials must be an array of at most 65535 entries";return false;}
    for(const auto& record:records){Mesh mesh;if(!applyCodmMaterial(record,directory,mesh,error))return false;}return true;
}
}
