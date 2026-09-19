#include "scene/CodmNative.h"
#include "scene/CodmWorldBody.h"
#include <fstream>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace { void require(bool value,const char* message){if(!value)throw std::runtime_error(message);} }
int main(){try{
    scene::CastScene s;scene::Bone bone;bone.restLocal.position={1,2,3};bone.restLocal.scale={2,3,4};bone.restGlobal=scene::trs(bone.restLocal.position,{},bone.restLocal.scale);bone.inverseBind=scene::inverseAffine(bone.restGlobal);s.skeleton.bones.push_back(bone);
    scene::Mesh mesh;scene::Vertex v;v.position={.2f,.3f,.4f};mesh.vertices.push_back(v);s.meshes.push_back(mesh);
    scene::Animation animation;animation.durationFrames=10;scene::Track t;t.property=scene::TrackProperty::TranslationX;t.frames={0,10};t.scalarValues={1,2};animation.tracks.push_back(t);s.animations.push_back(animation);
    scene::codm::normalizeToCentimetres(s);require(s.codmNativeCentimetres,"unit state missing");require(s.meshes[0].vertices[0].position.x==20,"mesh units");require(s.skeleton.bones[0].restLocal.position.x==100,"bind units");require(s.skeleton.bones[0].restLocal.scale.x==2,"authored scale changed");require(s.animations[0].tracks[0].scalarValues[1]==200,"animation units");
    // Use a fresh metre scene rather than scaling an already converted copy.
    scene::CastScene metres;metres.skeleton.bones={bone};metres.meshes={mesh};metres.animations={animation};
    scene::codm::normalizeToCentimetres(metres,scene::codm::presentationScale);
    const float factor=100.f*scene::codm::presentationScale;
    require(std::abs(metres.codmTranslationFactor-factor)<1e-5f,"presentation factor missing");
    require(std::abs(metres.meshes[0].vertices[0].position.x-.2f*factor)<1e-4f,"presentation geometry scale");
    require(std::abs(metres.skeleton.bones[0].restLocal.position.x-factor)<1e-4f,"presentation bind scale");
    require(std::abs(metres.animations[0].tracks[0].scalarValues[1]-2*factor)<1e-4f,"presentation motion scale");
    const auto fittedPose=metres.samplePose(0,5);scene::codm::normalizeToCentimetres(metres,scene::codm::presentationScale);
    require(fittedPose[0].v==metres.samplePose(0,5)[0].v,"presentation applied twice");
    const auto before=s.samplePose(0,5);scene::codm::normalizeToCentimetres(s);const auto after=s.samplePose(0,5);require(before[0].v==after[0].v,"double normalization");
    const auto identity=s.skeleton.bones[0].restGlobal*s.skeleton.bones[0].inverseBind;float inverseError=0;for(int i=0;i<16;++i)inverseError=std::max(inverseError,std::abs(identity.v[i]-(i%5==0?1.f:0.f)));std::cout<<"Inverse bind matrix residual: "<<inverseError<<'\n';require(inverseError<1e-4f,"inverse bind units");
    auto follower=s.skeleton.bones[0];follower.parent=-1;s.skeleton.bones.push_back(follower);s.nativePoseFollowers={{1,0}};
    for(float f=0;f<=10;f+=.25f){const auto pose=s.samplePose(0,f);require(pose[0].v==pose[1].v,"native follower not exact");}
    auto fitted=s;fitted.skeleton.bones.push_back(follower);auto adapter=std::make_shared<scene::CodmRigAdapter>();adapter->firstBone=2;adapter->identity="synthetic regression";adapter->bindings.push_back({0,scene::translation({2,3,4})});fitted.codmRigAdapter=adapter;
    fitted.animations.push_back(fitted.animations.front());fitted.animations.back().tracks[0].scalarValues={4,6};
    for(float alpha:{0.f,.25f,.5f,.75f,1.f}){for(const auto pose:{fitted.sampleBlendedPose(0,2,1,8,alpha),fitted.sampleLayeredPose(0,2,1,8,alpha,scene::LayerMode::Override,false)}){const auto expected=pose[0]*adapter->bindings[0].offset;require(expected.v==pose[2].v,"adapter did not follow native blend/layer");}}
    const auto copy=fitted;require(copy.codmRigAdapter==fitted.codmRigAdapter,"immutable adapter not shared across scene cache copy");
    s.codmNativeCentimetres=false;require(s.samplePose(0,5)[0].v!=s.samplePose(0,5)[1].v,"legacy pose path changed");
    bool rejected=false;try{scene::codm::parseJson("{\"units\":\"metres\",\"units\":\"inches\"}");}catch(...){rejected=true;}require(rejected,"contradictory metadata accepted");
    std::string error;require(!scene::codm::nativeMetres("nonexistent-native-codm-fixture.cast",error)&&!error.empty(),"missing metadata guessed");
    {
        const auto fixture=std::filesystem::temp_directory_path()/"c_codm_player_cadence_unit_regression.cast";
        auto sidecar=fixture;sidecar.replace_extension(".json");
        {std::ofstream out(sidecar);out<<R"({"units":"metres","upAxis":"Z","t6Compatible":false})";}
        scene::CastScene body;body.meshes={mesh};
        for(const auto* name:{"Bip01","b_Hips","b_RightLegUpper","b_LeftHand","b_RightHand"}){auto b=bone;b.name=name;b.parent=-1;const auto i=body.skeleton.bones.size();body.skeleton.boneByName[name]=i;body.skeleton.bones.push_back(b);}
        scene::codm::prepareWorldBody(body,fixture);std::filesystem::remove(sidecar);
        require(body.codmTranslationFactor==100,"body used first-person presentation scale");
        require(std::abs(body.meshes[0].vertices[0].position.x+30)<1e-4f&&std::abs(body.meshes[0].vertices[0].position.y-20)<1e-4f,"world forward-axis conversion");
        require(body.skeleton.boneByCanonicalName.at("j_wrist_ri")==4,"CODM wrist semantic alias");
        const auto beforeBody=body.skeleton.bones[0].restGlobal;const auto beforeVertex=body.meshes[0].vertices[0].position;
        {std::ofstream out(sidecar);out<<R"({"units":"metres","upAxis":"Z","t6Compatible":false})";}
        scene::codm::prepareWorldBody(body,fixture);std::filesystem::remove(sidecar);
        require(beforeBody.v==body.skeleton.bones[0].restGlobal.v&&scene::length(beforeVertex-body.meshes[0].vertices[0].position)==0,"world units/basis applied twice");
        const auto bind=body.skeleton.bones[0].restGlobal*body.skeleton.bones[0].inverseBind;
        for(int i=0;i<16;++i)require(std::abs(bind.v[i]-(i%5==0?1.f:0.f))<1e-3f,"world inverse bind mismatch");
    }
    std::cout<<"Native CODM units, inverse binds, followers, isolation and metadata: PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
