#include "scene/ProximalContinuation.h"
#include "scene/WristTransitionDiagnostic.h"
#include "scene/NativeProximalDiagnostic.h"
#include <iostream>
#include <cstring>
#include <limits>
using namespace scene;
Skeleton rig(){Skeleton s;for(auto p:{Vec3{-8,2,0},Vec3{},Vec3{10,0,0}}){Bone b;b.restGlobal=translation(p);b.inverseBind=inverseAffine(b.restGlobal);s.bones.push_back(b);}return s;}
Mesh tube(bool mirror=false){Mesh m;m.skinned=true;m.name="bare_forearm";for(int ring=0;ring<2;++ring)for(int j=0;j<8;++j){const float a=j*2*kPi/8;Vertex v;v.position={float(ring*8),std::cos(a)*(mirror?-1.f:1.f),std::sin(a)};v.normal={0,v.position.y,v.position.z};v.uv={j/8.f,float(ring)};v.bones={1,0,0,0};m.vertices.push_back(v);}for(uint32_t j=0;j<8;++j){const uint32_t n=(j+1)%8;m.indices.insert(m.indices.end(),{j,n,j+8,n,n+8,j+8});}return m;}
int main(){int fails=0;const auto check=[&](bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';++fails;}};const auto s=rig();const imported::ProximalArm a{0,1,2,true};
    for(bool mirror:{false,true}){std::vector<Mesh> m{tube(mirror)};const auto before=m[0];auto off=imported::continueProximalDiagnostic(m,s,a);check(off.modifiedMeshes==0,"default off");
        auto r=imported::continueProximalDiagnostic(m,s,a,true);check(r.modifiedMeshes==1&&r.addedVertices==80&&r.requiresTextureReview,"closed rim extended but texture gate retained");
        check(std::equal(before.indices.begin(),before.indices.end(),m[0].indices.begin()),"original index prefix preserved");check(std::memcmp(before.vertices.data(),m[0].vertices.data(),before.vertices.size()*sizeof(Vertex))==0,"original attributes untouched");
        for(const auto& v:m[0].vertices){float sum=0;for(float w:v.weights)sum+=w;check(std::abs(sum-1)<1e-5f,"normalized weights");check(std::isfinite(v.position.x)&&std::isfinite(v.normal.y),"finite extension");}}
    {std::vector<Mesh> m{tube()};auto ineligible=a;ineligible.donorShoulderless=false;check(!imported::continueProximalDiagnostic(m,s,ineligible,true).modifiedMeshes,"shoulder-bearing donor unchanged");}
    {std::vector<Mesh> m{tube(),tube()};check(!imported::continueProximalDiagnostic(m,s,a,true).modifiedMeshes,"ambiguous loops unchanged");}
    {std::vector<Mesh> m{tube()};for(auto& v:m[0].vertices)v.bones[0]=2;check(!imported::continueProximalDiagnostic(m,s,a,true).modifiedMeshes,"wrist finger holes excluded");}
    {std::vector<Mesh> m{tube()};m[0].indices.insert(m[0].indices.end(),{0,1,8});check(!imported::continueProximalDiagnostic(m,s,a,true).modifiedMeshes,"nonmanifold rejected");}
    {std::vector<Mesh> m{tube()};m[0].vertices[0].position.x=std::numeric_limits<float>::quiet_NaN();check(!imported::continueProximalDiagnostic(m,s,a,true).modifiedMeshes,"nonfinite rejected");}
    for(int bad=0;bad<5;++bad){std::vector<Mesh> m{tube()};auto& v=m[0].vertices[0];if(bad==0)v.normal.x=std::numeric_limits<float>::quiet_NaN();if(bad==1)v.normal={};if(bad==2)v.weights[0]=std::numeric_limits<float>::infinity();if(bad==3)v.weights={};if(bad==4)v.weights[0]=-1;const auto count=m[0].vertices.size();check(!imported::continueProximalDiagnostic(m,s,a,true).modifiedMeshes&&m[0].vertices.size()==count,"invalid normal or weights rejected without geometry mutation");}
    {std::vector<Mesh> m{tube()};auto duplicate=m[0].vertices[0];duplicate.uv.x=1;m[0].vertices.push_back(duplicate);m[0].indices[0]=16;check(imported::continueProximalDiagnostic(m,s,a,true).modifiedMeshes==1,"duplicate UV seam index supported");}
    {std::vector<Mesh> m{tube()};Vertex center;center.bones={1,0,0,0};m[0].vertices.push_back(center);for(uint32_t j=0;j<8;++j)m[0].indices.insert(m[0].indices.end(),{j,(j+1)%8,16});check(!imported::continueProximalDiagnostic(m,s,a,true).modifiedMeshes,"closed proximal end unchanged");}
    for(bool mirrored:{false,true}){std::vector<Mesh> m{tube(mirrored)};const auto before=m[0];const auto r=imported::continueProximalDiagnostic(m,s,a,true,true);
        check(r.modifiedMeshes==1&&r.requiresTextureReview,"surface UV candidate retains visual gate");
        check(std::memcmp(before.vertices.data(),m[0].vertices.data(),before.vertices.size()*sizeof(Vertex))==0,"UV candidate preserves original attributes");
        check(std::equal(before.indices.begin(),before.indices.end(),m[0].indices.begin()),"UV candidate preserves original triangles");
        float maximumV=0;for(size_t i=before.vertices.size();i<m[0].vertices.size();++i){const auto uv=m[0].vertices[i].uv;maximumV=std::max(maximumV,uv.y);check(uv.y>=-1e-5f&&uv.y<=.43751f,"UV stays within validated original forearm strip");}
        check(maximumV>.2f,"UV changes longitudinally instead of frozen rim smear");}
    {std::vector<Mesh> m{tube()};for(auto& v:m[0].vertices)v.uv={};const auto count=m[0].vertices.size();check(!imported::continueProximalDiagnostic(m,s,a,true,true).modifiedMeshes&&m[0].vertices.size()==count,"zero area UV strip rejects without mutation");}
    {std::vector<Mesh> m{tube()};for(size_t i=8;i<16;++i)m[0].vertices[i].bones[0]=0;check(!imported::continueProximalDiagnostic(m,s,a,true,true).modifiedMeshes,"UV path into nonforearm anatomy rejected");}
    {std::vector<Mesh> m{tube()};for(size_t i=8;i<16;++i){m[0].vertices[i].bones={2,0,0,0};}const auto r=imported::continueProximalDiagnostic(m,s,a,true,true);check(r.modifiedMeshes==1,"partially wrist weighted edge reuses only forearm portion");for(size_t i=16;i<m[0].vertices.size();++i)check(m[0].vertices[i].uv.y<=.25001f,"wrist UV reuse clipped at forearm threshold");}
    for(bool mirror:{false,true}){
        auto shoulderRig=s;shoulderRig.bones[0].restGlobal=translation({0,0,0});shoulderRig.bones[1].restGlobal=translation({10,0,0});
        std::vector<Mesh> m{tube(mirror)};for(auto& v:m[0].vertices)v.bones[0]=0;const auto before=m[0];
        const imported::ProximalArm upper{0,1,2,false};
        check(!imported::continueShoulderRimDiagnostic(m,shoulderRig,upper).modifiedMeshes,"shoulder continuation default off");
        const auto r=imported::continueShoulderRimDiagnostic(m,shoulderRig,upper,true,true);
        check(r.modifiedMeshes==1&&r.requiresTextureReview,"shoulder rim did not extend with texture review gate");
        check(std::memcmp(before.vertices.data(),m[0].vertices.data(),before.vertices.size()*sizeof(Vertex))==0,"shoulder extension changed original skin");
        check(std::equal(before.indices.begin(),before.indices.end(),m[0].indices.begin()),"shoulder extension changed original triangles");
        for(size_t i=before.vertices.size();i<m[0].vertices.size();++i){const auto& v=m[0].vertices[i];check(v.bones[0]==0&&std::abs(v.weights[0]-1)<1e-6f,"virtual endpoint leaked into animated skin");check(std::isfinite(v.position.x)&&v.position.x<0,"shoulder extension did not leave proximal rim");}
    }
    {auto sr=s;sr.bones[0].restGlobal=translation({0,0,0});sr.bones[1].restGlobal=translation({10,0,0});
        for(bool nearRim:{false,true}){std::vector<Mesh> m{tube()};for(auto& v:m[0].vertices)v.bones[0]=0;
            if(nearRim)m[0].indices.insert(m[0].indices.end(),{0,1,8});
            else {for(int i=0;i<3;++i){auto v=m[0].vertices[8+i];v.position.x+=10;m[0].vertices.push_back(v);}for(int i=0;i<3;++i)m[0].indices.insert(m[0].indices.end(),{16,17,18});}
            const auto before=m[0];const auto r=imported::continueShoulderRimDiagnostic(m,sr,{0,1,2,false},true,true);
            check(bool(r.modifiedMeshes)!=nearRim,"local topology did not distinguish rim from unrelated geometry");
            check(std::equal(before.indices.begin(),before.indices.end(),m[0].indices.begin()),"local topology deleted original triangles");
        }
    }
    {std::vector<Mesh> m{tube()};for(int i=0;i<8;++i)m[0].vertices[i].position.x=7;
        for(int i=8;i<16;++i)m[0].vertices[i].bones[0]=2;
        auto far=m[0].vertices[0];far.position.x=0;m[0].vertices.push_back(far);const auto before=m[0];
        check(!imported::wristTransitionDiagnostic(m,s,1,2),"wrist candidate default off");
        check(imported::wristTransitionDiagnostic(m,s,1,2,true)==8,"rigid wrist seam candidate count");
        check(m[0].indices==before.indices,"wrist candidate preserves topology");
        for(size_t i=0;i<m[0].vertices.size();++i){check(length(m[0].vertices[i].position-before.vertices[i].position)==0,"wrist bind positions preserved");
            if(i>=8)check(std::memcmp(&m[0].vertices[i],&before.vertices[i],sizeof(Vertex))==0,"palm and proximal anchor preserved");}
        check(!imported::wristTransitionDiagnostic(m,s,1,2,true),"wrist candidate is idempotent");
    }
    {std::vector<Mesh> m{tube()};auto& mesh=m[0];
        for(int i=0;i<8;++i)mesh.vertices[i].position.x=5;
        for(int i=8;i<16;++i)mesh.vertices[i].position.x=7;
        for(int i=0;i<8;++i){auto v=mesh.vertices[8+i];v.position.x=9;v.bones[0]=2;mesh.vertices.push_back(v);}
        for(uint32_t j=0;j<8;++j){auto n=(j+1)%8;mesh.indices.insert(mesh.indices.end(),{8+j,8+n,16+j,8+n,16+n,16+j});}
        const auto before=mesh;check(imported::wristTransitionDiagnostic(m,s,1,2,true,true)>8,"refined wrist band sampled");
        check(mesh.vertices.size()>before.vertices.size(),"refinement adds samples");
        for(size_t i=0;i<before.vertices.size();++i)check(length(mesh.vertices[i].position-before.vertices[i].position)==0,"refinement preserves original positions");
        for(size_t i=16;i<24;++i)check(std::memcmp(&mesh.vertices[i],&before.vertices[i],sizeof(Vertex))==0,"refinement preserves palm");
        for(size_t i=0;i+2<mesh.indices.size();i+=3){auto a=mesh.vertices[mesh.indices[i]].position,b=mesh.vertices[mesh.indices[i+1]].position,c=mesh.vertices[mesh.indices[i+2]].position;check(length(cross(b-a,c-a))>1e-7f,"refinement no degenerate triangles");}
    }
    {
        CastScene native;native.skeleton=s;Mesh mesh;mesh.skinned=true;
        for(int i=0;i<3;++i){Vertex v;v.position={-9.f,2.f+i*.1f,0};v.bones={0,0,0,0};v.weights={1,0,0,0};mesh.vertices.push_back(v);}
        native.meshes.push_back(mesh);
        const auto extent=imported::nativeProximalExtent(native,0,1);
        check(extent&&std::abs(*extent-8.4f/68.f)<1e-5f,"native proximal extent measured");
        native.meshes[0].viewmodelWeapon=true;check(!imported::nativeProximalExtent(native,0,1),"weapon excluded from proximal extent");
        native.meshes[0].viewmodelWeapon=false;native.meshes[0].vertices.resize(1);check(!imported::nativeProximalExtent(native,0,1),"isolated native point rejected");
        native.meshes[0]=mesh;for(auto& v:native.meshes[0].vertices)v.position.x=-100;check(!imported::nativeProximalExtent(native,0,1),"unbounded torso support rejected");
        auto sr=s;sr.bones[0].restGlobal=translation({0,0,0});sr.bones[1].restGlobal=translation({10,0,0});
        std::vector<Mesh> fitted{tube()};for(auto& v:fitted[0].vertices)v.bones[0]=0;
        const auto before=fitted;const auto r=imported::continueShoulderRimDiagnostic(fitted,sr,{0,1,2,false},true,true,.125f);
        check(r.modifiedMeshes==1,"bounded native extent continuation");
        for(size_t i=16;i<fitted[0].vertices.size();++i)check(fitted[0].vertices[i].position.x>=-1.2501f,"continuation exceeds measured extent");
        for(float invalid:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN()}){auto unchanged=before;check(!imported::continueShoulderRimDiagnostic(unchanged,sr,{0,1,2,false},true,true,invalid).modifiedMeshes,"invalid extent accepted");}
    }
    return fails?1:0;
}
