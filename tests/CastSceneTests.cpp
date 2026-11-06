#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "scene/MuzzleGeometry.h"
#include "scene/Source2BodyPolicy.h"
#include "scene/TestCourse.h"
#include "scene/T6Magazine.h"
#include "scene/NativeBoneNames.h"
#include "scene/ImportedNative.h"
#include "ImportedNativeHandTests.h"
#include "app/ImportedAnimationPolicy.h"
#include "scene/ColdWarViewmodel.h"
#include "scene/ColdWarLegacyBridge.h"
#include "render/Hbao.h"
#include "render/DepthOfField.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using Bytes=std::vector<std::byte>;

template<class T> void append(Bytes& out,T value){const auto p=out.size();out.resize(p+sizeof(T));std::memcpy(out.data()+p,&value,sizeof(T));}
void appendRaw(Bytes& out,const void* data,std::size_t size){const auto p=out.size();out.resize(p+size);std::memcpy(out.data()+p,data,size);}

Bytes stringProperty(std::string_view name,std::string_view value){
    Bytes out;const char type[2]={'s','\0'};appendRaw(out,type,2);append<std::uint16_t>(out,static_cast<std::uint16_t>(name.size()));append<std::uint32_t>(out,1);
    appendRaw(out,name.data(),name.size());appendRaw(out,value.data(),value.size());append<char>(out,'\0');return out;
}
template<class T> Bytes numericProperty(const char type[2],std::string_view name,std::uint32_t count,const std::vector<T>& values){
    Bytes out;appendRaw(out,type,2);append<std::uint16_t>(out,static_cast<std::uint16_t>(name.size()));append(out,count);appendRaw(out,name.data(),name.size());appendRaw(out,values.data(),values.size()*sizeof(T));return out;
}
Bytes node(std::uint32_t id,std::uint64_t hash,std::vector<Bytes> properties,std::vector<Bytes> children={}){
    std::uint32_t size=24;for(auto& p:properties)size+=static_cast<std::uint32_t>(p.size());for(auto& c:children)size+=static_cast<std::uint32_t>(c.size());
    Bytes out;append(out,id);append(out,size);append(out,hash);append<std::uint32_t>(out,static_cast<std::uint32_t>(properties.size()));append<std::uint32_t>(out,static_cast<std::uint32_t>(children.size()));
    for(auto& p:properties)appendRaw(out,p.data(),p.size());for(auto& c:children)appendRaw(out,c.data(),c.size());return out;
}
Bytes animatedTriangle(std::string_view materialName="test_material",std::string_view boneName="root",std::string_view albedo="textures/test.png",bool pbBody=false){
    const char i[2]={'i','\0'},l[2]={'l','\0'},b[2]={'b','\0'},f[2]={'f','\0'},v2[2]={'2','v'},v3[2]={'3','v'},v4[2]={'4','v'};
    auto bone=node(0x656E6F62,3,{stringProperty("n",boneName),numericProperty(i,"p",1,std::vector<std::uint32_t>{0xffffffffu}),
        numericProperty(v3,"lp",1,std::vector<float>{0,0,0}),numericProperty(v4,"lr",1,std::vector<float>{0,0,0,1}),numericProperty(v3,"s",1,std::vector<float>{1,1,1})});
    std::vector<Bytes> bones{std::move(bone)};
    if(pbBody)for(const auto name:{"Pelvis","L Thigh","R Thigh"})bones.push_back(node(0x656E6F62,100+bones.size(),{stringProperty("n",name),numericProperty(i,"p",1,std::vector<std::uint32_t>{0})}));
    auto skeleton=node(0x6C656B73,2,{}, std::move(bones));
    auto textureFile=node(0x656C6966,11,{stringProperty("p",albedo)});
    auto material=node(0x6C74616D,10,{stringProperty("n",materialName),stringProperty("t","pbr"),numericProperty(l,"diffuse",1,std::vector<std::uint64_t>{11})},{std::move(textureFile)});
    auto mesh=node(0x6873656D,4,{stringProperty("n","triangle"),numericProperty(v3,"vp",3,std::vector<float>{-1,0,0,1,0,0,0,0,2}),
        numericProperty(v2,"u0",3,std::vector<float>{-.25f,.125f,1.75f,.875f,.5f,2.25f}),
        numericProperty(b,"f",3,std::vector<std::uint8_t>{0,1,2}),numericProperty(b,"mi",1,std::vector<std::uint8_t>{1}),
        numericProperty(b,"wb",3,std::vector<std::uint8_t>{0,0,0}),numericProperty(f,"wv",3,std::vector<float>{1,1,1}),numericProperty(l,"m",1,std::vector<std::uint64_t>{10})});
    auto model=node(0x6C646F6D,1,{stringProperty("n","test_model")},{std::move(skeleton),std::move(mesh),std::move(material)});
    auto curve=node(0x76727563,6,{stringProperty("nn","ROOT"),stringProperty("kp","tx"),
        numericProperty(b,"kb",2,std::vector<std::uint8_t>{0,30}),numericProperty(f,"kv",2,std::vector<float>{0,2}),stringProperty("m","absolute")});
    auto relativeCurve=node(0x76727563,7,{stringProperty("nn","root"),stringProperty("kp","ty"),
        numericProperty(b,"kb",2,std::vector<std::uint8_t>{0,30}),numericProperty(f,"kv",2,std::vector<float>{0,2}),stringProperty("m","relative")});
    auto additiveCurve=node(0x76727563,8,{stringProperty("nn","root"),stringProperty("kp","tz"),
        numericProperty(b,"kb",2,std::vector<std::uint8_t>{0,30}),numericProperty(f,"kv",2,std::vector<float>{0,2}),stringProperty("m","additive"),numericProperty(f,"ab",1,std::vector<float>{0.5f})});
    auto rotationCurve=node(0x76727563,9,{stringProperty("nn","root"),stringProperty("kp","rq"),
        numericProperty(b,"kb",2,std::vector<std::uint8_t>{0,30}),numericProperty(v4,"kv",2,std::vector<float>{0,0,0,1,0,0,1,0}),stringProperty("m","absolute")});
    auto notification=node(0x6669746E,12,{stringProperty("n","footstep"),numericProperty(b,"kb",2,std::vector<std::uint8_t>{10,25})});
    auto animation=node(0x6D696E61,5,{stringProperty("n","pb_stand_run_fwd"),numericProperty(f,"fr",1,std::vector<float>{30}),numericProperty(b,"lo",1,std::vector<std::uint8_t>{1})},
        {std::move(curve),std::move(relativeCurve),std::move(additiveCurve),std::move(rotationCurve),std::move(notification)});
    auto root=node(0x746F6F72,0,{}, {std::move(model),std::move(animation)});
    Bytes file;append(file,cast::Document::kMagic);append<std::uint32_t>(file,1);append<std::uint32_t>(file,1);append<std::uint32_t>(file,0);appendRaw(file,root.data(),root.size());return file;
}
bool expect(bool value,const char* message){if(!value)std::cerr<<"FAILED: "<<message<<'\n';return value;}
}

