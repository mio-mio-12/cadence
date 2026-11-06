#include "scene/ImportedViewRetarget.h"
#include "scene/ImportedNative.h"
#include "scene/NativeProximalDiagnostic.h"
#include "app/DualWield.h"
#include <iostream>
scene::CastScene makeHands(bool halo){
    scene::CastScene s;const auto add=[&](std::string name,int parent,scene::Vec3 p){scene::Bone b;b.name=name;b.parent=parent;b.restLocal.position=p;b.restGlobal=parent<0?scene::translation(p):s.skeleton.bones[parent].restGlobal*scene::translation(p);b.inverseBind=scene::inverseAffine(b.restGlobal);const auto i=s.skeleton.bones.size();s.skeleton.boneByName[name]=i;s.skeleton.bones.push_back(b);return static_cast<int>(i);};
    for(int side=0;side<2;++side){const std::string c=side?"ri":"le",h=side?"r":"l";
        const auto shoulder=add(halo?h+"_upperarm":"j_shoulder_"+c,-1,{0,float(side?8:-8),0});const auto elbow=add(halo?h+"_forearm":"j_elbow_"+c,shoulder,{10,0,0});const auto wrist=add(halo?h+"_hand":"j_wrist_"+c,elbow,{10,0,0});
        int f=0;for(const std::string finger:{"thumb","index","middle","ring","pinky"}){int parent=wrist;for(int j=0;j<3;++j){const auto name=halo?h+"_"+finger+"_"+(j==0?"low":j==1?"mid":"tip"):"j_"+(finger=="middle"?"mid":finger)+"_"+c+"_"+std::to_string(j);parent=add(name,parent,j?scene::Vec3{2,0,0}:scene::Vec3{4,float(f-2),float(f==0?2:0)});}++f;}
        scene::Mesh mesh;for(int j=0;j<3;++j){scene::Vertex v;v.position={float(20+j),float(side?8:-8),0};v.bones[0]=wrist;v.uv={.2f*j,.75f};mesh.vertices.push_back(v);}mesh.indices={0,1,2};s.meshes.push_back(mesh);
    }return s;
}
int main(){int failures{};const auto check=[&](bool ok,const char* msg){if(!ok){++failures;std::cerr<<msg<<'\n';}};
    {
        auto skin=makeHands(false),driver=skin;
        skin.skeleton.boneByName.clear();
        const auto mirror=scene::translation({1.5f,-4.f,-8.5f})*scene::scale({1,-1,1});
        const auto nameBone=[&](int index,const std::string& shared,const std::string& native){
            auto& source=skin.skeleton.bones[index];
            source.name=shared;source.parent=-1;
            skin.skeleton.boneByName[shared]=index;
            auto& target=driver.skeleton.bones[index];
            target.name="cs_weapon_test_v_weapon."+native;
            target.restGlobal=scene::translation(scene::transformPoint(mirror*source.restGlobal,{}));
        };
        const std::array<const char*,5> names{"Thumb","Index","Middle","Ring","Pinky"};
        for(int side=0;side<2;++side){
            const int base=side*18;
            const std::string shared=side?"cs_hand_R.":"cs_hand_L.",native=side?"Right_":"Left_";
            nameBone(base+1,shared+"forearm",native+"Arm");
            nameBone(base+2,shared+"palm",native+"Hand");
            for(int digit=0;digit<5;++digit)for(int joint=0;joint<3;++joint)
                nameBone(base+3+digit*3+joint,shared+"finger"+std::to_string(digit)+"."+std::to_string(joint),
                    native+names[digit]+"0"+std::to_string(joint+1));
        }
        auto mismatch=driver;
        mismatch.skeleton.bones[2].restGlobal.v[13]+=.5f;
        check(!scene::imported::bindMirroredSourceViewSkin(mismatch,skin),"mirrored binding accepted mismatched hand anatomy");
        check(scene::imported::bindMirroredSourceViewSkin(driver,skin),"mirrored Source bind was not recognized");
        check(driver.meshes[0].indices==std::vector<std::uint32_t>{0,2,1},"mirrored hand winding was not corrected");
        check(scene::length(driver.meshes[0].vertices[0].position-scene::transformPoint(mirror,skin.meshes[0].vertices[0].position))<1e-5f,
            "mirrored hand did not preserve its translated bind");
        check(driver.meshes[0].vertices[0].bones[0]==2,"mirrored hand lost its authored wrist driver");
    }
    for(const std::string game:{"cs1.6","cz","css","csnz","blacksquad","pointblank","cs2"}){
        auto driver=makeHands(false),donor=makeHands(true);driver.importedViewGame=game;
        driver.importedTranslationScale=2.54f;donor.importedViewGame="bo2";
        for(auto& bone:donor.skeleton.bones){bone.restLocal.position=bone.restLocal.position*1.5f;for(int k:{12,13,14})bone.restGlobal.v[k]*=1.5f;bone.inverseBind=scene::inverseAffine(bone.restGlobal);}
        for(auto& mesh:donor.meshes)for(auto& v:mesh.vertices)v.position=v.position*1.5f;
        std::string error;check(scene::imported::fitPoseOnlyViewSkin(driver,donor,error),error.c_str());
        const bool legacy=game=="cs1.6"||game=="cz"||game=="css"||game=="csnz";
        check(driver.codmRigAdapter&&std::abs(driver.codmRigAdapter->weaponFitScale-(legacy?1.5f:1.f))<1e-5f,"grip fit policy affected wrong library or missed legacy CS");
        for(size_t m=0;m<donor.meshes.size();++m)for(size_t v=0;v<donor.meshes[m].vertices.size();++v)
            check(scene::length(driver.meshes[m].vertices[v].position-donor.meshes[m].vertices[v].position)<1e-5f,"grip fit altered donor mesh anatomy");
    }
    {scene::CastScene rig;rig.skeleton.bones.resize(3);
     rig.skeleton.bones[0].parent=-1;rig.skeleton.bones[0].restGlobal=scene::translation({0,0,10});
     rig.skeleton.bones[1].parent=0;rig.skeleton.bones[1].restGlobal=scene::Mat4::identity();
     rig.skeleton.bones[2].parent=1;rig.skeleton.bones[2].restGlobal=scene::translation({10,0,0});
     scene::Mesh mesh;mesh.skinned=true;scene::Vertex v;v.position={0,4,0};v.bones={0,1,0,0};v.weights={.92f,.08f,0,0};mesh.vertices.push_back(v);
     v.position={0,2,9};v.weights={1,0,0,0};mesh.vertices.push_back(v);rig.meshes.push_back(mesh);
     const auto support=scene::prepareForearmClearance(rig,0,0,1,2);
     check(std::abs(support.radius-4)<1e-5f,"blended upper sleeve missing or bind bend counted as thickness");
     check(support.movedBones==std::vector<std::size_t>{0,1},"support moved palm or omitted proximal chain");
     check(rig.meshes[0].vertices[0].position.y==4&&rig.meshes[0].vertices[0].weights[0]==.92f,"support preparation changed donor geometry/weights");
    }
    {scene::CastScene rig;rig.skeleton.bones.resize(4);
     auto adapter=std::make_shared<scene::CodmRigAdapter>();
     scene::ForearmClearanceSupport support;support.shoulder=0;support.elbow=1;support.wrist=2;support.radius=4.75f;support.movedBones={0,1};adapter->forearmClearance={support};rig.codmRigAdapter=adapter;
     std::vector<scene::Mat4> pose={scene::translation({-12,-3,-.5f}),scene::translation({-1,.5f,-3}),scene::translation({10,3.5f,-5}),scene::translation({12,3.5f,-5})};
     const auto original=pose;auto skin=pose;
     const auto vp=scene::perspective(75*scene::kPi/180,16.f/9,.02f,10000)*scene::lookAtDirection({0,0,0},{1,0,0},{0,0,1});
     scene::applyForearmClearance(rig,pose,vp,skin);
     check(skin[1].v!=original[1].v,"camera-intersecting forearm did not move");
     check(skin[2].v==original[2].v&&skin[3].v==original[3].v,"clearance changed grip transforms");
     check(pose[0].v==original[0].v,"clearance changed recorded pose");
     const auto position=[](const scene::Mat4& m){return scene::transformPoint(m,{});};
     check(std::abs(scene::length(position(skin[1])-position(skin[2]))-scene::length(position(pose[1])-position(pose[2])))<1e-4f,"clearance resized forearm");
     check(std::abs(scene::length(position(skin[0])-position(skin[1]))-scene::length(position(pose[0])-position(pose[1])))<1e-4f,"clearance resized upper arm");
     check(std::abs(scene::length(scene::transformPoint(skin[1],{0,2,0})-position(skin[1]))-2)<1e-4f,"clearance resized arm width");
     auto flipped=vp;for(int k:{0,4,8,12})flipped.v[k]*=-1;auto mirror=pose;scene::applyForearmClearance(rig,pose,flipped,mirror);
     for(int k=0;k<16;++k)check(std::abs(mirror[1].v[k]-skin[1].v[k])<1e-5f,"mirroring changed clearance pose");
     auto untouched=pose;scene::applyForearmClearance(rig,pose,scene::Mat4{},untouched);check(untouched[1].v==pose[1].v,"invalid camera modified forearm");
     auto distant=pose;for(auto& p:distant)p=scene::translation({0,0,-100})*p;auto safe=distant;scene::applyForearmClearance(rig,distant,vp,safe);check(safe[1].v==distant[1].v,"clear arm modified");
     rig.codmRigAdapter.reset();auto native=pose;scene::applyForearmClearance(rig,pose,vp,native);check(native[1].v==pose[1].v,"native rig modified");
    }
    {scene::CastScene rig;rig.skeleton.bones.resize(2);
     auto adapter=std::make_shared<scene::CodmRigAdapter>();
     scene::SleeveClearanceSupport support;support.shoulder=0;support.elbow=1;
     support.bindPoints={{0,0,0},{0,.1f,0},{0,0,.1f}};support.movedBones={0};adapter->sleeves={support};rig.codmRigAdapter=adapter;
     std::vector<scene::Mat4> pose={scene::translation({0,1,0}),scene::Mat4::identity()},skin=pose;
     const auto original=pose;auto mirrored=skin;
     scene::applySleeveClearance(rig,pose,scene::Mat4::identity(),false,skin);
     auto flip=scene::Mat4::identity();flip.v[0]=-1;
     scene::applySleeveClearance(rig,pose,flip,true,mirrored);
     check(skin[0].v!=original[0].v,"sleeve clearance did not rotate violating support");
     check(skin[1].v==original[1].v&&pose[0].v==original[0].v,"sleeve clearance mutated elbow or recorded pose");
     for(int k=0;k<16;++k)check(std::abs(skin[0].v[k]-mirrored[0].v[k])<1e-5f,"flipped projection changed anatomical clearance");
     check(std::abs(scene::length(scene::transformPoint(skin[0],{0,0,.1f})-scene::transformPoint(skin[0],{}))-.1f)<1e-5f,"clearance changed donor width");
     const auto safe=skin;auto invalid=scene::Mat4{};scene::applySleeveClearance(rig,pose,invalid,false,skin);
     check(skin[0].v==safe[0].v,"invalid projection changed skin");
    }
    for(bool alternateNames:{false,true}){auto driver=makeHands(false),donor=makeHands(alternateNames),baseline=driver;
     driver.importedViewGame="iw_sp";baseline.importedViewGame="mw";donor.importedViewGame="aw";
     const auto native=scene::imported::viewLayout(driver),foreign=scene::imported::viewLayout(donor);
     std::string error;check(scene::imported::fitPoseOnlyViewSkin(driver,donor,error),error.c_str());
     check(scene::imported::fitPoseOnlyViewSkin(baseline,donor,error),error.c_str());
     if(driver.codmRigAdapter&&baseline.codmRigAdapter){
       for(int side=0;side<2;++side){const auto& a=foreign.arms[side];const auto& b=native.arms[side];
         check(driver.codmRigAdapter->bindings[a.shoulder].source==size_t(b.elbow),"IW donor sleeve did not follow forearm");
         check(baseline.codmRigAdapter->bindings[a.shoulder].source==size_t(b.shoulder),"non-IW donor sleeve route changed");
         for(int bone:{a.elbow,a.wrist,a.fingers[1][0],a.fingers[4][2]}){
           const auto& x=driver.codmRigAdapter->bindings[bone];const auto& y=baseline.codmRigAdapter->bindings[bone];
           check(x.source==y.source,"sleeve correction changed distal driver");
           for(int k=0;k<16;++k)check(std::abs(x.offset.v[k]-y.offset.v[k])<1e-5f,"sleeve correction changed distal pose");
         }
       }
       check(driver.meshes.size()==baseline.meshes.size(),"sleeve correction changed mesh count");
       for(size_t m=0;m<driver.meshes.size();++m){check(driver.meshes[m].indices==baseline.meshes[m].indices,"sleeve correction changed triangles");
         for(size_t v=0;v<driver.meshes[m].vertices.size();++v)check(scene::length(driver.meshes[m].vertices[v].position-baseline.meshes[m].vertices[v].position)<1e-6f,"sleeve correction resized donor");}
     }
    }
    {auto driver=makeHands(false),donor=makeHands(true);
     for(auto& bone:driver.skeleton.bones)if(bone.name.starts_with("j_ring_")){driver.skeleton.boneByName.erase(bone.name);bone.name="unused_"+bone.name;}
     const auto native=scene::imported::viewLayout(driver),foreign=scene::imported::viewLayout(donor);
     std::string error;check(scene::imported::fitPoseOnlyViewSkin(driver,donor,error),error.c_str());
     if(driver.codmRigAdapter)for(int side=0;side<2;++side)for(int j=0;j<3;++j){const auto& binding=driver.codmRigAdapter->bindings[foreign.arms[side].fingers[3][j]];
       check(binding.source==size_t(native.arms[side].fingers[2][j])&&binding.rollSource==size_t(native.arms[side].fingers[4][j])&&std::abs(binding.rollWeight-.5f)<1e-6f,"four-digit pose-only interpolation dropped second neighbour");}
    }
    {auto driver=makeHands(false),donor=makeHands(true),gun=driver;driver.importedViewGame="cs1.6";
     std::string error;check(scene::imported::fitPoseOnlyViewSkin(driver,donor,error),error.c_str());
     const auto adapter=driver.codmRigAdapter;const auto count=driver.skeleton.bones.size();const auto original=driver.meshes;
     scene::CastScene mounted;check(scene::imported::assemblePrepared(gun,driver,mounted,error),error.c_str());
     check(mounted.codmRigAdapter==adapter&&mounted.skeleton.bones.size()==count,"weapon remount discarded donor palette");
     check(mounted.meshes.size()==gun.meshes.size()+original.size(),"weapon remount missing geometry");
     if(mounted.meshes.size()==gun.meshes.size()+original.size())for(size_t i=0;i<original.size();++i)for(size_t v=0;v<original[i].vertices.size();++v)
       check(scene::length(mounted.meshes[gun.meshes.size()+i].vertices[v].position-original[i].vertices[v].position)<1e-6f,"weapon remount reshaped donor");
    }
    {auto driver=makeHands(false),donor=makeHands(true);const auto anatomy=scene::imported::viewLayout(donor);const auto& arm=anatomy.arms[0];
     scene::Bone twist;twist.name="L Twist2";twist.parent=arm.shoulder;twist.restGlobal=scene::translation({15,-8,0});twist.inverseBind=scene::inverseAffine(twist.restGlobal);twist.restLocal.position={15,0,0};const auto id=donor.skeleton.bones.size();donor.skeleton.boneByName[twist.name]=id;donor.skeleton.bones.push_back(twist);
     std::string error;check(scene::imported::fitPoseOnlyViewSkin(driver,donor,error),error.c_str());
     std::vector<scene::Transform> local;for(const auto& b:driver.skeleton.bones)local.push_back(b.restLocal);
     local[driver.skeleton.boneByName.at("j_elbow_le")].rotation=scene::fromAxisAngle({0,0,1},1.2f);
     const auto pose=driver.globalPose(local);const auto base=driver.codmRigAdapter->firstBone;
     const auto e=scene::transformPoint(pose[base+arm.elbow],{}),w=scene::transformPoint(pose[base+arm.wrist],{}),t=scene::transformPoint(pose[base+id],{});
     check(scene::length(t-(e+w)*.5f)<1e-3f,"pose-only forearm helper followed nominal upper-arm parent");
    }
    {auto driver=makeHands(false),donor=makeHands(true);donor.importedViewGame="cs1.6";driver.importedViewGame="pointblank";
     const auto original=donor;std::string error;check(scene::imported::fitPoseOnlyViewSkin(driver,donor,error),error.c_str());
     check(driver.codmRigAdapter&&!driver.codmRigAdapter->positionLinks.empty(),"pose-only shared adapter missing");
     check(driver.meshes.size()==donor.meshes.size(),"pose-only lost donor mesh");
     for(size_t m=0;m<donor.meshes.size()&&m<driver.meshes.size();++m){check(driver.meshes[m].indices==donor.meshes[m].indices,"pose-only lost triangles");
      for(size_t v=0;v<donor.meshes[m].vertices.size();++v)check(scene::length(driver.meshes[m].vertices[v].position-donor.meshes[m].vertices[v].position*2.54f)<1e-4f,"pose-only changed proportions instead of units");}
     check(donor.skeleton.bones.size()==original.skeleton.bones.size()&&donor.meshes[0].vertices[0].position.x==original.meshes[0].vertices[0].position.x,"pose-only modified source");
     const auto pose=driver.samplePose(0,0);const auto base=driver.codmRigAdapter->firstBone;const auto layout=scene::imported::viewLayout(donor);
     for(const auto& arm:layout.arms){const auto actual=scene::length(scene::transformPoint(pose[base+arm.wrist],{})-scene::transformPoint(pose[base+arm.elbow],{}));check(std::abs(actual-25.4f)<1e-3f,"pose-only runtime used native arm length");}
     const auto nativeLayout=scene::imported::viewLayout(original.importedViewGame.empty()?original:makeHands(false));
     for(int side=0;side<2;++side)check(scene::length(scene::transformPoint(pose[base+layout.arms[side].fingers[2][0]],{})-scene::transformPoint(pose[nativeLayout.arms[side].fingers[2][0]],{}))<1e-3f,"pose-only palm contact drifts with different donor size");
    }
    {auto skin=makeHands(false),assembled=skin;
     scene::Bone helper=skin.skeleton.bones[0];helper.name="authored_helper";helper.parent=0;skin.skeleton.bones.push_back(helper);
     skin.meshes[0].vertices[0].bones={2,static_cast<uint32_t>(skin.skeleton.bones.size()-1),0,0};skin.meshes[0].vertices[0].weights={.8f,.2f,0,0};
     assembled=skin;scene::Mesh gun;gun.viewmodelWeapon=true;assembled.meshes.insert(assembled.meshes.begin(),gun);assembled.skeleton.bones.push_back(helper);
     check(scene::imported::nativeViewSkinUnchanged(assembled,skin),"identity rejected retained native helper skin with appended weapon");
     auto different=assembled;different.meshes[1].vertices[0].weights={1,0,0,0};check(!scene::imported::nativeViewSkinUnchanged(different,skin),"identity accepted changed helper weights");
     different=assembled;different.skeleton.bones[0].restGlobal.v[12]+=1;check(!scene::imported::nativeViewSkinUnchanged(different,skin),"identity accepted changed native bind");
     different=assembled;different.meshes[1].indices.clear();check(!scene::imported::nativeViewSkinUnchanged(different,skin),"identity accepted missing native triangles");
     different=assembled;different.meshes[1].vertices[0].position.x+=1;check(!scene::imported::nativeViewSkinUnchanged(different,skin),"identity accepted moved skin");
     check(!scene::imported::nativeViewSkinUnchanged({},{}),"empty scene accepted as native skin");
    }
    {scene::Skeleton rig;for(int i=0;i<7;++i){scene::Bone b;b.name="bone";b.parent=-1;rig.bones.push_back(b);}
     rig.bones[1].parent=0;rig.bones[2].parent=1;
     rig.bones[3].name="upper_twist";rig.bones[3].parent=0;
     rig.bones[4].name="fore_roll";rig.bones[4].parent=1;
     rig.bones[5].name="wrist_twist";rig.bones[5].parent=2;
     rig.bones[6].name="cycle_twist";rig.bones[6].parent=6;
     const auto upper=scene::imported::diagnosticArmEnvelopeOwners(rig,0,1),fore=scene::imported::diagnosticArmEnvelopeOwners(rig,1,2);
     check(upper[0]&&upper[3]&&!upper[1]&&!upper[4]&&!upper[5]&&!upper[6],"upper envelope crossed anatomical boundary");
     check(fore[1]&&fore[4]&&!fore[0]&&!fore[3]&&!fore[5]&&!fore[6],"forearm envelope crossed wrist or cycle");
    }
    {auto skin=makeHands(false),driver=makeHands(true);std::string error;
     skin.importedViewGame=driver.importedViewGame="native_test";
     check(scene::imported::fitForeignViewSkin(driver,skin,error),"same-native fit failed");
     for(const auto& m:driver.meshes)for(const auto& v:m.vertices)check(v.armSkinning==0,"same-native skin changed to DQ");
     driver=makeHands(true);driver.importedViewGame="other_test";
     check(scene::imported::fitForeignViewSkin(driver,skin,error),"foreign fit failed");
     bool marked=false;for(const auto& m:driver.meshes)for(const auto& v:m.vertices)marked|=v.armSkinning>0;
     check(marked,"foreign arm skin not marked");
    }
    {auto source=makeHands(false),native=makeHands(true);
        for(int side=0;side<2;++side){const int base=side*18;for(int b=0;b<3;++b)source.skeleton.bones[base+b].parent=-1;
            scene::Mesh tube;tube.name="continuity_"+std::to_string(side);tube.skinned=true;
            for(int ring=0;ring<4;++ring)for(int j=0;j<8;++j){const float angle=j*2*scene::kPi/8;scene::Vertex v;
                const float x=ring==0?0.f:ring==1?4.5f:ring==2?9.f:11.f;v.position={x,float(side?8:-8)+std::cos(angle),std::sin(angle)};
                v.normal={0,std::cos(angle),std::sin(angle)};v.uv={j/8.f,ring/3.f};v.bones={static_cast<uint32_t>(base+(ring==3?1:0)),0,0,0};tube.vertices.push_back(v);}
            for(uint32_t ring=0;ring<3;++ring)for(uint32_t j=0;j<8;++j){const auto a=ring*8+j,b=ring*8+(j+1)%8;tube.indices.insert(tube.indices.end(),{a,b,a+8,b,b+8,a+8});}
            source.meshes.push_back(tube);native.skeleton.bones[base].restGlobal=scene::translation({0,float(side?8:-8),float(side?-10:10)});native.skeleton.bones[base].inverseBind=scene::inverseAffine(native.skeleton.bones[base].restGlobal);
        }
        const auto fit=[&](scene::CastScene& target,const scene::CastScene& skin,bool enabled,bool automatic=false){std::string error;return scene::imported::fitForeignViewSkin(target,skin,error,true,false,false,false,false,false,false,false,false,enabled,automatic);};
        auto baseline=native,candidate=native;check(fit(baseline,source,false)&&fit(candidate,source,true),"proximal continuity fit failed");
        const auto layout=scene::imported::viewLayout(source);bool changed=false;
        for(int side=0;side<2;++side){const auto field=scene::imported::proximalContinuityField(source,source.meshes[2+side],layout.arms[side]);
            check(field.affected.size()==32,"continuity connected seam not identified");if(field.affected.size()!=32)continue;
            check(field.weight(field.origin)==0&&field.weight(field.origin+field.axis*field.end)==1,"continuity endpoint weights changed");
            const float h=.001f;check(field.weight(field.origin+field.axis*h)/h<.001f&&(1-field.weight(field.origin+field.axis*(field.end-h)))/h<.001f,"continuity endpoint slope is nonzero");
            const auto& b=baseline.meshes[2+side];const auto& c=candidate.meshes[2+side];check(b.indices==c.indices&&b.vertices.size()==c.vertices.size(),"continuity changed topology");
            for(size_t i=0;i<c.vertices.size();++i){const auto& v=c.vertices[i];const auto& old=b.vertices[i];float sum=0;for(float w:v.weights)sum+=w;
                check(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z)&&std::isfinite(v.normal.x)&&std::isfinite(v.normal.y)&&std::isfinite(v.normal.z)&&std::abs(sum-1)<1e-6f,"continuity invalid geometry or weights");
                check(v.uv.x==old.uv.x&&v.uv.y==old.uv.y,"continuity changed UV");
                if(i<8||i>=24)check(scene::length(v.position-old.position)<1e-6f&&v.bones==old.bones&&v.weights==old.weights,"continuity altered shoulder endpoint or forearm");
                if(i>=16&&i<24)check(v.bones[0]==static_cast<uint32_t>(side*18+1)&&v.weights[0]==1,"continuity seam is not forearm rigid");
                changed|=scene::length(v.position-old.position)>.001f;
            }
        }
        check(changed,"continuity fixture did not exercise field");
        for(size_t m=0;m<2;++m)for(size_t i=0;i<baseline.meshes[m].vertices.size();++i){const auto& b=baseline.meshes[m].vertices[i];const auto& c=candidate.meshes[m].vertices[i];check(scene::length(b.position-c.position)<1e-6f&&b.bones==c.bones&&b.weights==c.weights,"continuity altered palm");}
        auto hierarchical=source;hierarchical.skeleton.bones[1].parent=0;check(scene::imported::proximalContinuityField(hierarchical,hierarchical.meshes[2],scene::imported::viewLayout(hierarchical).arms[0]).affected.empty(),"continuity accepted hierarchical source");
        auto bent=source;bent.skeleton.bones[0].restGlobal=scene::translation({0,-8,10});check(scene::imported::proximalContinuityField(bent,bent.meshes[2],scene::imported::viewLayout(bent).arms[0]).affected.empty(),"continuity accepted bent source");
        auto ambiguous=source.meshes[2];const auto n=static_cast<uint32_t>(ambiguous.vertices.size());for(auto v:source.meshes[2].vertices){v.position.y+=3;ambiguous.vertices.push_back(v);}for(auto index:source.meshes[2].indices)ambiguous.indices.push_back(index+n);
        check(scene::imported::proximalContinuityField(source,ambiguous,layout.arms[0]).affected.empty(),"continuity accepted ambiguous disconnected seams");
        auto missing=source.meshes[2];missing.indices.clear();check(scene::imported::proximalContinuityField(source,missing,layout.arms[0]).affected.empty(),"continuity accepted missing boundary");
        auto split=source.meshes[2];split.vertices.clear();split.indices.clear();for(auto index:source.meshes[2].indices){split.indices.push_back(static_cast<uint32_t>(split.vertices.size()));split.vertices.push_back(source.meshes[2].vertices[index]);}
        check(!scene::imported::proximalContinuityField(source,split,layout.arms[0]).affected.empty(),"continuity rejected UV-split connected surface");
        const auto originalField=scene::imported::proximalContinuityField(source,source.meshes[2],layout.arms[0]);
        const auto splitField=scene::imported::proximalContinuityField(source,split,layout.arms[0]);
        check(splitField.start==originalField.start&&splitField.end==originalField.end,"spatial weld changed valid field endpoints");
        if(splitField.affected.size()==split.vertices.size())for(size_t i=0;i<split.vertices.size();++i){const auto originalIndex=source.meshes[2].indices[i];
            check(splitField.affected[i]==originalField.affected[originalIndex]&&splitField.weight(split.vertices[i].position)==originalField.weight(source.meshes[2].vertices[originalIndex].position),"spatial weld changed valid UV-split ownership or weights");}
        for(int i=0;i<=1000;++i){const float t=i/1000.f,w=originalField.weight(originalField.origin+originalField.axis*(originalField.start+(originalField.end-originalField.start)*t));
            const float expected=std::clamp(t*t*t*(t*(t*6-15)+10),0.f,1.f);
            check(std::isfinite(w)&&w>=0&&w<=1&&std::abs(w-expected)<2e-6f,"continuity weight not finite/clamped or changed polynomial");}
        const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
        for(int kind=0;kind<5;++kind){auto invalid=source.meshes[2];
            if(kind==0)invalid.vertices[0].position.x=nan;if(kind==1)invalid.vertices[0].normal.z=inf;
            if(kind==2)invalid.vertices[0].weights[0]=nan;if(kind==3)invalid.modelTransform.v[0]=inf;
            if(kind==4)invalid.vertices[0].weights[1]=-.1f;
            check(scene::imported::proximalContinuityField(source,invalid,layout.arms[0]).affected.empty(),"continuity accepted invalid mesh input");}
        auto invalidBones=source;invalidBones.skeleton.bones[0].restGlobal.v[12]=nan;
        check(scene::imported::proximalContinuityField(invalidBones,invalidBones.meshes[2],layout.arms[0]).affected.empty(),"continuity accepted invalid bone input");
        auto invalidArm=layout.arms[0];invalidArm.shoulder=100000;
        check(scene::imported::proximalContinuityField(source,source.meshes[2],invalidArm).affected.empty(),"continuity accepted invalid bone index");
        auto invalidField=originalField;invalidField.end=invalidField.start;
        check(invalidField.weight({})==0&&originalField.weight({nan,0,0})==0,"continuity invalid evaluation not safely rejected");
        auto sameX=source.meshes[2];for(int i=0;i<4096;++i){auto v=sameX.vertices[0];v.position={0,float(100+i),0};sameX.vertices.push_back(v);}
        const auto sameXField=scene::imported::proximalContinuityField(source,sameX,layout.arms[0]);
        check(!sameXField.affected.empty()&&sameXField.start==originalField.start&&sameXField.end==originalField.end,"same-X spatial population changed valid component");
        auto crowded=source.meshes[2];for(int i=0;i<1600;++i){auto v=crowded.vertices[0];v.position={float(i%20)*1e-6f,float((i/20)%20)*1e-6f,float(i/400)*1e-6f};crowded.vertices.push_back(v);}
        check(scene::imported::proximalContinuityField(source,crowded,layout.arms[0]).affected.empty(),"continuity failed to bound crowded-cell comparisons");
        const auto sameMeshes=[](const scene::CastScene& a,const scene::CastScene& b){if(a.meshes.size()!=b.meshes.size())return false;
            for(size_t m=0;m<a.meshes.size();++m){const auto& x=a.meshes[m];const auto& y=b.meshes[m];if(x.indices!=y.indices||x.vertices.size()!=y.vertices.size()||x.modelTransform.v!=y.modelTransform.v)return false;
                for(size_t i=0;i<x.vertices.size();++i){const auto& u=x.vertices[i];const auto& v=y.vertices[i];if(scene::length(u.position-v.position)!=0||scene::length(u.normal-v.normal)!=0||u.bones!=v.bones||u.weights!=v.weights||u.uv.x!=v.uv.x||u.uv.y!=v.uv.y)return false;}}
            return true;};
        auto automatic=native;check(fit(automatic,source,false,true)&&sameMeshes(automatic,candidate),"automatic straight-to-bent differs from reviewed diagnostic");
        auto defaultAutomatic=native;std::string autoError;
        check(scene::imported::fitForeignViewSkin(defaultAutomatic,source,autoError)&&sameMeshes(defaultAutomatic,candidate),"default production continuity differs from diagnostic");
        auto flatOff=makeHands(true),flatOn=flatOff;check(fit(flatOff,source,false)&&fit(flatOn,source,false,true)&&sameMeshes(flatOff,flatOn),"automatic altered flat-to-flat fitting");
        auto sameOff=source,sameOn=source;check(fit(sameOff,source,false)&&fit(sameOn,source,false,true)&&sameMeshes(sameOff,sameOn),"automatic altered same-native flat fitting");
        auto nativeOff=native,nativeOn=native;check(fit(nativeOff,native,false)&&fit(nativeOn,native,false,true)&&sameMeshes(nativeOff,nativeOn),"automatic altered same-native hierarchical fitting");
        auto ambiguousSource=source;ambiguousSource.meshes[2]=ambiguous;auto rejectedOff=native,rejectedOn=native;
        check(fit(rejectedOff,ambiguousSource,false)&&fit(rejectedOn,ambiguousSource,false,true),"ambiguous production fallback failed to load");
        // The other mirrored component remains eligible; compare only rejected mesh.
        rejectedOff.meshes.erase(rejectedOff.meshes.begin()+3);rejectedOn.meshes.erase(rejectedOn.meshes.begin()+3);
        check(sameMeshes(rejectedOff,rejectedOn),"ambiguous component did not retain prior production fit");
        std::vector<scene::volume_fit::Bone> sampleFits(source.skeleton.bones.size(),scene::volume_fit::prepare(scene::Mat4::identity(),{}));
        check(scene::imported::proximalContinuitySafe(originalField,source.meshes[2],sampleFits,layout.arms[0]),"valid identity field failed preflight");
        for(auto& bone:sampleFits)bone.stretch=scene::scale({0,0,0});
        check(!scene::imported::proximalContinuitySafe(originalField,source.meshes[2],sampleFits,layout.arms[0]),"collapsed finite field passed preflight");
        sampleFits[0].stretch.v[0]=nan;
        check(!scene::imported::proximalContinuitySafe(originalField,source.meshes[2],sampleFits,layout.arms[0]),"nonfinite field passed preflight");
        sampleFits.assign(source.skeleton.bones.size(),scene::volume_fit::prepare(scene::Mat4::identity(),{}));
        for(auto& bone:sampleFits)bone.real={0,0,0,0};
        bool singularDeclined=false;try{singularDeclined=!scene::imported::proximalContinuitySafe(originalField,source.meshes[2],sampleFits,layout.arms[0]);}catch(...){ }
        check(singularDeclined,"degenerate rotation escaped component fallback");
    }
    {auto source=makeHands(false),driver=makeHands(true);source.skeleton.boneByName.clear();
        for(int side=0;side<2;++side){const int base=side*18;const std::string cap=side?"R":"L",prefix="cs_hand_"+cap+".";
            source.skeleton.bones[base].name="cs_hand_Bone_"+cap+"_Upper";source.skeleton.bones[base+1].name=prefix+"forearm";source.skeleton.bones[base+2].name=prefix+"palm";
            for(int j=0;j<3;++j)source.skeleton.bones[base+j].parent=-1;
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)source.skeleton.bones[base+3+f*3+j].name=prefix+"finger"+std::to_string(f)+"."+std::to_string(j);
        }
        for(size_t i=0;i<source.skeleton.bones.size();++i)source.skeleton.boneByName[source.skeleton.bones[i].name]=i;
        scene::Mesh proximal;proximal.name="straight_proximal";proximal.skinned=true;proximal.indices={0,1,2};
        for(const auto p:{scene::Vec3{2,-7,0},scene::Vec3{4,-8,1},scene::Vec3{6,-9,0}}){scene::Vertex v;v.position=p;v.bones={0,0,0,0};proximal.vertices.push_back(v);}source.meshes.push_back(proximal);
        driver.skeleton.bones[0].restGlobal=scene::translation({0,-8,-10});driver.skeleton.bones[0].inverseBind=scene::inverseAffine(driver.skeleton.bones[0].restGlobal);
        const auto fit=[&](scene::CastScene& target,const scene::CastScene& skin,bool enabled){std::string error;return scene::imported::fitForeignViewSkin(target,skin,error,true,false,false,false,false,false,false,false,enabled);};
        auto baseline=driver,candidate=driver;check(fit(baseline,source,false)&&fit(candidate,source,true),"straight carrier fit failed");
        for(size_t m=0;m<candidate.meshes.size();++m){const auto& b=baseline.meshes[m];const auto& c=candidate.meshes[m];check(b.indices==c.indices&&b.vertices.size()==c.vertices.size(),"straight carrier changed topology");
            for(size_t i=0;i<c.vertices.size();++i){if(c.name=="straight_proximal"){check(c.vertices[i].bones[0]==1&&c.vertices[i].weights[0]==1,"straight proximal did not use elbow carrier");check(scene::length(c.vertices[i].position-proximal.vertices[i].position)<1e-4f,"straight carrier did not preserve forearm continuation frame");}
                else check(scene::length(c.vertices[i].position-b.vertices[i].position)<1e-6f&&c.vertices[i].bones==b.vertices[i].bones&&c.vertices[i].weights==b.vertices[i].weights,"straight carrier altered hands");}}
        for(bool bent:{false,true}){auto rejected=source;if(bent)rejected.skeleton.bones[0].restGlobal=scene::translation({0,-8,-20});else rejected.skeleton.bones[1].parent=0;
            auto a=driver,b=driver;check(fit(a,rejected,false)&&fit(b,rejected,true),"straight carrier negative fixture failed");
            for(size_t m=0;m<a.meshes.size();++m)for(size_t i=0;i<a.meshes[m].vertices.size();++i)check(scene::length(a.meshes[m].vertices[i].position-b.meshes[m].vertices[i].position)<1e-6f&&a.meshes[m].vertices[i].bones==b.meshes[m].vertices[i].bones,"straight carrier accepted bent or hierarchical arm");}
    }
    {auto skin=makeHands(false),native=makeHands(true);skin.skeleton.boneByName.clear();
        for(int side=0;side<2;++side){const int base=side*18;const std::string cap=side?"R":"L",p="cs_hand_"+cap+".";
            skin.skeleton.bones[base].name="cs_hand_Bone_"+cap+"_Upper";
            skin.skeleton.bones[base+1].name=p+"forearm";skin.skeleton.bones[base+2].name=p+"palm";
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)skin.skeleton.bones[base+3+f*3+j].name=p+"finger"+std::to_string(f)+"."+std::to_string(j);
        }
        for(size_t i=0;i<skin.skeleton.bones.size();++i)skin.skeleton.boneByName[skin.skeleton.bones[i].name]=i;
        // Identical joint lengths; padded native arm radii are deliberately
        // different from the bare donor. Palm/fingers keep envelope fitting.
        for(int bone:{0,1,2,6}){scene::Mesh sm,dm;sm.skinned=dm.skinned=true;sm.name=dm.name="radius_"+std::to_string(bone);
            const auto center=scene::transformPoint(skin.skeleton.bones[bone].restGlobal,{});
            for(int j=0;j<8;++j){const float angle=j*2*scene::kPi/8;scene::Vertex v;v.position=center+scene::Vec3{1,std::cos(angle),std::sin(angle)};v.normal={0,std::cos(angle),std::sin(angle)};v.bones={static_cast<uint32_t>(bone),0,0,0};sm.vertices.push_back(v);
                v.position=center+scene::Vec3{1,3*std::cos(angle),4*std::sin(angle)};dm.vertices.push_back(v);}
            for(uint32_t j=1;j<7;++j){sm.indices.insert(sm.indices.end(),{0,j,j+1});dm.indices.insert(dm.indices.end(),{0,j,j+1});}
            skin.meshes.push_back(sm);native.meshes.push_back(dm);
        }
        auto baseline=native,candidate=native;std::string issue;
        check(scene::imported::fitForeignViewSkin(baseline,skin,issue),"uniform radius baseline failed");
        check(scene::imported::fitForeignViewSkin(candidate,skin,issue,true,false,false,false,false,false,false,true),"uniform radius candidate failed");
        bool changed=false;
        for(size_t m=0;m<skin.meshes.size();++m){const auto& sm=skin.meshes[m];const auto& bm=baseline.meshes[m];const auto& cm=candidate.meshes[m];
            check(bm.indices==cm.indices&&bm.vertices.size()==cm.vertices.size(),"uniform radius changed topology");
            const bool arm=sm.name=="radius_0"||sm.name=="radius_1";
            for(size_t i=0;i<sm.vertices.size();++i){const auto& b=bm.vertices[i];const auto& c=cm.vertices[i];
                check(b.bones==c.bones&&b.weights==c.weights,"uniform radius changed weights");
                if(arm){check(scene::length(c.position-sm.vertices[i].position)<1e-4f,"arm did not retain unit anatomical scale");changed|=scene::length(c.position-b.position)>1e-3f;}
                else check(scene::length(c.position-b.position)<1e-6f&&scene::length(c.normal-b.normal)<1e-6f&&c.uv.x==b.uv.x&&c.uv.y==b.uv.y,"uniform radius altered palm/finger fit");
            }
        }
        check(changed,"uniform radius fixture did not exercise padded envelope");
    }
    {auto skin=makeHands(false),driver=makeHands(true);
        scene::Mesh tube;tube.skinned=true;tube.name="source_forearm";
        for(int ring=0;ring<2;++ring)for(int j=0;j<8;++j){const float angle=j*2*scene::kPi/8;scene::Vertex v;
            v.position={10.f+ring*8.f,-8.f+std::cos(angle),std::sin(angle)};v.normal={0,std::cos(angle),std::sin(angle)};
            v.uv={j/8.f,float(ring)};v.bones={1,0,0,0};tube.vertices.push_back(v);}
        for(uint32_t j=0;j<8;++j){const auto n=(j+1)%8;tube.indices.insert(tube.indices.end(),{j,n,j+8,n,n+8,j+8});}
        skin.meshes.push_back(tube);const auto layout=scene::imported::viewLayout(skin);
        check(!scene::imported::viewHasWeightedUpperArm(skin,layout.arms[0]),"unweighted shoulder must allow continuation");
        auto upper=skin;upper.meshes.back().vertices[0].bones[0]=0;
        check(scene::imported::viewHasWeightedUpperArm(upper,layout.arms[0]),"authored upper arm must block continuation");
        auto follower=skin;const auto followerId=static_cast<uint32_t>(follower.skeleton.bones.size());
        follower.skeleton.bones.push_back(follower.skeleton.bones[0]);follower.nativePoseFollowers.emplace_back(followerId,0);
        follower.meshes.back().vertices[0].bones[0]=followerId;
        check(scene::imported::viewHasWeightedUpperArm(follower,layout.arms[0]),"upper-arm bind follower must block continuation");
        std::string issue;check(scene::imported::fitForeignViewSkin(driver,skin,issue),"shared continuation integration fit failed");
        const auto& fitted=driver.meshes.back();check(fitted.vertices.size()>tube.vertices.size(),"shared fit did not extend eligible forearm");
        check(std::equal(tube.indices.begin(),tube.indices.end(),fitted.indices.begin()),"shared continuation changed original topology prefix");
        for(size_t i=0;i<tube.vertices.size();++i){check(scene::length(fitted.vertices[i].position-tube.vertices[i].position)<1e-4f,"shared continuation moved original forearm");check(fitted.vertices[i].uv.x==tube.vertices[i].uv.x&&fitted.vertices[i].uv.y==tube.vertices[i].uv.y,"shared continuation changed original UV");}
        for(size_t i=tube.vertices.size();i<fitted.vertices.size();++i)for(int k=0;k<4;++k)if(fitted.vertices[i].weights[k]>0)check(fitted.vertices[i].bones[k]<=2,"continuation used nonanatomical palette");
        auto fullDriver=makeHands(true);check(scene::imported::fitForeignViewSkin(fullDriver,upper,issue),"full upper-arm fit failed");
        check(fullDriver.meshes.back().vertices.size()==tube.vertices.size(),"existing weighted upper arm was extended");
    }
    {auto source=makeHands(false),native=makeHands(true);
        scene::Mesh cuff;cuff.name="native_profile_cuff";cuff.indices={0,1,2};
        for(const auto p:{scene::Vec3{8,-8,1},scene::Vec3{9,-7,1},scene::Vec3{9,-8,2}}){scene::Vertex v;v.position=p;v.bones={0,1,0,0};v.weights={.9f,.1f,0,0};cuff.vertices.push_back(v);}source.meshes.push_back(cuff);
        // Assembled native hands may retain shifted bind-space followers.
        // Both their anatomical IDs and positions must be resolved before
        // sampling, otherwise this profile would silently have no coverage.
        const auto followerStart=static_cast<uint32_t>(native.skeleton.bones.size());
        for(int bone=0;bone<2;++bone){auto follower=native.skeleton.bones[bone];follower.name="profile_follower_"+std::to_string(bone);follower.restGlobal.v[12]+=50;follower.inverseBind=scene::inverseAffine(follower.restGlobal);native.nativePoseFollowers.emplace_back(native.skeleton.bones.size(),bone);native.skeleton.bones.push_back(follower);}
        auto nativeCuff=cuff;for(auto& v:nativeCuff.vertices){v.position.x+=50;v.bones={followerStart,followerStart+1,0,0};v.weights={.2f,.8f,0,0};}native.meshes.push_back(nativeCuff);
        auto armor=native.skeleton.bones[0];armor.name="l_shield";armor.parent=0;
        const auto armorId=static_cast<uint32_t>(native.skeleton.bones.size());native.skeleton.bones.push_back(armor);native.skeleton.boneByName[armor.name]=armorId;
        auto armorMesh=cuff;for(auto& v:armorMesh.vertices){v.bones={armorId,0,0,0};v.weights={1,0,0,0};}native.meshes.push_back(armorMesh);
        auto baseline=native,candidate=native;std::string issue;
        check(scene::imported::fitForeignViewSkin(baseline,source,issue),"native upper profile baseline failed");
        check(scene::imported::fitForeignViewSkin(candidate,source,issue,true,false,false,false,false,false,true),"native upper profile diagnostic failed");
        bool changed=false;
        for(std::size_t m=0;m<std::min(baseline.meshes.size(),candidate.meshes.size());++m){const auto& a=baseline.meshes[m];const auto& b=candidate.meshes[m];check(a.indices==b.indices,"native upper profile changed topology");
            for(std::size_t i=0;i<a.vertices.size();++i){const auto& v=a.vertices[i];const auto& w=b.vertices[i];
                check(scene::length(v.position-w.position)<1e-6f,"native upper profile changed fitted geometry");
                float total=0,elbow=0;for(int k=0;k<4;++k){total+=w.weights[k];if(w.weights[k]>0)check(w.bones[k]!=armorId,"native upper profile transferred armor");if(w.bones[k]==1)elbow+=w.weights[k];}
                check(std::abs(total-1)<1e-6f,"native upper profile not normalized");
                if(a.name=="native_profile_cuff"){changed|=v.weights!=w.weights;check(std::abs(elbow-.8f)<1e-5f,"native elbow distribution not transferred");check(std::abs(v.weights[0]-.9f)<1e-6f,"default upper weights changed");}
                else check(v.bones==w.bones&&v.weights==w.weights,"native upper profile changed palm weights");
            }}check(changed,"native upper profile fixture did not exercise transfer");
        for(std::size_t i=0;i<native.skeleton.bones.size();++i)check(native.skeleton.bones[i].restGlobal.v==candidate.skeleton.bones[i].restGlobal.v,"native upper profile changed native bind");
        auto roll=native.skeleton.bones[1];roll.name="native_forearm_twist";roll.parent=1;
        roll.restGlobal=scene::translation({18,-8,0});roll.inverseBind=scene::inverseAffine(roll.restGlobal);
        const auto rollId=static_cast<uint32_t>(native.skeleton.bones.size());native.skeleton.bones.push_back(roll);native.skeleton.boneByName[roll.name]=rollId;
        auto rollMesh=cuff;for(auto& v:rollMesh.vertices){v.bones={1,rollId,0,0};v.weights={.5f,.5f,0,0};}native.meshes.push_back(rollMesh);
        candidate=native;
        check(scene::imported::fitForeignViewSkin(candidate,source,issue,true,false,false,false,false,false,true),"combined profile/roll fit failed");
        bool checkedCombined=false;
        for(const auto& mesh:candidate.meshes)if(mesh.name=="native_profile_cuff")for(const auto& v:mesh.vertices){
            float shoulder=0,forearm=0,rolled=0;
            for(int k=0;k<4;++k){if(v.bones[k]==0)shoulder+=v.weights[k];if(v.bones[k]==1||v.bones[k]==rollId)forearm+=v.weights[k];if(v.bones[k]==rollId)rolled+=v.weights[k];}
            check(std::abs(shoulder-.2f)<1e-5f&&std::abs(forearm-.8f)<1e-5f,"distal helper blocked upper profile");
            check(rolled>0,"upper profile lost distal roll redistribution");checkedCombined=true;
        }
        check(checkedCombined,"combined profile/roll fixture not exercised");
    }
    {auto skin=makeHands(false),native=makeHands(true);
        const auto shoulder=[](scene::CastScene& model,scene::Vec3 p){model.skeleton.bones[0].restGlobal=scene::translation(p);model.skeleton.bones[0].inverseBind=scene::inverseAffine(model.skeleton.bones[0].restGlobal);};
        shoulder(skin,{0,-8,-7});shoulder(native,{0,-13,-4});
        scene::Mesh cuff;cuff.name="bend_plane_cuff";cuff.indices={0,1,2};
        for(const auto p:{scene::Vec3{9,-7,0},scene::Vec3{9,-8,1},scene::Vec3{8,-7,1}}){scene::Vertex v;v.position=p;v.bones={0,1,0,0};v.weights={.8f,.2f,0,0};cuff.vertices.push_back(v);}skin.meshes.push_back(cuff);
        const auto fit=[&](scene::CastScene& target,const scene::CastScene& source,bool enabled){std::string issue;return scene::imported::fitForeignViewSkin(target,source,issue,true,false,false,false,false,enabled);};
        auto baseline=native,candidate=native;check(fit(baseline,skin,false)&&fit(candidate,skin,true),"bend-plane fixture failed");
        bool changed=false;
        for(std::size_t m=0;m<std::min(baseline.meshes.size(),candidate.meshes.size());++m){const auto& a=baseline.meshes[m];const auto& b=candidate.meshes[m];
            check(a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"bend-plane changed topology");
            for(std::size_t i=0;i<std::min(a.vertices.size(),b.vertices.size());++i){const auto& v=a.vertices[i];const auto& w=b.vertices[i];
                check(v.bones==w.bones&&v.weights==w.weights,"bend-plane changed animation weights");
                const float delta=scene::length(v.position-w.position);if(a.name=="bend_plane_cuff")changed|=delta>1e-4f;else check(delta<1e-6f,"bend-plane changed palm geometry");
            }}check(changed,"bend-plane fixture did not change upper-arm fitting");
        for(std::size_t i=0;i<native.skeleton.bones.size();++i)check(native.skeleton.bones[i].restGlobal.v==candidate.skeleton.bones[i].restGlobal.v,"bend-plane changed driver bind");
        const auto mirror=[](scene::CastScene model){for(auto& b:model.skeleton.bones){b.restGlobal.v[13]*=-1;b.inverseBind=scene::inverseAffine(b.restGlobal);}for(auto& mesh:model.meshes)for(auto& v:mesh.vertices)v.position.y*=-1;return model;};
        auto reflected=mirror(native);const auto reflectedSkin=mirror(skin);check(fit(reflected,reflectedSkin,true),"mirrored bend-plane fit failed");
        for(std::size_t m=0;m<std::min(candidate.meshes.size(),reflected.meshes.size());++m)for(std::size_t i=0;i<candidate.meshes[m].vertices.size();++i){auto p=candidate.meshes[m].vertices[i].position;p.y*=-1;check(scene::length(p-reflected.meshes[m].vertices[i].position)<1e-4f,"bend-plane fitting is not mirror symmetric");}
        // Either side of the fit being nearly straight must retain the old
        // frame, not replace only one normal and introduce an arbitrary roll.
        for(bool straightSource:{false,true}){auto source=skin,target=native;shoulder(straightSource?source:target,{0,-8,-.1f});auto control=target,probe=target;check(fit(control,source,false)&&fit(probe,source,true),"near-straight bend-plane fit failed");
            for(std::size_t m=0;m<std::min(control.meshes.size(),probe.meshes.size());++m)for(std::size_t i=0;i<control.meshes[m].vertices.size();++i)check(scene::length(control.meshes[m].vertices[i].position-probe.meshes[m].vertices[i].position)<1e-6f,"near-straight bend-plane did not fall back");}
    }
    {auto skin=makeHands(false),native=makeHands(true);
        // Bent source upper arm versus straight native arm makes the two
        // segment fits differ at the elbow cuff. Wrist geometry is unchanged.
        skin.skeleton.bones[0].restGlobal=scene::translation({0,-8,-10});
        skin.skeleton.bones[0].inverseBind=scene::inverseAffine(skin.skeleton.bones[0].restGlobal);
        scene::Mesh cuff;cuff.name="fit_only_cuff";cuff.indices={0,1,2};
        for(const auto p:{scene::Vec3{10.5f,-8,1},scene::Vec3{11,-7.5f,1},scene::Vec3{11,-8,1.5f}}){scene::Vertex v;v.position=p;v.bones={0,1,0,0};v.weights={.9f,.1f,0,0};cuff.vertices.push_back(v);}
        skin.meshes.push_back(cuff);auto baseline=native,candidate=native;std::string issue;
        check(scene::imported::fitForeignViewSkin(baseline,skin,issue),"fit-only baseline failed");
        check(scene::imported::fitForeignViewSkin(candidate,skin,issue,true,false,false,false,true),"fit-only diagnostic failed");
        check(candidate.meshes.size()==baseline.meshes.size(),"fit-only changed mesh count");
        bool changed=false;
        if(candidate.meshes.size()==baseline.meshes.size())for(std::size_t m=0;m<baseline.meshes.size();++m){
            const auto& a=baseline.meshes[m];const auto& b=candidate.meshes[m];
            check(a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"fit-only changed topology");
            for(std::size_t v=0;v<std::min(a.vertices.size(),b.vertices.size());++v){
                check(a.vertices[v].bones==b.vertices[v].bones&&a.vertices[v].weights==b.vertices[v].weights,"fit-only changed animation weights");
                const float displacement=scene::length(a.vertices[v].position-b.vertices[v].position);
                if(a.name=="fit_only_cuff")changed|=displacement>1e-4f;
                else check(displacement<1e-6f,"fit-only moved palm geometry");
                const auto p=b.vertices[v].position;check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"fit-only made nonfinite geometry");
            }
        }
        check(changed,"fit-only fixture did not exercise geometry change");
        for(std::size_t b=0;b<native.skeleton.bones.size();++b)check(candidate.skeleton.bones[b].restGlobal.v==native.skeleton.bones[b].restGlobal.v,"fit-only changed native bind");
    }
    {auto skin=makeHands(false),driver=makeHands(true);
        const auto ancestors=[](scene::CastScene& model){
            scene::Bone root;root.name="shared_torso";root.parent=-1;
            root.restGlobal=scene::Mat4::identity();root.inverseBind=scene::Mat4::identity();
            const int rootId=static_cast<int>(model.skeleton.bones.size());
            model.skeleton.bones.push_back(root);model.skeleton.boneByName[root.name]=rootId;
            for(int side=0;side<2;++side){
                auto clavicle=model.skeleton.bones[side*18];clavicle.name=side?"right_clavicle_anchor":"left_clavicle_anchor";clavicle.parent=rootId;
                const int id=static_cast<int>(model.skeleton.bones.size());model.skeleton.bones.push_back(clavicle);
                model.skeleton.boneByName[clavicle.name]=id;model.skeleton.bones[side*18].parent=id;
            }
            return rootId;
        };
        const int sourceRoot=ancestors(skin),targetRoot=ancestors(driver);
        {
            auto probe=skin;auto patch=skin.meshes[0];patch.name="minor_ancestor_patch";
            for(auto& v:patch.vertices){v.bones={0,static_cast<uint32_t>(sourceRoot),0,0};v.weights={.98f,.02f,0,0};}
            probe.meshes.push_back(patch);const auto original=probe;
            check(scene::imported::diagnosticMinorAncestorWeights(probe)==patch.vertices.size(),"minor ancestor diagnostic did not retain arm patch");
            check(probe.meshes.back().indices==patch.indices,"minor ancestor diagnostic changed topology");
            for(std::size_t i=0;i<patch.vertices.size();++i){const auto& v=probe.meshes.back().vertices[i];
                check(v.weights[0]==1&&v.weights[1]==0,"minor ancestor residual not normalized");
                check(scene::length(v.position-patch.vertices[i].position)<1e-6f,"minor ancestor diagnostic moved bind geometry");}
            for(int mode=0;mode<5;++mode){auto rejected=original;for(auto& v:rejected.meshes.back().vertices){
                if(mode==0)v.weights={.9f,.1f,0,0};
                if(mode==1){v.bones[2]=18;v.weights={.49f,.02f,.49f,0};}
                if(mode==2)v.bones[1]=static_cast<uint32_t>(sourceRoot+100);
                if(mode==3)v.weights={.98f,.02f,-.1f,0};
                if(mode==4)v.weights={0,1,0,0};
            }check(scene::imported::diagnosticMinorAncestorWeights(rejected)==0,"minor ancestor diagnostic accepted torso/mixed/invalid skin");}
            auto control=driver,retained=driver;std::string fitIssue;
            check(scene::imported::fitForeignViewSkin(control,original,fitIssue)&&scene::imported::fitForeignViewSkin(retained,probe,fitIssue),"minor ancestor fit failed");
            check(retained.meshes.size()==control.meshes.size()+1,"minor ancestor patch not retained by fitter");
            auto tiny=original;
            for(auto& v:tiny.meshes.back().vertices)v.weights={.994575f,.005425f,0,0};
            auto recovered=driver;
            check(scene::imported::fitForeignViewSkin(recovered,tiny,fitIssue),"tiny ancestor production fit failed");
            check(recovered.meshes.size()==retained.meshes.size(),"tiny ancestor production path lost arm triangles");
            check(recovered.meshes.back().indices==patch.indices,"tiny ancestor changed retained topology");
            check(tiny.meshes.back().vertices[0].weights[1]>.005f,"fitter mutated source weights");
            auto mixed=tiny;for(auto& v:mixed.meshes.back().vertices){v.bones[2]=18;v.weights={.497f,.006f,.497f,0};}
            auto rejected=driver;check(scene::imported::fitForeignViewSkin(rejected,mixed,fitIssue),"mixed-arm fit failed");
            check(rejected.meshes.size()==control.meshes.size(),"tiny residual accepted mixed-arm torso patch");
            auto nativeDriver=driver,nativeSkin=tiny;nativeDriver.importedViewGame="same";nativeSkin.importedViewGame="same";
            check(scene::imported::fitForeignViewSkin(nativeDriver,nativeSkin,fitIssue),"native residual control failed");
            check(nativeDriver.meshes.size()==control.meshes.size(),"native residual weights were altered");
        }
        for(int part=0;part<3;++part){auto mesh=skin.meshes[0];mesh.name="ancestor_skin_"+std::to_string(part);
            for(auto& v:mesh.vertices){v.bones={static_cast<uint32_t>(sourceRoot+part),0,0,0};v.weights={1,0,0,0};}
            skin.meshes.push_back(mesh);
        }
        const auto bind=driver.skeleton.bones;auto baseline=driver,candidate=driver;std::string issue;
        check(scene::imported::fitForeignViewSkin(baseline,skin,issue,false),"ancestor control fit failed");
        check(scene::imported::fitForeignViewSkin(candidate,skin,issue),"ancestor preservation fit failed");
        check(baseline.meshes.size()==2,"control unexpectedly retained unmapped ancestor geometry");
        check(candidate.meshes.size()==4,"side-exclusive clavicle geometry not preserved or shared torso leaked");
        for(const auto& mesh:candidate.meshes){
            check(mesh.name!="ancestor_skin_0","shared torso must not be assigned to either arm");
            if(mesh.name=="ancestor_skin_1"||mesh.name=="ancestor_skin_2"){
                const auto target=static_cast<uint32_t>(targetRoot+(mesh.name.back()=='1'?1:2));
                for(const auto& v:mesh.vertices)check(v.bones[0]==target&&std::abs(v.weights[0]-1)<1e-6f,"clavicle skin crossed arms or attached to upper arm");
            }
        }
        if(candidate.meshes.size()>=2)for(int side=0;side<2;++side)for(size_t i=0;i<baseline.meshes[side].vertices.size();++i){
            const auto& a=baseline.meshes[side].vertices[i];const auto& b=candidate.meshes[side].vertices[i];
            check(scene::length(a.position-b.position)<1e-6f&&a.bones==b.bones&&a.weights==b.weights,"ancestor preservation changed hand contact");
        }
        for(size_t i=0;i<bind.size();++i)check(bind[i].restGlobal.v==candidate.skeleton.bones[i].restGlobal.v,"ancestor preservation changed driver bind pose");
    }
    check(scene::imported::viewForearmTwist("j_wristtwist_le",0),"Black Squad wrist helper missing");
    check(!scene::imported::viewForearmTwist("j_wristtwist_le",1),"wrist helper crosses arms");
    check(scene::imported::viewForearmTwist("j_sleave_reshape_top_ri_1",1),"Black Squad cuff helper missing");
    check(scene::imported::viewForearmTwist("skelbone_l_foretwist",0,true)&&scene::imported::viewForearmTwist("skelbone_r_foretwist1",1,true),"underscore forearm helper missing");
    check(!scene::imported::viewForearmTwist("skelbone_l_foretwist1",1,true)&&!scene::imported::viewForearmTwist("skelbone_l_foretwist_socket",0,true),"underscore helper accepts wrong side or socket");
    check(!scene::imported::viewForearmTwist("skelbone_l_foretwist1",0),"unapproved contour experiment enabled by default");
    check(scene::imported::viewForearmTwist("b_rightforearmroll",1,true)&&scene::imported::viewForearmTwist("b_leftforearmroll",0,true),"CODM pose-only forearm roll helper missing");
    check(!scene::imported::viewForearmTwist("b_rightforearmroll",0,true)&&!scene::imported::viewForearmTwist("b_rightforearmroll_socket",1,true),"CODM roll helper accepts wrong side or socket");
    {scene::CastScene driver;const auto add=[&](const std::string& n){driver.skeleton.boneByName[n]=driver.skeleton.bones.size();scene::Bone b;b.name=n;driver.skeleton.bones.push_back(b);};
        add("armUpperShoulder_L");add("arm_upper_L");add("arm_lower_L");add("hand_L");
        check(scene::imported::viewLayout(driver).arms[0].shoulder==1,"Source 2 sleeve uses stationary shoulder anchor instead of rotating upper arm");
        driver.skeleton.boneByName.erase("arm_upper_L");check(scene::imported::viewLayout(driver).arms[0].shoulder==0,"Source 2 older shoulder-only layout fallback missing");
    }
    {auto skin=makeHands(false),target=makeHands(true);const auto bind=target.skeleton.bones;
        for(int side=0;side<2;++side){const int base=side*18;
            for(int j=0;j<4;++j){scene::Bone twist;twist.name=std::string(side?"R":"L")+" Twist"+std::to_string(j+1);twist.parent=j?static_cast<int>(skin.skeleton.bones.size()-1):base;
                twist.restGlobal=scene::translation({10.f+2.5f*j,float(side?8:-8),0});twist.inverseBind=scene::inverseAffine(twist.restGlobal);
                const auto id=skin.skeleton.bones.size();skin.skeleton.boneByName[twist.name]=id;skin.skeleton.bones.push_back(twist);
                scene::Mesh mesh;for(int k=0;k<3;++k){scene::Vertex v;v.position={10.f+2.5f*j,float(side?8:-8),float(k)*.1f};v.bones={static_cast<uint32_t>(id),0,0,0};v.weights={1,0,0,0};mesh.vertices.push_back(v);}mesh.indices={0,1,2};skin.meshes.push_back(mesh);
            }
        }
        std::string error;check(scene::imported::fitForeignViewSkin(target,skin,error),"PB twist-chain fitting failed");
        for(int side=0;side<2;++side)for(int j=0;j<4;++j){const auto& v=target.meshes[2+side*4+j].vertices[0];float wrist{},elbow{};
            for(int k=0;k<4;++k)if(v.weights[k]>0){if(v.bones[k]==side*18+1)elbow+=v.weights[k];else if(v.bones[k]==side*18+2)wrist+=v.weights[k];else check(false,"PB twist skin attached outside forearm");}
            check(std::abs(wrist-j*.25f)<.001f&&std::abs(elbow-(1-j*.25f))<.001f,"PB twist lost longitudinal distribution");
        }
        for(size_t i=0;i<bind.size();++i)check(bind[i].restGlobal.v==target.skeleton.bones[i].restGlobal.v,"PB twist fitting modified driver");
        check(!scene::imported::viewForearmTwist("r twist2",0)&&!scene::imported::viewForearmTwist("l twist_unknown",0),"twist matching crossed sides or accepted unknown helper");
    }
    {auto source=makeHands(false);source.skeleton.boneByName.clear();for(int side=0;side<2;++side){const std::string suffix=side?"R":"L";const int base=side*18;source.skeleton.bones[base].name="armUpperShoulder_"+suffix;source.skeleton.bones[base+1].name="arm_lower_"+suffix;source.skeleton.bones[base+2].name="hand_"+suffix;int f=0;for(const std::string digit:{"thumb","index","middle","ring","pinky"}){for(int j=0;j<3;++j)source.skeleton.bones[base+3+f*3+j].name="finger_"+digit+"_"+std::to_string(j)+"_"+suffix;++f;}}for(size_t i=0;i<source.skeleton.bones.size();++i)source.skeleton.boneByName[source.skeleton.bones[i].name]=i;check(scene::imported::viewLayout(source).valid(),"Source 2 hand anatomy missing");auto driver=makeHands(true);std::string error;check(scene::imported::fitForeignViewSkin(driver,source,error),"Source 2 skin fitting failed");}
    {auto flat=makeHands(false);flat.skeleton.boneByName.clear();for(int side=0;side<2;++side){const auto p=std::string("cs_hand_")+(side?"Right":"Left")+"_H_Bone";const auto names=side?std::array<int,18>{2,1,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19}:std::array<int,18>{37,20,21,34,35,36,31,32,33,28,29,30,25,26,27,22,23,24};for(int j=0;j<18;++j){auto& b=flat.skeleton.bones[side*18+j];b.name=p+(names[j]<10?"0":"")+std::to_string(names[j]);b.parent=-1;flat.skeleton.boneByName[b.name]=side*18+j;}}check(scene::imported::viewLayout(flat).valid(),"flat H_Bone hand anatomy missing");auto target=makeHands(true);std::string error;check(scene::imported::fitForeignViewSkin(target,flat,error),"flat H_Bone skin fit failed");}
    {auto hashed=makeHands(false);hashed.skeleton.boneByName.clear();for(size_t i=0;i<hashed.skeleton.bones.size();++i){auto& b=hashed.skeleton.bones[i];if(i%18>=3)b.name.back()+=1;b.name=scene::nativeBoneHash(b.name);hashed.skeleton.boneByName[b.name]=i;}check(scene::imported::viewLayout(hashed).valid(),"one-based hashed CoD finger anatomy missing");}
    {auto hashed=makeHands(false);hashed.skeleton.boneByName.clear();for(size_t i=0;i<hashed.skeleton.bones.size();++i){auto& b=hashed.skeleton.bones[i];b.name=scene::nativeBoneHash(b.name);hashed.skeleton.boneByName[b.name]=i;}check(scene::imported::viewLayout(hashed).valid(),"hashed CoD hand anatomy missing");std::string error;auto driver=makeHands(true);check(scene::imported::fitForeignViewSkin(driver,hashed,error),"hashed skin cannot fit native driver");check(scene::imported::fitForeignViewSkin(hashed,makeHands(true),error),"foreign skin cannot fit hashed driver");}
    {scene::CastScene skin;scene::Bone b;b.name="cs_hand_CSO2_MHAND L ForeTwist1";skin.skeleton.bones.push_back(b);scene::Mesh m;m.name="weapon/forearm";m.viewmodelWeapon=true;m.vertices.resize(3);for(auto& v:m.vertices){v.bones={};v.weights={1,0,0,0};}m.indices={0,1,2};skin.meshes.push_back(m);scene::imported::retainExplicitHandMeshes(skin);check(!skin.meshes[0].viewmodelWeapon,"universal twist-weighted sleeve classified as weapon");skin.skeleton.bones[0].name="cs_weapon_fixture_gun";scene::imported::retainExplicitHandMeshes(skin);check(skin.meshes[0].viewmodelWeapon,"weapon bones classified as shared hands");}
    {scene::CastScene skin;scene::Bone palm;palm.name="cs_hand_L.palm";skin.skeleton.bones.push_back(palm);scene::Bone twist;twist.name="cs_hand_CSO2_MHAND L ForeTwist1";skin.skeleton.bones.push_back(twist);scene::Mesh hand;hand.name="hands/glove";hand.vertices.resize(3);hand.indices={0,1,2};for(auto& v:hand.vertices){v.bones={};v.weights={1,0,0,0};}auto sleeve=hand;sleeve.name="weapon/sleeve";for(auto& v:sleeve.vertices)v.bones[0]=1;skin.meshes={hand,sleeve};scene::imported::retainExplicitHandMeshes(skin);check(skin.meshes.size()==2&&skin.meshes[1].indices.size()==3&&!skin.meshes[1].viewmodelWeapon,"flat universal helper triangles lost beside explicit glove mesh");}
    for(const auto game:{"cs1.6","css","csnz","cso2"})check(assets::imported::role(game,"hands_shared")==assets::Role::ViewHands,"universal viewhands not classified");
    {auto driver=makeHands(false);const auto original=driver.skeleton.bones;driver.skeleton.boneByName.clear();for(size_t i=0;i<original.size();++i){auto& b=driver.skeleton.bones[i];b.name=i<18?"cs_hand_Left_H_Bone"+std::to_string(i+20):"unrelated_"+std::to_string(i);b.parent=-1;driver.skeleton.boneByName[b.name]=i;}for(size_t i=0;i<18;++i){auto b=original[i];b.name="cs_weapon_fixture_"+driver.skeleton.bones[i].name.substr(8);if(b.parent>=0)b.parent+=static_cast<int>(original.size());driver.skeleton.boneByName[b.name]=driver.skeleton.bones.size();driver.skeleton.bones.push_back(b);}const auto layout=scene::imported::viewLayout(driver);check(layout.usable()&&layout.arms[0].valid()&&!layout.arms[1].valid(),"flat one-arm hierarchy not recovered");driver.meshes.clear();std::string error;check(scene::imported::fitForeignViewSkin(driver,makeHands(true),error),"foreign hand cannot fit flat one-arm driver");check(driver.meshes.size()==1,"unauthored offhand was grafted onto one-arm driver");check(driver.skeleton.bones[2].parent==-1,"anatomy discovery modified animated hierarchy");}
    {auto universal=makeHands(false);universal.skeleton.boneByName.clear();for(int side=0;side<2;++side){const auto base=side*18;const std::string p=std::string("cs_hand_")+(side?"R.":"L.");universal.skeleton.bones[base+1].name=p+"forearm";universal.skeleton.bones[base+2].name=p+"palm";for(int f=0;f<5;++f)for(int j=0;j<3;++j)universal.skeleton.bones[base+3+f*3+j].name=p+"finger"+std::to_string(f)+"."+std::to_string(j);}for(size_t i=0;i<universal.skeleton.bones.size();++i)universal.skeleton.boneByName[universal.skeleton.bones[i].name]=i;check(scene::imported::viewLayout(universal).valid(),"universal CS anatomy missing");auto driver=makeHands(true);std::string error;check(scene::imported::fitForeignViewSkin(driver,universal,error),"universal CS skin cannot fit Halo driver");}
    {scene::CastScene s;const std::string prefix="magnum_1_fp_0_2_dual_first_person_dual_";for(const auto name:{"idle_var1__left_3","idle_var1__right_3","fire_1_var1__left_0","fire_1_var1__right_0"}){scene::Animation a;a.sourceName=prefix+name+".cast";a.tracks.resize(1);s.animations.push_back(a);}check(cadence::dual::sideClip(s,prefix,"idle",0,false)==1,"Halo right idle selection");check(cadence::dual::sideClip(s,prefix,"fire",1,false)==2,"Halo left fire selection");check(assets::imported::matchesAnimation("eldewrito","magnum_1_fp_0_2_dual_weapon",prefix+"idle_var1__left_3.cast"),"Halo dual catalog discovery");}
    for(bool reverse:{false,true}){auto driver=makeHands(reverse),skin=makeHands(!reverse);const auto bind=driver.skeleton.bones;scene::Mesh weapon;weapon.name="mechanism";weapon.viewmodelWeapon=true;driver.meshes.push_back(weapon);std::string error;
        check(scene::imported::fitForeignViewSkin(driver,skin,error),error.c_str());check(driver.skeleton.bones.size()==bind.size(),"driver bone count changed");for(std::size_t i=0;i<bind.size();++i)check(driver.skeleton.bones[i].restGlobal.v==bind[i].restGlobal.v,"driver bind changed");check(driver.meshes.size()==3&&driver.meshes[0].name=="mechanism","weapon geometry removed");check(driver.meshes.back().vertices[2].uv.x==.4f,"UV changed");}
    auto driver=makeHands(false),incomplete=makeHands(true);incomplete.skeleton.boneByName.erase("l_thumb_low");const auto before=driver.meshes.size();std::string error;check(!scene::imported::fitForeignViewSkin(driver,incomplete,error)&&driver.meshes.size()==before,"incomplete anatomy modified driver");
    {auto four=makeHands(true);for(const auto* side:{"l","r"})for(const auto* joint:{"low","mid","tip"})four.skeleton.boneByName.erase(std::string(side)+"_middle_"+joint);
     check(scene::imported::viewLayout(four).valid(),"complete four-digit anatomy rejected");
     auto native=makeHands(false);const auto original=native.skeleton.bones;check(scene::imported::fitForeignViewSkin(native,four,error),"four-digit skin cannot bind five-digit driver");
     for(std::size_t i=0;i<original.size();++i)check(native.skeleton.bones[i].restGlobal.v==original[i].restGlobal.v,"four-digit fit changed driver bind");
     check(native.meshes.size()==four.meshes.size()&&native.meshes[0].indices==four.meshes[0].indices,"four-digit fitting discarded skin geometry");
     auto partial=makeHands(true);partial.skeleton.boneByName.erase("l_middle_low");check(!scene::imported::viewLayout(partial).usable(),"partial middle chain accepted as four-digit anatomy");
     auto sparse=four;sparse.skeleton.boneByName.erase("l_ring_low");sparse.skeleton.boneByName.erase("l_ring_mid");sparse.skeleton.boneByName.erase("l_ring_tip");check(!scene::imported::viewLayout(sparse).usable(),"unsupported three-digit anatomy accepted");
     auto five=makeHands(false);for(auto& v:five.meshes[0].vertices){v.bones={9,0,0,0};v.weights={1,0,0,0};}const auto boneCount=four.skeleton.bones.size();
     check(scene::imported::fitForeignViewSkin(four,five,error),"five-digit skin cannot use four-digit driver");check(four.skeleton.bones.size()==boneCount,"extra digit rebuilt driver rig");
     const auto& v=four.meshes[0].vertices[0];check(v.weights[0]>.49f&&v.weights[1]>.49f&&v.bones[0]!=v.bones[1],"missing digit collapsed onto one neighbour");}
    for(bool pb:{false,true}){auto native=makeHands(false);native.pointBlankNativeCentimetres=pb;native.codmNativeCentimetres=!pb;check(scene::imported::fitForeignViewSkin(native,makeHands(true),error),"native marker fixture fit failed");check(native.viewHandsDriverGame==(pb?"pointblank":"codm"),"native driver identity lost during foreign skin fit");}
    {auto shared=makeHands(false);auto& rig=shared.skeleton;rig.boneByName.clear();
        for(std::size_t i=0;i<rig.bones.size();++i){auto& b=rig.bones[i];b.name=i==2?"Bone_Lefthand":i==20?"Bone_Righthand":"Bone"+std::to_string(i);rig.boneByName[b.name]=i;}
        // The second arm shares the first arm's top-level root, as in CSNZ.
        rig.bones[18].parent=0;
        const auto layout=scene::imported::viewLayout(shared);
        check(layout.valid()&&layout.arms[0].wrist==2&&layout.arms[1].wrist==20,"named wrists sharing one root not recognized");
        check(scene::imported::fitForeignViewSkin(shared,makeHands(true),error),"shared-root native hand fit failed");
    }
    {auto source=makeHands(true),target=makeHands(false);const int finger=6;auto bridge=source.skeleton.bones[finger];bridge.name="unnamed coincident bridge";const auto index=source.skeleton.bones.size();source.skeleton.bones.push_back(bridge);source.skeleton.bones[finger].parent=static_cast<int>(index);for(auto& v:source.meshes[0].vertices){v.bones[0]=static_cast<uint32_t>(index);v.weights={1,0,0,0};}check(scene::imported::fitForeignViewSkin(target,source,error),"coincident helper fit failed");check(target.meshes[0].vertices[0].bones[0]==finger,"coincident finger bridge mapped to wrist");}
    {auto source=makeHands(true),target=makeHands(false);auto twist=source.skeleton.bones[1];twist.name="CSO2_MHAND L ForeTwist1";twist.restGlobal=scene::translation({15,-8,0});twist.inverseBind=scene::inverseAffine(twist.restGlobal);const auto index=source.skeleton.bones.size();source.skeleton.bones.push_back(twist);for(auto& v:source.meshes[0].vertices){v.bones[0]=static_cast<uint32_t>(index);v.weights={1,0,0,0};}check(scene::imported::fitForeignViewSkin(target,source,error),"distributed twist fit failed");const auto& v=target.meshes[0].vertices[0];check(v.weights[0]>.49f&&v.weights[1]>.49f&&((v.bones[0]==1&&v.bones[1]==2)||(v.bones[0]==2&&v.bones[1]==1)),"forearm twist lost measured elbow/wrist distribution");}
    {auto source=makeHands(true),target=makeHands(false);for(auto& b:target.skeleton.bones)if(b.name.starts_with("j_index_")||b.name.starts_with("j_pinky_")){const float center=b.name.find("_le_")!=std::string::npos?-8.f:8.f;b.restGlobal.v[13]=center+(b.restGlobal.v[13]-center)*2.f;}for(auto& v:source.meshes[0].vertices){v.position={10,-7,0};v.bones={1,0,0,0};v.weights={1,0,0,0};}check(scene::imported::fitForeignViewSkin(target,source,error),"different palm proportion fit failed");check(std::abs(scene::length(target.meshes[0].vertices[0].position-scene::Vec3{10,-8,0})-1.f)<.001f,"palm width incorrectly inflates forearm");}
    {auto source=makeHands(false),target=makeHands(true);
        // A donor upper arm points up, while the shoulderless driver extends
        // horizontally. Its cap must continue behind the target elbow.
        for(int side=0;side<2;++side){const int base=side*18;auto& shoulder=source.skeleton.bones[base];shoulder.restGlobal=scene::translation({10,float(side?8:-8)+10,0});shoulder.inverseBind=scene::inverseAffine(shoulder.restGlobal);
            target.skeleton.boneByName.erase(side?"r_upperarm":"l_upperarm");
            auto& m=source.meshes[side];for(auto& v:m.vertices){v.position={10,float(side?8:-8)+10,0};v.bones={static_cast<uint32_t>(base),0,0,0};v.weights={1,0,0,0};}}
        const auto bind=target.skeleton.bones;check(scene::imported::fitForeignViewSkin(target,source,error),"shoulderless arm fit failed");
        for(int side=0;side<2;++side){const auto p=target.meshes[side].vertices[0].position;check(scene::length(p-scene::Vec3{0,float(side?8:-8),0})<.001f,"upper arm retains donor bend");}
        for(size_t i=0;i<bind.size();++i)check(bind[i].restGlobal.v==target.skeleton.bones[i].restGlobal.v,"straightening modified driver");}
    {auto source=makeHands(false),target=makeHands(true);for(auto& b:target.skeleton.bones){b.restGlobal.v[12]*=.25f;b.restGlobal.v[13]*=.25f;b.restGlobal.v[14]*=.25f;b.inverseBind=scene::inverseAffine(b.restGlobal);}
        for(int side=0;side<2;++side){const int base=side*18;target.skeleton.boneByName.erase(side?"r_upperarm":"l_upperarm");auto& shoulder=source.skeleton.bones[base];shoulder.restGlobal=scene::translation({10,float(side?8:-8)+10,0});shoulder.inverseBind=scene::inverseAffine(shoulder.restGlobal);
            for(auto& v:source.meshes[side].vertices){v.position={10,float(side?8:-8)+5,0};v.bones={static_cast<uint32_t>(base),static_cast<uint32_t>(base+1),0,0};v.weights={.5f,.5f,0,0};}}
        check(scene::imported::fitForeignViewSkin(target,source,error),"scaled straight sleeve fit failed");for(int side=0;side<2;++side)check(scene::length(target.meshes[side].vertices[0].position-scene::Vec3{1.25f,float(side?2:-2),0})<.001f,"scaled sleeve deviated from straight forearm");}
    {auto skin=makeHands(false),target=makeHands(true);skin.skeleton.boneByName.clear();
        for(int side=0;side<2;++side){const int base=side*18;const std::string p=std::string("cs_hand_")+(side?"R.":"L.");
            skin.skeleton.bones[base].name=p+"upperarm";skin.skeleton.bones[base+1].name=p+"forearm";skin.skeleton.bones[base+2].name=p+"palm";
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)skin.skeleton.bones[base+3+f*3+j].name=p+"finger"+std::to_string(f)+"."+std::to_string(j);
            auto clavicle=skin.skeleton.bones[base];clavicle.name=p+"clavicle";const auto index=skin.skeleton.bones.size();skin.skeleton.bones.push_back(clavicle);
            for(auto& v:skin.meshes[side].vertices){v.bones={static_cast<uint32_t>(base),static_cast<uint32_t>(index),0,0};v.weights={.5f,.5f,0,0};}
        }
        for(size_t i=0;i<skin.skeleton.bones.size();++i){auto& b=skin.skeleton.bones[i];b.parent=-1;skin.skeleton.boneByName[b.name]=i;}
        check(scene::imported::viewLayout(skin).arms[0].shoulder==0,"universal upper arm missing");
        check(scene::imported::fitForeignViewSkin(target,skin,error),"flat universal upper-arm/clavicle fit failed");
        check(target.meshes.size()==2&&target.meshes[0].indices.size()==3&&target.meshes[1].indices.size()==3,"universal sleeve triangles discarded");
    }
    {auto skin=makeHands(false),target=makeHands(true);skin.skeleton.boneByName.clear();
        for(int side=0;side<2;++side){const int base=side*18;const std::string cap=side?"R":"L",p="cs_hand_"+cap+".";
            skin.skeleton.bones[base].name="cs_hand_Bone_"+cap+"_Upper";
            skin.skeleton.bones[base+1].name=p+"forearm";skin.skeleton.bones[base+2].name=p+"palm";
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)skin.skeleton.bones[base+3+f*3+j].name=p+"finger"+std::to_string(f)+"."+std::to_string(j);
            auto bridge=skin.skeleton.bones[base+2];bridge.name="cs_hand_Bone_Se_Hand-"+cap;
            const auto id=skin.skeleton.bones.size();skin.skeleton.bones.push_back(bridge);
            for(auto& v:skin.meshes[side].vertices){v.bones={static_cast<uint32_t>(id),0,0,0};v.weights={1,0,0,0};}
            auto sleeve=skin.meshes[side];for(auto& v:sleeve.vertices){v.bones[0]=base;v.position={0,float(side?8:-8),0};}skin.meshes.push_back(sleeve);
        }
        for(size_t i=0;i<skin.skeleton.bones.size();++i){auto& b=skin.skeleton.bones[i];b.parent=-1;skin.skeleton.boneByName[b.name]=i;}
        const auto layout=scene::imported::viewLayout(skin);
        check(layout.arms[0].shoulder==0&&layout.arms[1].shoulder==18,"native universal upper-arm roots lost");
        std::string fitError;check(scene::imported::fitForeignViewSkin(target,skin,fitError),"native universal wrist-helper fit failed");
        check(target.meshes.size()==4,"native universal wrist/sleeve triangles discarded");
        if(target.meshes.size()==4)for(int side=0;side<2;++side){check(target.meshes[side].indices.size()==3,"wrist triangles missing");check(target.meshes[side].vertices[0].bones[0]==side*18+2,"native wrist helper attached to wrong arm/joint");}
    }
    {auto driver=makeHands(false);auto roll=driver.skeleton.bones[1];roll.name="j_elbow_leRoll";const auto id=driver.skeleton.bones.size();driver.skeleton.boneByName[roll.name]=id;driver.skeleton.bones.push_back(roll);
        for(auto& v:driver.meshes[0].vertices){v.position={15,-8,0};v.bones={1,static_cast<uint32_t>(id),0,0};v.weights={.25f,.75f,0,0};}
        auto fitted=driver.meshes;for(auto& v:fitted[0].vertices){v.bones={1,2,0,0};v.weights={.8f,.2f,0,0};}
        const auto bind=driver.skeleton.bones;
        scene::imported::transferNativeForearmRoll(driver,fitted,1,2);
        const auto& v=fitted[0].vertices[0];float rollWeight{},wristWeight{},elbowWeight{};for(int k=0;k<4;++k){if(v.bones[k]==id)rollWeight+=v.weights[k];if(v.bones[k]==2)wristWeight+=v.weights[k];if(v.bones[k]==1)elbowWeight+=v.weights[k];}
        check(std::abs(rollWeight-.6f)<1e-5f&&std::abs(wristWeight-.2f)<1e-5f&&std::abs(elbowWeight-.2f)<1e-5f,"native roll distribution not retained or wrist changed");
        check(scene::length(v.position-scene::Vec3{15,-8,0})<1e-5f,"roll transfer changed fitted bind position");
        for(size_t i=0;i<bind.size();++i)check(bind[i].restGlobal.v==driver.skeleton.bones[i].restGlobal.v,"roll transfer changed driver skeleton");
        auto untouched=fitted;scene::imported::transferNativeForearmRoll(driver,untouched,19,20);check(untouched[0].vertices[0].weights==v.weights,"missing native roll changed skin");
    }
    {auto driver=makeHands(false);auto helper=driver.skeleton.bones[2];helper.name="generic_wrist_twist";helper.parent=1;
        helper.restGlobal=scene::translation({19,-8,0});helper.inverseBind=scene::inverseAffine(helper.restGlobal);
        const auto id=static_cast<uint32_t>(driver.skeleton.bones.size());driver.skeleton.bones.push_back(helper);
        for(auto& v:driver.meshes[0].vertices){v.position={15,-8,0};v.bones={1,id,0,0};v.weights={.25f,.75f,0,0};}
        check(scene::imported::nativeDistalForearmRoll(driver,1,2)==int(id),"weighted distal helper not discovered");
        auto fitted=driver.meshes;for(auto& v:fitted[0].vertices){v.bones={1,2,0,0};v.weights={.8f,.2f,0,0};}
        auto explicitFit=fitted;scene::imported::transferNativeForearmRoll(driver,explicitFit,1,2,id);
        scene::imported::transferNativeForearmRoll(driver,fitted,1,2);
        check(fitted[0].vertices[0].weights==explicitFit[0].vertices[0].weights&&fitted[0].vertices[0].bones==explicitFit[0].vertices[0].bones,"automatic helper differs from native explicit transfer");
        auto invalid=driver;invalid.skeleton.bones[id].parent=0;
        check(scene::imported::nativeDistalForearmRoll(invalid,1,2)<0,"wrong-parent helper accepted");
        invalid=driver;invalid.skeleton.bones[id].restGlobal=scene::translation({19,-6,0});
        check(scene::imported::nativeDistalForearmRoll(invalid,1,2)<0,"off-axis helper accepted");
        invalid=driver;invalid.meshes[0].viewmodelWeapon=true;
        check(scene::imported::nativeDistalForearmRoll(invalid,1,2)<0,"weapon weights selected arm helper");
        invalid=driver;invalid.skeleton.bones.push_back(helper);invalid.meshes[0].vertices[0].bones[0]=id+1;
        check(scene::imported::nativeDistalForearmRoll(invalid,1,2)<0,"ambiguous helpers selected by ordering");
        invalid=driver;invalid.skeleton.bones.push_back(helper);for(auto& v:invalid.meshes[0].vertices)v.bones[1]=id+1;
        invalid.nativePoseFollowers.emplace_back(id+1,id);
        check(scene::imported::nativeDistalForearmRoll(invalid,1,2)==int(id),"native follower weights lost");
    }
    {auto driver=makeHands(false);auto helper=driver.skeleton.bones[0];helper.name="shoulder_twist";helper.parent=0;
        const auto id=static_cast<uint32_t>(driver.skeleton.bones.size());driver.skeleton.bones.push_back(helper);
        for(auto& v:driver.meshes[0].vertices){v.position={.5f,-8,0};v.bones={id,0,0,0};v.weights={1,0,0,0};}
        auto fitted=driver.meshes;for(auto& v:fitted[0].vertices){v.bones={0,0,0,0};v.weights={1,0,0,0};}
        const auto original=fitted;
        check(scene::imported::nativeProximalDiagnostic(driver,fitted,0,1)==0,"proximal diagnostic enabled by default");
        check(scene::imported::nativeProximalDiagnostic(driver,fitted,0,1,true)==3,"proximal samples did not transfer");
        check(fitted[0].vertices[0].bones[0]==id,"proximal helper not retained");
        auto coherent=original;check(scene::imported::nativeProximalDiagnostic(driver,coherent,0,1,true,true)==0,"degenerate coherent ring accepted");
        check(coherent[0].vertices[0].bones==original[0].vertices[0].bones,"coherent rejection did not restore weights");
        coherent=original;for(int i=0;i<3;++i)coherent[0].vertices[i].position.y+=i*.1f;
        check(scene::imported::nativeProximalDiagnostic(driver,coherent,0,1,true,true)==3,"narrow coherent section rejected");
        check(coherent[0].vertices[0].weights==coherent[0].vertices[2].weights,"coherent section does not share weights");
        auto transition=original;for(int i=0;i<3;++i)transition[0].vertices[i].position.y+=i*.1f;
        auto neighbor=transition[0].vertices[0];neighbor.position.x=5;neighbor.bones={0,1,0,0};neighbor.weights={.5f,.5f,0,0};
        transition[0].vertices.push_back(neighbor);transition[0].vertices.push_back(neighbor);
        transition[0].indices.insert(transition[0].indices.end(),{0,1,3});
        check(scene::imported::nativeProximalDiagnostic(driver,transition,0,1,true,true,true)==4,"connected proximal transition count");
        float helperWeight=0,elbowWeight=0;for(int k=0;k<4;++k){if(transition[0].vertices[3].bones[k]==id)helperWeight+=transition[0].vertices[3].weights[k];if(transition[0].vertices[3].bones[k]==1)elbowWeight+=transition[0].vertices[3].weights[k];}
        check(std::abs(helperWeight-.5f)<1e-5f&&std::abs(elbowWeight-.5f)<1e-5f,"transition discarded distal influence");
        check(transition[0].vertices[4].bones==neighbor.bones&&transition[0].vertices[4].weights==neighbor.weights,"transition crossed disconnected component");
        auto torsoDriver=driver;auto torso=driver.skeleton.bones[0];torso.name="torso";torso.parent=-1;const auto torsoId=static_cast<uint32_t>(torsoDriver.skeleton.bones.size());torsoDriver.skeleton.bones.push_back(torso);
        for(auto& v:torsoDriver.meshes[0].vertices){v.bones={id,torsoId,0,0};v.weights={.5f,.5f,0,0};}
        auto local=original;for(int i=0;i<3;++i)local[0].vertices[i].position.y+=i*.1f;
        check(scene::imported::nativeProximalDiagnostic(torsoDriver,local,0,1,true,true,true,true)==3,"local shoulder candidate rejected");
        for(const auto& v:local[0].vertices){check(v.bones[0]==id&&std::abs(v.weights[0]-1.f)<1e-5f,"local shoulder transfer retained torso influence");}
        for(std::size_t m=0;m<fitted.size();++m){check(fitted[m].indices==original[m].indices,"proximal transfer changed topology");for(std::size_t i=0;i<fitted[m].vertices.size();++i)check(scene::length(fitted[m].vertices[i].position-original[m].vertices[i].position)<1e-6f,"proximal transfer changed geometry");}
        check(fitted[1].vertices[0].weights==original[1].vertices[0].weights&&fitted[1].vertices[0].bones==original[1].vertices[0].bones,"proximal transfer changed opposite/distal hand");
    }
    return failures?1:0;}
