#include "scene/ImportedWorldGrip.h"
#include <iostream>
#include <limits>
using namespace scene;
namespace {
void add(Skeleton& s,const char* name,Vec3 position){Bone b;b.name=name;b.restGlobal=translation(position);b.inverseBind=inverseAffine(b.restGlobal);s.boneByName[name]=s.bones.size();s.bones.push_back(b);}
Skeleton hand(const Mat4& basis){Skeleton s;for(const auto& [n,p]:std::initializer_list<std::pair<const char*,Vec3>>{{"j_wrist_ri",{0,0,0}},{"j_index_ri_1",{7,2,0}},{"j_mid_ri_1",{8,0,0}},{"j_pinky_ri_1",{6,-2,0}},{"j_thumb_ri_1",{2,4,0}},{"tag_weapon_right",{4,0,-3}}}){add(s,n,{});auto& b=s.bones.back();b.restGlobal=basis*translation(p);b.inverseBind=inverseAffine(b.restGlobal);}return s;}
bool near(Vec3 a,Vec3 b){return length(a-b)<.001f;}
}
int main(int argc,char** argv){int failures{};const auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<m<<'\n';++failures;}};
    auto donor=hand(translation({10,20,100})),target=hand(translation({-30,40,150})*rotation(fromEulerRadians({.2f,.3f,1.f})));
    CastScene world;add(world.skeleton,"tag_weapon",{2,0,0});
    CastScene view;view.skeleton=donor;
    Mesh gun;gun.name="weapon/test";gun.vertices.resize(3);gun.indices={0,1,2};
    for(auto& v:gun.vertices){v.position={14,20,100};v.bones[0]=5;v.weights={1,0,0,0};}
    view.meshes.push_back(gun);auto hands=gun;hands.name="hands/test";view.meshes.push_back(hands);
    const auto placeholder=imported::world_grip::fromViewHold(view,imported::world_grip::restPose(donor),target,2.54f);
    check(placeholder.has_value(),"view hold supplies missing worldmodel mount");
    check(view.meshes.size()==1,"placeholder excludes embedded hands");
    {
        CastScene unfamiliar;add(unfamiliar.skeleton,"gun",{3,4,5});add(unfamiliar.skeleton,"cs_hand_R.palm",{});
        Mesh weapon;weapon.name="weapon/body";weapon.skinned=true;weapon.vertices.resize(3);weapon.indices={0,1,2};for(auto& v:weapon.vertices){v.position={4,4,5};v.normal={0,0,1};v.bones[0]=0;v.weights={1,0,0,0};}unfamiliar.meshes.push_back(weapon);
        auto arms=weapon;arms.name="weapon/legacy_arm_label";arms.viewmodelWeapon=true;for(auto& v:arms.vertices)v.bones[0]=1;unfamiliar.meshes.push_back(arms);
        add(unfamiliar.skeleton,"arbitrary_sleeve_helper",{});unfamiliar.skeleton.bones.back().parent=1;
        auto sleeve=arms;sleeve.name="weapon/unknown_helper_mesh";for(auto& v:sleeve.vertices)v.bones[0]=2;unfamiliar.meshes.push_back(sleeve);
        unfamiliar.skeleton.bones[0].parent=1;
        auto raw=unfamiliar;std::string error;const auto pose=imported::world_grip::restPose(raw.skeleton);
        auto m=imported::world_grip::fromAuthoredAnchor(unfamiliar,pose,target,2.54f,error);
        check(m.has_value()&&unfamiliar.meshes.size()==1,"authored placeholder retains wrist-parented gun and rejects mislabeled arms and helper descendants");
        if(m){const auto combined=target.bones[m->bone].restGlobal*m->transform;check(near(transformPoint(combined,{3,4,5}),transformPoint(target.bones[m->bone].restGlobal,{})),"placeholder source anchor maps to authored target socket");check(std::abs(length(transformPoint(combined,{4,4,5})-transformPoint(combined,{3,4,5}))-2.54f)<.001f,"authored placeholder units convert once");}
        auto noSocket=target;noSocket.boneByName.erase("tag_weapon_right");auto rejected=raw;
        check(!imported::world_grip::fromAuthoredAnchor(rejected,pose,noSocket,2.54f,error)&&rejected.meshes.size()==raw.meshes.size(),"no authored target socket never attaches to pelvis and preserves input");
        auto corrupt=raw;corrupt.meshes[0].vertices[0].bones[0]=999;
        check(!imported::world_grip::fromAuthoredAnchor(corrupt,pose,target,2.54f,error),"corrupt skin rejected rather than approximate mount");
        auto badPose=pose;badPose[0].v[12]=std::numeric_limits<float>::quiet_NaN();
        check(!imported::world_grip::fromAuthoredAnchor(raw,badPose,target,2.54f,error),"nonfinite anchor rejected");
        auto rootOnly=raw;rootOnly.skeleton.boneByName.erase("gun");
        check(!imported::world_grip::fromAuthoredAnchor(rootOnly,pose,target,2.54f,error),"arbitrary root is not a grip anchor");
        auto qualified=rootOnly;qualified.skeleton.bones[0].name="dew2cast_weapon__gun";qualified.skeleton.boneByName[qualified.skeleton.bones[0].name]=0;
        const auto qualifiedMount=imported::world_grip::fromAuthoredAnchor(qualified,pose,target,2.54f,error);
        check(qualifiedMount.has_value()&&qualified.meshes.size()==1,"explicit exporter-qualified gun anchor supports separated weapon rigs without viewhands");
        if(m&&qualifiedMount)for(int k=0;k<16;++k)check(std::abs(m->transform.v[k]-qualifiedMount->transform.v[k])<.00001f,"qualified authored gun anchor preserves existing unit and socket convention");
        for(const char* badName:{"unknown_exporter__gun","dew2cast_weapon__gunner","dew2cast_weapon__root"}){auto bad=rootOnly;bad.skeleton.bones[0].name=badName;bad.skeleton.boneByName[badName]=0;check(!imported::world_grip::fromAuthoredAnchor(bad,pose,target,2.54f,error),"unrecognized qualified root is not accepted as gun anchor");}
        auto rigidGun=raw;rigidGun.meshes.resize(1);rigidGun.meshes[0].skinned=false;rigidGun.meshes[0].viewmodelWeapon=true;for(auto& v:rigidGun.meshes[0].vertices)v.bones[0]=1;
        check(imported::world_grip::fromAuthoredAnchor(rigidGun,pose,target,2.54f,error).has_value(),"unskinned classified weapon ignores inactive arm-index weights");
        rigidGun=raw;rigidGun.meshes.resize(1);rigidGun.meshes[0].skinned=false;rigidGun.meshes[0].viewmodelWeapon=false;
        check(!imported::world_grip::fromAuthoredAnchor(rigidGun,pose,target,2.54f,error),"unskinned default weights do not establish weapon ownership");
    }
    if(argc==3){auto asset=buildScene(cast::Document::load(argv[1]));appendAnimations(cast::Document::load(argv[2]),asset);
        check(!asset.animations.empty(),"real asset idle loaded");
        if(!asset.animations.empty()){const auto pose=asset.samplePose(0,0);const auto m=imported::world_grip::fromViewHold(asset,pose,target,2.54f);
            check(m.has_value(),"real asset mounted");check(!asset.meshes.empty(),"real asset weapon retained");
            std::cout<<"Fallback meshes="<<asset.meshes.size()<<" mounted="<<m.has_value()<<'\n';}}
    if(placeholder){const auto m=target.bones[placeholder->bone].restGlobal*placeholder->transform;
        check(near(transformPoint(m,{10,20,100}),transformPoint(target.bones[0].restGlobal,{})),"view wrist maps to target wrist");
        check(std::abs(length(transformPoint(m,{20,20,100})-transformPoint(m,{10,20,100}))-25.4f)<.001f,"placeholder units applied once");}
    std::string error;auto mount=imported::world_grip::fromNativeSocket("bo2",world,donor,imported::world_grip::restPose(donor),target,error);
    {
        CastScene rifle;
        add(rifle.skeleton,"cs_hand_L.palm",{3,2,0});
        add(rifle.skeleton,"cs_hand_R.palm",{13,0,1});
        add(rifle.skeleton,"gun",{});
        Mesh mesh;mesh.name="weapon/rifle";mesh.vertices.resize(3);for(auto& v:mesh.vertices){v.bones[0]=2;v.weights={1,0,0,0};}mesh.indices={0,1,2};rifle.meshes.push_back(mesh);
        auto held=donor;add(held,"j_wrist_le",{34,29,101});
        const auto sourcePose=imported::world_grip::restPose(rifle.skeleton);
        const auto heldPose=imported::world_grip::restPose(held);
        const auto originalRifle=rifle;
        const auto mount=imported::world_grip::fromViewHold(rifle,sourcePose,held,2.54f,&held,heldPose,true);
        check(mount.has_value(),"universal rifle grip available");
        if(mount){
            const auto combined=heldPose[mount->bone]*mount->transform;
            const auto trigger=transformPoint(combined,{3,2,0}),support=transformPoint(combined,{13,0,1});
            check(near(trigger,transformPoint(heldPose[0],{})),"rear palm mounts to trigger hand, not support palm");
            check(near(normalize(support-trigger),normalize(Vec3{34,29,101}-trigger)),"authored support-hand direction preserved");
            check(std::abs(length(support-trigger)-length(Vec3{10,-2,1})*2.54f)<.001f,"grip alignment preserves physical units");
        }
        const auto convention=imported::world_grip::deriveViewHoldConvention(held,heldPose);
        check(convention.has_value(),"compact hold convention derivable");
        if(convention)for(bool barrel:{false,true})for(bool rear:{false,true}){
            auto legacy=originalRifle,compact=originalRifle;if(barrel){legacy.muzzleAnchors.push_back({2,{},0});compact.muzzleAnchors=legacy.muzzleAnchors;}
            const auto a=imported::world_grip::fromViewHold(legacy,sourcePose,target,2.54f,&held,heldPose,rear);
            const auto b=imported::world_grip::fromViewHold(compact,sourcePose,target,2.54f,nullptr,{},rear,&*convention);
            bool same=a&&b;if(same)for(int i=0;i<16;++i)same=same&&std::abs(a->transform.v[i]-b->transform.v[i])<.0001f;
            check(same,"compact convention preserves camera/span/barrel calibration without donor at mount time");
        }
    }
    check(mount.has_value(),"native donor socket gives a mount");
    if(mount){const auto combined=target.bones[mount->bone].restGlobal*mount->transform;check(near(transformPoint(combined,{2,0,0}),transformPoint(target.bones.back().restGlobal,{})),"weapon socket matches anatomically transferred target socket");check(std::abs(length(transformPoint(combined,{12,0,0})-transformPoint(combined,{2,0,0}))-10)<.001f,"target body never resizes world weapon");}
    check(!imported::world_grip::fromNativeSocket("cs2",world,donor,imported::world_grip::restPose(donor),target,error),"CS2 is excluded");
    {
        auto eldDonor=donor;eldDonor.boneByName["dew2cast_socket__right_hand__0"]=5;
        CastScene eld;eld.importedTranslationScale=304.8f;add(eld.skeleton,"gun",{3,0,12});
        const auto original=imported::world_grip::fromNativeSocket("eldewrito",eld,eldDonor,imported::world_grip::restPose(eldDonor),target,error);
        check(original.has_value(),"normalized Eld without grip marker retains gun mount");
        add(eld.skeleton,"dew2cast_socket__right_hand__0",{});eld.skeleton.bones[1].parent=0;
        eld.skeleton.bones[1].restGlobal=translation({0,-.2f,-1.4f})*rotation(fromEulerRadians({.2f,-.1f,.3f}));
        const auto marked=imported::world_grip::fromNativeSocket("eldewrito",eld,eldDonor,imported::world_grip::restPose(eldDonor),target,error);
        check(marked.has_value()&&imported::world_grip::nativeSocket("eldewrito",eld,eldDonor).weapon==1,"explicit valid Eld descendant grip selected");
        if(original&&marked){const auto expected=original->transform*eld.skeleton.bones[0].restGlobal*inverseAffine(eld.skeleton.bones[1].restGlobal);for(int k=0;k<16;++k)check(std::abs(expected.v[k]-marked->transform.v[k])<.0001f,"explicit grip frame matches full translated and rotated socket math");}
        const auto valid=eld;
        for(int fault=0;fault<9;++fault){eld=valid;
            if(fault==0)eld.skeleton.bones[1].parent=-1;
            if(fault==1)eld.skeleton.bones[1].restGlobal=scale({0,1,1});
            if(fault==2)eld.skeleton.bones[1].restGlobal=scale({-1,1,1});
            if(fault==3)eld.skeleton.bones[1].parent=1;
            if(fault==4)eld.skeleton.bones[0].parent=1;
            if(fault==5)eld.skeleton.bones[1].parent=100;
            if(fault==6)eld.skeleton.bones[1].restGlobal.v[12]=std::numeric_limits<float>::quiet_NaN();
            if(fault==7)eld.skeleton.bones[1].restGlobal=scale({2,1,1});
            if(fault==8){eld.skeleton.boneByName.erase("dew2cast_socket__right_hand__0");eld.skeleton.boneByName["dew2cast_socket__right_hand_brute__0"]=1;}
            check(imported::world_grip::nativeSocket("eldewrito",eld,eldDonor).weapon==0,"invalid/unrelated/nonexact Eld marker preserves gun fallback");
            const auto fallback=imported::world_grip::fromNativeSocket("eldewrito",eld,eldDonor,imported::world_grip::restPose(eldDonor),target,error);
            check(fallback.has_value(),"invalid optional Eld marker does not disable existing mount");
            if(original&&fallback)for(int k=0;k<16;++k)check(original->transform.v[k]==fallback->transform.v[k],"invalid marker fallback mount remains exact");
        }
        eld=valid;eld.importedTranslationScale=1;check(imported::world_grip::nativeSocket("eldewrito",eld,eldDonor).weapon==-1,"unnormalized Eld marker remains excluded");
        check(!imported::world_grip::fromNativeSocket("cs2",valid,eldDonor,imported::world_grip::restPose(eldDonor),target,error),"explicit marker cannot bypass CS2 exclusion");
        eld=valid;add(eld.skeleton,"unweighted_helper",{});eld.skeleton.bones[1].parent=2;eld.skeleton.bones[2].parent=0;
        check(imported::world_grip::nativeSocket("eldewrito",eld,eldDonor).weapon==1,"valid helper ancestry supports explicit marker");
    }
    check(!imported::world_grip::fromNativeSocket("cs1.6",world,donor,imported::world_grip::restPose(donor),target,error),"unregistered dropped model fails closed");
    auto far=imported::world_grip::restPose(donor);far.back()=translation({0,0,0});check(!imported::world_grip::fromNativeSocket("bo2",world,donor,far,target,error),"parked donor socket rejected");
    auto missing=target;missing.boneByName.erase("j_wrist_ri");check(!imported::world_grip::fromNativeSocket("bo2",world,donor,imported::world_grip::restPose(donor),missing,error),"missing target wrist rejected");
    CastScene sourceWorld;add(sourceWorld.skeleton,"CSO2_W_bone",{});auto csoDonor=donor;csoDonor.boneByName["CSO2_W_bone"]=csoDonor.boneByName["tag_weapon_right"];
    auto scaled=imported::world_grip::fromNativeSocket("cso2",sourceWorld,csoDonor,imported::world_grip::restPose(csoDonor),target,error);
    check(scaled.has_value(),"native Source unit scale available");if(scaled){const auto m=target.bones[scaled->bone].restGlobal*scaled->transform;check(std::abs(length(transformPoint(m,{10,0,0})-transformPoint(m,{}))-25.4f)<.001f,"raw world Source units convert once to centimetres");}
    auto ikDonor=csoDonor;add(ikDonor,"CSO2_W_bone_RHand",{});
    ikDonor.bones.back().restGlobal=ikDonor.bones[0].restGlobal*rotation(fromEulerRadians({.8f,.2f,1.1f}));
    auto ikMount=imported::world_grip::fromNativeSocket("cso2",sourceWorld,ikDonor,imported::world_grip::restPose(ikDonor),target,error);
    check(ikMount.has_value(),"CSO2 native IK helper provides palm reference");
    if(ikMount&&scaled)for(int i=0;i<16;++i)check(std::abs(ikMount->transform.v[i]-scaled->transform.v[i])<.001f,"CSO2 helper bind axes cannot rotate the anatomical mount");
    CastScene knife;add(knife.skeleton,"CSO2_BipM R Hand",{});auto knifeDonor=ikDonor;knifeDonor.boneByName["CSO2_BipM R Hand"]=0;
    const auto knifeMount=imported::world_grip::fromNativeSocket("cso2",knife,knifeDonor,imported::world_grip::restPose(knifeDonor),target,error);
    check(knifeMount.has_value(),"CSO2 hand-rooted world knife uses anatomical registration");
    if(knifeMount)check(near(transformPoint(target.bones[knifeMount->bone].restGlobal*knifeMount->transform,{}),transformPoint(target.bones[0].restGlobal,{})),"world knife hand origin maps to actual destination wrist without gun IK offsets");
    CastScene cssWorld;add(cssWorld.skeleton,"ValveBiped.weapon_bone",{});auto cssDonor=donor;add(cssDonor,"ValveBiped.weapon_bone",{});add(cssDonor,"ValveBiped.weapon_bone_RHand",{0,0,5});
    check(imported::world_grip::fromNativeSocket("css",cssWorld,cssDonor,imported::world_grip::restPose(cssDonor),target,error).has_value(),"explicit Source weapon-hand IK reference replaces detached bind pose");
    Skeleton variant;add(variant,"Bip01 R Hand",{});
    add(variant,"Bip01 R Finger0_anymodel",{2,4,0});variant.bones.back().parent=0;
    add(variant,"Bip01 R Finger1_anymodel",{7,2,0});variant.bones.back().parent=0;
    add(variant,"Bip01 R Finger2_anymodel",{8,0,0});variant.bones.back().parent=0;
    check(imported::world_grip::palmFrame(variant,imported::world_grip::restPose(variant)).has_value(),"GoldSrc model-suffixed base fingers provide anatomical palm");
    variant.bones[2].parent=1;
    check(!imported::world_grip::palmFrame(variant,imported::world_grip::restPose(variant)),"variant lookup rejects non-wrist finger descendants");
    return failures?1:0;
}
