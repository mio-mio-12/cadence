#pragma once
#include "scene/CastScene.h"
#include "scene/BoundedJson.h"
#include <fstream>

namespace scene::codm {
inline constexpr float presentationScale=51.7f/29.7f;
inline nlohmann::json metadata(const std::filesystem::path& path){
    auto sidecar=path;sidecar.replace_extension(".json");std::error_code ec;
    const auto size=std::filesystem::file_size(sidecar,ec);if(ec||size>64u*1024u*1024u)return {};
    std::ifstream in(sidecar);if(!in)return {};
    try{return parseJson(std::string(std::istreambuf_iterator<char>(in),{}));}catch(...){return {};}
}
inline bool nativeMetres(const std::filesystem::path& path,std::string& error){
    auto j=metadata(path);
    if(!j.is_object()||!j.contains("units")||j["units"]!="metres"||!j.contains("upAxis")||j["upAxis"]!="Z"||!j.contains("t6Compatible")||j["t6Compatible"]!=false||j.contains("t6Compatibility")){
        error="CODM native import requires unconverted metre/Z-up metadata: "+path.filename().string();return false;
    }
    return true;
}
inline void normalizeToCentimetres(CastScene& s,float fitScale=1.f){
    if(s.codmNativeCentimetres)return;
    const float factor=100.f*fitScale;s.codmTranslationFactor=factor;
    for(auto& m:s.meshes){for(auto& v:m.vertices)v.position=v.position*factor;for(int k:{12,13,14})m.modelTransform.v[k]*=factor;}
    for(auto& b:s.skeleton.bones){b.restLocal.position=b.restLocal.position*factor;b.absoluteTranslationOffset=b.absoluteTranslationOffset*factor;for(int k:{12,13,14}){b.restGlobal.v[k]*=factor;b.inverseBind.v[k]*=factor;}}
    for(auto& a:s.animations){for(auto& t:a.tracks)if(t.property<=TrackProperty::TranslationZ)for(auto& v:t.scalarValues)v*=factor;if(a.viewmodelCameraReference)for(int k:{12,13,14})a.viewmodelCameraReference->v[k]*=factor;}
    s.bounds.minimum=s.bounds.minimum*factor;s.bounds.maximum=s.bounds.maximum*factor;
    s.codmNativeCentimetres=true;
}
inline bool assemble(const cast::Document& weapon,const cast::Document& hands,CastScene& out,std::string& error){
    if(!nativeMetres(weapon.sourceName(),error)||!nativeMetres(hands.sourceName(),error))return false;
    auto driver=buildScene(weapon),skin=buildScene(hands);if(driver.meshes.empty()||skin.meshes.empty()){error="CODM native mesh missing";return false;}
    normalizeToCentimetres(driver,presentationScale);normalizeToCentimetres(skin,presentationScale);
    driver.codmNativeWeaponStem=std::filesystem::path(weapon.sourceName()).stem().string();
    for(const auto* s:{&driver,&skin}){
        for(const auto& b:s->skeleton.bones)for(float v:b.restGlobal.v)if(!std::isfinite(v)){error="Non-finite native CODM bind transform";return false;}
        for(const auto& m:s->meshes)for(const auto& v:m.vertices)if(!std::isfinite(v.position.x)||!std::isfinite(v.position.y)||!std::isfinite(v.position.z)){error="Non-finite native CODM geometry";return false;}
    }
    // Separate skeleton branches preserve each mesh's inverse bind. Shared
    // semantic joints follow source globals only after all native layers blend.
    const auto offset=driver.skeleton.bones.size();std::size_t mapped=0;
    for(std::size_t i=0;i<skin.skeleton.bones.size();++i){auto b=skin.skeleton.bones[i];
        if(b.name.starts_with("b_")){
            auto source=driver.skeleton.boneByName.find(b.name);
            if(source==driver.skeleton.boneByName.end()){error="CODM native joint missing: "+b.name;return false;}
            const auto& sb=driver.skeleton.bones[source->second];
            if(b.parent>=0&&skin.skeleton.bones[b.parent].name.starts_with("b_")&&(sb.parent<0||driver.skeleton.bones[sb.parent].name!=skin.skeleton.bones[b.parent].name)){
                error="CODM joint hierarchy mismatch: "+b.name;return false;
            }
            driver.nativePoseFollowers.emplace_back(offset+i,source->second);++mapped;
        }
        b.name="codm_hands|"+b.name;if(b.parent>=0)b.parent+=static_cast<std::int32_t>(offset);
        driver.skeleton.boneByName.emplace(b.name,driver.skeleton.bones.size());driver.skeleton.boneByCanonicalName.emplace(b.name,driver.skeleton.bones.size());driver.skeleton.bones.push_back(std::move(b));
    }
    if(mapped<35){error="CODM native hand correspondence incomplete";return false;}
    for(auto& m:driver.meshes)m.viewmodelWeapon=true;
    for(auto m:skin.meshes){for(auto& v:m.vertices)for(auto& b:v.bones)b+=static_cast<std::uint32_t>(offset);m.viewmodelWeapon=false;driver.meshes.push_back(std::move(m));}
    driver.bounds.valid=false;for(const auto& m:driver.meshes)for(const auto& v:m.vertices){const auto p=transformPoint(m.modelTransform,v.position);if(!driver.bounds.valid){driver.bounds.minimum=driver.bounds.maximum=p;driver.bounds.valid=true;}else{driver.bounds.minimum={std::min(driver.bounds.minimum.x,p.x),std::min(driver.bounds.minimum.y,p.y),std::min(driver.bounds.minimum.z,p.z)};driver.bounds.maximum={std::max(driver.bounds.maximum.x,p.x),std::max(driver.bounds.maximum.y,p.y),std::max(driver.bounds.maximum.z,p.z)};}}
    // Native exported camera space is Z-up looking along -Y. This defines the
    // viewer basis; it does not rotate or reposition the exported assets.
    const auto add=[&](std::string name,Quat rotation){Bone b;b.name=name;b.restLocal.rotation=rotation;b.restGlobal=trs({},rotation,{1,1,1});b.inverseBind=inverseAffine(b.restGlobal);const auto index=driver.skeleton.bones.size();driver.skeleton.boneByName[name]=index;driver.skeleton.boneByCanonicalName[name]=index;driver.skeleton.bones.push_back(b);};
    for(const auto name:{"tag_view","codm_camera_motion","tag_camera"})if(driver.skeleton.boneByName.contains(name)){error="Native CODM viewer bone identity collision: "+std::string(name);return false;}
    add("tag_view",{});add("codm_camera_motion",{});const auto cameraParent=driver.skeleton.bones.size()-1;add("tag_camera",fromEulerRadians({0,0,-kPi*.5f}));driver.skeleton.bones.back().parent=static_cast<std::int32_t>(cameraParent);
    driver.warnings.push_back("Native CODM: metres -> cm (100x) plus user-approved global fit ("+std::to_string(presentationScale)+"x), once; "+std::to_string(mapped)+" verified native hand joints. No legacy rig conversion.");
    out=std::move(driver);return true;
}
}
