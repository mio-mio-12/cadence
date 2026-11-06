#include "scene/ProximalSupportFit.h"
#include "scene/ArmComponentSupport.h"
#include "scene/NativeSupportAssembly.h"
#include <iostream>
#include <stdexcept>
using namespace scene;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::size_t add(CastScene& s,std::string name,int parent,Vec3 p){Bone b;b.name=name;b.parent=parent;b.restGlobal=translation(p);b.inverseBind=inverseAffine(b.restGlobal);b.restLocal.position=parent>=0?p-transformPoint(s.skeleton.bones[parent].restGlobal,{}):p;auto i=s.skeleton.bones.size();s.skeleton.boneByName[name]=i;s.skeleton.bones.push_back(b);return i;}
static CastScene fixture(){CastScene s;add(s,"j_spineupper",-1,{});for(const auto* side:{"le","ri"}){const float y=std::string(side)=="le"?0:20;auto clav=add(s,"j_clavicle_"+std::string(side),0,{0,y,0});auto shoulder=add(s,"j_shoulder_"+std::string(side),clav,{0,y,0});auto twist=add(s,"j_shouldertwist_"+std::string(side),shoulder,{0,y,0});auto elbow=add(s,"j_elbow_"+std::string(side),shoulder,{10,y,0});auto wrist=add(s,"j_wrist_"+std::string(side),elbow,{20,y,0});for(const auto* finger:{"thumb","index","mid","ring","pinky"}){int parent=wrist;for(int j=0;j<3;++j)parent=add(s,"j_"+std::string(finger)+"_"+side+"_"+std::to_string(j),parent,{21.f+j,y,0});}
    Mesh m;m.name="connected_arm";m.skinned=true;for(auto p:{Vec3{1,y,0},Vec3{2,y,0},Vec3{1,y+1,0},Vec3{10,y,0}}){Vertex v;v.position=p;v.normal={0,0,1};v.bones={static_cast<unsigned>(twist),static_cast<unsigned>(shoulder),0,0};v.weights={.8f,.2f,0,0};m.vertices.push_back(v);}m.vertices[3].bones={static_cast<unsigned>(elbow),0,0,0};m.vertices[3].weights={1,0,0,0};m.indices={0,1,2,1,3,2};s.meshes.push_back(m);
    // A shield descendant connected to the arm still is not anatomical support.
    const auto shield=add(s,"j_shield_"+std::string(side),shoulder,{1,y,0});
    auto shieldVertex=m.vertices[0];shieldVertex.position={1.2f,y+.2f,.01f};shieldVertex.bones={static_cast<unsigned>(shield),0,0,0};shieldVertex.weights={1,0,0,0};
    s.meshes.back().vertices.push_back(shieldVertex);s.meshes.back().indices.insert(s.meshes.back().indices.end(),{0,4,2});
    // Closer disconnected armor triangle must not displace arm correspondence.
    m.name="disconnected_armor";m.vertices.resize(3);for(auto& v:m.vertices){v.position.z=.02f;v.bones={static_cast<unsigned>(shoulder),0,0,0};v.weights={1,0,0,0};}m.indices={0,1,2};s.meshes.push_back(m);
}return s;}
int main(){try{
auto component=fixture();component.meshes.resize(1);auto& cm=component.meshes[0];
const auto rollFixture=fixture();const auto rollLayout=imported::viewLayout(rollFixture);
check(imported::nativeProximalRoll(rollFixture,rollLayout.arms[0])==static_cast<int>(rollFixture.skeleton.boneByName.at("j_shouldertwist_le")),"native weighted roll not resolved");
auto unusedRoll=rollFixture;unusedRoll.meshes.clear();check(imported::nativeProximalRoll(unusedRoll,rollLayout.arms[0])<0,"unused roll accepted");
auto farRoll=rollFixture;farRoll.skeleton.bones[farRoll.skeleton.boneByName.at("j_shouldertwist_le")].restGlobal=translation({5,0,0});check(imported::nativeProximalRoll(farRoll,rollLayout.arms[0])<0,"distal helper accepted as proximal carrier");
const auto cs=static_cast<unsigned>(component.skeleton.boneByName.at("j_shoulder_le"));
for(auto& v:cm.vertices){v.bones={cs,0,0,0};v.weights={.6f,.4f,0,0};}
const auto originalComponent=component;
check(imported::retainArmComponentSupport(component)==cm.vertices.size(),"isolated same-arm ancestors not retained");
for(std::size_t i=0;i<cm.vertices.size();++i){const auto& before=originalComponent.meshes[0].vertices[i];check(cm.vertices[i].bones[1]==cs&&cm.vertices[i].weights==before.weights&&length(cm.vertices[i].position-before.position)==0,"component geometry or weights changed");}
auto mixed=originalComponent;mixed.meshes[0].vertices[0].bones[0]=static_cast<unsigned>(mixed.skeleton.boneByName.at("j_shoulder_ri"));check(imported::retainArmComponentSupport(mixed)==0,"mixed arm component remapped");
auto torso=originalComponent;torso.meshes[0].vertices[0].weights={.2f,.8f,0,0};check(imported::retainArmComponentSupport(torso)==0,"torso component remapped");
auto malformed=originalComponent;malformed.meshes[0].indices.push_back(999);check(imported::retainArmComponentSupport(malformed)==0,"invalid topology accepted");
auto weapon=originalComponent;weapon.meshes[0].viewmodelWeapon=true;check(imported::retainArmComponentSupport(weapon)==0,"weapon support changed");
auto native=fixture(),fitted=native;fitted.meshes.clear();auto adapter=std::make_shared<CodmRigAdapter>();adapter->firstBone=fitted.skeleton.bones.size();const auto source=fitted.skeleton.boneByName.at("j_shoulder_le");const auto follower=add(fitted,"arbitrary_shifted_follower",source,{3,0,0});CodmRigBinding binding;binding.source=source;binding.offset=inverseAffine(native.skeleton.bones[source].restGlobal)*fitted.skeleton.bones[follower].restGlobal;adapter->bindings.push_back(binding);fitted.codmRigAdapter=adapter;
Mesh mesh;mesh.name="target";mesh.skinned=true;for(auto p:{Vec3{1.2f,.2f,.02f},Vec3{1.4f,.2f,.02f},Vec3{9,.2f,.02f}}){Vertex v;v.position=p;v.normal={0,0,1};v.bones={static_cast<unsigned>(follower),0,0,0};v.weights={1,0,0,0};mesh.vertices.push_back(v);}mesh.indices={0,1,2};fitted.meshes.push_back(mesh);const auto before=fitted;
proximal_support::Options options;proximal_support::Report report;std::string error;check(proximal_support::apply(fitted,native,options,report,error)&&report.edited==0,"default off");options.enabled=true;check(proximal_support::apply(fitted,native,options,report,error),error.c_str());check(report.edited==2,"proximal vertices modified, distal untouched");check(report.eligibleTriangles==2,"disconnected armor excluded on both arms");const auto twist=native.skeleton.boneByName.at("j_shouldertwist_le");
for(std::size_t i=0;i<3;++i){const auto& a=fitted.meshes[0].vertices[i];const auto& b=before.meshes[0].vertices[i];check(length(a.position-b.position)==0&&length(a.normal-b.normal)==0,"geometry unchanged");float sum=0;Vec3 rest{};bool support=false;for(int k=0;k<4;++k)if(a.weights[k]>0){sum+=a.weights[k];rest+=transformPoint(fitted.skeleton.bones[a.bones[k]].restGlobal*fitted.skeleton.bones[a.bones[k]].inverseBind,a.position)*a.weights[k];support|=a.bones[k]==twist;}check(std::abs(sum-1)<1e-6f&&length(rest-a.position)<1e-5f,"normalized palette preserves fitted bind");if(i<2)check(support,"native connected support transferred");else check(a.bones==b.bones&&a.weights==b.weights,"distal palette exact");}
for(std::size_t b=0;b<fitted.skeleton.bones.size();++b)check(proximal_support::matrixError(fitted.skeleton.bones[b].inverseBind,before.skeleton.bones[b].inverseBind)==0,"inverse binds unchanged");check(fitted.meshes[0].indices==before.meshes[0].indices,"topology unchanged");
auto conformed=before;options.conformSurface=true;
check(proximal_support::apply(conformed,native,options,report,error)&&report.edited==2,"surface candidate rejected valid support");
check(std::abs(conformed.meshes[0].vertices[0].position.z)<1e-6f,"surface candidate did not reach native surface");
check(length(conformed.meshes[0].vertices[2].position-before.meshes[0].vertices[2].position)==0,"surface candidate changed distal geometry");
check(conformed.meshes[0].indices==before.meshes[0].indices,"surface candidate changed topology");
for(int i=0;i<3;++i)check(conformed.meshes[0].vertices[i].uv.x==before.meshes[0].vertices[i].uv.x&&conformed.meshes[0].vertices[i].uv.y==before.meshes[0].vertices[i].uv.y,"surface candidate changed UVs");
auto smoothNative=native;for(auto& m:smoothNative.meshes)for(auto& v:m.vertices)v.normal=normalize(Vec3{.3f,0,1});
auto smoothFit=before;check(proximal_support::apply(smoothFit,smoothNative,options,report,error),"smooth normal fit failed");
check(length(smoothFit.meshes[0].vertices[0].normal-normalize(Vec3{.3f,0,1}))<1e-5f,"authored smooth normals must not become face normals");
auto scaled=Mat4::identity();scaled.v[0]=2;scaled.v[5]=3;scaled.v[10]=4;
check(length(proximal_support::surfaceNormal(scaled,{1,1,1},{0,0,1})-normalize(Vec3{.5f,1.f/3,.25f}))<1e-5f,"inverse transpose normal transform");
check(length(proximal_support::surfaceNormal(scaled,{0,0,0},{0,0,1})-Vec3{0,0,1})<1e-5f,"zero normal fallback");
scaled.v[0]=0;check(length(proximal_support::surfaceNormal(scaled,{1,1,1},{0,0,1})-Vec3{0,0,1})<1e-5f,"singular transform fallback");
auto seamOriginal=before.meshes[0].vertices;seamOriginal.resize(4,seamOriginal[0]);
seamOriginal[1]=seamOriginal[0];seamOriginal[1].normal={0,1,0};seamOriginal[1].uv={.8f,.7f};
seamOriginal[2]=seamOriginal[0];seamOriginal[2].bones[0]+=1;
seamOriginal[3]=seamOriginal[0];seamOriginal[3].position.x+=.01f;
auto seam=seamOriginal;seam[0].position.z+=1;seam[0].bones[0]=0;
proximal_support::preserveSeams(seam,seamOriginal,.001f);
check(length(seam[0].position-seam[1].position)<1e-6f&&seam[0].bones==seam[1].bones,"hard normal seam split");
check(seam[1].normal.y==1&&seam[1].uv.x==.8f,"seam shading or UV overwritten");
check(length(seam[2].position-seamOriginal[2].position)==0,"different support merged");
check(length(seam[3].position-seamOriginal[3].position)==0,"nearby nonseam merged");
options.conformSurface=false;
auto invalid=before;auto broken=std::make_shared<CodmRigAdapter>(*adapter);broken->bindings[0].offset.v[12]+=2;invalid.codmRigAdapter=broken;check(proximal_support::apply(invalid,native,options,report,error)&&report.edited==0,"non-equivalent follower rejected");
invalid=before;invalid.meshes[0].modelTransform=translation({1,0,0});check(!proximal_support::apply(invalid,native,options,report,error),"mixed mesh frame rejected");
const Vec3 axis{1,0,0},normal=normalize(Vec3{1,2,3});
Mesh holeMesh;holeMesh.vertices.resize(5);
for(auto& v:holeMesh.vertices){v.bones={3,0,0,0};v.weights={1,0,0,0};}
holeMesh.vertices[1].position={1,0,0};holeMesh.vertices[2].position={0,1,0};holeMesh.vertices[3]=holeMesh.vertices[1];holeMesh.vertices[3].uv={.4f,.7f};holeMesh.vertices[4].position={99,0,0};holeMesh.indices={0,1,2,0,3,2};
const auto holeOriginal=holeMesh.vertices;
for(auto i:{0,2}){holeMesh.vertices[i].bones={1,3,0,0};holeMesh.vertices[i].weights={.5f,.5f,0,0};}
const auto holeBefore=holeMesh;
check(proximal_support::extendSupportHoles(holeMesh,holeOriginal,{true,true,true,true,true},2),"hole solve failed");
check(holeMesh.vertices[1].weights[0]>.49f&&holeMesh.vertices[1].bones[0]==1,"connected hole not filled");
check(holeMesh.vertices[1].weights==holeMesh.vertices[3].weights&&holeMesh.vertices[1].bones==holeMesh.vertices[3].bones,"hole seams must be exact");
check(holeMesh.vertices[4].weights==holeOriginal[4].weights&&holeMesh.vertices[4].bones==holeOriginal[4].bones,"unanchored island changed");
check(holeMesh.vertices[0].weights==holeBefore.vertices[0].weights&&holeMesh.vertices[2].weights==holeBefore.vertices[2].weights,"native anchors changed");
check(holeMesh.vertices[3].uv.x==.4f&&holeMesh.indices==holeBefore.indices,"hole correction changed UV/topology");
for(int i=0;i<5;++i)check(length(holeMesh.vertices[i].position-holeOriginal[i].position)==0,"hole correction moved bind geometry");
auto protectedHole=holeBefore;check(proximal_support::extendSupportHoles(protectedHole,holeOriginal,{true,false,true,false,true},2)&&protectedHole.vertices[1].weights==holeOriginal[1].weights,"protected support changed");
auto distantHole=holeBefore;check(proximal_support::extendSupportHoles(distantHole,holeOriginal,{true,true,true,true,true},.1f)&&distantHole.vertices[1].weights==holeOriginal[1].weights,"hole exceeded geodesic bound");
auto invalidHole=holeBefore;invalidHole.indices.push_back(99);check(!proximal_support::extendSupportHoles(invalidHole,holeOriginal,{true,true,true,true,true},2),"invalid hole topology accepted");
Mesh confidenceMesh;confidenceMesh.vertices.resize(4);confidenceMesh.vertices[1].position={1,0,0};confidenceMesh.vertices[2].position={0,1,0};confidenceMesh.vertices[3].position={9,9,0};confidenceMesh.indices={0,1,2};
std::vector<float> confidenceResult;
check(proximal_support::coherentConfidence(confidenceMesh,{true,true,true,true},{1,.01f,1,.01f},confidenceResult),"confidence solve failed");
check(std::abs(confidenceResult[1]-1)<1e-6f&&confidenceResult[3]==0,"connected low-confidence match or unanchored island incorrect");
check(proximal_support::coherentConfidence(confidenceMesh,{true,false,true,true},{1,.01f,1,.01f},confidenceResult)&&confidenceResult[1]==0,"rejected match gained confidence");
confidenceMesh.indices.push_back(99);check(!proximal_support::coherentConfidence(confidenceMesh,{true,true,true,true},{1,.01f,1,.01f},confidenceResult),"malformed topology accepted");
auto directNative=native;
const auto clavicle=native.skeleton.boneByName.at("j_clavicle_le");
for(int i=0;i<3;++i){auto& v=directNative.meshes[0].vertices[i];v.bones={static_cast<unsigned>(twist),static_cast<unsigned>(clavicle),0,0};v.weights={.15f,.45f,.4f,0};}
auto direct=before;direct.codmRigAdapter.reset();
for(auto& v:direct.meshes[0].vertices){v.bones={static_cast<unsigned>(twist),0,0,0};v.weights={1,0,0,0};}
const auto directBefore=direct;
auto fullMatch=directBefore;fullMatch.meshes[0].vertices[0].position.z=1.45f;fullMatch.meshes[0].vertices[1].position.z=1.51f;
const auto fullBefore=fullMatch;
check(proximal_support::applyDirectNative(fullMatch,directNative,report,error,false,false,true),"full matched support failed");
float fullSpine=0;for(int k=0;k<4;++k)if(fullMatch.meshes[0].vertices[0].bones[k]==0)fullSpine+=fullMatch.meshes[0].vertices[0].weights[k];
check(fullSpine>.3f,"accepted near-radius correspondence lost native support");
check(fullMatch.meshes[0].vertices[1].bones==fullBefore.meshes[0].vertices[1].bones&&fullMatch.meshes[0].vertices[1].weights==fullBefore.meshes[0].vertices[1].weights,"full match expanded distance gate");
check(length(fullMatch.meshes[0].vertices[0].position-fullBefore.meshes[0].vertices[0].position)==0,"full match changed bind geometry");
auto wrongNormal=fullBefore;wrongNormal.meshes[0].vertices[0].normal={0,0,-1};
check(proximal_support::applyDirectNative(wrongNormal,directNative,report,error,false,false,true)&&wrongNormal.meshes[0].vertices[0].weights==fullBefore.meshes[0].vertices[0].weights,"full match ignored normal gate");
check(proximal_support::applyDirectNative(direct,directNative,report,error)&&report.edited==2,"direct native spine/clavicle support");
check(report.eligibleTriangles==2,"direct mode must exclude shield and disconnected armor");
for(int i=0;i<3;++i){const auto& v=direct.meshes[0].vertices[i];check(length(v.position-directBefore.meshes[0].vertices[i].position)==0&&length(v.normal-directBefore.meshes[0].vertices[i].normal)==0,"direct support changed geometry");if(i<2){float spine=0;for(int k=0;k<4;++k)if(v.bones[k]==0)spine+=v.weights[k];check(spine>.1f,"native spine contribution omitted");}else check(v.weights==directBefore.meshes[0].vertices[i].weights&&v.bones==directBefore.meshes[0].vertices[i].bones,"direct distal weights changed");}
auto wrongPalette=directBefore;wrongPalette.skeleton.bones[twist].inverseBind.v[12]+=1;
auto residue=directBefore;
const auto elbowIndex=static_cast<unsigned>(native.skeleton.boneByName.at("j_elbow_le"));
for(auto& v:residue.meshes[0].vertices){v.bones={static_cast<unsigned>(twist),elbowIndex,0,0};v.weights={.999999f,.000001f,0,0};}
check(proximal_support::applyDirectNative(residue,directNative,report,error,false,false,true)&&report.edited==2,"numerical distal residue blocked proximal support");
float residueSpine=0;for(int k=0;k<4;++k)if(residue.meshes[0].vertices[0].bones[k]==0)residueSpine+=residue.meshes[0].vertices[0].weights[k];
check(residueSpine>.1f,"residue candidate omitted native support");
auto realDistal=directBefore;for(auto& v:realDistal.meshes[0].vertices){v.bones={static_cast<unsigned>(twist),elbowIndex,0,0};v.weights={.99f,.01f,0,0};}
const auto protectedDistal=realDistal;
check(proximal_support::applyDirectNative(realDistal,directNative,report,error,false,false,true)&&report.edited==0,"genuine distal blend accepted as numerical residue");
check(realDistal.meshes[0].vertices[0].weights==protectedDistal.meshes[0].vertices[0].weights,"genuine distal blend changed");
check(proximal_support::applyDirectNative(wrongPalette,directNative,report,error)&&report.edited==0,"direct inverse bind mismatch accepted");
auto wrongFrame=directBefore;wrongFrame.meshes[0].modelTransform=translation({1,0,0});
check(!proximal_support::applyDirectNative(wrongFrame,directNative,report,error),"direct mixed frame accepted");
check(length(proximal_support::axialNormal(normal,axis,.2f)-normalize(Vec3{5,2,3}))<1e-6f,"axial inverse transpose");
check(length(proximal_support::axialNormal(normal,axis,1)-normal)<1e-6f,"join normal continuity");
check(length(proximal_support::axialNormal(normal,axis,0)-axis)<1e-6f,"zero derivative axial limit");
check(length(proximal_support::axialNormal({0,1,0},axis,0)-Vec3{0,1,0})<1e-6f,"zero derivative radial limit");
const Vec3 oblique=normalize(Vec3{1,2,1});
const Vec3 tangent=normalize(cross(normal,oblique));
const auto bentTangent=tangent+oblique*(dot(tangent,oblique)*(.13f-1));
check(std::abs(dot(proximal_support::axialNormal(normal,oblique,.13f),bentTangent))<1e-6f,"normal remains perpendicular to transformed tangent");
check(length(proximal_support::axialNormal(normal,axis,-1)-normal)==0,"invalid derivative preserves normal");
std::cout<<"Proximal support candidate tests passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
