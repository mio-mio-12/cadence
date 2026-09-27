#pragma once
#include "scene/ImportedNative.h"
#include <iostream>

inline int testImportedNativeHandCorrespondence(){
    int failures{};
    const auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAILED native hands: "<<text<<'\n';}};
    {scene::Skeleton curled;for(const auto p:{scene::Vec3{1.7122f,-23.7958f,4.04792f},scene::Vec3{3.10658f,-25.9818f,3.66713f},scene::Vec3{3.59966f,-25.6113f,3.40712f},scene::Vec3{3.91659f,-25.1365f,3.16748f},scene::Vec3{4.02862f,-24.4936f,2.93726f}}){scene::Bone b;b.restGlobal=scene::translation(p);curled.bones.push_back(b);}std::vector<int> order{4,2,0,3,1};scene::imported::orderAnonymousFingerRoots(curled,order);check(order==std::vector<int>({0,1,2,3,4}),"curled knuckles follow adjacent row, not radial thumb distance");}
    {scene::CastScene labelled;scene::Mesh hands,weapon;hands.name="hands/gloves/0";weapon.name="studio/gun_furniture/0";labelled.meshes={hands,weapon};scene::imported::retainExplicitHandMeshes(labelled);check(labelled.meshes.size()==1&&labelled.meshes[0].name==hands.name,"explicit hand categories exclude exported weapon furniture");scene::CastScene unlabelled;weapon.name="native_mesh";unlabelled.meshes={weapon};scene::imported::retainExplicitHandMeshes(unlabelled);check(unlabelled.meshes.size()==1,"unlabelled native hand sets remain untouched");}
    {scene::CastScene mixed;mixed.skeleton.bones.resize(3);mixed.skeleton.bones[0].name="arm";mixed.skeleton.bones[1].parent=0;mixed.skeleton.bones[1].name="finger";mixed.skeleton.bones[2].parent=1;mixed.skeleton.bones[2].name="weapon_attachment";
        for(int bone:{1,0,2}){scene::Mesh mesh;mesh.name=bone==1?"hands/palm":"studio/part";scene::Vertex v;v.bones[0]=bone;mesh.vertices={v,v,v};mesh.indices={0,1,2};mixed.meshes.push_back(mesh);}scene::imported::retainExplicitHandMeshes(mixed);check(mixed.meshes.size()==2&&mixed.meshes[1].vertices[0].bones[0]==0,"unlabelled arm-weighted sleeve retained while wrist-attached furniture is excluded");}
    const auto make=[](bool target){
        scene::CastScene s;s.importedViewGame="cz";
        const auto add=[&](std::string name,int parent,scene::Vec3 p){
            scene::Bone b;b.name=std::move(name);b.parent=parent;b.restLocal.position=p;
            b.restGlobal=parent<0?scene::translation(p):s.skeleton.bones[parent].restGlobal*scene::translation(p);b.inverseBind=scene::inverseAffine(b.restGlobal);
            const auto index=s.skeleton.bones.size();s.skeleton.boneByName[b.name]=index;s.skeleton.bones.push_back(b);return static_cast<int>(index);
        };
        for(int side=0;side<2;++side){
            const auto root=add("Bone"+std::to_string((target?70:0)+side*18),-1,{float(side*20),0,0});
            const auto elbow=add(target?(side==0?"Bone_Righthand":"Bone_Lefthand"):"elbow"+std::to_string(side),root,{0,1,0});
            const auto wrist=add("wrist"+std::to_string(side),elbow,{0,1,0});
            for(int i=0;i<5;++i){const int digit=target?4-i:i;auto parent=wrist;for(int j=0;j<3;++j)parent=add("digit"+std::to_string(side)+"_"+std::to_string(i)+"_"+std::to_string(j),parent,j==0?scene::Vec3{digit==0?-5.f:float(digit),digit==0?-2.f:0.f,0}:scene::Vec3{0,1,0});}
            scene::Mesh mesh;mesh.name=side==0?"rhand/test":"lhand/test";scene::Vertex v;v.position={float(side*20),0,0};v.normal={0,0,1};v.weights[0]=1;v.bones[0]=root;v.uv={.25f,.75f};mesh.vertices.push_back(v);s.meshes.push_back(mesh);
        }
        return s;
    };
    const auto source=make(false),target=make(true);
    const auto map=scene::imported::goldSrcHandCorrespondence(source,target);
    check(map.size()==36,"mapping preserves source size");
    check(map[0]==0&&map[18]==18&&map[2]==2&&map[20]==20,"arm roots and wrists follow side labels");
    check(map[3]==15&&map[6]==12&&map[15]==3,"digit order is geometric, not exported numbering");
    auto incomplete=target;incomplete.skeleton.bones[5].parent=2;
    const auto rejected=scene::imported::goldSrcHandCorrespondence(source,incomplete);
    check(std::all_of(rejected.begin(),rejected.end(),[](int v){return v<0;}),"incomplete anatomy never partially remaps");
    auto ambiguous=target;for(auto& b:ambiguous.skeleton.bones)b.name="unlabelled";for(auto& m:ambiguous.meshes)m.name="weapon";
    const auto unknown=scene::imported::goldSrcHandCorrespondence(source,ambiguous);
    check(std::all_of(unknown.begin(),unknown.end(),[](int v){return v<0;}),"unidentified handedness is not guessed");
    scene::CastScene result;std::string error;
    {auto gun=source,skin=source;gun.meshes[0].name="weapon/family/0";skin.meshes[0].name="hands/glove/0";auto furniture=skin.meshes[0];furniture.name="weapon/family/2";furniture.indices={0,0,0};skin.meshes.push_back(furniture);scene::imported::retainExplicitHandMeshes(skin);check(skin.meshes.back().viewmodelWeapon,"weapon furniture not tagged");scene::CastScene merged;check(scene::imported::assemblePrepared(gun,skin,merged,error)&&merged.meshes.back().name==furniture.name&&merged.meshes.back().viewmodelWeapon,"matching split weapon furniture not recovered");skin.meshes.back().name="weapon/other/2";check(scene::imported::assemblePrepared(gun,skin,merged,error)&&std::none_of(merged.meshes.begin(),merged.meshes.end(),[](const auto& m){return m.name=="weapon/other/2";}),"foreign hand donor weapon furniture leaked");}
    check(scene::imported::assemblePrepared(target,source,result,error),"topology assembly succeeds");
    check(result.meshes.size()==4&&result.meshes.back().vertices[0].uv.x==.25f&&result.meshes.back().vertices[0].uv.y==.75f,"assembly keeps hand UVs untouched");
    scene::CastScene invalidResult;
    check(!scene::imported::assemblePrepared(incomplete,source,invalidResult,error)&&!error.empty(),"incomplete incompatible donor fails instead of collapsing");
    auto matchingIncomplete=source;matchingIncomplete.skeleton.bones[5].parent=2;
    check(scene::imported::assemblePrepared(matchingIncomplete,matchingIncomplete,invalidResult,error),"exact incomplete native pair retains valid identity assembly");
    auto conflicting=source;conflicting.skeleton.bones[0].parent=18;
    check(!scene::imported::assemblePrepared(conflicting,source,invalidResult,error),"same BoneNN name with incompatible ancestry is rejected");
    return failures;
}