int main(){
    int failures{};
    {
        scene::CastScene gun;scene::Bone bone;bone.name="gun";gun.skeleton.bones.push_back(bone);
        scene::Mesh mesh;for(float y:{2.f,8.f}){scene::Vertex v;v.position={0,y,0};v.bones={0,0,0,0};v.weights={1,0,0,0};mesh.vertices.push_back(v);}gun.meshes.push_back(mesh);
        const std::vector<scene::Mat4> idle{scene::rotation(scene::fromEulerRadians({0,0,-scene::kPi*.5f}))};
        const auto anchor=scene::imported::geometryMuzzle(gun,{1,0,0},{},&idle);
        failures+=!expect(anchor&&scene::length(anchor->local-scene::Vec3{0,8,0})<.001f,"muzzle calibration uses animated forward despite reversed bind axis");
        if(anchor){gun.muzzleAnchors.push_back(*anchor);const std::vector<scene::Mat4> recoil{scene::translation({3,4,5})*idle[0]};const auto p=scene::resolveMuzzlePosition(gun,recoil);failures+=!expect(p&&scene::length(*p-scene::Vec3{11,4,5})<.001f,"calibrated muzzle follows the moving gun bone");}
    }
    {
        scene::CastScene fitted;fitted.skeleton.bones.resize(5);
        std::vector<scene::Transform> local(5);
        local[0].position={10,0,0};local[1].position={30,0,0};local[2].position={0,0,8};
        const auto mutableAdapter=std::make_shared<scene::CodmRigAdapter>();fitted.codmRigAdapter=mutableAdapter;auto& adapter=*mutableAdapter;
        adapter.firstBone=3;adapter.bindings.resize(2);adapter.weaponFitScale=.75f;adapter.weaponFitAnchor=0;adapter.weaponFitCameras={2};
        adapter.positionLinks.push_back({3,0,3,{2,0,0}});adapter.positionLinks.push_back({4,3,3,{5,0,0}});
        const auto pose=fitted.globalPose(local);
        failures+=!expect(std::abs(pose[1].v[12]-25.f)<.0001f&&std::abs(pose[1].v[0]-.75f)<.0001f,"weapon fit scales position and geometry around grip");
        failures+=!expect(std::abs(pose[2].v[14]-8.f)<.0001f&&pose[2].v[0]==1.f,"weapon fit leaves camera unchanged");
        failures+=!expect(std::abs(pose[4].v[12]-pose[3].v[12]-5.f)<.0001f&&pose[3].v[0]==1.f,"weapon fit preserves donor lengths and scale");
        adapter.weaponFitScale=1.5f;const auto enlarged=fitted.globalPose(local);
        failures+=!expect(std::abs(enlarged[1].v[12]-40.f)<.0001f&&std::abs(enlarged[1].v[0]-1.5f)<.0001f,"undersized weapon enlarges around grip");
        failures+=!expect(enlarged[2].v[14]==8.f&&std::abs(enlarged[4].v[12]-enlarged[3].v[12]-5.f)<.0001f,"enlargement preserves camera and donor anatomy");
        adapter.weaponFitScale=1.f;const auto unchanged=fitted.globalPose(local);
        failures+=!expect(unchanged[1].v[12]==30.f&&unchanged[1].v[0]==1.f,"default weapon fit remains identity");
    }
    {
        const auto document=cast::Document::parse(animatedTriangle("test","R Hand","textures/test.png",true));
        auto target=scene::buildScene(document,false);const auto original=target.meshes[0];target.meshes.clear();
        const auto correction=scene::trs({8,3,2},scene::fromEulerRadians({0,0,.7f}),{1,1,1});
        for(auto& bone:target.skeleton.bones){bone.restGlobal=correction*bone.restGlobal;bone.inverseBind=scene::inverseAffine(bone.restGlobal);}
        const auto count=target.skeleton.bones.size();
        scene::appendRigModel(document,target,"PB body variant");
        failures+=!expect(target.skeleton.bones.size()==count,"PB skin rebase keeps recorded bone layout");
        for(size_t v=0;v<original.vertices.size();++v){
            failures+=!expect(scene::length(target.meshes[0].vertices[v].position-scene::transformPoint(correction,original.vertices[v].position))<.0001f,"PB variant uses shared bind before skinning");
            failures+=!expect(target.meshes[0].vertices[v].uv.x==original.vertices[v].uv.x&&target.meshes[0].vertices[v].weights==original.vertices[v].weights,"PB rebase preserves UVs and weights");
        }
        const auto torsoDoc=cast::Document::parse(animatedTriangle("test","Spine1","textures/test.png",true));
        auto torso=scene::buildScene(torsoDoc,false);const auto torsoVertices=torso.meshes[0].vertices;torso.meshes.clear();
        for(auto& bone:torso.skeleton.bones){bone.restGlobal=correction*bone.restGlobal;bone.inverseBind=scene::inverseAffine(bone.restGlobal);}
        scene::appendRigModel(torsoDoc,torso,"PB torso variant");
        for(size_t v=0;v<torsoVertices.size();++v)failures+=!expect(scene::length(torso.meshes[0].vertices[v].position-torsoVertices[v].position)<.0001f,"PB arm correction leaves torso and head proportions unchanged");
    }
    failures+=testImportedNativeHandCorrespondence();
    {
        scene::CastScene s;scene::Bone root;root.name="CharacterWorldSpaceTM";root.parent=-1;root.restGlobal=scene::Mat4::identity();root.inverseBind=scene::Mat4::identity();s.skeleton.bones.push_back(root);s.skeleton.boneByName[root.name]=0;
        scene::Bone camera;camera.name="tag_camera";camera.parent=-1;camera.restLocal.position={0,0,150};camera.restGlobal=scene::translation(camera.restLocal.position);camera.inverseBind=scene::inverseAffine(camera.restGlobal);s.skeleton.bones.push_back(camera);s.skeleton.boneByName[camera.name]=1;
        scene::imported::parentSyntheticViewCamera(s,"gitsfa");
        failures+=!expect(s.skeleton.bones[1].parent==0&&s.skeleton.bones[1].restGlobal.v==camera.restGlobal.v,"synthetic GITS camera follows placement without changing bind");
        std::vector<scene::Transform> local{root.restLocal,s.skeleton.bones[1].restLocal};local[0].position.z=-98;const auto pose=s.globalPose(local);
        failures+=!expect(std::abs(pose[1].v[14]-52)<.0001f,"GITS reload root displacement includes camera");
        s.skeleton.bones[1]=camera;s.skeleton.boneByName["Camera_dummy"]=0;scene::imported::parentSyntheticViewCamera(s,"gitsfa");
        failures+=!expect(s.skeleton.bones[1].parent==-1,"authored GITS camera rigs remain untouched");
    }
    {
        scene::CastScene native;scene::Bone bone;bone.name="root";bone.restLocal.position={3,4,5};bone.restLocal.rotation=scene::fromEulerRadians({.2f,.3f,.4f});bone.restLocal.scale={2,3,4};native.skeleton.bones.push_back(bone);
        scene::Transform mount;mount.position={1,2,3};mount.rotation=scene::fromEulerRadians({.1f,0,0});native.rigMountReferences[0]=mount;
        for(bool aim:{false,true}){scene::Animation a;a.sourceName=aim?"AR_Zoom_Idle.cast":"AR_Idle.cast";a.domain=scene::AnimationDomain::ViewModel;
            scene::Track t;t.boneIndex=0;t.property=scene::TrackProperty::TranslationX;t.frames={0};t.scalarValues={aim?8.f:2.f};a.tracks.push_back(t);
            t.property=scene::TrackProperty::ScaleY;t.scalarValues={aim?5.f:3.f};a.tracks.push_back(t);
            t.property=scene::TrackProperty::Rotation;t.scalarValues.clear();t.rotationValues={scene::fromEulerRadians({aim?.8f:.1f,.2f,.3f})};a.tracks.push_back(t);native.animations.push_back(a);}
        const auto untouched=native;const auto hip=native.sampleLocalPose(0,0)[0],ads=native.sampleLocalPose(1,0)[0];
        assets::Asset asset;asset.game="dirtybomb";asset.name="viewmodel_SK_Wpn_AssaultRifle_04";weapon::Profile profile;
        cadence::imported_actions::populate(native,profile,asset);
        failures+=!expect(native.animations.size()==4&&profile.animations.contains("ads_up")&&profile.animations.contains("ads_down"),"missing imported ADS transitions generated from poses");
        const auto close=[](const scene::Transform& a,const scene::Transform& b){const auto x=scene::trs(a.position,a.rotation,a.scale),y=scene::trs(b.position,b.rotation,b.scale);for(int i=0;i<16;++i)if(std::abs(x.v[i]-y.v[i])>.0001f)return false;return true;};
        if(native.animations.size()==4){
            failures+=!expect(close(native.sampleLocalPose(2,0)[0],hip)&&close(native.sampleLocalPose(2,30)[0],ads)&&close(native.sampleLocalPose(3,0)[0],ads)&&close(native.sampleLocalPose(3,30)[0],hip),"ADS endpoints preserve transforms including scale and mounted channels");
            failures+=!expect(native.animations[2].notifications.empty()&&!native.animations[2].looping,"ADS fallback does not invent events or loop");
        }
        cadence::imported_actions::populate(native,profile,asset);failures+=!expect(native.animations.size()==4,"ADS preparation idempotent");
        failures+=!expect(!cadence::imported_actions::useAdsPose(native,3,false,false),"completed generated ADS cannot mask primary reload");
        failures+=!expect(cadence::imported_actions::useAdsPose(native,3,true,false)&&cadence::imported_actions::useAdsPose(native,3,false,true),"generated ADS remains during engagement or release");
        failures+=!expect(cadence::imported_actions::useAdsPose(native,0,false,false),"authored sparse ADS behavior unchanged");
        auto black=native;black.viewHandsDriverGame="blacksquad";
        black.animations[0].sourceName="SK_AK47_1P__mesh_abc__ZoomOut__export_1.cast";
        failures+=!expect(!cadence::imported_actions::useAdsPose(black,0,false,false),"cached Black Squad hip endpoint cannot mask pullout");
        failures+=!expect(cadence::imported_actions::useAdsPose(black,0,true,false)&&cadence::imported_actions::useAdsPose(black,0,false,true),"Black Squad active ADS and release retained");
        profile={};cadence::imported_actions::populate(native,profile,asset);failures+=!expect(native.animations.size()==4&&profile.animations.contains("ads_up"),"fresh profiles reuse generated ADS indices");
        const auto savedProfile=profile;native=untouched;native.viewHandsDriverGame="dirtybomb";
        cadence::imported_actions::restoreAdsEndpoints(native,savedProfile);
        failures+=!expect(native.animations.size()==4&&close(native.sampleLocalPose(2,30)[0],ads),"saved generated ADS restores from exact endpoints");
        cadence::imported_actions::restoreAdsEndpoints(native,savedProfile);failures+=!expect(native.animations.size()==4,"ADS profile restoration idempotent");
        native=untouched;native.viewHandsDriverGame="dirtybomb";auto unavailable=savedProfile;unavailable.animations["ads_idle"]="missing.cast";cadence::imported_actions::restoreAdsEndpoints(native,unavailable);failures+=!expect(native.animations.size()==2,"missing saved aim endpoint stays missing");
        native=untouched;profile={};profile.stats.instantScope=true;cadence::imported_actions::populate(native,profile,asset);failures+=!expect(native.animations.size()==2,"instant scope unchanged");
        native=untouched;profile={};asset.name="viewmodel_SK_Wpn_SniperRifle_03";cadence::imported_actions::populate(native,profile,asset);failures+=!expect(native.animations.size()==2,"native sniper category excluded regardless of profile archetype");
        native=untouched;profile={};asset.game="cs2";cadence::imported_actions::populate(native,profile,asset);failures+=!expect(native.animations.size()==2,"CS2 excluded from endpoint generation");
        native=untouched;profile={};asset.game="dirtybomb";asset.name="viewmodel_SK_Wpn_AssaultRifle_04";profile.animations["ads_up"]="AR_Zoom_Idle.cast";cadence::imported_actions::populate(native,profile,asset);failures+=!expect(native.animations.size()==2&&profile.animations.at("ads_up")=="AR_Zoom_Idle.cast","explicit ADS mapping not replaced");
    }
    {
        scene::CastScene native;assets::Asset asset;asset.game="csnz";asset.name="viewmodel_v_nataknifed";
        for(const auto name:{"viewmodel_v_nataknifed_melee.cast","viewmodel_v_nataknifed_melee_variant1.cast","diagnostic_native_v_nataknifed_stap_miss2_clip99.cast"}){scene::Animation clip;clip.sourceName=name;clip.domain=scene::AnimationDomain::ViewModel;clip.tracks.emplace_back();native.animations.push_back(std::move(clip));}
        weapon::Profile profile;cadence::imported_actions::populate(native,profile,asset);
        failures+=!expect(profile.animationVariants["melee"].size()==3,"imported knife normalized and native attacks join melee cycle");
        profile.animationVariants["melee"]={"custom.cast"};cadence::imported_actions::populate(native,profile,asset);
        failures+=!expect(profile.animationVariants["melee"]==std::vector<std::string>{"custom.cast"},"explicit melee variants retained");
        failures+=!expect(assets::imported::diagnosticAction("csnz",asset.name,"diagnostic_native_v_nataknifed_stap_miss2_clip99.cast")=="melee","native knife stab alias");
        failures+=!expect(assets::imported::diagnosticAction("csnz",asset.name,"diagnostic_native_v_nataknifed_stap_reload_clip99.cast")!="melee","compound action not guessed as melee");
        failures+=!expect(assets::imported::diagnosticAction("cso2","viewmodel_v_defaultknifecamo1","diagnostic_native_v_defaultknifecamo1_Knife_midslash2_clip99.cast")=="melee","prefixed native midslash action");
        failures+=!expect(assets::imported::diagnosticAction("cso2","viewmodel_v_defaultknifecamo1","diagnostic_native_v_defaultknifecamo1_Knife_stab_miss_clip99.cast")=="melee","prefixed native stab miss action");
    }
    {
        scene::CastScene native;assets::Asset asset;asset.game="cso2";asset.name="viewmodel_v_m27iar";
        for(const auto action:{"idle","fire","reload","draw"})for(const auto variant:{2,1}){
            scene::Animation clip;clip.sourceName="diagnostic_native_v_m27iar_m27iar_"+std::string(action)+"_mag"+std::to_string(variant)+"_clip"+(variant==1?"12345678901234567890":"1")+".cast";
            clip.tracks.emplace_back();native.animations.push_back(std::move(clip));
        }
        weapon::Profile profile;cadence::imported_actions::populate(native,profile,asset);
        for(const auto slot:{"idle","fire","reload","pullout"})failures+=!expect(profile.animations[slot].find("_mag1_")!=std::string::npos,"native action variant remains consistent regardless of opaque clip ID length");
    }
    {
        scene::CastScene native;assets::Asset asset;asset.game="blacksquad";asset.name="SK_ASW338_1P__123";
        for(const auto action:{"Idle","Equip","ADD_Run","ADD_Fire"}){
            scene::Animation clip;clip.sourceName="SK_ASW338_1P__mesh_123__"+std::string(action)+"__Startup_12.cast";
            clip.tracks.emplace_back();native.animations.push_back(std::move(clip));
        }
        weapon::Profile profile;cadence::imported_actions::populate(native,profile,asset);
        failures+=!expect(profile.animations["sprint_loop"].find("ADD_Run")!=std::string::npos,"hashed native sprint binds through semantic action");
        failures+=!expect(profile.animations["pullout"].find("Equip")!=std::string::npos,"hashed native equip binds through semantic action");
        failures+=!expect(profile.animations["fire"].find("ADD_Fire")!=std::string::npos,"hashed native fire binds through semantic action");
        native.animations.clear();asset.game="dirtybomb";asset.name="viewmodel_SK_Wpn_SniperRifle_03_123";
        scene::Animation shared;shared.name="shared: fire";shared.sourceName="SK_Wpn_AssaultRifle_04_Fire_01_123.cast";shared.tracks.emplace_back();native.animations.push_back(shared);
        scene::Animation exact;exact.sourceName="SK_Wpn_SniperRifle_03_Fire_01_Part_01_123.cast";exact.tracks.emplace_back();native.animations.push_back(exact);
        profile={};cadence::imported_actions::populate(native,profile,asset);
        failures+=!expect(profile.animations["fire"]==exact.sourceName,"weapon-specific action outranks shared fallback");
    }
    for(const char* game:{"eldewrito","cs1.6","cz","css","csnz","cso2"}){
        const auto s=scene::buildScene(cast::Document::parse(animatedTriangle(),std::string("fixture/")+game+"/models/viewmodel_rifle_test.cast"),false);
        failures+=!expect(s.meshes.size()==1&&s.meshes[0].vertices[0].uv.x==-.25f&&s.meshes[0].vertices[0].uv.y==.125f&&s.meshes[0].vertices[1].uv.x==1.75f&&s.meshes[0].vertices[2].uv.y==2.25f,"imported games preserve signed tiled UVs without folding or double flipping");
    }
    {
        const auto doc=cast::Document::parse(animatedTriangle("native","root","textures/\xe9\x9b\xa8.png"),"fixture/csnz/models/viewmodel_test.cast");
        try { const auto s=scene::buildScene(doc,false);failures+=!expect(!s.meshes.empty()&&s.meshes[0].albedoPath.filename().u8string()==u8"\u96e8.png","UTF-8 native texture filename preserved"); }
        catch(...){std::cerr<<"UTF-8 native texture import threw\n";++failures;}
        auto s=scene::buildScene(cast::Document::parse(animatedTriangle(),"fixture.cast"),false);
        failures+=!expect(s.sourceModelPath==std::filesystem::path("fixture.cast"),"scene retains source model identity");
        const auto sameNameA=cast::Document::parse(animatedTriangle(),"fixture/cs1.6/models/shared.cast");
        const auto sameNameB=cast::Document::parse(animatedTriangle(),"fixture/cso2/models/shared.cast");
        auto identityScene=scene::buildScene(sameNameA,false);
        scene::appendRigModel(sameNameB,identityScene,"shared");
        failures+=!expect(identityScene.rigParts.back().sourceModelPath==std::filesystem::path(sameNameB.sourceName()),"rig source is not inferred from duplicate display name");
        scene::appendPreparedAttachment(scene::buildScene(sameNameB,false),identityScene,0,"shared");
        failures+=!expect(identityScene.attachments.back().sourceModelPath==std::filesystem::path(sameNameB.sourceName()),"prepared attachment retains original source identity");
        scene::imported::normalizeDew(s);const auto first=s.meshes[0].vertices[0].position;
        const auto firstUv=s.meshes[0].vertices[0].uv;
        failures+=!expect(firstUv.x==-.25f&&firstUv.y==.875f&&s.meshes[0].vertices[1].uv.x==1.75f&&s.meshes[0].vertices[2].uv.y==-1.25f,"dew2cast V conversion preserves signed tiled coordinates");
        scene::imported::normalizeDew(s);failures+=!expect(std::abs(first.x+304.8f)<.001f&&s.meshes[0].vertices[0].position.x==first.x,"Blam normalization is idempotent");
        failures+=!expect(s.meshes[0].vertices[0].uv.x==firstUv.x&&s.meshes[0].vertices[0].uv.y==firstUv.y&&s.meshes[0].vertices[2].uv.y==-1.25f,"dew2cast V conversion is idempotent");
    }
    {
        // MP-prefix reload exports contain full-body channels, but their runtime
        // action layer must leave the locomotion root and legs untouched.
        const char f[2]={'f','\0'},b[2]={'b','\0'};
        auto curve=[&](const char* name){return node(0x76727563,1,{stringProperty("nn",name),stringProperty("kp","tx"),numericProperty(b,"kb",2,std::vector<std::uint8_t>{0,30}),numericProperty(f,"kv",2,std::vector<float>{0,1}),stringProperty("m","absolute")});};
        auto animation=node(0x6d696e61,2,{stringProperty("n","mp_pistol_crouch_reload"),numericProperty(f,"fr",1,std::vector<float>{30})},{curve("j_mainroot"),curve("j_spine4"),curve("j_wrist_ri"),curve("j_knee_le")});
        auto root=node(0x746f6f72,0,{}, {animation});Bytes file;append(file,cast::Document::kMagic);append<std::uint32_t>(file,1);append<std::uint32_t>(file,1);append<std::uint32_t>(file,0);appendRaw(file,root.data(),root.size());
        scene::CastScene actor;
        for(auto [name,parent]:{std::pair{"j_mainroot",-1},std::pair{"j_spine4",0},std::pair{"j_wrist_ri",1},std::pair{"j_knee_le",0}}){scene::Bone bone;bone.name=name;bone.parent=parent;actor.skeleton.boneByName[name]=actor.skeleton.boneByCanonicalName[name]=actor.skeleton.bones.size();actor.skeleton.bones.push_back(bone);}
        scene::appendAnimations(cast::Document::parse(file,"mp_pistol_crouch_reload.cast"),actor);
        failures+=!expect(actor.animations.size()==1,"MP crouch reload imported");
        if(!actor.animations.empty()){const auto& clip=actor.animations[0];failures+=!expect(clip.domain==scene::AnimationDomain::PlayerTorso&&clip.tracks.size()==4,"MP crouch reload uses torso domain");for(const auto& track:clip.tracks)failures+=!expect(track.ownsLayer==(track.boneIndex==1||track.boneIndex==2),"MP reload layer only owns upper body");}
    }
    {
        const auto absent=scene::buildScene(cast::Document::parse(animatedTriangle("m_mtl_char_mp_patch_emblem_faction_opfor_00","root","missing_emblem.png"),"fixture/mwr/models/body.cast"));
        failures+=!expect(absent.meshes.empty(),"absent MWR emblem artwork must not become an opaque white patch");
        // Surface import only checks file availability; decoding belongs to the renderer.
        const auto present=scene::buildScene(cast::Document::parse(animatedTriangle("m_mtl_char_mp_patch_emblem_faction_opfor_00","root",__FILE__),"fixture/mwr/models/body.cast"));
        failures+=!expect(!present.meshes.empty(),"available MWR emblem artwork must be preserved");
        const auto ordinary=scene::buildScene(cast::Document::parse(animatedTriangle("m_mtl_sleeve","root","missing_sleeve.png"),"fixture/mwr/models/body.cast"));
        failures+=!expect(!ordinary.meshes.empty(),"missing ordinary MWR textures must not remove body geometry");
        const auto other=scene::buildScene(cast::Document::parse(animatedTriangle("m_mtl_char_mp_patch_emblem_faction_opfor_00","root","missing_emblem.png"),"fixture/bo2/models/body.cast"));
        failures+=!expect(!other.meshes.empty(),"MWR optional emblem policy is game-scoped");
    }
    {
        // A modular head shares j_head with the body but adds facial bones.
        // Every influence must be corrected into the same destination bind.
        const char i[2]={'i','\0'},b[2]={'b','\0'},f[2]={'f','\0'},v3[2]={'3','v'},v4[2]={'4','v'};
        const auto bone=[&](const char* name,std::uint32_t parent,float z){return node(0x656e6f62,1,{stringProperty("n",name),numericProperty(i,"p",1,std::vector<std::uint32_t>{parent}),numericProperty(v3,"lp",1,std::vector<float>{0,0,z}),numericProperty(v4,"lr",1,std::vector<float>{0,0,0,1})});};
        auto skeleton=node(0x6c656b73,2,{}, {bone("j_head",0xffffffffu,0),bone("j_jaw",0,2),bone("j_chin",1,1)});
        auto mesh=node(0x6873656d,3,{numericProperty(v3,"vp",3,std::vector<float>{-1,0,3,1,0,3,0,0,4}),numericProperty(b,"f",3,std::vector<std::uint8_t>{0,1,2}),numericProperty(b,"mi",1,std::vector<std::uint8_t>{2}),numericProperty(b,"wb",6,std::vector<std::uint8_t>{0,1,0,2,1,2}),numericProperty(f,"wv",6,std::vector<float>{.5f,.5f,.5f,.5f,.5f,.5f})});
        auto model=node(0x6c646f6d,4,{}, {skeleton,mesh});auto root=node(0x746f6f72,0,{}, {model});Bytes file;append(file,cast::Document::kMagic);append<std::uint32_t>(file,1);append<std::uint32_t>(file,1);append<std::uint32_t>(file,0);appendRaw(file,root.data(),root.size());
        scene::CastScene body;scene::Bone head;head.name="j_head";head.parent=-1;head.restLocal.position={0,0,64};head.restGlobal=scene::translation({0,0,64});head.inverseBind=scene::inverseAffine(head.restGlobal);body.skeleton.bones.push_back(head);body.skeleton.boneByName[head.name]=body.skeleton.boneByCanonicalName[head.name]=0;
        auto awBody=body;
        scene::appendRigModel(cast::Document::parse(file),awBody,"mp_head_cormack_LOD0");
        failures+=!expect(awBody.skeleton.bones.size()==3&&awBody.meshes.size()==1,"AW character part merges native bone hierarchy");
        if(awBody.skeleton.bones.size()==3&&awBody.meshes.size()==1){
            failures+=!expect(std::abs(awBody.meshes[0].vertices[0].position.z-67)<.001f,"AW local-space vertices follow destination body bind");
            failures+=!expect(!awBody.skeleton.bones[1].translationTracksAreDeltas&&!awBody.skeleton.bones[2].translationTracksAreDeltas,"AW character joints do not inherit weapon translation-offset semantics");
        }
        scene::appendRigModel(cast::Document::parse(file),body,"head_delta_test");
        failures+=!expect(body.meshes.size()==1&&body.skeleton.bones.size()==3,"modular head adds jaw and nested chin to shared head bone");
        if(body.meshes.size()==1&&body.skeleton.bones.size()==3){
            const auto& vertices=body.meshes.front().vertices;
            failures+=!expect(std::abs(vertices[0].position.z-67)<.001f&&std::abs(vertices[1].position.z-67)<.001f&&std::abs(vertices[2].position.z-68)<.001f,"mixed head/jaw/chin weights receive identical body-space placement");
            failures+=!expect(std::abs(body.skeleton.bones[1].restGlobal.v[14]-66)<.001f&&std::abs(body.skeleton.bones[2].restGlobal.v[14]-67)<.001f,"new facial binds follow remapped parents before vertex correction");
            const auto identity=body.skeleton.bones[2].restGlobal*body.skeleton.bones[2].inverseBind;
            failures+=!expect(std::abs(identity.v[14])<.001f,"new facial inverse bind matches corrected geometry space");
        }
    }
    {
        const auto transparent=scene::buildScene(cast::Document::parse(animatedTriangle("material_hashed","root","$blacktransparent_color.png"),"wpn_t9_test_view.cast"));
        failures+=!expect(transparent.meshes.empty(),"T9 transparent placeholders do not render opaque black decals");
        const auto stencil=scene::buildScene(cast::Document::parse(animatedTriangle("mc_mtl_wpn_t9_test_scope_stencil","root","scope_ramp.png"),"wpn_t9_test_scope_view.cast"));
        failures+=!expect(stencil.meshes.empty(),"T9 stencil ramps do not render as grey albedo cards");
        const auto other=scene::buildScene(cast::Document::parse(animatedTriangle("test","root","$blacktransparent_color.png"),"t6_test.cast"));
        failures+=!expect(!other.meshes.empty(),"T9 material policy is scoped to T9 weapons");
        render::HbaoSettings ao;ao.enabled=true;ao.weaponBackgroundHalo=true;ao.radius=85;ao.power=2;std::stringstream stream;stream<<ao;render::HbaoSettings copy;stream>>copy;
        failures+=!expect(copy.enabled&&copy.weaponBackgroundHalo&&copy.radius==85&&copy.power==2,"HBAO settings round trip");
        copy.radius=-10;copy.steps=100;copy.blurRadius=-1;copy.sanitize();
        failures+=!expect(copy.radius==1&&copy.steps==12&&copy.blurRadius==0,"HBAO bounded cost and valid parameter ranges");
        render::DepthOfFieldSettings dof;dof.enabled=true;dof.nearRadius=3;dof.farRadius=11;dof.hollowness=.7f;std::stringstream ds;ds<<dof;render::DepthOfFieldSettings dc;ds>>dc;
        failures+=!expect(dc.enabled&&dc.nearRadius==3&&dc.farRadius==11&&dc.hollowness==.7f,"independent DOF and bokeh settings round trip");
        dc.samples=100000;dc.downsample=0;dc.gamma=0;dc.sanitize();failures+=!expect(dc.samples==96&&dc.downsample==2&&dc.gamma==.25f,"DOF cost and numeric bounds");
        dc.downsample=1;dc.sanitize();std::stringstream full;full<<dc;render::DepthOfFieldSettings restored;full>>restored;restored.sanitize();failures+=!expect(restored.downsample==1,"full-resolution DOF survives save/load");
    }
    {
        scene::CastScene source,target;
        const auto add=[](scene::CastScene& s,std::string name,int parent){
            if(s.skeleton.boneByName.contains(name))return s.skeleton.boneByName.at(name);
            const auto i=s.skeleton.bones.size();scene::Bone b;b.name=name;b.parent=parent;
            s.skeleton.bones.push_back(b);s.skeleton.boneByName[name]=s.skeleton.boneByCanonicalName[name]=i;return i;
        };
        for(auto* s:{&source,&target}){add(*s,"tag_view",-1);add(*s,"tag_ads",0);add(*s,"tag_torso",1);}
        for(const auto& c:scene::kT9LegacyConstraints){add(source,std::string(c.source),2);if(c.target!="tag_weapon")add(target,std::string(c.target),2);}
        add(source,"tag_weapon",static_cast<int>(source.skeleton.boneByName.at("tag_weapon_right")));
        const auto mount=add(target,"t9_hands|tag_weapon_mount",static_cast<int>(target.skeleton.boneByName.at("j_wrist_ri")));
        const auto gun=add(target,"tag_weapon",static_cast<int>(mount));
        scene::Animation idle;idle.sourceName="vm_test_idle.cast";idle.durationFrames=1;
        scene::Animation ads=idle;ads.sourceName="vm_test_ads.cast";
        scene::Track move;move.boneIndex=1;move.property=scene::TrackProperty::TranslationX;move.mode=scene::TrackMode::Absolute;move.ownsLayer=true;move.frames={0,1};move.scalarValues={0,10};ads.tracks.push_back(move);
        source.animations={idle,ads};target.animations=source.animations;
        failures+=!expect(scene::coldWarLegacyTarget(target.skeleton),"T9 bridge eligibility uses complete legacy anatomy without a game whitelist");
        {auto missing=target.skeleton;missing.boneByCanonicalName.erase("j_wrist_le");failures+=!expect(!scene::coldWarLegacyTarget(missing),"incomplete legacy target rejected");
         missing=target.skeleton;missing.boneByCanonicalName["j_wrist_le"]=missing.bones.size();failures+=!expect(!scene::coldWarLegacyTarget(missing),"invalid legacy target alias rejected");}
        failures+=!expect(scene::retargetColdWarLegacyRange(target,0,source,0),"T9 legacy bridge accepts complete constraint skeleton");
        {
            scene::CastScene iw;iw.animations=source.animations;
            add(iw,"tag_view",-1);add(iw,"tag_ads",0);add(iw,"tag_torso",1);
            const auto spineRoot=add(iw,"j_mainroot",2),spine=add(iw,"j_spineupper",static_cast<int>(spineRoot));
            for(const auto side:{"le","ri"}){
                const auto clav=add(iw,std::string("j_clavicle_")+side,static_cast<int>(spine));
                const auto shoulder=add(iw,std::string("j_shoulder_")+side,static_cast<int>(clav));
                const auto elbow=add(iw,std::string("j_elbow_")+side,static_cast<int>(shoulder));
                add(iw,std::string("j_wrist_")+side,static_cast<int>(elbow));
            }
            for(const auto& b:target.skeleton.bones){
                int parent=b.parent>=0?static_cast<int>(iw.skeleton.boneByName.at(target.skeleton.bones[b.parent].name)):-1;
                if(b.name=="j_shoulder_le"||b.name=="j_shoulder_ri"){
                    const auto root=add(iw,"j_mainroot",2);
                    parent=static_cast<int>(add(iw,"j_spineupper",static_cast<int>(root)));
                    parent=static_cast<int>(add(iw,"j_clavicle_"+b.name.substr(b.name.size()-2),parent));
                }
                add(iw,b.name,parent);
            }
            const auto twist=add(iw,"j_wristfronttwist1_le",2);
            for(const auto side:{"le","ri"}){
                const auto meta=add(iw,std::string("j_metaindex_")+side+"_1",static_cast<int>(iw.skeleton.boneByName.at(std::string("j_wrist_")+side)));
                iw.skeleton.boneByCanonicalName[std::string("j_index_")+side+"_0"]=meta;
            }
            auto bodySource=source;const auto bodyRoot=add(bodySource,"j_mainroot",0);add(bodySource,"j_spineupper",static_cast<int>(bodyRoot));
            failures+=!expect(scene::retargetColdWarLegacyRange(iw,0,bodySource,0),"T9 bridge supports IW calibration composition");
            const auto hip=iw.samplePose(0,0),aim=iw.sampleLayeredPose(0,0,1,1,1,scene::LayerMode::Override,false);
            const auto wrist=iw.skeleton.boneByName.at("j_wrist_le");
            failures+=!expect(std::abs((aim[wrist].v[12]-hip[wrist].v[12])-10.f)<.001f,"Sparse T9 ADS retains calibrated IW arm pose across body-only names");
            for(const float phase:{0.f,1.f}){
                const auto pose=iw.samplePose(1,phase);
                const auto clavicle=iw.skeleton.boneByName.at("j_clavicle_le"),shoulder=iw.skeleton.boneByName.at("j_shoulder_le");
                for(int k=0;k<16;++k)failures+=!expect(std::abs(pose[clavicle].v[k]-pose[shoulder].v[k])<.001f,"IW clavicle follows calibrated shoulder through ADS for identity test bind");
            }
            const auto calibration=scene::legacyToIwCalibration("j_wristfronttwist1_le");
            failures+=!expect(calibration.has_value(),"IW wrist helper has explicit legacy calibration");
            if(calibration){
                const auto legacy=target.skeleton.boneByName.at(calibration->legacyBone);
                const auto expected=target.samplePose(0,0)[legacy]*calibration->offset;
                const auto actual=iw.samplePose(0,0)[twist];
                for(int k=0;k<16;++k)failures+=!expect(std::abs(expected.v[k]-actual.v[k])<.001f,"T9 to IW wrist composes calibrated frames instead of legacy offset alone");
            }
        }
        for(const auto& track:target.animations[1].tracks)if(track.boneIndex==gun)failures+=!expect(!track.ownsLayer,"T9 ADS cannot own grip-relative gun channels across an unmapped mount");
        for(const float weight:{0.f,.5f,1.f}){
            const auto a=source.sampleLayeredPose(0,0,1,1,weight,scene::LayerMode::Override,false),b=target.sampleLayeredPose(0,0,1,1,weight,scene::LayerMode::Override,false);
            const auto sg=source.skeleton.boneByName.at("tag_weapon");for(int k=0;k<16;++k)failures+=!expect(std::abs(a[sg].v[k]-b[gun].v[k])<.001f,"T9 ADS layered gun transform matches source");
        }
        auto other=source;other.animations[0].sourceName="viewmodel_t6_idle.cast";auto output=target;output.animations.resize(1);
        failures+=!expect(!scene::retargetColdWarLegacyRange(output,0,other,0),"T9 conversion excludes non-native animation names");
    }
    {
        scene::CastScene sleeves;
        for(const auto* name:{"j_spineupper","j_clavicle_le","j_clavicle_ri","j_shouldertwist_le","j_hip_le"}){
            scene::Bone b;b.name=name;sleeves.skeleton.boneByName[name]=sleeves.skeleton.bones.size();sleeves.skeleton.bones.push_back(b);
        }
        sleeves.skeleton.bones[1].restGlobal=scene::trs({0,10,0},{},{1,1,1});
        sleeves.skeleton.bones[2].restGlobal=scene::trs({0,-10,0},{},{1,1,1});
        scene::Mesh arm;scene::Vertex v;v.position={0,8,0};v.bones={0,3,0,0};v.weights={.4f,.6f,0,0};arm.vertices.push_back(v);
        v.position.y=-8;arm.vertices.push_back(v);arm.indices={0,1,0};
        auto body=arm;v.bones={4,0,0,0};v.weights={1,0,0,0};body.vertices.push_back(v);
        auto weapon=arm;weapon.viewmodelWeapon=true;sleeves.meshes={arm,body,weapon};
        scene::supportIwLegacySleeves(sleeves);
        failures+=!expect(sleeves.meshes[0].vertices[0].bones[0]==1&&sleeves.meshes[0].vertices[1].bones[0]==2,"IW sleeve support chooses each bind-space side");
        failures+=!expect(sleeves.meshes[1].vertices[0].bones[0]==0&&sleeves.meshes[2].vertices[0].bones[0]==0,"IW sleeve support preserves body and weapon weights");
        failures+=!expect(sleeves.meshes[0].vertices[0].weights==arm.vertices[0].weights&&sleeves.meshes[0].indices==arm.indices,"IW sleeve support preserves weights and geometry");
        scene::supportIwLegacySleeves(sleeves);
        failures+=!expect(sleeves.meshes[0].vertices[0].bones[0]==1,"IW sleeve support is idempotent across appended clips");
    }
    {
        scene::CastScene native;
        const auto bone=[&](const char* name,int parent,scene::Vec3 position){
            scene::Bone b;b.name=name;b.parent=parent;b.restLocal.position=position;
            b.restGlobal=(parent>=0?native.skeleton.bones[parent].restGlobal:scene::Mat4::identity())*scene::translation(position);b.inverseBind=scene::inverseAffine(b.restGlobal);
            const auto i=native.skeleton.bones.size();native.skeleton.boneByName[name]=native.skeleton.boneByCanonicalName[name]=i;native.skeleton.bones.push_back(b);return static_cast<int>(i);
        };
        const int root=bone("tag_origin",-1,{}),head=bone("j_head",root,{0,0,150}),spine=bone("j_spine4",root,{0,0,120});
        const int cl=bone("j_clavicle_le",spine,{-2,6,25}),cr=bone("j_clavicle_ri",spine,{-2,-6,25});
        const int wl=bone("j_wrist_le",cl,{30,35,-35}),wr=bone("j_wrist_ri",cr,{30,-35,-35});
        bone("tag_weapon_left",wl,{8,0,0});bone("tag_weapon_right",wr,{8,0,0});bone("tag_camera",head,{0,0,2});
        scene::Mesh mesh;mesh.skinned=true;scene::Vertex vertex;vertex.bones={static_cast<std::uint32_t>(wr),0,0,0};vertex.weights={1,0,0,0};mesh.vertices.push_back(vertex);native.meshes.push_back(mesh);
        const auto before=native.samplePose(static_cast<std::size_t>(-1),0);const auto originalBones=native.skeleton.bones;
        failures+=!expect(!scene::prepareColdWarViewmodel(native,"c_usa_mp_seal6_viewhands.cast"),"native T9 setup excludes other games");
        failures+=!expect(scene::prepareColdWarViewmodel(native,"c_t9_test_arms_black2_LOD0.cast"),"T9 character-hierarchy arms receive native VM roots");
        const auto after=native.samplePose(static_cast<std::size_t>(-1),0);
        for(std::size_t i=0;i<before.size();++i){
            for(int k=0;k<16;++k)failures+=!expect(std::abs(before[i].v[k]-after[i+3].v[k])<.001f,"native hierarchy preserves original global bind pose");
            failures+=!expect(originalBones[i].inverseBind.v==native.skeleton.bones[i+3].inverseBind.v,"native setup never rebakes skin inverse binds");
        }
        failures+=!expect(native.meshes[0].vertices[0].bones[0]==wr+3,"skin indices follow inserted native roots");
        failures+=!expect(native.skeleton.bones[native.skeleton.boneByCanonicalName.at("tag_camera")].parent==0&&native.skeleton.bones[native.skeleton.boneByCanonicalName.at("tag_weapon_right")].parent==2,"camera and weapon detach from animated body joints");
        failures+=!expect(!scene::prepareColdWarViewmodel(native,"c_t9_test_arms_black2_LOD0.cast"),"native hierarchy setup is idempotent");
        const int legacyMount=bone("tag_weapon",0,{7,0,152});
        scene::appendRigModel(cast::Document::parse(animatedTriangle("test_material","tag_weapon")),native,"wpn_t9_sniper_standard_view_LOD0");
        const auto gun=native.skeleton.boneByCanonicalName.at("tag_weapon");
        failures+=!expect(native.skeleton.bones[gun].parent==legacyMount&&gun!=legacyMount,"T9 weapon keeps a separate bone below the legacy socket");
        failures+=!expect(native.skeleton.bones[gun].inverseBind.v==scene::Mat4::identity().v,"T9 receiver retains its own zero-space inverse bind");
        scene::appendRigModel(cast::Document::parse(animatedTriangle("test_material","tag_clip")),native,"wpn_t9_sniper_standard_mag_view_LOD0");
        const auto& mag=native.skeleton.bones[native.skeleton.boneByCanonicalName.at("tag_clip")];
        failures+=!expect(mag.parent==gun&&!mag.translationTracksAreDeltas&&scene::length(mag.absoluteTranslationOffset)==0,"T9 magazine binds without BO2 offset heuristics");
    }
    failures+=!expect(scene::resolveNativeBoneName("string_63fb05c1549234a3")=="tag_camera","native hashed camera resolves by exact FNV identity");
    failures+=!expect(scene::resolveNativeBoneName("string_8e9a1d23fc2769a2")=="tag_clip","native hashed magazine resolves");
    failures+=!expect(scene::resolveNativeBoneName("string_bc89899caa5c5a20")=="tag_scope","native hashed scope resolves");
    for(const auto name:{"wpn","weapon","tag_camera","string_0000000000000000","string_not_a_hash"})
        failures+=!expect(scene::resolveNativeBoneName(name)==name,"ordinary and unknown bone identities remain untouched");
    const auto hashedCamera=scene::buildScene(cast::Document::parse(animatedTriangle("test_material","string_63fb05c1549234a3")));
    failures+=!expect(hashedCamera.skeleton.boneByName.contains("string_63fb05c1549234a3")&&hashedCamera.skeleton.boneByCanonicalName.contains("tag_camera"),"raw native identity remains available beside canonical camera name");
    failures+=!expect(hashedCamera.skeleton.bones[0].parent==-1&&hashedCamera.skeleton.bones[0].restGlobal.v==scene::Mat4::identity().v,"name decoding never changes native transforms or hierarchy");
    for(const auto name:{"mc_mtl_gen_eye_cornea","mtl_shiny_lense_eye","character_eye_overlay","character_eye_clearcoat"}){
        const auto eye=scene::buildScene(cast::Document::parse(animatedTriangle(name),"player.cast"));
        failures+=!expect(eye.meshes.size()==1&&eye.meshes[0].eyeOverlay&&eye.meshes[0].forceAlpha&&!eye.meshes[0].camoBlend,"eye shell uses clear overlay rendering");
        failures+=!expect(eye.meshes[0].albedoPath.filename()=="test.png","eye shell texture reference preserved");
    }
    for(const auto name:{"mtl_gen_eye_iris_blue","mtl_mp_eye_a","eyeball_diffuse","mtl_mp_head_cormack_eye_shader_l","mtl_mp_eyewear_01a_lense","weapon_glass"}){
        const auto eye=scene::buildScene(cast::Document::parse(animatedTriangle(name),"player.cast"));
        failures+=!expect(eye.meshes.size()==1&&!eye.meshes[0].eyeOverlay,"iris and ordinary glass do not use eye-shell shading");
    }
    failures+=!expect(!scene::isEmissiveMaterialIdentity("mtl_cod4_sas_glove"),"glove material must not be mistaken for a _glo emissive map");
    failures+=!expect(scene::isEmissiveMaterialIdentity("mtl_t6_attach_tritium_red_glo"),"delimited _glo material remains emissive");
    auto document=cast::Document::parse(animatedTriangle(),"animated_triangle.cast");
    failures+=!expect(document.valid(),"fixture parses");auto scene=scene::buildScene(document);
    auto skinVariant=scene;
    skinVariant.meshes[0].albedoPath="orange_camo.png";skinVariant.meshes[0].materialName="other_skin";
    failures+=!expect(scene::sameRigGeometry(scene,skinVariant),"material-only variant duplicates rig geometry");
    skinVariant.meshes[0].vertices[0].position.x+=1;
    failures+=!expect(!scene::sameRigGeometry(scene,skinVariant),"different modular geometry is retained");
    skinVariant=scene;skinVariant.meshes[0].vertices[0].weights[0]=0.5f;
    failures+=!expect(!scene::sameRigGeometry(scene,skinVariant),"different skin weights are not duplicates");
    skinVariant=scene;skinVariant.skeleton.bones[0].inverseBind.v[12]+=1;
    failures+=!expect(!scene::sameRigGeometry(scene,skinVariant),"different bind poses are not duplicates");
    skinVariant=scene;skinVariant.meshes[0].modelTransform.v[12]+=1;
    failures+=!expect(!scene::sameRigGeometry(scene,skinVariant),"different mesh transforms are not duplicates");
    failures+=!expect(scene.meshes.size()==1,"one mesh");failures+=!expect(scene.meshes[0].vertices.size()==3,"three vertices");
    failures+=!expect(scene.meshes[0].skinned,"mesh is skinned");failures+=!expect(scene.skeleton.bones.size()==1,"one bone");
    failures+=!expect(scene.meshes[0].albedoPath.filename()=="test.png","material file reference resolves");
    failures+=!expect(scene.animations.size()==1,"one animation");failures+=!expect(scene.animations[0].durationFrames==30,"duration is 30 frames");
    failures+=!expect(scene.animations[0].role==scene::AnimationRole::Run&&scene.animations[0].stance==scene::Stance::Stand&&scene.animations[0].direction==scene::Direction::Forward,"animation semantics infer from CoD-style name");
    failures+=!expect(scene.animations[0].notifications.size()==1&&scene.animations[0].notifications[0].frames.size()==2,"notification track imports");
    const auto pose=scene.samplePose(0,15.0f);failures+=!expect(pose.size()==1,"pose contains bone");
    failures+=!expect(!pose.empty()&&std::abs(pose[0].v[12]-1.0f)<0.001f,"translation curve interpolates");
    failures+=!expect(!pose.empty()&&std::abs(pose[0].v[13]-1.0f)<0.001f,"relative curve interpolates");
    failures+=!expect(!pose.empty()&&std::abs(pose[0].v[14]-0.5f)<0.001f,"weighted additive curve interpolates");
    failures+=!expect(!pose.empty()&&std::abs(pose[0].v[0])<0.001f,"quaternion curve slerps");
    const auto blended=scene.sampleBlendedPose(999,0,0,15,0.5f);
    failures+=!expect(!blended.empty()&&std::abs(blended[0].v[12]-0.5f)<0.001f,"local poses crossfade safely");
    failures+=!expect(scene::appendAnimations(document,scene)==1&&scene.animations.size()==2,"animations append to a loaded model");
    auto compositeRig=scene::buildScene(document);failures+=!expect(scene::appendRigModel(document,compositeRig,"shared rig")==1,"rig model appends its meshes");
    failures+=!expect(compositeRig.skeleton.bones.size()==1&&compositeRig.meshes.size()==2&&compositeRig.attachments.empty(),"rig merge deduplicates shared bone names without creating a rigid prop");
    failures+=!expect(scene::appendAttachment(document,scene,0,"weapon")==1,"model appends as an attachment");
    failures+=!expect(scene.attachments.size()==1&&scene.meshes.size()==2&&scene.meshes[1].attachmentIndex==0,"attachment tracks its mesh range");
    struct ClassificationCase { const char* name;scene::MotionRole motion;scene::ActionRole action;scene::WeaponClass weapon;scene::Stance stance;scene::Direction direction;scene::AnimationDomain domain; };
    const ClassificationCase classifications[]={
        {"pb_knife_crouch_run_forward",scene::MotionRole::Run,scene::ActionRole::None,scene::WeaponClass::Knife,scene::Stance::Crouch,scene::Direction::Forward,scene::AnimationDomain::PlayerBody},
        {"pb_crouch_shoot_run_left",scene::MotionRole::Run,scene::ActionRole::Fire,scene::WeaponClass::Rifle,scene::Stance::Crouch,scene::Direction::Left,scene::AnimationDomain::PlayerBody},
        {"pt_dw_prone_flinch_back",scene::MotionRole::Idle,scene::ActionRole::Flinch,scene::WeaponClass::DualWield,scene::Stance::Prone,scene::Direction::Backward,scene::AnimationDomain::PlayerTorso},
        {"pb_standmg42gunner_fire_45right_15up",scene::MotionRole::Idle,scene::ActionRole::Fire,scene::WeaponClass::Heavy,scene::Stance::Stand,scene::Direction::Right,scene::AnimationDomain::PlayerBody},
        {"pb_minigun_stand_walk",scene::MotionRole::Walk,scene::ActionRole::None,scene::WeaponClass::Minigun,scene::Stance::Stand,scene::Direction::Any,scene::AnimationDomain::PlayerBody},
        {"pb_hold_idle",scene::MotionRole::Idle,scene::ActionRole::None,scene::WeaponClass::Equipment,scene::Stance::Stand,scene::Direction::Any,scene::AnimationDomain::PlayerBody},
        {"pb_grenade_stand_run_b_2_idle",scene::MotionRole::Transition,scene::ActionRole::None,scene::WeaponClass::Grenade,scene::Stance::Stand,scene::Direction::Backward,scene::AnimationDomain::PlayerBody},
        {"pb_riotshield_crouch_walk_f",scene::MotionRole::Walk,scene::ActionRole::None,scene::WeaponClass::RiotShield,scene::Stance::Crouch,scene::Direction::Forward,scene::AnimationDomain::PlayerBody},
        {"viewmodel_default_putaway",scene::MotionRole::Unknown,scene::ActionRole::Unequip,scene::WeaponClass::Any,scene::Stance::Any,scene::Direction::Any,scene::AnimationDomain::ViewModel},
        {"viewmodel_beretta2023r_fire",scene::MotionRole::Unknown,scene::ActionRole::Fire,scene::WeaponClass::Pistol,scene::Stance::Any,scene::Direction::Any,scene::AnimationDomain::ViewModel},
        {"viewmodel_an94_sprint_loop",scene::MotionRole::Sprint,scene::ActionRole::None,scene::WeaponClass::Rifle,scene::Stance::Any,scene::Direction::Any,scene::AnimationDomain::ViewModel},
        {"vm_bal27_sprint_loop",scene::MotionRole::Sprint,scene::ActionRole::None,scene::WeaponClass::Any,scene::Stance::Any,scene::Direction::Any,scene::AnimationDomain::ViewModel},
        {"vm_bal27_first_time_pullout",scene::MotionRole::Unknown,scene::ActionRole::FirstRaise,scene::WeaponClass::Any,scene::Stance::Any,scene::Direction::Any,scene::AnimationDomain::ViewModel},
        {"viewmodel_ak47_first_time_pullout",scene::MotionRole::Unknown,scene::ActionRole::FirstRaise,scene::WeaponClass::Any,scene::Stance::Any,scene::Direction::Any,scene::AnimationDomain::ViewModel},
        {"va_fm4_reload",scene::MotionRole::Unknown,scene::ActionRole::Reload,scene::WeaponClass::Any,scene::Stance::Any,scene::Direction::Any,scene::AnimationDomain::ViewModel},
        {"h1_wpn_pst_m9_reload",scene::MotionRole::Unknown,scene::ActionRole::Reload,scene::WeaponClass::Any,scene::Stance::Any,scene::Direction::Any,scene::AnimationDomain::ViewModel},
        {"pb_rpg_runjump_land",scene::MotionRole::Land,scene::ActionRole::None,scene::WeaponClass::Launcher,scene::Stance::Stand,scene::Direction::Any,scene::AnimationDomain::PlayerBody},
        {"pb_terrain_slide",scene::MotionRole::Slide,scene::ActionRole::None,scene::WeaponClass::Any,scene::Stance::Stand,scene::Direction::Any,scene::AnimationDomain::PlayerBody},
        {"pb_briefcase_crouch2prone",scene::MotionRole::Transition,scene::ActionRole::None,scene::WeaponClass::Briefcase,scene::Stance::Crouch,scene::Direction::Any,scene::AnimationDomain::PlayerBody}
    };
    for(const auto& item:classifications){scene::Animation classified;scene::classifyAnimationName(item.name,classified);
        const auto correct=classified.motion==item.motion&&classified.action==item.action&&classified.weapon==item.weapon&&classified.stance==item.stance&&classified.direction==item.direction&&classified.domain==item.domain;
        failures+=!expect(correct,item.name);
    }
    for(const char* name:{"pt_rifle_ar_slowfire_t9_male_stand_first_raise.cast","pt_rifle_ar_slowfire_t9_male_stand_first_pullout.cast"}){
        scene::Animation raised;scene::classifyAnimationName(name,raised);
        failures+=!expect(raised.action==scene::ActionRole::FirstRaise,"explicit first raise overrides fire-family label");
    }
    scene::Animation slowFire;scene::classifyAnimationName("pt_rifle_ar_slowfire_t9_male_stand_fire.cast",slowFire);
    failures+=!expect(slowFire.action==scene::ActionRole::Fire,"actual slowfire firing clip remains fire");
    scene::Animation angular;scene::classifyAnimationName("pb_standmg42gunner_aim_45left_15down",angular);
    failures+=!expect(angular.hasAimYaw&&angular.aimYawDegrees==-45&&angular.hasAimPitch&&angular.aimPitchDegrees==-15,"BO2 angular aim sectors classify");
    scene::Animation alert;scene::classifyAnimationName("pb_crouch_alert_pistol",alert);
    failures+=!expect(alert.motion==scene::MotionRole::Idle&&alert.action==scene::ActionRole::None,"BO2 alert pose remains an idle locomotion candidate");
    scene::Animation frontDeath;scene::classifyAnimationName("pb_death_headshot_front",frontDeath);
    failures+=!expect(frontDeath.action==scene::ActionRole::Death&&frontDeath.direction==scene::Direction::Forward,"BO2 death front tag maps to forward locomotion direction");
    scene::Animation rifleShoot;scene::classifyAnimationName("pb_stand_shoot_walk_forward",rifleShoot);
    failures+=!expect(rifleShoot.action==scene::ActionRole::None&&rifleShoot.motion==scene::MotionRole::Walk&&rifleShoot.weapon==scene::WeaponClass::Rifle&&rifleShoot.ads,"combined shoot-walk is sustained aiming locomotion, not one-shot fire");
    scene::Animation shuffle;scene::classifyAnimationName("pb_rifle_stand_shuffle_f",shuffle);
    failures+=!expect(shuffle.motion==scene::MotionRole::Walk&&shuffle.direction==scene::Direction::Forward&&!shuffle.contextual,"rifle shuffle is walk, not idle");
    scene::Animation slope;scene::classifyAnimationName("pb_rifle_stand_run_slopedown",slope);
    failures+=!expect(slope.motion==scene::MotionRole::Run&&slope.contextual,"slope run is contextual");
    {scene::CastScene selection;
     for(const char* name:{"pb_rifle_swim_move_over_f","pb_rifle_dodge_run_long_f","pb_atv_passenger_fire","pb_rifle_zipline_idle","pb_rifle_stand_assassination_back_v1","pb_rifle_endgame_1stplace_idle"}){
        scene::Animation a;scene::classifyAnimationName(name,a);a.sourceName=name;failures+=!expect(a.contextual,"scripted body action excluded from ordinary locomotion");selection.animations.push_back(a);
     }
     scene::Animation idle;scene::classifyAnimationName("pb_rifle_stand_idle_lowready",idle);selection.animations.push_back(idle);
     scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Idle;q.weapon=scene::WeaponClass::Rifle;q.requireMappedTracks=false;
     failures+=!expect(scene::findBestAnimation(selection,q)==selection.animations.size()-1,"ordinary idle skips swim zipline and podium clips");
     selection.animations.resize(1);q.allowContextual=true;
     failures+=!expect(scene::findBestAnimation(selection,q)==0,"explicit contextual lookup retains special clips");
     scene::Animation view;scene::classifyAnimationName("viewmodel_atv_idle",view);failures+=!expect(!view.contextual,"body context filter leaves viewmodel clips unchanged");}
    scene::Animation ballisticIdle;scene::classifyAnimationName("pb_b_knife_stand_idle",ballisticIdle);
    failures+=!expect(ballisticIdle.weapon==scene::WeaponClass::BallisticKnife&&ballisticIdle.direction==scene::Direction::Any,"ballistic prefix is not backward direction");
    for(const char* suffix:{"f","b","l","r"})for(const char* ending:{"",".cast","_variant"}){
        scene::Animation idleVariant;scene::classifyAnimationName(std::string("mp_stand_idle_twitch_")+suffix+ending,idleVariant);
        failures+=!expect(idleVariant.motion==scene::MotionRole::Idle&&idleVariant.direction==scene::Direction::Any,"single-letter idle variants are not directional");
    }
    {scene::Animation explicitIdle,walkBack,runBack;
     scene::classifyAnimationName("mp_stand_idle_backward",explicitIdle);
     scene::classifyAnimationName("pb_stand_walk_b",walkBack);
     scene::classifyAnimationName("pb_stand_run_b.cast",runBack);
     failures+=!expect(explicitIdle.direction==scene::Direction::Backward&&walkBack.direction==scene::Direction::Backward&&runBack.direction==scene::Direction::Backward,"explicit idle directions and abbreviated locomotion directions remain valid");
     scene::CastScene idleSelection;
     scene::Animation twitch,neutral;
     scene::classifyAnimationName("mp_stand_idle_twitch_b",twitch);twitch.looping=false;
     scene::classifyAnimationName("mp_stand_idle",neutral);neutral.looping=true;
     idleSelection.animations={twitch,neutral};
     scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Idle;q.direction=scene::Direction::Backward;q.requireMappedTracks=false;
     failures+=!expect(scene::findBestAnimation(idleSelection,q)==1,"backward idle query prefers neutral loop over twitch variant b");}
    struct TorsoCase {const char* name;scene::ActionRole action;scene::WeaponClass weapon;scene::Stance stance;bool ads;};
    const TorsoCase torsoCases[]={
        {"pt_stand_shoot_ads_pistol",scene::ActionRole::Fire,scene::WeaponClass::Pistol,scene::Stance::Stand,true},
        {"pt_prone_shoot_auto",scene::ActionRole::Fire,scene::WeaponClass::Automatic,scene::Stance::Prone,false},
        {"pt_m1216_stand_reload",scene::ActionRole::Reload,scene::WeaponClass::M1216,scene::Stance::Stand,false},
        {"pt_lmg_crouch_reload",scene::ActionRole::Reload,scene::WeaponClass::LMG,scene::Stance::Crouch,false},
        {"pt_crossbow_prone_reload",scene::ActionRole::Reload,scene::WeaponClass::Crossbow,scene::Stance::Prone,false},
        {"pt_b_knife_stand_fire",scene::ActionRole::Fire,scene::WeaponClass::BallisticKnife,scene::Stance::Stand,false},
        {"pt_grenade_stand_prime",scene::ActionRole::GrenadePrep,scene::WeaponClass::Grenade,scene::Stance::Stand,false},
        {"pt_tomahawk_crouch_throw",scene::ActionRole::Throw,scene::WeaponClass::Grenade,scene::Stance::Crouch,false},
        {"pt_rifle_stand_raise",scene::ActionRole::Equip,scene::WeaponClass::Rifle,scene::Stance::Stand,false},
        {"pt_sniper_stand_firstraise",scene::ActionRole::FirstRaise,scene::WeaponClass::Sniper,scene::Stance::Stand,false},
        {"pt_riotshield_stand_deploy_fire",scene::ActionRole::Deploy,scene::WeaponClass::RiotShield,scene::Stance::Stand,false}
    };
    for(const auto& item:torsoCases){scene::Animation classified;scene::classifyAnimationName(item.name,classified);
        failures+=!expect(classified.domain==scene::AnimationDomain::PlayerTorso&&classified.action==item.action&&classified.weapon==item.weapon&&classified.stance==item.stance&&classified.ads==item.ads,item.name);}
    scene::Animation glReload;scene::classifyAnimationName("pt_rifle_stand_reload_gl",glReload);
    failures+=!expect(glReload.weapon==scene::WeaponClass::Rifle&&glReload.reloadStyle==scene::ReloadStyle::GL,"rifle GL reload style classifies independently");
    scene::Animation judgeReload;scene::classifyAnimationName("pt_judge_stand_reload",judgeReload);
    failures+=!expect(judgeReload.weapon==scene::WeaponClass::Judge,"Judge reload does not collide with generic shotgun");
    scene::CastScene catalog;
    scene::Animation generic;scene::classifyAnimationName("pb_stand_idle",generic);generic.looping=true;generic.tracks.push_back({});catalog.animations.push_back(generic);
    scene::Animation specific;scene::classifyAnimationName("pb_pistol_stand_idle_forward",specific);specific.looping=true;specific.tracks.push_back({});catalog.animations.push_back(specific);
    scene::Animation unmapped;scene::classifyAnimationName("pb_pistol_stand_idle_forward",unmapped);catalog.animations.push_back(unmapped);
    scene::Animation firing;scene::classifyAnimationName("pb_pistol_stand_fire",firing);firing.tracks.push_back({});catalog.animations.push_back(firing);
    scene::AnimationQuery query;query.motion=scene::MotionRole::Idle;query.weapon=scene::WeaponClass::Pistol;query.stance=scene::Stance::Stand;query.direction=scene::Direction::Forward;
    const auto locomotion=scene::findBestAnimation(catalog,query);
    failures+=!expect(locomotion&&*locomotion==1,"resolver favors the exact mapped locomotion clip");
    query.action=scene::ActionRole::Fire;const auto fire=scene::findBestAnimation(catalog,query);
    failures+=!expect(fire&&*fire==3,"resolver requires the requested weapon action");
    scene::Animation adsFiring=firing;adsFiring.ads=true;catalog.animations.push_back(adsFiring);query.ads=true;
    const auto adsFire=scene::findBestAnimation(catalog,query);failures+=!expect(adsFire&&*adsFire==4,"ADS fire resolves only an ADS-tagged torso variant");query.ads=false;
    query.action=scene::ActionRole::Reload;failures+=!expect(!scene::findBestAnimation(catalog,query),"resolver reports an unavailable action");
    scene::CastScene reloadCatalog;scene::Animation standardReload;scene::classifyAnimationName("pt_reload_stand_rifle",standardReload);standardReload.tracks.push_back({});reloadCatalog.animations.push_back(standardReload);
    scene::Animation glVariant;scene::classifyAnimationName("pt_rifle_stand_reload_gl",glVariant);glVariant.tracks.push_back({});reloadCatalog.animations.push_back(glVariant);
    scene::AnimationQuery reloadQuery;reloadQuery.domain=scene::AnimationDomain::PlayerTorso;reloadQuery.action=scene::ActionRole::Reload;reloadQuery.weapon=scene::WeaponClass::Rifle;reloadQuery.stance=scene::Stance::Stand;
    const auto standardMatch=scene::findBestAnimation(reloadCatalog,reloadQuery);failures+=!expect(standardMatch&&*standardMatch==0,"standard reload does not select an attachment-specific variant");
    reloadQuery.reloadStyle=scene::ReloadStyle::GL;const auto glMatch=scene::findBestAnimation(reloadCatalog,reloadQuery);failures+=!expect(glMatch&&*glMatch==1,"reload mechanism selects the requested GL variant");
    {
        scene::CastScene bodyReloads;
        for(const auto* name:{"mp_stand_bullpup_reload.cast","mp_stand_reload.cast"}){
            scene::Animation clip;scene::classifyAnimationName(name,clip);clip.tracks.push_back({});bodyReloads.animations.push_back(clip);
        }
        failures+=!expect(bodyReloads.animations[0].reloadStyle==scene::ReloadStyle::RearClip,"body bullpup reload is rear-magazine style");
        auto q=reloadQuery;q.reloadStyle=scene::ReloadStyle::Any;
        failures+=!expect(scene::findBestAnimation(bodyReloads,q)==1,"unspecified body reload prefers standard over alphabetically first bullpup");
        q.reloadStyle=scene::ReloadStyle::RearClip;
        failures+=!expect(scene::findBestAnimation(bodyReloads,q)==0,"explicit rear-magazine reload retained");
        bodyReloads.animations.pop_back();q.reloadStyle=scene::ReloadStyle::Any;
        failures+=!expect(scene::findBestAnimation(bodyReloads,q)==0,"specialized-only body reload remains available");
        q.reloadStyle=scene::ReloadStyle::Standard;
        failures+=!expect(!scene::findBestAnimation(bodyReloads,q),"explicit standard request does not silently select bullpup");
    }
    scene::CastScene layered;scene::Bone root;root.name="root";root.parent=-1;scene::Bone child;child.name="spine";child.parent=0;child.restLocal.position={1,0,0};
    layered.skeleton.bones={root,child};scene::Animation baseLayer,actionLayer;
    scene::Track baseRoot;baseRoot.boneIndex=0;baseRoot.property=scene::TrackProperty::TranslationX;baseRoot.frames={0};baseRoot.scalarValues={5};baseLayer.tracks.push_back(baseRoot);
    scene::Track baseChild;baseChild.boneIndex=1;baseChild.property=scene::TrackProperty::TranslationX;baseChild.frames={0};baseChild.scalarValues={2};baseLayer.tracks.push_back(baseChild);
    scene::Track actionRoot=baseRoot;actionRoot.frames={0,1};actionRoot.scalarValues={0,10};actionLayer.tracks.push_back(actionRoot);
    scene::Track actionChild=baseChild;actionChild.frames={0,1};actionChild.scalarValues={1,3};actionLayer.tracks.push_back(actionChild);actionLayer.durationFrames=1;
    layered.animations={baseLayer,actionLayer};const auto layeredPose=layered.sampleLayeredPose(0,0,1,1,1,scene::LayerMode::Additive,true);
    failures+=!expect(layeredPose.size()==2&&std::abs(layeredPose[0].v[12]-5.0f)<0.001f,"layer suppresses action root translation");
    failures+=!expect(layeredPose.size()==2&&std::abs(layeredPose[1].v[12]-9.0f)<0.001f,"layer applies authored child translation relative to frame zero");
    scene::Animation secondAction=actionLayer;layered.animations.push_back(secondAction);
    const auto stackedPose=layered.sampleLayerStack(0,0,{{1,1,1,scene::LayerMode::Additive,true},{2,1,1,scene::LayerMode::Additive,true}});
    failures+=!expect(stackedPose.size()==2&&std::abs(stackedPose[0].v[12]-5.0f)<0.001f,"animation stack suppresses root motion for every retained layer");
    failures+=!expect(stackedPose.size()==2&&std::abs(stackedPose[1].v[12]-11.0f)<0.001f,"animation stack composes multiple partial-pose layers in order");
    const auto overridePose=layered.sampleLayeredPose(0,0,1,1,1,scene::LayerMode::Override,true);
    failures+=!expect(overridePose.size()==2&&std::abs(overridePose[1].v[12]-8.0f)<0.001f,"override layer uses the authored partial-pose channel instead of adding a rest delta");
    scene::Animation incoming=actionLayer;incoming.tracks[1].scalarValues={5,7};layered.animations.push_back(incoming);
    const auto interruptedPose=layered.samplePoseSlots(0,0,{{{{1,1,0.75f,scene::LayerMode::Override,true},{3,1,0.25f,scene::LayerMode::Override,true}}}});
    failures+=!expect(interruptedPose.size()==2&&std::abs(interruptedPose[1].v[12]-9.0f)<0.001f,"same-slot interruption normalizes outgoing and incoming absolute poses");
    scene::Animation torsoOnly;scene::Track torsoTrack=baseChild;torsoTrack.frames={0};torsoTrack.scalarValues={9};torsoOnly.tracks.push_back(torsoTrack);layered.animations.push_back(torsoOnly);
    const auto disjointSlots=layered.samplePoseSlots(0,0,{{{{1,1,1,scene::LayerMode::Override,true}}},{{{4,0,1,scene::LayerMode::Override,true}}}});
    failures+=!expect(disjointSlots.size()==2&&std::abs(disjointSlots[1].v[12]-14.0f)<0.001f,"later disjoint slot owns only its authored channels");
    const auto mixedSlots=layered.samplePoseSlots(0,0,{{{{3,1,1,scene::LayerMode::Override,true}}},{{{1,1,0.5f,scene::LayerMode::Additive,true}}}});
    failures+=!expect(mixedSlots.size()==2&&std::abs(mixedSlots[0].v[12]-5.0f)<0.001f&&std::abs(mixedSlots[1].v[12]-13.0f)<0.001f,"mixed pose slots preserve override pose and additive frame-zero delta");
    const auto baseReference=layered.sampleLocalPose(0,0);const auto rootRelativeStart=layered.sampleRootRelativePose(1,0,baseReference),rootRelativeEnd=layered.sampleRootRelativePose(1,1,baseReference);
    failures+=!expect(rootRelativeStart.size()==2&&std::abs(rootRelativeStart[0].v[12]-5.0f)<0.001f,"full-body action starts at the outgoing root position");
    failures+=!expect(rootRelativeEnd.size()==2&&std::abs(rootRelativeEnd[0].v[12]-15.0f)<0.001f,"full-body action preserves authored root delta without recentering");
    scene::CastScene retargeted;scene::Bone targetHand;targetHand.name="tag_camera";targetHand.restLocal.position={10,2,3};retargeted.skeleton.bones.push_back(targetHand);retargeted.skeleton.boneByName["tag_camera"]=0;retargeted.skeleton.boneByCanonicalName["tag_camera"]=0;scene::Animation foreignAnimation;scene::Track foreignTrack;foreignTrack.boneIndex=0;foreignTrack.property=scene::TrackProperty::TranslationX;foreignTrack.frames={0,1};foreignTrack.scalarValues={1,3};foreignAnimation.tracks.push_back(foreignTrack);retargeted.animations.push_back(foreignAnimation);scene::Skeleton sourceHands;scene::Bone sourceHand;sourceHand.name="tag_camera";sourceHand.restLocal.position={1,2,3};sourceHands.bones.push_back(sourceHand);sourceHands.boneByName["tag_camera"]=0;sourceHands.boneByCanonicalName["tag_camera"]=0;scene::retargetAnimationRange(retargeted,0,sourceHands);
    failures+=!expect(std::abs(retargeted.animations[0].tracks[0].scalarValues[0]-10.0f)<0.001f&&std::abs(retargeted.animations[0].tracks[0].scalarValues[1]-12.0f)<0.001f,"cross-game absolute channels preserve motion around selected viewhands bind pose");
    scene::CastScene mayaTarget;scene::Bone tTorso;tTorso.name="tag_torso";mayaTarget.skeleton.bones.push_back(tTorso);mayaTarget.skeleton.boneByName["tag_torso"]=0;mayaTarget.skeleton.boneByCanonicalName["tag_torso"]=0;scene::Bone tWeapon;tWeapon.name="tag_weapon";tWeapon.parent=0;tWeapon.restLocal.position={50,0,15};mayaTarget.skeleton.bones.push_back(tWeapon);mayaTarget.skeleton.boneByName["tag_weapon"]=1;mayaTarget.skeleton.boneByCanonicalName["tag_weapon"]=1;scene::Animation dummyAnim;dummyAnim.durationFrames=1;mayaTarget.animations.push_back(dummyAnim);scene::CastScene mayaSource;scene::Bone sTorso;sTorso.name="tag_torso";mayaSource.skeleton.bones.push_back(sTorso);mayaSource.skeleton.boneByName["tag_torso"]=0;mayaSource.skeleton.boneByCanonicalName["tag_torso"]=0;scene::Bone sWeapon;sWeapon.name="tag_weapon";sWeapon.parent=0;mayaSource.skeleton.bones.push_back(sWeapon);mayaSource.skeleton.boneByName["tag_weapon"]=1;mayaSource.skeleton.boneByCanonicalName["tag_weapon"]=1;scene::Animation sAnim;sAnim.durationFrames=1;scene::Track sTr;sTr.boneIndex=1;sTr.property=scene::TrackProperty::TranslationZ;sTr.frames={0,1};sTr.scalarValues={19.17f,20.0f};sAnim.tracks.push_back(sTr);mayaSource.animations.push_back(sAnim);scene::retargetAnimationRange(mayaTarget,0,mayaSource,0);const auto mayaSample=mayaTarget.samplePose(0,0.0f);
    failures+=!expect(mayaSample.size()==2&&std::abs(mayaSample[1].v[14]-19.17f)<0.01f,"maya constraint retargeting transfers animated bone pose accurately");
    scene::CastScene mechanism;scene::Bone boltBone;boltBone.name="tag_bolt";mechanism.skeleton.bones.push_back(boltBone);scene::Animation rechamberClip,adsDownClip;scene::Track boltTravel;boltTravel.boneIndex=0;boltTravel.property=scene::TrackProperty::TranslationX;boltTravel.frames={0};boltTravel.scalarValues={-5.9f};rechamberClip.tracks.push_back(boltTravel);scene::Track adsBolt=boltTravel;adsBolt.scalarValues={0};adsDownClip.tracks.push_back(adsBolt);mechanism.animations={rechamberClip,adsDownClip};const auto protectedBolt=mechanism.samplePoseSlots(0,0,{{{{1,0,1,scene::LayerMode::Override,false,true}}}});
    failures+=!expect(protectedBolt.size()==1&&std::abs(protectedBolt[0].v[12]+5.9f)<0.001f,"ADS pose cannot overwrite a concurrently authored weapon mechanism channel");
    scene::Animation integratedBolt;integratedBolt.durationFrames=8;scene::Track integratedTravel=boltTravel;integratedTravel.frames={0,1,2,3,4,5,6,7,8};integratedTravel.scalarValues={0,2,4,2,0,0,0,0,0};integratedBolt.tracks={integratedTravel};mechanism.animations.push_back(integratedBolt);
    const auto readyFrame=scene::mechanismReadyFrame(mechanism,2,"bolt");
    failures+=!expect(readyFrame&&std::abs(*readyFrame-4.0f)<0.001f,"integrated bolt fire becomes ready when the mechanism reaches and remains at its closed pose");
    scene::Animation wrongStance=firing;wrongStance.stance=scene::Stance::Crouch;scene::CastScene wrongCatalog;wrongCatalog.animations.push_back(wrongStance);
    query.action=scene::ActionRole::Fire;failures+=!expect(!scene::findBestAnimation(wrongCatalog,query),"action resolver does not cross stance boundaries");
    scene::Animation heavyFire=firing;heavyFire.weapon=scene::WeaponClass::Heavy;scene::CastScene heavyCatalog;heavyCatalog.animations.push_back(heavyFire);
    query.weapon=scene::WeaponClass::Any;failures+=!expect(!scene::findBestAnimation(heavyCatalog,query),"generic action request does not select a special-weapon clip");
    query.weapon=scene::WeaponClass::Heavy;failures+=!expect(scene::findBestAnimation(heavyCatalog,query).has_value(),"explicit weapon request selects its action family");
    scene::CastScene combinedFire;rifleShoot.tracks.push_back({});combinedFire.animations.push_back(rifleShoot);scene::AnimationQuery rifleFireQuery;
    rifleFireQuery.action=scene::ActionRole::Fire;rifleFireQuery.weapon=scene::WeaponClass::Rifle;rifleFireQuery.motion=scene::MotionRole::Idle;rifleFireQuery.direction=scene::Direction::Forward;
    failures+=!expect(!scene::findBestAnimation(combinedFire,rifleFireQuery),"idle fire does not substitute a combined walk-shoot clip");
    rifleFireQuery.motion=scene::MotionRole::Walk;failures+=!expect(!scene::findBestAnimation(combinedFire,rifleFireQuery),"sustained shoot-walk is not restarted as a one-shot fire action");
    rifleFireQuery.action=scene::ActionRole::None;failures+=!expect(scene::findBestAnimation(combinedFire,rifleFireQuery).has_value(),"shoot-walk resolves as sustained rifle locomotion");
    scene::CastScene locomotionCatalog;shuffle.tracks.push_back({});ballisticIdle.motion=scene::MotionRole::Walk;ballisticIdle.tracks.push_back({});locomotionCatalog.animations={ballisticIdle,shuffle};
    scene::AnimationQuery walkQuery;walkQuery.motion=scene::MotionRole::Walk;walkQuery.weapon=scene::WeaponClass::Rifle;walkQuery.stance=scene::Stance::Stand;walkQuery.direction=scene::Direction::Forward;
    const auto rifleWalk=scene::findBestAnimation(locomotionCatalog,walkQuery);failures+=!expect(rifleWalk&&*rifleWalk==1,"locomotion resolver cannot leak across weapon families");
    scene::CastScene deaths;scene::Animation leftDeath;scene::classifyAnimationName("pb_death_run_left",leftDeath);leftDeath.tracks.push_back({});
    scene::Animation rightDeath;scene::classifyAnimationName("pb_death_run_right",rightDeath);rightDeath.tracks.push_back({});deaths.animations={leftDeath,rightDeath};
    scene::AnimationQuery deathQuery;deathQuery.motion=scene::MotionRole::Run;deathQuery.action=scene::ActionRole::Death;deathQuery.direction=scene::Direction::Right;
    const auto resolvedDeath=scene::findBestAnimation(deaths,deathQuery);failures+=!expect(resolvedDeath&&*resolvedDeath==1,"death resolver follows locomotion direction tag");
    failures+=!expect(std::abs(scene::course::groundHeight(scene::course::scaled(256),scene::course::scaled(-272))-scene::course::scaled(32.0f))<0.001f,"test-course ramp has a continuous 64-IW-unit rise");
    const auto blocked=scene::course::constrainMove({scene::course::scaled(200),0,0},{scene::course::scaled(250),0,0});failures+=!expect(std::abs(blocked.x-scene::course::scaled(200))<0.001f,"controller cannot walk through a 72-IW-unit obstacle");
    const auto slide=scene::course::constrainMove({scene::course::scaled(225),scene::course::scaled(-90),0},{scene::course::scaled(225),scene::course::scaled(-70),0});failures+=!expect(slide.y>scene::course::scaled(-90),"controller can slide along an expanded obstacle boundary without sticking");
    const auto bounded=scene::course::constrainMove({scene::course::scaled(2000),0,0},{scene::course::scaled(3000),0,0});failures+=!expect(bounded.x<=scene::course::kHalfExtent-scene::course::kPlayerRadius,"controller remains inside course boundary");

    scene::Animation mpMantle;
    scene::classifyAnimationName("mp_mantle_up_56", mpMantle);
    failures+=!expect(mpMantle.domain==scene::AnimationDomain::PlayerBody&&mpMantle.motion==scene::MotionRole::Climb,"mp_ prefix is classified as PlayerBody Climb");

    scene::Animation ladderClimb;
    scene::classifyAnimationName("pb_climb_ladder_up", ladderClimb);
    failures+=!expect(ladderClimb.motion==scene::MotionRole::Ladder,"pb_climb_ladder_up is classified as Ladder, strictly not Climb");

    scene::Animation miniGun;
    scene::classifyAnimationName("pb_mini_stand_run_f", miniGun);
    failures+=!expect(miniGun.weapon==scene::WeaponClass::Minigun&&miniGun.motion==scene::MotionRole::Run&&miniGun.direction==scene::Direction::Forward,"pb_mini is classified as Minigun Forward Run");

    scene::Animation exoKnifeJump;
    scene::classifyAnimationName("pb_standjump_boost_takeoff_knife", exoKnifeJump);
    failures+=!expect(exoKnifeJump.motion==scene::MotionRole::Jump&&exoKnifeJump.weapon==scene::WeaponClass::Knife,"pb_standjump_boost_takeoff_knife is classified as Jump Knife");

    scene::Animation diagRun;
    scene::classifyAnimationName("pb_combatrun_fl_smg", diagRun);
    failures+=!expect(diagRun.motion==scene::MotionRole::Run&&diagRun.direction==scene::Direction::ForwardLeft&&diagRun.weapon==scene::WeaponClass::Automatic,"pb_combatrun_fl_smg is classified as Run ForwardLeft SMG");

    // Native filenames verified in the loaded Saluki AW/BO2/Ghosts/MW/MW3 exports.
    for(const auto direction:{scene::Direction::Forward,scene::Direction::Backward,scene::Direction::Left,scene::Direction::Right}){
        const char* suffix=direction==scene::Direction::Forward?"forward":direction==scene::Direction::Backward?"back":direction==scene::Direction::Left?"left":"right";
        scene::Animation combat;scene::classifyAnimationName(std::string("pb_combatrun_")+suffix+"_loop.cast",combat);
        failures+=!expect(combat.motion==scene::MotionRole::Run&&combat.weapon==scene::WeaponClass::Rifle&&combat.direction==direction,"native generic combat-run direction resolves as rifle run");
        scene::Animation shield;scene::classifyAnimationName(std::string("pb_combatrun_")+suffix+"_shield.cast",shield);
        failures+=!expect(shield.motion==scene::MotionRole::Run&&shield.weapon==scene::WeaponClass::RiotShield&&shield.direction==direction,"native shield combat-run must not enter rifle locomotion pool");
    }
    for(const auto name:{"pb_prone2sprint.cast","pb_prone2sprint_pistol.cast","pb_knife_prone2sprint.cast"}){
        scene::Animation transition;scene::classifyAnimationName(name,transition);
        failures+=!expect(transition.motion==scene::MotionRole::Transition,"prone-to-sprint entry is not a steady sprint loop");
    }
    scene::Animation crouchTransition;scene::classifyAnimationName("mp_shield_cr_idle_2_cr_run_l.cast",crouchTransition);
    failures+=!expect(crouchTransition.stance==scene::Stance::Crouch&&crouchTransition.motion==scene::MotionRole::Transition&&crouchTransition.weapon==scene::WeaponClass::RiotShield,"native cr shorthand retains crouched transition semantics");
    scene::Animation numberedAim;scene::classifyAnimationName("mp_rpg_ads_aim_2.cast",numberedAim);
    failures+=!expect(numberedAim.motion==scene::MotionRole::Idle&&numberedAim.weapon==scene::WeaponClass::Launcher,"numbered aim pose is not a transition");
    scene::CastScene combatCatalog;
    for(const auto name:{"pb_combatrun_left_shield.cast","pb_combatrun_left_loop.cast","pb_combatrun_right_loop.cast"}){
        scene::Animation clip;scene::classifyAnimationName(name,clip);clip.tracks.push_back({});combatCatalog.animations.push_back(clip);
    }
    scene::AnimationQuery combatQuery;combatQuery.domain=scene::AnimationDomain::PlayerBody;combatQuery.motion=scene::MotionRole::Run;combatQuery.stance=scene::Stance::Stand;combatQuery.direction=scene::Direction::Left;combatQuery.weapon=scene::WeaponClass::Rifle;
    failures+=!expect(scene::findBestAnimation(combatCatalog,combatQuery)==std::optional<std::size_t>{1},"rifle resolver skips earlier shield clip and selects actual matching combat run");

    {
        scene::Skeleton vmSkel;
        scene::Bone torsoBone; torsoBone.name = "tag_torso"; vmSkel.bones.push_back(torsoBone);
        vmSkel.boneByName["tag_torso"] = 0; vmSkel.boneByCanonicalName["tag_torso"] = 0;
        scene::Bone shBone; shBone.name = "j_shoulder_ri"; shBone.parent = 0;
        shBone.restLocal.rotation = {0, 0, 0, 1};
        vmSkel.bones.push_back(shBone);
        vmSkel.boneByName["j_shoulder_ri"] = 1; vmSkel.boneByCanonicalName["j_shoulder_ri"] = 1;
        scene::Bone elBone; elBone.name = "j_elbow_ri"; elBone.parent = 1;
        elBone.restLocal.position = {-30.0f, 0, 0};
        vmSkel.bones.push_back(elBone);
        vmSkel.boneByName["j_elbow_ri"] = 2; vmSkel.boneByCanonicalName["j_elbow_ri"] = 2;
        scene::Bone wpnBone; wpnBone.name = "tag_weapon"; wpnBone.parent = 0;
        vmSkel.bones.push_back(wpnBone);
        vmSkel.boneByName["tag_weapon"] = 3; vmSkel.boneByCanonicalName["tag_weapon"] = 3;

        const char bProp[2]={'b','\0'}, fProp[2]={'f','\0'}, v4Prop[2]={'4','v'};
        auto clavCurve = node(0x76727563, 100, {stringProperty("nn", "j_clavicle_ri"), stringProperty("kp", "rq"),
            numericProperty(bProp, "kb", 1, std::vector<std::uint8_t>{0}),
            numericProperty(v4Prop, "kv", 1, std::vector<float>{0, 0.7071068f, 0, 0.7071068f}), stringProperty("m", "absolute")});
        auto shCurve = node(0x76727563, 101, {stringProperty("nn", "j_shoulder_ri"), stringProperty("kp", "rq"),
            numericProperty(bProp, "kb", 1, std::vector<std::uint8_t>{0}),
            numericProperty(v4Prop, "kv", 1, std::vector<float>{0.7071068f, 0, 0, 0.7071068f}), stringProperty("m", "absolute")});
        auto elbowTransCurve = node(0x76727563, 102, {stringProperty("nn", "j_elbow_ri"), stringProperty("kp", "tx"),
            numericProperty(bProp, "kb", 1, std::vector<std::uint8_t>{0}),
            numericProperty(fProp, "kv", 1, std::vector<float>{29.0f}), stringProperty("m", "absolute")});
        auto wpnTransCurve = node(0x76727563, 103, {stringProperty("nn", "tag_weapon"), stringProperty("kp", "tx"),
            numericProperty(bProp, "kb", 1, std::vector<std::uint8_t>{0}),
            numericProperty(fProp, "kv", 1, std::vector<float>{50.0f}), stringProperty("m", "absolute")});
        auto vmAnimNode = node(0x6D696E61, 104, {stringProperty("n", "vm_test_idle"), numericProperty(fProp, "fr", 1, std::vector<float>{30})},
            {std::move(clavCurve), std::move(shCurve), std::move(elbowTransCurve), std::move(wpnTransCurve)});
        auto vmRoot = node(0x746F6F72, 0, {}, {std::move(vmAnimNode)});
        Bytes vmFile; append(vmFile, cast::Document::kMagic); append<std::uint32_t>(vmFile, 1); append<std::uint32_t>(vmFile, 1); append<std::uint32_t>(vmFile, 0);
        appendRaw(vmFile, vmRoot.data(), vmRoot.size());
        auto vmDoc = cast::Document::parse(vmFile, "vm_test_idle.cast");
        scene::CastScene vmScene;
        vmScene.skeleton = vmSkel;
        scene::appendAnimations(vmDoc, vmScene);

        failures+=!expect(vmScene.animations.size()==1, "viewmodel test animation appends");
        if(!vmScene.animations.empty()){
            const auto& a = vmScene.animations[0];
            bool hasElbowTrans = false;
            bool hasWpnTrans = false;
            scene::Quat shRot{};
            bool hasShRot = false;
            for(const auto& tr : a.tracks){
                if(tr.boneIndex == 2 && tr.property == scene::TrackProperty::TranslationX) hasElbowTrans = true;
                if(tr.boneIndex == 3 && tr.property == scene::TrackProperty::TranslationX) hasWpnTrans = true;
                if(tr.boneIndex == 1 && tr.property == scene::TrackProperty::Rotation){
                    hasShRot = true;
                    if(!tr.rotationValues.empty()) shRot = tr.rotationValues[0];
                }
            }
            failures+=!expect(hasElbowTrans, "viewmodel elbow translation track is preserved for native viewmodel rigs");
            failures+=!expect(hasWpnTrans, "viewmodel weapon translation track is preserved");
            failures+=!expect(hasShRot && std::abs(shRot.x - 0.7071068f) < 0.01f && std::abs(shRot.w - 0.7071068f) < 0.01f,
                             "authored shoulder rotation is preserved cleanly for clavicle-free skeleton");
        }

        // Test native non-clavicle animation preserves authored shoulder translations (e.g. BO2)
        auto nativeShTrans = node(0x76727563, 110, {stringProperty("nn", "j_shoulder_ri"), stringProperty("kp", "tx"),
            numericProperty(bProp, "kb", 1, std::vector<std::uint8_t>{0}),
            numericProperty(fProp, "kv", 1, std::vector<float>{-30.35f}), stringProperty("m", "absolute")});
        auto nativeWpnTrans = node(0x76727563, 111, {stringProperty("nn", "tag_weapon"), stringProperty("kp", "tx"),
            numericProperty(bProp, "kb", 1, std::vector<std::uint8_t>{0}),
            numericProperty(fProp, "kv", 1, std::vector<float>{29.5f}), stringProperty("m", "absolute")});
        auto nativeAnimNode = node(0x6D696E61, 112, {stringProperty("n", "vm_native_idle"), numericProperty(fProp, "fr", 1, std::vector<float>{30})},
            {std::move(nativeShTrans), std::move(nativeWpnTrans)});
        auto nativeRoot = node(0x746F6F72, 0, {}, {std::move(nativeAnimNode)});
        Bytes nativeFile; append(nativeFile, cast::Document::kMagic); append<std::uint32_t>(nativeFile, 1); append<std::uint32_t>(nativeFile, 1); append<std::uint32_t>(nativeFile, 0);
        appendRaw(nativeFile, nativeRoot.data(), nativeRoot.size());
        auto nativeDoc = cast::Document::parse(nativeFile, "vm_native_idle.cast");
        scene::CastScene nativeScene;
        nativeScene.skeleton = vmSkel;
        scene::appendAnimations(nativeDoc, nativeScene);
        failures+=!expect(nativeScene.animations.size()==1, "native viewmodel test animation appends");
        if(!nativeScene.animations.empty()){
            bool hasNativeShTrans = false;
            float nativeShVal = 0.0f;
            for(const auto& tr : nativeScene.animations[0].tracks){
                if(tr.boneIndex == 1 && tr.property == scene::TrackProperty::TranslationX){
                    hasNativeShTrans = true;
                    if(!tr.scalarValues.empty()) nativeShVal = tr.scalarValues[0];
                }
            }
            failures+=!expect(hasNativeShTrans && std::abs(nativeShVal - (-30.35f)) < 0.01f,
                             "native viewmodel animations preserve authored shoulder translation channels");
        }

        // Test compact tag_weapon translation (e.g. handguns/snipers idle) remains Absolute and does not become Relative
        scene::Skeleton pistolVmSkel;
        scene::Bone rootBone; rootBone.name = "tag_torso"; rootBone.parent = -1;
        pistolVmSkel.bones.push_back(rootBone);
        pistolVmSkel.boneByName["tag_torso"] = 0; pistolVmSkel.boneByCanonicalName["tag_torso"] = 0;
        scene::Bone socketBone; socketBone.name = "tag_weapon"; socketBone.parent = 0;
        socketBone.restLocal.position = scene::Vec3{38.5059f, 0.0f, -17.1519f}; // MW3 delta hands restLocal
        pistolVmSkel.bones.push_back(socketBone);
        pistolVmSkel.boneByName["tag_weapon"] = 1; pistolVmSkel.boneByCanonicalName["tag_weapon"] = 1;

        auto compactWpnTx = node(0x76727563, 120, {stringProperty("nn", "tag_weapon"), stringProperty("kp", "tx"),
            numericProperty(bProp, "kb", 1, std::vector<std::uint8_t>{0}),
            numericProperty(fProp, "kv", 1, std::vector<float>{5.5f}), stringProperty("m", "absolute")});
        auto compactAnimNode = node(0x6D696E61, 121, {stringProperty("n", "vm_p99_idle"), numericProperty(fProp, "fr", 1, std::vector<float>{30})},
            {std::move(compactWpnTx)});
        auto compactRoot = node(0x746F6F72, 0, {}, {std::move(compactAnimNode)});
        Bytes compactFile; append(compactFile, cast::Document::kMagic); append<std::uint32_t>(compactFile, 1); append<std::uint32_t>(compactFile, 1); append<std::uint32_t>(compactFile, 0);
        appendRaw(compactFile, compactRoot.data(), compactRoot.size());
        auto compactDoc = cast::Document::parse(compactFile, "vm_p99_idle.cast");
        scene::CastScene compactScene;
        compactScene.skeleton = pistolVmSkel;
        scene::appendAnimations(compactDoc, compactScene);
        failures+=!expect(compactScene.animations.size()==1, "compact tag_weapon test animation appends");
        if(!compactScene.animations.empty()){
            bool hasAbsoluteTx = false;
            for(const auto& tr : compactScene.animations[0].tracks){
                if(tr.boneIndex == 1 && tr.property == scene::TrackProperty::TranslationX){
                    if(tr.mode == scene::TrackMode::Absolute) hasAbsoluteTx = true;
                }
            }
            failures+=!expect(hasAbsoluteTx, "compact tag_weapon translation remains Absolute (not erroneously converted to Relative)");
        }
    }

    {
        scene::CastScene muzzleScene;
        muzzleScene.skeleton.boneByCanonicalName["muzzle"]=0;
        const std::vector<scene::Mat4> muzzlePose{scene::translation({12,23,34})};
        const auto native=scene::resolveMuzzlePosition(muzzleScene,muzzlePose);
        failures+=!expect(native&&scene::length(*native-scene::Vec3{12,23,34})<.0001f,"CS2 native muzzle resolves without tag_flash in evaluated world space");
        muzzleScene.skeleton.boneByCanonicalName.clear();
        scene::Attachment weapon;weapon.boneIndex=0;weapon.muzzleLocal=scene::Vec3{10,0,0};
        weapon.position={3,4,5};weapon.scale={2,2,2};weapon.rotationDegrees={0,0,90};
        muzzleScene.attachments.push_back(weapon);
        const auto attached=scene::resolveMuzzlePosition(muzzleScene,muzzlePose);
        failures+=!expect(attached&&scene::length(*attached-scene::Vec3{15,47,39})<.001f,"rigid CS2 muzzle uses socket, attachment rotation, scale and translation once");
        muzzleScene.attachments.clear();muzzleScene.skeleton.boneByCanonicalName["tag_brass"]=0;
        failures+=!expect(!scene::resolveMuzzlePosition(muzzleScene,muzzlePose),"unresolved muzzle cannot fall back to camera or brass origin");
        muzzleScene.skeleton.boneByCanonicalName["tag_flash"]=0;
        failures+=!expect(scene::resolveMuzzlePosition(muzzleScene,muzzlePose).has_value(),"native COD flash tags remain supported");
        muzzleScene.skeleton.boneByCanonicalName.clear();
        muzzleScene.muzzleAnchors={{0,{4,5,6},0},{0,{-4,5,6},1}};
        const auto right=scene::resolveMuzzlePosition(muzzleScene,muzzlePose,-1,0),left=scene::resolveMuzzlePosition(muzzleScene,muzzlePose,-1,1);
        failures+=!expect(right&&left&&scene::length(*right-scene::Vec3{16,28,40})<.001f&&scene::length(*left-scene::Vec3{8,28,40})<.001f,"imported barrel anchors follow animated bones independently per hand");
        muzzleScene.muzzleAnchors.clear();muzzleScene.meshes.resize(2);muzzleScene.meshes[0].actorVariant=0;muzzleScene.meshes[1].actorVariant=1;
        scene::Attachment one;one.boneIndex=0;one.firstMesh=0;one.muzzleLocal=scene::Vec3{1,0,0};
        auto two=one;two.firstMesh=1;two.muzzleLocal=scene::Vec3{30,0,0};muzzleScene.attachments={one,two};
        const auto variant=scene::resolveMuzzlePosition(muzzleScene,muzzlePose,1);
        failures+=!expect(variant&&std::abs(variant->x-42)<.001f,"bot muzzle uses its own weapon variant rather than the first attachment");
    }
    {
        // A converted sparse ADS layer must not own the dense bind-support
        // tracks needed when that same clip is sampled as a standalone pose.
        scene::CastScene sparseSource;
        for(const auto name:{"tag_torso","tag_ads","j_wrist_le","tag_camera"}){
            scene::Bone b;b.name=name;b.parent=sparseSource.skeleton.bones.empty()?-1:0;
            const auto index=sparseSource.skeleton.bones.size();
            sparseSource.skeleton.boneByName[name]=index;sparseSource.skeleton.boneByCanonicalName[name]=index;
            sparseSource.skeleton.bones.push_back(b);
        }
        sparseSource.skeleton.bones[3].restLocal.position={0,0,163};
        sparseSource.skeleton.bones[3].restGlobal=scene::translation({0,0,163});
        scene::Animation baseClip,adsClip;
        scene::Track wrist;wrist.boneIndex=2;wrist.property=scene::TrackProperty::TranslationX;wrist.frames={0};wrist.scalarValues={30};baseClip.tracks.push_back(wrist);
        scene::Track ads;ads.boneIndex=1;ads.property=scene::TrackProperty::TranslationX;ads.frames={0};ads.scalarValues={5};adsClip.tracks.push_back(ads);
        sparseSource.animations={baseClip,adsClip};auto sparseTarget=sparseSource;
        auto sprintScene=sparseSource;sprintScene.animations[1].sourceName="vm_test_sprint_offset.cast";
        sprintScene.animations[1].tracks[0].boneIndex=2;
        failures+=!expect(scene::composeIwSprintOffset(sprintScene,1,0),"IW sprint offset composes with native idle");
        const auto sprintLocal=sprintScene.sampleLocalPose(1,0);
        failures+=!expect(std::abs(sprintLocal[2].position.x-35)<.001f,"IW sprint adds offset instead of resetting grip to bind");
        failures+=!expect(std::abs(sprintLocal[3].position.z-163)<.001f&&sprintScene.animations[1].looping,"IW sprint retains idle camera and holds composed pose");
        sparseTarget.skeleton.bones[3].restGlobal=scene::Mat4::identity();
        scene::retargetAnimationRange(sparseTarget,0,sparseSource,0);
        failures+=!expect(sparseTarget.animations[0].viewmodelCameraReference&&std::abs(sparseTarget.animations[0].viewmodelCameraReference->v[14]-163)<.001f,"IW conversion retains source camera reference, not target height");
        bool wristOwned=false,adsOwned=false;
        for(const auto& t:sparseTarget.animations[1].tracks){if(t.boneIndex==2)wristOwned=wristOwned||t.ownsLayer;if(t.boneIndex==1)adsOwned=adsOwned||t.ownsLayer;}
        failures+=!expect(!wristOwned&&adsOwned,"sparse IW ADS owns aim bone but not unanimated wrist");
        scene::PoseSlot sparseSlot;sparseSlot.nodes.push_back({1,0,1,scene::LayerMode::Override,false});
        const auto sparseBase=sparseTarget.samplePose(0,0);
        const auto sparseLayer=sparseTarget.samplePoseSlots(0,0,{sparseSlot});
        const auto simpleLayer=sparseTarget.sampleLayeredPose(0,0,1,0,1,scene::LayerMode::Override,false);
        const auto stackLayer=sparseTarget.sampleLayerStack(0,0,sparseSlot.nodes);
        for(int c=0;c<16;++c){
            failures+=!expect(std::abs(sparseBase[2].v[c]-sparseLayer[2].v[c])<.001f,"persistent ADS slot preserves wrist pose");
            failures+=!expect(std::abs(sparseBase[2].v[c]-simpleLayer[2].v[c])<.001f,"simple ADS layer preserves wrist pose");
            failures+=!expect(std::abs(sparseBase[2].v[c]-stackLayer[2].v[c])<.001f,"stack ADS layer preserves wrist pose");
        }
        // Calibrated finger-index shift and wrist basis must invert without drift.
        for(const auto pair:{std::pair{"j_wrist_le","j_wrist_le"},std::pair{"j_index_le_1","j_index_le_0"},std::pair{"j_wrist_ri","j_wrist_ri"}}){
            auto iw=scene::buildScene(document);iw.skeleton.bones[0].name=pair.first;
            iw.skeleton.boneByName={{pair.first,0}};iw.skeleton.boneByCanonicalName={{pair.first,0}};
            auto legacy=iw;legacy.skeleton.bones[0].name=pair.second;
            legacy.skeleton.boneByName={{pair.second,0}};legacy.skeleton.boneByCanonicalName={{pair.second,0}};
            scene::retargetAnimationRange(legacy,0,iw,0);
            auto restored=iw;scene::retargetAnimationRange(restored,0,legacy,0,true);
            for(const float frame:{0.f,7.f,15.f,30.f}){
                const auto a=iw.samplePose(0,frame),b=restored.samplePose(0,frame);
                for(int component=0;component<16;++component)failures+=!expect(std::abs(a[0].v[component]-b[0].v[component])<.001f,"IW calibration forward/inverse round trip");
            }
        }
        auto torsoSource=scene::buildScene(document);torsoSource.skeleton.bones[0].name="tag_torso";
        torsoSource.skeleton.boneByName={{"tag_torso",0},{"j_spinelower",0}};torsoSource.skeleton.boneByCanonicalName=torsoSource.skeleton.boneByName;
        auto torsoTarget=torsoSource;scene::Bone spine;spine.name="j_spinelower";spine.parent=0;spine.restLocal.position={0,0,7};
        spine.restLocal.rotation=scene::normalize(scene::Quat{.5f,-.5f,.5f,.5f});torsoTarget.skeleton.bones.push_back(spine);
        torsoTarget.skeleton.boneByName["j_spinelower"]=1;torsoTarget.skeleton.boneByCanonicalName["j_spinelower"]=1;
        scene::retargetAnimationRange(torsoTarget,0,torsoSource,0,true);
        const auto torsoPose=torsoTarget.sampleLocalPose(0,15);
        failures+=!expect(scene::length(torsoPose[1].position-spine.restLocal.position)<.001f,"inverse IW conversion rejects torso-to-spine alias collapse");
        failures+=!expect(std::abs(torsoPose[1].rotation.x-spine.restLocal.rotation.x)<.001f,"inverse IW conversion retains native spine basis");

        auto hands=scene::buildScene(document);hands.animations.clear();hands.skeleton.bones[0].name="j_wrist_ri";
        scene::Bone helper;helper.name="j_gun";helper.parent=0;hands.skeleton.bones.push_back(helper);
        hands.skeleton.boneByName={{"j_wrist_ri",0},{"j_gun",1}};hands.skeleton.boneByCanonicalName=hands.skeleton.boneByName;
        for(const auto name:{"weapon_revolver_camo","weapon_revolver_camo_LOD0"}){
            auto revolverHands=hands;
            scene::Bone socket;socket.name="tag_weapon";socket.parent=-1;
            revolverHands.skeleton.boneByName[socket.name]=2;revolverHands.skeleton.boneByCanonicalName[socket.name]=2;
            revolverHands.skeleton.bones.push_back(socket);
            const auto revolverDoc=cast::Document::parse(animatedTriangle("test_material","j_gun"),std::string(name)+".cast");
            scene::appendRigModel(revolverDoc,revolverHands,name);
            const auto root=revolverHands.skeleton.boneByCanonicalName.at("j_gun");
            failures+=!expect(root!=1&&revolverHands.skeleton.boneByCanonicalName.at("j_gun_hand")==1,"IW revolver owns a separate gun root instead of inheriting the hand helper");
            failures+=!expect(revolverHands.skeleton.bones[root].parent==2,"IW revolver mounts under the animated weapon socket");
            failures+=!expect(revolverHands.meshes.back().vertices[0].bones[0]==root,"IW revolver geometry follows the weapon root");
        }
        const auto weaponDoc=cast::Document::parse(animatedTriangle("test_material","j_gun"),"t6_wpn_ar_test_view.cast");
        scene::appendRigModel(weaponDoc,hands,"t6_wpn_ar_test_view");
        failures+=!expect(hands.skeleton.boneByCanonicalName.at("j_gun")!=1&&hands.skeleton.boneByCanonicalName.at("j_gun_hand")==1,"T6 weapon root cannot merge with IW wrist-owned gun helper");
    }
    {
        // Recover an arbitrary socket from contact motion, with no weapon-name
        // knowledge. Stationary/degenerate data must not invent a replacement.
        scene::CastScene contact;
        for(const auto name:{"j_gun","j_wrist_le","tag_clip"}){scene::Bone b;b.name=name;b.parent=contact.skeleton.bones.empty()?-1:0;const auto i=contact.skeleton.bones.size();contact.skeleton.boneByName[name]=i;contact.skeleton.boneByCanonicalName[name]=i;contact.skeleton.bones.push_back(b);}
        contact.rigParts.push_back({"t6_attach_mag_arbitrary_view",0,0,{2}});
        scene::Animation motion;motion.sourceName="viewmodel_arbitrary_reload.cast";motion.durationFrames=60;
        for(auto bone:{1u,2u})for(auto property:{scene::TrackProperty::TranslationX,scene::TrackProperty::TranslationY,scene::TrackProperty::TranslationZ,scene::TrackProperty::Rotation}){
            scene::Track t;t.boneIndex=bone;t.property=property;t.mode=bone==2&&property!=scene::TrackProperty::Rotation?scene::TrackMode::Relative:scene::TrackMode::Absolute;motion.tracks.push_back(t);
        }
        const scene::Vec3 expectedMount{8,-2,-6},grip{4,3,2};
        for(unsigned frame=0;frame<=60;++frame){const float f=float(frame);const auto q=scene::fromEulerRadians({std::sin(f*.13f),std::cos(f*.17f),f*.05f});const scene::Vec3 wrist{f*.5f,std::sin(f*.1f)*5,0};const auto mag=wrist+scene::transformPoint(scene::rotation(q),grip)-expectedMount;
            for(auto& t:motion.tracks){t.frames.push_back(frame);if(t.property==scene::TrackProperty::Rotation)t.rotationValues.push_back(q);else{const auto p=t.boneIndex==1?wrist:mag;t.scalarValues.push_back(t.property==scene::TrackProperty::TranslationX?p.x:t.property==scene::TrackProperty::TranslationY?p.y:p.z);}}
        }
        contact.animations.push_back(motion);const auto fitted=scene::inferT6MagazineMount(contact);
        failures+=!expect(fitted&&scene::length(fitted->position-expectedMount)<.01f,"native contact reconstructs an arbitrary missing magazine socket");
        {
            auto shortContact=contact;auto shortClip=motion;shortClip.durationFrames=16;
            for(auto& t:shortClip.tracks)if(t.boneIndex==2&&t.property==scene::TrackProperty::TranslationX)t.scalarValues[16]+=1000;
            shortContact.animations={shortClip};
            failures+=!expect(!scene::inferT6MagazineMount(shortContact),"one short contact interval is insufficient");
            shortContact.animations.push_back(shortClip);
            failures+=!expect(!scene::inferT6MagazineMount(shortContact),"duplicating a clip is not independent contact evidence");
            shortContact.animations.back().sourceName="viewmodel_arbitrary_reload_empty.cast";
            const auto crossClip=scene::inferT6MagazineMount(shortContact);
            failures+=!expect(crossClip&&scene::length(crossClip->position-expectedMount)<.01f,"two distinct tightly agreeing reload contacts recover socket");
        }
        if(fitted){const auto inverse=contact.skeleton.bones[2].inverseBind;scene::applyT6MagazineMount(contact,*fitted);const auto position=contact.skeleton.bones[2].restLocal.position;scene::applyT6MagazineMount(contact,*fitted);failures+=!expect(scene::length(contact.skeleton.bones[2].restLocal.position-position)<.0001f&&contact.skeleton.bones[2].inverseBind.v==inverse.v,"calibration is idempotent and preserves magazine inverse bind");}
        contact.animations[0].tracks.clear();failures+=!expect(!scene::inferT6MagazineMount(contact),"stationary reload cannot fabricate a socket");
        contact.rigParts[0].name="ak47_model";failures+=!expect(!scene::hasSeparateT6Magazine(contact)&&!scene::applyT6MagazineMount(contact,{}),"non-T6 assemblies are untouched");
        // These view-only stations must not inherit the receiver bolt offset.
        for(const auto name:{"t6_wpn_ar_an94_view_LOD0","t6_wpn_ar_scarh_view_LOD0","t6_wpn_sniper_ballista_view_LOD0"}){
            auto rig=scene::buildScene(cast::Document::parse(animatedTriangle("test_material","j_gun"),"receiver.cast"));
            rig.rigParts.push_back({name,0,rig.meshes.size(),{0}});
            const auto mag=cast::Document::parse(animatedTriangle("test_material","tag_clip"),"mag.cast");
            scene::appendRigModel(mag,rig,"t6_attach_mag_test_view_LOD0");
            const auto& b=rig.skeleton.bones[rig.skeleton.boneByCanonicalName.at("tag_clip")];
            failures+=!expect(scene::length(b.restLocal.position)<.0001f,"T6 missing station does not use a weapon-specific offset");
            failures+=!expect(b.translationTracksAreDeltas,"mounted magazine keeps authored translation offsets");
        }
        failures+=!expect(!scene::inferT6MagazineMount(scene::CastScene{}),"magazine calibration rejects unavailable source data");
        const char i[2]={'i',0},v3[2]={'3','v'},v4[2]={'4','v'},f[2]={'f',0},b[2]={'b',0};
        auto makeFile=[](std::vector<Bytes> children){auto root=node(0x746F6F72,0,{},std::move(children));Bytes out;append(out,cast::Document::kMagic);append<std::uint32_t>(out,1);append<std::uint32_t>(out,1);append<std::uint32_t>(out,0);appendRaw(out,root.data(),root.size());return out;};
        auto gun=node(0x656E6F62,2,{stringProperty("n","j_gun"),numericProperty(i,"p",1,std::vector<std::uint32_t>{0xffffffffu})});
        auto dummy=node(0x656E6F62,3,{stringProperty("n","tag_clip1"),numericProperty(i,"p",1,std::vector<std::uint32_t>{0}),numericProperty(v3,"lp",1,std::vector<float>{-35.4636f,-2.75643f,-3.8091f}),numericProperty(v4,"lr",1,std::vector<float>{0,0,0,1})});
        auto mesh=node(0x6873656D,7,{stringProperty("n","dummy_triangle"),numericProperty(v3,"vp",3,std::vector<float>{0,0,0,1,0,0,0,1,0}),numericProperty(b,"f",3,std::vector<std::uint8_t>{0,1,2})});
        auto model=node(0x6C646F6D,1,{}, {node(0x6C656B73,4,{}, {gun,dummy}),mesh});
        scene::CastScene rig;
        scene::appendRigModel(cast::Document::parse(makeFile({model}),"receiver.cast"),rig,"t6_wpn_ar_an94_view_LOD0");
        const auto index=rig.skeleton.boneByCanonicalName.at("tag_clip1");
        failures+=!expect(rig.skeleton.bones[index].translationTracksAreDeltas,"AN94 embedded dummy retains bind-relative translation semantics");
        for(const auto name:{"viewmodel_an94_reload.cast","viewmodel_an94_reload_empty.cast","viewmodel_an94_fastmag.cast"}){
            auto curve=node(0x76727563,5,{stringProperty("nn","tag_clip1"),stringProperty("kp","tx"),stringProperty("m","absolute"),numericProperty(b,"kb",2,std::vector<std::uint8_t>{0,30}),numericProperty(f,"kv",2,std::vector<float>{-41.4547f,46.8261f})});
            auto anim=node(0x6D696E61,6,{stringProperty("n",name),numericProperty(f,"fr",1,std::vector<float>{30})},{curve});
            scene::appendAnimations(cast::Document::parse(makeFile({anim}),name),rig);
            const auto ai=rig.animations.size()-1;const auto& t=rig.animations[ai].tracks.front();
            failures+=!expect(std::abs(t.scalarValues.front()+41.4547f)<.0001f&&std::abs(t.scalarValues.back()-46.8261f)<.0001f,"AN94 source keys never endpoint-rebased");
            failures+=!expect(std::abs(rig.sampleLocalPose(ai,30)[index].position.x-11.3625f)<.001f,"AN94 incoming magazine ends at installed station rather than parked bind");
        }
        scene::appendRigModel(cast::Document::parse(animatedTriangle("test_material","tag_clip"),"mag.cast"),rig,"t6_attach_mag_an94_view_LOD0");
        scene::Animation idle;idle.name="idle";idle.sourceName="viewmodel_an94_idle.cast";rig.animations.push_back(idle);
        const auto installed=rig.skeleton.boneByCanonicalName.at("tag_clip");
        scene::applyT6MagazineMount(rig,{{11.3625f,-2.75643f,-3.8091f},0,2,true});
        auto blended=rig.samplePose(rig.animations.size()-1,0);blended[installed].v[12]=-100;
        const auto gunBefore=blended[0];
        scene::restoreT6MagazineAfterReload(rig,blended,rig.animations.size()-1,0);
        failures+=!expect(std::abs(blended[installed].v[12]-11.3625f)<.001f,"reload exit cannot interpolate discarded magazine through scene");
        failures+=!expect(blended[index].v[0]==0&&blended[index].v[5]==0&&blended[index].v[10]==0,"reload exit hides incoming dummy after replacement");
        failures+=!expect(blended[0].v==gunBefore.v,"magazine handoff preserves receiver blend");
        for(const auto dummyName:{"tag_clip_full","tag_clip_full1","tag_clip1"}){
            const char* installedName=std::string(dummyName)=="tag_clip_full1"?"tag_clip1":"tag_clip";
            auto seated=node(0x656E6F62,8,{stringProperty("n",installedName),numericProperty(i,"p",1,std::vector<std::uint32_t>{0}),numericProperty(v3,"lp",1,std::vector<float>{-3.74436f,0,-3.45698f})});
            auto incoming=node(0x656E6F62,9,{stringProperty("n",dummyName),numericProperty(i,"p",1,std::vector<std::uint32_t>{0}),numericProperty(v3,"lp",1,std::vector<float>{-50.8019f,0,-3.45698f})});
            auto twoMagModel=node(0x6C646F6D,1,{}, {node(0x6C656B73,4,{}, {gun,seated,incoming}),mesh});
            scene::CastScene pistol;scene::appendRigModel(cast::Document::parse(makeFile({twoMagModel}),"pistol.cast"),pistol,"t6_wpn_pistol_test_view_LOD0");
            const auto spare=pistol.skeleton.boneByCanonicalName.at(dummyName),seat=pistol.skeleton.boneByCanonicalName.at(installedName);
            failures+=!expect(pistol.skeleton.bones[spare].translationTracksAreDeltas,"all T6 embedded magazine naming variants keep bind-relative semantics");
            auto curve=node(0x76727563,5,{stringProperty("nn",dummyName),stringProperty("kp","tx"),stringProperty("m","absolute"),numericProperty(b,"kb",2,std::vector<std::uint8_t>{0,30}),numericProperty(f,"kv",2,std::vector<float>{47.192f,47.0574f})});
            auto anim=node(0x6D696E61,6,{stringProperty("n","viewmodel_fn57_reload"),numericProperty(f,"fr",1,std::vector<float>{30})},{curve});
            scene::appendAnimations(cast::Document::parse(makeFile({anim}),"viewmodel_fn57_reload.cast"),pistol);
            failures+=!expect(std::abs(pistol.sampleLocalPose(0,30)[spare].position.x+3.7445f)<.001f,"Five-Seven incoming magazine lands in actual magwell");
            pistol.animations.push_back(idle);auto exit=pistol.samplePose(1,0);exit[seat].v[12]=-100;
            scene::restoreT6MagazineAfterReload(pistol,exit,1,0);
            failures+=!expect(exit[spare].v[0]==0&&std::abs(exit[seat].v[12]+3.74436f)<.001f,"T6 full and numbered magazine handoffs restore only installed mesh");
            // Nearby tag_clip1 is a second installed magazine, not a dummy.
            if(std::string(dummyName)=="tag_clip1"){
                pistol.skeleton.bones[spare].restLocal.position={-3.74436f,5,-3.45698f};
                auto dual=pistol.samplePose(1,0);const auto before=dual;
                scene::restoreT6MagazineAfterReload(pistol,dual,1,0);
                failures+=!expect(dual[spare].v==before[spare].v,"dual-wield installed magazine must not be hidden");
            }
        }
    }
    {
        scene::CastScene rig;rig.skeleton.bones.resize(1);
        rig.rigMountReferences[0]=rig.skeleton.bones[0].restLocal;
        rig.skeleton.bones[0].restLocal.position.x=7;
        scene::Animation clip;clip.durationFrames=10;
        scene::Track track;track.boneIndex=0;track.property=scene::TrackProperty::TranslationX;track.mode=scene::TrackMode::Absolute;track.frames={0,10};track.scalarValues={2,12};clip.tracks.push_back(track);rig.animations.push_back(clip);
        failures+=!expect(std::abs(rig.sampleLocalPose(0,5)[0].position.x-14)<.001f,"mount offset survives absolute animation tracks");
        failures+=!expect(std::abs(rig.sampleLocalPose(99,0)[0].position.x-7)<.001f,"unanimated mount is not applied twice");
    }
    {
        scene::CastScene rig;scene::RigPart part;part.muzzleParent=0;part.muzzleOffset=scene::translation({22,0,8});rig.rigParts.push_back(part);
        const std::vector<scene::Mat4> pose{scene::translation({100,200,300})};
        const auto muzzle=scene::resolveMuzzlePosition(rig,pose);
        failures+=!expect(muzzle&&std::abs(muzzle->x-122)<.001f&&std::abs(muzzle->z-308)<.001f,"model muzzle follows animated parent");
    }
    {
        scene::CastScene actor;
        for(const auto name:{"pt_crouch_rifle_fire.cast","pt_crouch_shoot.cast","pt_crouch_rifle_fire_ads.cast","pt_crouch_shoot_ads.cast"}){
            scene::Animation clip;scene::classifyAnimationName(name,clip);clip.sourceName=name;clip.sourceGame="mw";clip.tracks.resize(1);actor.animations.push_back(clip);
        }
        scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerTorso;q.action=scene::ActionRole::Fire;q.stance=scene::Stance::Crouch;q.weapon=scene::WeaponClass::Rifle;
        failures+=!expect(scene::findBestAnimation(actor,q)==1,"COD4 crouch fire matches multiplayer rifle hold");
        q.ads=true;failures+=!expect(scene::findBestAnimation(actor,q)==3,"COD4 aimed crouch fire matches aimed multiplayer hold");
    }
    {
        const auto source=(std::filesystem::path(__FILE__).parent_path()/"fixtures/codm/models/test.cast").string();
        const auto soft=scene::buildScene(cast::Document::parse(animatedTriangle("unclassified_fx","root","materials/soft_albedo.png"),source));
        const auto solid=scene::buildScene(cast::Document::parse(animatedTriangle("unclassified_body","root","materials/solid_albedo.png"),source));
        failures+=!expect(soft.meshes.size()==1&&soft.meshes[0].forceAlpha&&!soft.meshes[0].ignoreAlbedoAlpha,"CODM soft alpha follows exported metadata without material-name heuristics");
        failures+=!expect(solid.meshes.size()==1&&!solid.meshes[0].forceAlpha&&solid.meshes[0].ignoreAlbedoAlpha,"CODM opaque material retains auxiliary albedo alpha");
    }
    {
        const auto source=(std::filesystem::path(__FILE__).parent_path()/"fixtures/mw3/models/test.cast").string();
        const auto cutout=scene::buildScene(cast::Document::parse(animatedTriangle("mc_test_cutout"),source));
        const auto blended=scene::buildScene(cast::Document::parse(animatedTriangle("mc_test_blend"),source));
        failures+=!expect(blended.meshes.size()==1&&blended.meshes[0].forceAlpha&&!blended.meshes[0].alphaTest,"blended technique retains opacity without cutout");
        const auto solid=scene::buildScene(cast::Document::parse(animatedTriangle("mc_test_solid"),source));
        failures+=!expect(cutout.meshes.size()==1&&cutout.meshes[0].alphaTest&&!cutout.meshes[0].forceAlpha,"exported alpha-test technique enables cutout without blending");
        failures+=!expect(solid.meshes.size()==1&&!solid.meshes[0].alphaTest,"ordinary exported technique does not imply opacity alpha");
    }
    {
        scene::CastScene hands;hands.skeleton.bones.resize(3);
        for(std::size_t n=0;n<3;++n){auto& bone=hands.skeleton.bones[n];bone.name=n==0?"j_wrist_ri":n==1?"j_gun":"tag_weapon";bone.parent=n==1?0:-1;hands.skeleton.boneByName[bone.name]=n;hands.skeleton.boneByCanonicalName[bone.name]=n;}
        const auto handTemplate=hands;
        for(const auto game:{"iw_sp","mw","mw3","mwr","ghosts"}){
        hands=handTemplate;
        scene::appendRigModel(cast::Document::parse(animatedTriangle("test","j_gun"),std::string("fixture/")+game+"/models/weapon_without_view_marker.cast"),hands,"weapon_without_view_marker");
        const auto gun=hands.skeleton.boneByCanonicalName.at("j_gun");
        failures+=!expect(gun!=1&&hands.skeleton.bones[gun].parent==2,"IW exported weapon root must not merge into the hands wrist helper when filename has no view marker");
        failures+=!expect(hands.skeleton.bones[1].parent==0,"IW ownership separation preserves wrist helper hierarchy");
        }
    }
    {
        const char b[2]={'b','\0'},f[2]={'f','\0'};
        const auto makeFile=[](std::vector<Bytes> children){auto root=node(0x746F6F72,0,{},std::move(children));Bytes out;append(out,cast::Document::kMagic);append<std::uint32_t>(out,1);append<std::uint32_t>(out,1);append<std::uint32_t>(out,0);appendRaw(out,root.data(),root.size());return out;};
        scene::CastScene rig;rig.skeleton.bones.resize(3);
        for(std::size_t n=0;n<3;++n){auto& bone=rig.skeleton.bones[n];bone.name=n==0?"weapon":n==1?"child":"other";bone.parent=n==1?0:-1;bone.restLocal.position.x=10;rig.skeleton.boneByName[bone.name]=n;rig.skeleton.boneByCanonicalName[bone.name]=n;}
        const auto curve=[&](const char* name){return node(0x76727563,10,{stringProperty("nn",name),stringProperty("kp","tx"),stringProperty("m","absolute"),numericProperty(b,"kb",1,std::vector<std::uint8_t>{0}),numericProperty(f,"kv",1,std::vector<float>{2})});};
        const auto mode=node(0x564F4D43,11,{stringProperty("nn","weapon"),stringProperty("m","relative"),numericProperty(b,"ot",1,std::vector<std::uint8_t>{1})});
        const auto animation=node(0x6D696E61,12,{stringProperty("n","vm_test_idle")},{curve("weapon"),curve("child"),curve("other"),mode});
        scene::appendAnimations(cast::Document::parse(makeFile({animation}),"fixture/iw_sp/vm_test_idle.cast"),rig);
        auto pose=rig.sampleLocalPose(0,0);
        failures+=!expect(pose[0].position.x==2&&pose[1].position.x==12&&pose[2].position.x==2,"CAST reference-importer translation override covers descendants but not the named bone or siblings regardless of node order");
        auto untouched=rig;untouched.animations.clear();
        scene::appendAnimations(cast::Document::parse(makeFile({animation}),"fixture/cs2/animations/viewmodel/test.cast"),untouched);
        failures+=!expect(untouched.sampleLocalPose(0,0)[0].position.x==2,"excluded Source2 animation pipeline remains unchanged");
        const auto disabled=node(0x564F4D43,13,{stringProperty("nn","weapon"),stringProperty("m","relative")});
        const auto defaults=node(0x6D696E61,14,{stringProperty("n","vm_test_idle")},{curve("weapon"),disabled});
        scene::appendAnimations(cast::Document::parse(makeFile({defaults}),"fixture/iw_sp/defaults.cast"),rig);
        failures+=!expect(rig.sampleLocalPose(1,0)[0].position.x==2,"omitted CAST override channel flags default off");
    }
    {
        scene::CastScene s;s.viewHandsDriverGame="dirtybomb";scene::Bone bone;bone.name="hand";s.skeleton.bones.push_back(bone);
        for(const auto& item:{std::pair{"AR_Idle.cast",10.f},std::pair{"AR_Zoom_Idle.cast",100.f},std::pair{"AR_Fire.cast",13.f},std::pair{"AR_Zoom_Fire.cast",104.f}}){
            scene::Animation a;a.sourceName=item.first;scene::Track t;t.boneIndex=0;t.property=scene::TrackProperty::TranslationX;t.frames={0};t.scalarValues={item.second};a.tracks.push_back(t);s.animations.push_back(a);
        }
        weapon::Profile p;p.animations["idle"]="AR_Idle.cast";p.animations["ads_idle"]="AR_Zoom_Idle.cast";
        scene::PoseLayer input{2,0,1,scene::LayerMode::Override,false};
        const auto layer=cadence::imported_actions::adsFireLayer(s,p,scene::ActionRole::Fire,input);
        scene::PoseSlot slot;slot.nodes.push_back(layer);const auto pose=s.samplePoseSlots(1,0,{slot});
        failures+=!expect(layer.referenceAnimation==0&&std::abs(pose[0].v[12]-103.f)<.001f,"retained hip recoil uses explicit neutral, preserving first-frame recoil during ADS");
        input.animation=3;const auto aimed=cadence::imported_actions::adsFireLayer(s,p,scene::ActionRole::Fire,input);
        failures+=!expect(aimed.referenceAnimation==1,"ADS fire uses ADS neutral");
        failures+=!expect(cadence::imported_actions::adsFireLayer(s,p,scene::ActionRole::Reload,input).mode==scene::LayerMode::Override,"reload remains full-pose");
        input.mode=scene::LayerMode::Additive;failures+=!expect(cadence::imported_actions::adsFireLayer(s,p,scene::ActionRole::Fire,input).referenceAnimation==SIZE_MAX,"authored additive reference retained");input.mode=scene::LayerMode::Override;
        auto missing=p;missing.animations.clear();failures+=!expect(cadence::imported_actions::adsFireLayer(s,missing,scene::ActionRole::Fire,input).mode==scene::LayerMode::Override,"missing neutral does not guess");
        s.viewHandsDriverGame="cs2";failures+=!expect(cadence::imported_actions::adsFireLayer(s,p,scene::ActionRole::Fire,input).mode==scene::LayerMode::Override,"CS2 fire pipeline untouched");
    }
    {
        scene::CastScene rig;rig.viewHandsDriverGame="cs2";
        for(const auto name:{"wpn","weapon"}){scene::Bone b;b.name=name;b.parent=-1;b.restGlobal=scene::Mat4::identity();b.inverseBind=scene::Mat4::identity();rig.skeleton.boneByName[name]=rig.skeleton.bones.size();rig.skeleton.boneByCanonicalName[name]=rig.skeleton.bones.size();rig.skeleton.bones.push_back(b);}
        rig.skeleton.bones[0].restLocal.position={17,0,0};rig.skeleton.bones[0].restGlobal=scene::translation({17,0,0});rig.skeleton.bones[0].inverseBind=scene::inverseAffine(rig.skeleton.bones[0].restGlobal);
        const auto source=cast::Document::parse(animatedTriangle("test","weapon"),"fixture/cs2/models/test.cast");
        scene::appendRigModel(source,rig,"test");
        failures+=!expect(rig.skeleton.bones[1].parent==0,"CS2 disconnected weapon root attaches to wpn");
        failures+=!expect(std::abs(rig.skeleton.bones[1].restGlobal.v[12]-17.f)<.001f,"CS2 attached bind follows socket");
        const auto before=rig.meshes[0].vertices[0].position;
        scene::appendRigModel(source,rig,"second");
        failures+=!expect(scene::length(rig.meshes.back().vertices[0].position-before)<.001f,"CS2 attachment is idempotent");
    }
    failures+=!expect(scene::source2ContextualBody("agents/models/ctm_fbi/ctm_fbi.vmdl_c","pb_generic_stand_idle_death_clip1906694010663493925.cast"),"normalized low-violence death excluded by source identity");
    failures+=!expect(!scene::source2ContextualBody("agents/models/ctm_fbi/ctm_fbi.vmdl_c","pb_generic_stand_idle_death_clip9605355687280257963.cast"),"normal chest death retained");
    failures+=!expect(scene::source2ContextualBody("characters/hostage.vmdl_c","pb_death.cast"),"hostage source excluded despite generic name");
    if(!failures)std::cout<<"All Cast scene tests passed.\n";return failures?1:0;
}
