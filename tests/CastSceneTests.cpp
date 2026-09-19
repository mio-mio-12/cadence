#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "scene/TestCourse.h"
#include "scene/T6Magazine.h"
#include "scene/NativeBoneNames.h"
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
Bytes animatedTriangle(std::string_view materialName="test_material",std::string_view boneName="root",std::string_view albedo="textures/test.png"){
    const char i[2]={'i','\0'},l[2]={'l','\0'},b[2]={'b','\0'},f[2]={'f','\0'},v3[2]={'3','v'},v4[2]={'4','v'};
    auto bone=node(0x656E6F62,3,{stringProperty("n",boneName),numericProperty(i,"p",1,std::vector<std::uint32_t>{0xffffffffu}),
        numericProperty(v3,"lp",1,std::vector<float>{0,0,0}),numericProperty(v4,"lr",1,std::vector<float>{0,0,0,1}),numericProperty(v3,"s",1,std::vector<float>{1,1,1})});
    auto skeleton=node(0x6C656B73,2,{}, {std::move(bone)});
    auto textureFile=node(0x656C6966,11,{stringProperty("p",albedo)});
    auto material=node(0x6C74616D,10,{stringProperty("n",materialName),stringProperty("t","pbr"),numericProperty(l,"diffuse",1,std::vector<std::uint64_t>{11})},{std::move(textureFile)});
    auto mesh=node(0x6873656D,4,{stringProperty("n","triangle"),numericProperty(v3,"vp",3,std::vector<float>{-1,0,0,1,0,0,0,0,2}),
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
        failures+=!expect(scene::retargetColdWarLegacyRange(target,0,source,0),"T9 legacy bridge accepts complete constraint skeleton");
        for(const auto& track:target.animations[1].tracks)if(track.boneIndex==gun)failures+=!expect(!track.ownsLayer,"T9 ADS cannot own grip-relative gun channels across an unmapped mount");
        for(const float weight:{0.f,.5f,1.f}){
            const auto a=source.sampleLayeredPose(0,0,1,1,weight,scene::LayerMode::Override,false),b=target.sampleLayeredPose(0,0,1,1,weight,scene::LayerMode::Override,false);
            const auto sg=source.skeleton.boneByName.at("tag_weapon");for(int k=0;k<16;++k)failures+=!expect(std::abs(a[sg].v[k]-b[gun].v[k])<.001f,"T9 ADS layered gun transform matches source");
        }
        auto other=source;other.animations[0].sourceName="viewmodel_t6_idle.cast";auto output=target;output.animations.resize(1);
        failures+=!expect(!scene::retargetColdWarLegacyRange(output,0,other,0),"T9 conversion excludes non-native animation names");
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
        {"pb_rpg_runjump_land",scene::MotionRole::Land,scene::ActionRole::None,scene::WeaponClass::Launcher,scene::Stance::Stand,scene::Direction::Any,scene::AnimationDomain::PlayerBody},
        {"pb_terrain_slide",scene::MotionRole::Slide,scene::ActionRole::None,scene::WeaponClass::Any,scene::Stance::Stand,scene::Direction::Any,scene::AnimationDomain::PlayerBody},
        {"pb_briefcase_crouch2prone",scene::MotionRole::Transition,scene::ActionRole::None,scene::WeaponClass::Briefcase,scene::Stance::Crouch,scene::Direction::Any,scene::AnimationDomain::PlayerBody}
    };
    for(const auto& item:classifications){scene::Animation classified;scene::classifyAnimationName(item.name,classified);
        const auto correct=classified.motion==item.motion&&classified.action==item.action&&classified.weapon==item.weapon&&classified.stance==item.stance&&classified.direction==item.direction&&classified.domain==item.domain;
        failures+=!expect(correct,item.name);
    }
    scene::Animation angular;scene::classifyAnimationName("pb_standmg42gunner_aim_45left_15down",angular);
    failures+=!expect(angular.hasAimYaw&&angular.aimYawDegrees==-45&&angular.hasAimPitch&&angular.aimPitchDegrees==-15,"BO2 angular aim sectors classify");
    scene::Animation alert;scene::classifyAnimationName("pb_crouch_alert_pistol",alert);
    failures+=!expect(alert.motion==scene::MotionRole::Idle&&alert.action==scene::ActionRole::None,"BO2 alert pose remains an idle locomotion candidate");
    scene::Animation frontDeath;scene::classifyAnimationName("pb_death_headshot_front",frontDeath);
    failures+=!expect(frontDeath.action==scene::ActionRole::Death&&frontDeath.direction==scene::Direction::Forward,"BO2 death front tag maps to forward locomotion direction");
    scene::Animation rifleShoot;scene::classifyAnimationName("pb_stand_shoot_walk_forward",rifleShoot);
    failures+=!expect(rifleShoot.action==scene::ActionRole::Fire&&rifleShoot.motion==scene::MotionRole::Walk&&rifleShoot.weapon==scene::WeaponClass::Rifle,"BO2 combined shoot-walk classifies as rifle locomotion action");
    scene::Animation shuffle;scene::classifyAnimationName("pb_rifle_stand_shuffle_f",shuffle);
    failures+=!expect(shuffle.motion==scene::MotionRole::Walk&&shuffle.direction==scene::Direction::Forward&&!shuffle.contextual,"rifle shuffle is walk, not idle");
    scene::Animation slope;scene::classifyAnimationName("pb_rifle_stand_run_slopedown",slope);
    failures+=!expect(slope.motion==scene::MotionRole::Run&&slope.contextual,"slope run is contextual");
    scene::Animation ballisticIdle;scene::classifyAnimationName("pb_b_knife_stand_idle",ballisticIdle);
    failures+=!expect(ballisticIdle.weapon==scene::WeaponClass::BallisticKnife&&ballisticIdle.direction==scene::Direction::Any,"ballistic prefix is not backward direction");
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
    rifleFireQuery.motion=scene::MotionRole::Walk;failures+=!expect(scene::findBestAnimation(combinedFire,rifleFireQuery).has_value(),"combined walk-shoot resolves during matching rifle locomotion");
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
        // These view-only stations must not inherit the receiver bolt offset.
        for(const auto name:{"t6_wpn_ar_an94_view_LOD0","t6_wpn_ar_scarh_view_LOD0","t6_wpn_sniper_ballista_view_LOD0"}){
            auto rig=scene::buildScene(cast::Document::parse(animatedTriangle("test_material","j_gun"),"receiver.cast"));
            rig.rigParts.push_back({name,0,rig.meshes.size(),{0}});
            const auto mag=cast::Document::parse(animatedTriangle("test_material","tag_clip"),"mag.cast");
            scene::appendRigModel(mag,rig,"t6_attach_mag_test_view_LOD0");
            const auto& b=rig.skeleton.bones[rig.skeleton.boneByCanonicalName.at("tag_clip")];
            failures+=!expect(scene::length(b.restLocal.position-*scene::t6MagazineMount(name))<.0001f,"measured T6 magazine station");
            failures+=!expect(b.translationTracksAreDeltas,"mounted magazine keeps authored translation offsets");
        }
        failures+=!expect(!scene::t6MagazineMount("t6_wpn_ar_an94_world_LOD0")&&!scene::t6MagazineMount("ak47_model")&&!scene::t6MagazineMount("t6_wpn_ar_other_view"),"magazine calibration excludes unmeasured assets and CS2");
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
    if(!failures)std::cout<<"All Cast scene tests passed.\n";return failures?1:0;
}
