#include "scene/ImportedWorldGrip.h"
#include "scene/ImportedBodyRetarget.h"
#include "app/WorldWeaponAssembly.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include "SupportGripDiagnostic.h"
#include "SourceHandednessDiagnostic.h"
#include "NativeGraspEvidence.h"
#include "FullGraspDiagnostic.h"

// Five inputs are CPU-only. Optional rifle/pistol + folder enables paired GPU
// renders; production callers and their calibration policy are not changed.
int main(int argc,char** argv){try{
    if(argc!=6&&argc!=8){std::cerr<<"Expected view idle donor hold target [rifle|pistol output]\n";return 2;}
    for(int i=1;i<=5;++i)if(!cast::Document::load(argv[i]).valid())return 2;
    const bool eligibility=argc==8&&std::string_view(argv[6])=="--grasp-eligibility";
    const bool fullGrasp=std::getenv("CADENCE_AUDIT_FULL_GRASP")!=nullptr;
    auto source=scene::buildScene(cast::Document::load(argv[1]));
    if(eligibility||fullGrasp)if(const auto* hands=std::getenv("CADENCE_AUDIT_ASSEMBLY_HANDS")){
        scene::CastScene complete;std::string error;
        if(!scene::imported::assemblePrepared(std::move(source),scene::buildScene(cast::Document::load(hands)),complete,error)){std::cerr<<"Native assembly failed: "<<error<<'\n';return 2;}
        source=std::move(complete);std::cout<<"native_assembly_hands="<<hands<<'\n';
    }
    scene::appendAnimations(cast::Document::load(argv[2]),source);
    auto donor=scene::buildScene(cast::Document::load(argv[3]),false);scene::appendAnimations(cast::Document::load(argv[4]),donor);
    const auto target=scene::buildScene(cast::Document::load(argv[5]),false);
    if(source.animations.empty()||donor.animations.empty())return 2;
    auto sourcePose=source.samplePose(0,0);const auto donorPose=donor.samplePose(0,0);
    const float units=source.importedTranslationScale==1.f?2.54f:1.f;
    int failures{};float maxMatrix{},maxPoint{};
    std::cout<<std::setprecision(9);
    if(eligibility){
        const auto layout=scene::imported::viewLayout(source);
        bool complete=true;std::array<scene::Vec3,2> contacts{};
        std::cout<<"bones="<<source.skeleton.bones.size()<<" meshes="<<source.meshes.size()<<" tracks="<<source.animations[0].tracks.size()<<" muzzles="<<source.muzzleAnchors.size()<<'\n';
        for(int side=0;side<2;++side){const auto& arm=layout.arms[side];bool valid=arm.wrist>=0&&arm.elbow>=0;
            for(int digit:{1,2,4})valid=valid&&arm.fingers[digit][0]>=0&&arm.fingers[digit][1]>=0;
            if(!valid){complete=false;std::cout<<"side="<<side<<" incomplete_arm=1\n";continue;}
            const auto at=[&](int b){return scene::transformPoint(sourcePose[b],{});};const auto rest=[&](int b){return scene::transformPoint(source.skeleton.bones[b].restGlobal,{});};
            contacts[side]=(at(arm.wrist)+at(arm.fingers[2][0]))*.5f;
            const auto n=scene::normalize(scene::cross(rest(arm.fingers[1][0])-rest(arm.wrist),rest(arm.fingers[4][0])-rest(arm.wrist)));
            const auto bend=scene::dot(rest(arm.fingers[2][1])-rest(arm.fingers[2][0]),n);
            std::vector<bool> owned(source.skeleton.bones.size());owned[arm.wrist]=true;for(const auto& digit:arm.fingers)for(int b:digit)if(b>=0)owned[b]=true;
            std::size_t weighted{};for(const auto& mesh:source.meshes)for(const auto& v:mesh.vertices){float w{};for(int k=0;k<4;++k)if(v.bones[k]<owned.size()&&owned[v.bones[k]])w+=v.weights[k];weighted+=w>.5f;}
            complete=complete&&weighted>0&&scene::length(n)>.99f;
            std::cout<<"side="<<side<<" wrist="<<source.skeleton.bones[arm.wrist].name<<" weighted_vertices="<<weighted<<" rest_middle_bend_signed="<<bend<<" palm_xyz="<<contacts[side].x<<','<<contacts[side].y<<','<<contacts[side].z<<'\n';
        }
        if(complete)inspectNativeGraspGeometry(source,sourcePose,contacts);
        bool rightRear=false;bool axisKnown=false;
        if(complete&&source.muzzleAnchors.size()==1){const auto& anchor=source.muzzleAnchors[0];if(anchor.bone<sourcePose.size()){
            const auto motion=sourcePose[anchor.bone]*source.skeleton.bones[anchor.bone].inverseBind;const auto o=scene::transformPoint(motion,{});
            // Only established Source -Y bind-barrel convention is accepted.
            // Halo marker position alone does not establish its forward axis.
            axisKnown=assets::imported::sourceFamily(scene::imported::gameForPath(argv[1]));
            if(axisKnown){const auto axis=scene::normalize(scene::transformPoint(motion,{0,-1,0})-o),tip=scene::transformPoint(sourcePose[anchor.bone],anchor.local);const float l=scene::dot(contacts[0]-tip,axis),r=scene::dot(contacts[1]-tip,axis);rightRear=r+1.f<l&&r<0&&l<0;std::cout<<"left_behind_tip="<<l<<" right_behind_tip="<<r<<'\n';}
        }}
        std::cout<<"complete_weighted_grasp="<<complete<<" barrel_axis_known="<<axisKnown<<" named_right_rear="<<rightRear<<" candidate_requires_visual_anatomy_review="<<(complete&&axisKnown&&rightRear&&!source.dualWield)<<" no_full_grasp_transfer_performed=1\n";
        return 0;
    }
    for(bool contacts:{false,true})for(bool muzzle:{false,true})for(bool rifle:{false,true}){
        auto reference=donor.skeleton;
        if(!contacts)for(const auto name:{"j_mid_ri_0","j_mid_ri_1","j_mid_le_0","j_mid_le_1"})reference.boneByName.erase(name);
        const auto compact=scene::imported::world_grip::deriveViewHoldConvention(reference,donorPose);
        if(!compact){++failures;continue;}
        auto a=source,b=source;if(!muzzle){a.muzzleAnchors.clear();b.muzzleAnchors.clear();}
        const auto old=scene::imported::world_grip::fromViewHold(a,sourcePose,target.skeleton,units,&reference,donorPose,rifle);
        const auto next=scene::imported::world_grip::fromViewHold(b,sourcePose,target.skeleton,units,nullptr,{},rifle,&*compact);
        bool valid=old&&next&&old->bone==next->bone&&a.meshes.size()==b.meshes.size();float matrix{},point{};
        if(valid){for(int k=0;k<16;++k)matrix=std::max(matrix,std::abs(old->transform.v[k]-next->transform.v[k]));
            for(std::size_t m=0;m<a.meshes.size();++m){if(a.meshes[m].vertices.size()!=b.meshes[m].vertices.size()){valid=false;break;}for(const auto& v:a.meshes[m].vertices){const auto p=scene::transformPoint(a.meshes[m].modelTransform,v.position);point=std::max(point,scene::length(scene::transformPoint(old->transform,p)-scene::transformPoint(next->transform,p)));}}
        }
        valid=valid&&matrix<=1e-4f&&point<=.001f;if(!valid)++failures;
        maxMatrix=std::max(maxMatrix,matrix);maxPoint=std::max(maxPoint,point);
        std::cout<<"contacts="<<contacts<<" muzzle="<<muzzle<<" rifle="<<rifle<<" matrix="<<matrix<<" point_cm="<<point<<" pass="<<valid<<'\n';
        if(contacts&&muzzle&&rifle){std::cout<<"convention camera=";for(float f:compact->camera.v)std::cout<<f<<',';std::cout<<"\nspanValid="<<compact->spanValid<<" socketValid="<<compact->socketValid<<" contacts="<<compact->contacts<<'\n';for(int k=0;k<2;++k){std::cout<<"span"<<k<<'=';for(float f:compact->span[k].v)std::cout<<f<<',';std::cout<<"\nsocket"<<k<<'=';for(float f:compact->socket[k].v)std::cout<<f<<',';std::cout<<'\n';}}
    }
    if(argc==8&&failures==0){
        const bool landmarks=std::string_view(argv[6])=="--landmarks";
        const std::string kind=landmarks?argv[7]:argv[6];if(kind!="rifle"&&kind!="pistol")return 2;
        auto actor=target;std::string error;
        // Diagnostic mesh isolation only; never changes production assembly.
        if(const auto* filter=std::getenv("CADENCE_AUDIT_HAND_LAYER")){
            const std::string mode=filter;
            if(mode=="glove"||mode=="bare")std::erase_if(actor.meshes,[&](const auto& mesh){const auto n=assets::imported::lower(mesh.name);return mode=="glove"?n.starts_with("hands/"):n.starts_with("glove/");});
            std::cout<<"diagnostic_hand_layer="<<mode<<'\n';
        }
        const auto hasGeometry=[](const scene::CastScene& s){return std::any_of(s.meshes.begin(),s.meshes.end(),[](const auto& m){return !m.vertices.empty()&&m.indices.size()>=3;});};
        if(!hasGeometry(actor)){std::cerr<<"Missing target body geometry\n";return 2;}
        if(std::filesystem::path(argv[3])==std::filesystem::path(argv[5])){
            scene::appendAnimations(cast::Document::load(argv[4]),actor);
        }else if(scene::imported::bodyLayout(actor.skeleton)){
            if(!scene::imported::appendRetargetedBody(actor,std::make_shared<scene::CastScene>(donor),0,error)){std::cerr<<error;return 2;}
        }else scene::appendAnimations(cast::Document::load(argv[4]),actor);
        if(actor.animations.empty())return 2;
        if(fullGrasp)if(const auto* path=std::getenv("CADENCE_AUDIT_BODY_CLIP")){
            auto moving=scene::buildScene(cast::Document::load(argv[3]),false);scene::appendAnimations(cast::Document::load(path),moving);if(moving.animations.empty())return 2;
            if(scene::imported::bodyLayout(actor.skeleton)){if(!scene::imported::appendRetargetedBody(actor,std::make_shared<scene::CastScene>(moving),0,error))return 2;}
            else scene::appendAnimations(cast::Document::load(path),actor);
            std::cout<<"moving_body_clip="<<path<<'\n';
        }
        const auto actorPose=actor.samplePose(actor.animations.size()-1,0);
        if(landmarks){
            const auto sourceLayout=scene::imported::bodyOrCodLayout(donor.skeleton),targetLayout=scene::imported::bodyOrCodLayout(actor.skeleton);
            std::array<scene::Mat4,2> nativePalm,targetPalm;
            for(int side=0;side<2;++side){const auto a=scene::imported::bodyPalmRestFrame(donor.skeleton,side),b=scene::imported::bodyPalmRestFrame(actor.skeleton,side);if(!a||!b)return 2;
                nativePalm[side]=donorPose[sourceLayout.hand[side]]*scene::inverseAffine(donor.skeleton.bones[sourceLayout.hand[side]].restGlobal)**a;
                targetPalm[side]=actorPose[targetLayout.hand[side]]*scene::inverseAffine(actor.skeleton.bones[targetLayout.hand[side]].restGlobal)**b;
            }
            const auto expected=targetPalm[1]*scene::inverseAffine(nativePalm[1])*nativePalm[0];
            const auto errorLocal=scene::transformPoint(scene::inverseAffine(targetPalm[1]),scene::transformPoint(targetPalm[0],{}))-scene::transformPoint(scene::inverseAffine(nativePalm[1]),scene::transformPoint(nativePalm[0],{}));
            float angular{};for(int axis=0;axis<3;++axis){scene::Vec3 a{expected.v[axis*4],expected.v[axis*4+1],expected.v[axis*4+2]},b{targetPalm[0].v[axis*4],targetPalm[0].v[axis*4+1],targetPalm[0].v[axis*4+2]};angular=std::max(angular,std::acos(std::clamp(scene::dot(scene::normalize(a),scene::normalize(b)),-1.f,1.f))*180.f/scene::kPi);}
            std::cout<<"support_error_cm="<<scene::length(errorLocal)<<" right_palm_local_xyz="<<errorLocal.x<<','<<errorLocal.y<<','<<errorLocal.z<<" support_palm_axis_error_deg="<<angular<<'\n';
            const auto srcHand=scene::imported::world_grip::rightPalm(donor.skeleton),dstHand=scene::imported::world_grip::rightPalm(actor.skeleton);
            if(srcHand.middle>=0&&dstHand.middle>=0)std::cout<<"source_half_middle_span_cm="<<scene::length(scene::transformPoint(donorPose[srcHand.middle],{})-scene::transformPoint(donorPose[srcHand.wrist],{}))*.5f<<" target_half_middle_span_cm="<<scene::length(scene::transformPoint(actorPose[dstHand.middle],{})-scene::transformPoint(actorPose[dstHand.wrist],{}))*.5f<<'\n';
            std::vector<bool> weighted(actor.skeleton.bones.size());for(const auto& mesh:actor.meshes)for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<weighted.size())weighted[v.bones[k]]=true;
            const auto local=actor.sampleLocalPose(actor.animations.size()-1,0);const auto adapter=actor.animations.back().coldWarWorldPose;int fingers{},mapped{};float restDelta{};
            for(std::size_t b=0;b<actor.skeleton.bones.size();++b){const auto n=assets::imported::lower(actor.skeleton.bones[b].name);bool finger=false;for(const auto* word:{"finger","index","thumb","middle","pinky","ring","j_mid_"})finger=finger||n.find(word)!=std::string::npos;if(!finger||!weighted[b])continue;++fingers;if(!adapter||(b<adapter->sourceBones.size()&&adapter->sourceBones[b]>=0))++mapped;
                const auto& x=local[b].rotation;const auto& y=actor.skeleton.bones[b].restLocal.rotation;const float d=std::abs(x.x*y.x+x.y*y.y+x.z*y.z+x.w*y.w);restDelta=std::max(restDelta,2*std::acos(std::clamp(d,0.f,1.f))*180.f/scene::kPi);
            }
            std::cout<<"weighted_finger_bones="<<fingers<<" mapped_finger_bones="<<mapped<<" maximum_finger_rotation_from_bind_deg="<<restDelta<<'\n';
            const auto donorLocal=donor.sampleLocalPose(0,0);int donorFingers{},donorTracked{};float donorDelta{};
            for(std::size_t b=0;b<donor.skeleton.bones.size();++b){const auto n=assets::imported::lower(donor.skeleton.bones[b].name);bool finger=false;for(const auto* word:{"index","thumb","middle","pinky","ring","j_mid_"})finger=finger||n.find(word)!=std::string::npos;if(!finger)continue;++donorFingers;
                if(std::any_of(donor.animations[0].tracks.begin(),donor.animations[0].tracks.end(),[&](const auto& t){return t.boneIndex==b&&t.property==scene::TrackProperty::Rotation;}))++donorTracked;
                const auto& x=donorLocal[b].rotation;const auto& y=donor.skeleton.bones[b].restLocal.rotation;const float d=std::abs(x.x*y.x+x.y*y.y+x.z*y.z+x.w*y.w);donorDelta=std::max(donorDelta,2*std::acos(std::clamp(d,0.f,1.f))*180.f/scene::kPi);
            }
            std::cout<<"donor_finger_bones="<<donorFingers<<" donor_rotation_tracked="<<donorTracked<<" donor_maximum_finger_rotation_from_bind_deg="<<donorDelta<<'\n';
            if(adapter){auto old=std::make_shared<scene::ColdWarWorldPose>(*adapter);for(std::size_t b=0;b<old->fingerBindings.size();++b)if(old->fingerBindings[b].sourceParent>=0)old->sourceBones[b]=-1;old->fingerBindings.clear();
                double checksum{};const auto measure=[&](const scene::ColdWarWorldPose& binding){const auto start=std::chrono::steady_clock::now();for(int i=0;i<1000;++i){const auto p=binding.sample(actor.skeleton,float(i%100)*.01f*actor.animations.back().durationFrames);checksum+=p.back().rotation.w;}return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/1000.;};
                for(int repeat=0;repeat<3;++repeat){const double previous=measure(*old),current=measure(*adapter);std::cout<<"finger_sample_repeat="<<repeat<<" baseline_us="<<previous<<" fingers_us="<<current<<" delta_us="<<current-previous<<'\n';}
                const auto original=old->sample(actor.skeleton,0),updated=adapter->sample(actor.skeleton,0);float mainDelta{};for(std::size_t b=0;b<updated.size();++b)if(b>=adapter->fingerBindings.size()||adapter->fingerBindings[b].sourceParent<0){const auto& a=original[b];const auto& z=updated[b];mainDelta=std::max(mainDelta,scene::length(a.position-z.position));mainDelta=std::max({mainDelta,std::abs(a.rotation.x-z.rotation.x),std::abs(a.rotation.y-z.rotation.y),std::abs(a.rotation.z-z.rotation.z),std::abs(a.rotation.w-z.rotation.w)});}std::cout<<"nonfinger_local_pose_delta="<<mainDelta<<" sample_checksum="<<checksum<<'\n';if(mainDelta>1e-5f)++failures;
            }
            return failures?1:0;
        }
        const auto rawSource=source;const auto rawPose=sourcePose;
        bool reflectedConstruction=false;
        const auto inputRole=assets::imported::role(scene::imported::gameForPath(argv[1]),std::filesystem::path(argv[1]).stem().string());
        if(std::getenv("CADENCE_AUDIT_REFLECTED_CONSTRUCTION")&&kind=="rifle"&&inputRole==assets::Role::ViewWeapon&&!source.dualWield){
            const auto layout=scene::imported::viewLayout(source);const auto l=layout.arms[0].wrist,r=layout.arms[1].wrist;
            if(l>=0&&r>=0&&source.skeleton.bones[l].name=="cs_hand_L.palm"&&source.skeleton.bones[r].name=="cs_hand_R.palm"&&scene::transformPoint(sourcePose[l],{}).x+1<scene::transformPoint(sourcePose[r],{}).x){
                if(!diagnostic::reflectNativeConstruction(source,sourcePose))return 2;reflectedConstruction=true;
                const auto a=scene::resolveMuzzlePosition(rawSource,rawPose),b=scene::resolveMuzzlePosition(source,sourcePose);
                if(!a||!b)return 2;const auto error=scene::length(scene::Vec3{a->x,-a->y,a->z}-*b);
                std::cout<<"construction_reflected=1 muzzle_reflection_error="<<error<<'\n';if(error>.0001f)++failures;
                const auto vertexAt=[](const scene::CastScene& rig,const std::vector<scene::Mat4>& p,const scene::Mesh& mesh,const scene::Vertex& v){
                    if(!mesh.skinned)return scene::transformPoint(mesh.modelTransform,v.position);
                    scene::Vec3 result{};float total{};for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<p.size()){result=result+scene::transformPoint(p[v.bones[k]]*rig.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];total+=v.weights[k];}
                    return scene::transformPoint(mesh.modelTransform,total>0?result/total:v.position);
                };
                float maxSkinError{},maxDeterminantError{};
                for(std::size_t m=0;m<source.meshes.size();++m)for(std::size_t v=0;v<source.meshes[m].vertices.size();++v){auto p=vertexAt(rawSource,rawPose,rawSource.meshes[m],rawSource.meshes[m].vertices[v]);p.y=-p.y;const auto q=vertexAt(source,sourcePose,source.meshes[m],source.meshes[m].vertices[v]);maxSkinError=std::max(maxSkinError,scene::length(p-q));}
                for(std::size_t i=0;i<sourcePose.size();++i)maxDeterminantError=std::max(maxDeterminantError,std::abs(diagnostic::determinant3(sourcePose[i])-diagnostic::determinant3(rawPose[i])));
                std::cout<<"reflected_skin_point_error="<<maxSkinError<<" bone_determinant_change="<<maxDeterminantError<<'\n';if(maxSkinError>.0001f||maxDeterminantError>.0001f)++failures;
            }
        }
        if(!reflectedConstruction)std::cout<<"construction_reflected=0 unchanged_control=1\n";
        const auto compact=scene::imported::world_grip::deriveViewHoldConvention(donor.skeleton,donorPose);if(!compact)return 2;
        auto first=source,second=source;
        auto live=scene::imported::world_grip::fromViewHold(first,sourcePose,actor.skeleton,units,&donor.skeleton,donorPose,kind=="rifle");
        auto derived=scene::imported::world_grip::fromViewHold(second,sourcePose,actor.skeleton,units,nullptr,{},kind=="rifle",&*compact);
        if(!live||!derived||!hasGeometry(first)||!hasGeometry(second)){std::cerr<<"Missing prepared weapon geometry or mount\n";return 2;}
        const auto productionMount=*live;
        if(fullGrasp){
            if(source.importedViewGame!="eldewrito"||source.dualWield)return 2;
            const int root=scene::imported::world_grip::explicitGunAnchor(source.skeleton);const auto arm=scene::imported::viewLayout(source).arms[1];
            const auto hand=scene::imported::world_grip::rightPalm(actor.skeleton);const auto palm=scene::imported::world_grip::palmFrame(actor.skeleton,scene::imported::world_grip::restPose(actor.skeleton));
            if(root<0||arm.wrist<0||arm.fingers[2][0]<0||!palm||!compact->socketValid)return 2;
            const auto contact=(scene::transformPoint(sourcePose[arm.wrist],{})+scene::transformPoint(sourcePose[arm.fingers[2][0]],{}))*.5f;
            auto authored=scene::imported::world_grip::rigid(sourcePose[root]);if(!authored)return 2;authored->v[12]=contact.x;authored->v[13]=contact.y;authored->v[14]=contact.z;
            live->transform=scene::inverseAffine(actor.skeleton.bones[hand.wrist].restGlobal)**palm*compact->socket[1]*scene::scale({units,units,units})*scene::inverseAffine(*authored);*derived=*live;
            std::cout<<"full_grasp_authored_root_calibration=1 different_from_production=1\n";
        }
        if(reflectedConstruction){
            // Reflection changes the local axis convention too. Transport the
            // actual ORIGINAL barrel direction, not the helper's fixed -Y.
            const auto arms=scene::imported::viewLayout(source);const auto& hand=arms.arms[1];
            const auto targetHand=scene::imported::world_grip::rightPalm(actor.skeleton);const auto targetPalm=scene::imported::world_grip::palmFrame(actor.skeleton,scene::imported::world_grip::restPose(actor.skeleton));
            if(hand.wrist<0||hand.fingers[2][0]<0||!targetPalm||!compact->socketValid||rawSource.muzzleAnchors.empty())return 2;
            const auto barrel=rawSource.muzzleAnchors.front().bone;if(barrel>=rawPose.size())return 2;
            const auto motion=rawPose[barrel]*rawSource.skeleton.bones[barrel].inverseBind;const auto o=scene::transformPoint(motion,{});
            auto forward=scene::transformPoint(motion,{0,-1,0})-o,up=scene::transformPoint(motion,{0,0,1})-o;forward.y=-forward.y;up.y=-up.y;
            const auto trigger=(scene::transformPoint(sourcePose[hand.wrist],{})+scene::transformPoint(sourcePose[hand.fingers[2][0]],{}))*.5f;
            const auto frame=scene::imported::world_grip::gripSpanFrame(trigger,trigger+forward,up);if(!frame)return 2;
            live->transform=scene::inverseAffine(actor.skeleton.bones[targetHand.wrist].restGlobal)**targetPalm*compact->socket[1]*scene::scale({units,units,units})*scene::inverseAffine(*frame);
            derived->transform=live->transform;
            std::cout<<"reflected_mount_determinant="<<diagnostic::determinant3(live->transform)<<" explicit_reflected_barrel_axis=1\n";
            if(diagnostic::determinant3(live->transform)<=0)++failures;
        }
        const bool contactPrototype=std::getenv("CADENCE_AUDIT_TARGET_CONTACT")!=nullptr;
        const bool rightContactOnly=std::getenv("CADENCE_AUDIT_RIGHT_CONTACT_ONLY")!=nullptr;
        const bool physicalContact=std::getenv("CADENCE_AUDIT_PHYSICAL_CONTACT")!=nullptr;
        const bool supportPrototype=std::getenv("CADENCE_AUDIT_SUPPORT_IK")!=nullptr;
        const bool orientationPrototype=std::getenv("CADENCE_AUDIT_SUPPORT_ORIENTATION")!=nullptr;
        if(contactPrototype){
            // Diagnostic-only change: compare against the exact live mount,
            // not compact-convention rounding, and retain all linear terms.
            *derived=*live;
            const auto a=scene::imported::world_grip::rightPalm(donor.skeleton),b=scene::imported::world_grip::rightPalm(actor.skeleton);
            const auto ap=scene::imported::bodyPalmRestFrame(donor.skeleton,1),bp=scene::imported::bodyPalmRestFrame(actor.skeleton,1);
            const auto layout=scene::imported::viewLayout(source);
            const auto left=layout.arms[0].wrist,right=layout.arms[1].wrist;
            const bool rearLeft=left>=0&&right>=0&&scene::transformPoint(sourcePose[left],{}).x+1.f<scene::transformPoint(sourcePose[right],{}).x;
            const bool fullContacts=layout.arms[0].fingers[2][0]>=0&&layout.arms[1].fingers[2][0]>=0&&a.index>=0&&a.pinky>=0&&b.index>=0&&b.pinky>=0;
            bool physicalValid=false;
            if(physicalContact&&kind=="rifle"&&fullContacts&&left>=0&&right>=0&&!source.dualWield&&source.muzzleAnchors.size()==1){
                const auto& anchor=source.muzzleAnchors.front();
                if(anchor.bone<sourcePose.size()){
                    const auto motion=sourcePose[anchor.bone]*source.skeleton.bones[anchor.bone].inverseBind;
                    const auto origin=scene::transformPoint(motion,{});
                    const auto forward=scene::normalize(scene::transformPoint(motion,{0,-1,0})-origin);
                    const auto muzzle=scene::transformPoint(sourcePose[anchor.bone],anchor.local);
                    const auto contact=[&](int side){const auto& hand=layout.arms[side];return (scene::transformPoint(sourcePose[hand.wrist],{})+scene::transformPoint(sourcePose[hand.fingers[2][0]],{}))*.5f;};
                    const auto l=scene::dot(contact(0)-muzzle,forward),r=scene::dot(contact(1)-muzzle,forward);
                    float front=-INFINITY;for(const auto& mesh:first.meshes)for(const auto& v:mesh.vertices)front=std::max(front,scene::dot(scene::transformPoint(mesh.modelTransform,v.position)-muzzle,forward));
                    const int geometricRear=l<r?0:1;
                    const bool currentSwap=rearLeft&&source.skeleton.bones[left].name=="cs_hand_L.palm"&&source.skeleton.bones[right].name=="cs_hand_R.palm";
                    physicalValid=scene::length(forward)>.99f&&std::isfinite(front)&&std::abs(front)*units<2.f&&std::abs(l-r)*units>2.f&&l<0&&r<0&&geometricRear==(currentSwap?0:1);
                    if(std::getenv("CADENCE_AUDIT_AMBIGUOUS_CONTROL")||std::getenv("CADENCE_AUDIT_DUAL_CONTROL"))physicalValid=false;
                    std::cout<<"physical_rear="<<geometricRear<<" current_rear="<<(currentSwap?0:1)<<" left_from_tip_cm="<<l*units<<" right_from_tip_cm="<<r*units<<" geometry_tip_delta_cm="<<front*units<<" physical_valid="<<physicalValid<<'\n';
                }
            }
            const bool eligible=physicalContact?physicalValid:(!rightContactOnly||(kind=="rifle"&&!rearLeft&&!source.dualWield&&fullContacts));
            std::cout<<"right_contact_only="<<rightContactOnly<<" rear_left="<<rearLeft<<" full_contacts="<<fullContacts<<" eligible="<<eligible<<'\n';
            if(left>=0&&right>=0)std::cout<<"source_left_wrist="<<source.skeleton.bones[left].name<<" source_right_wrist="<<source.skeleton.bones[right].name<<'\n';
            scene::Vec3 delta{};
            if(eligible&&kind=="rifle"&&a.middle>=0&&b.middle>=0&&ap&&bp){
                const auto midpoint=[](const scene::Skeleton& s,int wrist,int middle){return (scene::transformPoint(s.bones[wrist].restGlobal,{})+scene::transformPoint(s.bones[middle].restGlobal,{}))*.5f;};
                const auto ac=scene::transformPoint(scene::inverseAffine(*ap),midpoint(donor.skeleton,a.wrist,a.middle));
                const auto bc=scene::transformPoint(scene::inverseAffine(*bp),midpoint(actor.skeleton,b.wrist,b.middle));
                const auto frame=scene::inverseAffine(actor.skeleton.bones[b.wrist].restGlobal)**bp;
                delta=scene::transformPoint(frame,bc-ac)-scene::transformPoint(frame,{});
                derived->transform=scene::translation(delta)*live->transform;
                std::cout<<"donor_contact_palm_cm="<<ac.x<<','<<ac.y<<','<<ac.z<<" target_contact_palm_cm="<<bc.x<<','<<bc.y<<','<<bc.z<<'\n';
            }
            std::cout<<"contact_prototype="<<kind<<" wrist_local_delta_cm="<<delta.x<<','<<delta.y<<','<<delta.z<<" length_cm="<<scene::length(delta)<<'\n';
            for(int k=0;k<12;++k)if(derived->transform.v[k]!=live->transform.v[k]){std::cerr<<"Contact prototype changed linear transform\n";return 2;}
            if(!eligible||kind!="rifle")for(int k=0;k<16;++k)if(derived->transform.v[k]!=live->transform.v[k]){std::cerr<<"No-op control changed transform\n";return 2;}
        }
        if(physicalContact){
            if(first.meshes.size()!=second.meshes.size()||first.skeleton.bones.size()!=second.skeleton.bones.size())return 2;
            for(std::size_t b=0;b<first.skeleton.bones.size();++b)if(first.skeleton.bones[b].inverseBind.v!=second.skeleton.bones[b].inverseBind.v)return 2;
            for(std::size_t m=0;m<first.meshes.size();++m){const auto& a=first.meshes[m];const auto& b=second.meshes[m];if(a.vertices.size()!=b.vertices.size()||a.indices!=b.indices||a.modelTransform.v!=b.modelTransform.v)return 2;for(std::size_t v=0;v<a.vertices.size();++v){const auto& x=a.vertices[v];const auto& y=b.vertices[v];if(x.position.x!=y.position.x||x.position.y!=y.position.y||x.position.z!=y.position.z||x.normal.x!=y.normal.x||x.normal.y!=y.normal.y||x.normal.z!=y.normal.z||x.weights!=y.weights||x.bones!=y.bones)return 2;}}
            std::cout<<"weapon_geometry_bind_invariants=exact\n";
        }
        auto actors=std::array<scene::CastScene,2>{actor,actor};
        scene::appendPreparedAttachment(std::move(first),actors[0],live->bone,"live donor");scene::appendPreparedAttachment(std::move(second),actors[1],derived->bone,"compact convention");
        if(actors[0].attachments.empty()||actors[1].attachments.empty())return 2;
        cadence::world_weapon::setTransform(actors[0].attachments.back(),live->transform);cadence::world_weapon::setTransform(actors[1].attachments.back(),derived->transform);
        if(supportPrototype)cadence::world_weapon::setTransform(actors[0].attachments.back(),derived->transform);
        std::array<std::vector<scene::Mat4>,2> comparisonPoses{actorPose,actorPose};
        std::optional<scene::CastScene> productionControl;
        if(fullGrasp){
            productionControl=actors[0];cadence::world_weapon::setTransform(productionControl->attachments.back(),productionMount.transform);
            const auto frozen=actorPose[live->bone]*live->transform;
            const bool applied=diagnostic::fullNativeGrasp(source,sourcePose,frozen,actor.skeleton,comparisonPoses[1]);
            if(applied)cadence::world_weapon::setTransform(actors[1].attachments.back(),scene::inverseAffine(comparisonPoses[1][derived->bone])*frozen);
            float worldError{};const auto restored=comparisonPoses[1][derived->bone]*actors[1].attachments.back().localMatrix();for(int k=0;k<16;++k)worldError=std::max(worldError,std::abs(restored.v[k]-frozen.v[k]));
            std::cout<<"full_grasp_applied="<<applied<<" fixed_weapon_world_error="<<worldError<<'\n';if(worldError>.001f)++failures;
            if(!applied)for(std::size_t b=0;b<actorPose.size();++b)if(comparisonPoses[1][b].v!=actorPose[b].v)++failures;
            const auto samePose=[](const auto& a,const auto& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].v!=b[i].v)return false;return true;};
            for(int negative=0;negative<4;++negative){auto checkPose=actorPose;auto checkSource=source;auto checkRig=actor.skeleton;auto checkWorld=frozen;
                if(negative==0)checkWorld=scene::translation({10000,0,0})*frozen;
                if(negative==1)checkWorld=scene::scale({-1,1,1})*frozen;
                if(negative==2){const auto fingers=scene::imported::bodyFingerChains(checkRig);const int bone=fingers[0][2][1];if(bone<0)return 2;const auto name=checkRig.bones[bone].name;checkRig.boneByName.erase(name);checkRig.boneByCanonicalName.erase(assets::imported::lower(name));checkRig.bones[bone].name="diagnostic_absent_digit";}
                if(negative==3)checkSource.dualWield=true;
                const bool rejected=!diagnostic::fullNativeGrasp(checkSource,sourcePose,checkWorld,checkRig,checkPose)&&samePose(checkPose,actorPose);std::cout<<"full_grasp_negative="<<negative<<" exact_noop="<<rejected<<'\n';if(!rejected)++failures;
            }
            if(std::getenv("CADENCE_AUDIT_BODY_CLIP")){std::vector<scene::Mat4> previous;int solvedFrames{},transitions{};bool previousSolved{};float maximumStep{};const auto layout=scene::imported::bodyOrCodLayout(actor.skeleton);
                for(int sample=0;sample<25;++sample){const float frame=actor.animations.back().durationFrames*float(sample)/24.f;auto pose=actor.samplePose(actor.animations.size()-1,frame);const auto original=pose;const auto movingWorld=pose[live->bone]*live->transform;const bool solved=diagnostic::fullNativeGrasp(source,sourcePose,movingWorld,actor.skeleton,pose);solvedFrames+=solved;if(sample&&solved!=previousSolved)++transitions;if(!solved&&!samePose(pose,original))++failures;
                    if(!previous.empty())for(int side=0;side<2;++side)maximumStep=std::max(maximumStep,scene::length(scene::transformPoint(pose[layout.hand[side]],{})-scene::transformPoint(previous[layout.hand[side]],{})));previous=std::move(pose);previousSolved=solved;}
                std::cout<<"moving_grasp_solved="<<solvedFrames<<" total=25 solve_state_transitions="<<transitions<<" maximum_sampled_wrist_step_cm="<<maximumStep<<'\n';
            }
        }
        const auto muzzle0=scene::resolveMuzzlePosition(actors[0],comparisonPoses[0]),muzzle1=scene::resolveMuzzlePosition(actors[1],comparisonPoses[1]);
        const float muzzleDifference=muzzle0&&muzzle1?scene::length(*muzzle0-*muzzle1):INFINITY;
        if(physicalContact&&muzzle0&&muzzle1){const auto transformDelta=scene::transformPoint(actorPose[live->bone],{derived->transform.v[12]-live->transform.v[12],derived->transform.v[13]-live->transform.v[13],derived->transform.v[14]-live->transform.v[14]})-scene::transformPoint(actorPose[live->bone],{});const auto error=scene::length((*muzzle1-*muzzle0)-transformDelta);std::cout<<"muzzle_translation_error_cm="<<error<<'\n';if(error>.001f)++failures;}
        std::cout<<"render_muzzle_difference_cm="<<muzzleDifference<<'\n';if(!std::isfinite(muzzleDifference)||(!contactPrototype&&muzzleDifference>.001f))++failures;
        std::array<std::array<scene::Vec3,2>,2> weaponContacts{},bodyContacts{};bool contactsValid=false;
        std::optional<scene::Mat4> desiredSupportWrist;scene::Vec3 supportLocalContact{};
        if(contactPrototype){const auto arms=scene::imported::viewLayout(source);const auto body=scene::imported::bodyOrCodLayout(actor.skeleton);const auto fingers=scene::imported::bodyFingerChains(actor.skeleton);int grip=1;
            if(kind=="rifle"&&arms.arms[0].wrist>=0&&arms.arms[1].wrist>=0&&source.skeleton.bones[arms.arms[0].wrist].name=="cs_hand_L.palm"&&source.skeleton.bones[arms.arms[1].wrist].name=="cs_hand_R.palm"&&scene::transformPoint(sourcePose[arms.arms[0].wrist],{}).x+1.f<scene::transformPoint(sourcePose[arms.arms[1].wrist],{}).x)grip=0;
            contactsValid=true;for(int hand=0;hand<2;++hand){const auto& arm=arms.arms[hand?grip:1-grip];const int wrist=body.hand[hand],middle=fingers[hand][2][0];if(wrist<0||middle<0||arm.wrist<0||arm.fingers[2][0]<0){contactsValid=false;break;}
                const auto restContact=(scene::transformPoint(actor.skeleton.bones[wrist].restGlobal,{})+scene::transformPoint(actor.skeleton.bones[middle].restGlobal,{}))*.5f;
                bodyContacts[0][hand]=bodyContacts[1][hand]=scene::transformPoint(actorPose[wrist]*scene::inverseAffine(actor.skeleton.bones[wrist].restGlobal),restContact);
                const auto sourceContact=(scene::transformPoint(sourcePose[arm.wrist],{})+scene::transformPoint(sourcePose[arm.fingers[2][0]],{}))*.5f;
                if(hand==0&&orientationPrototype&&arm.fingers[1][0]>=0&&arm.fingers[4][0]>=0){const auto targetPalm=scene::imported::bodyPalmRestFrame(actor.skeleton,0);
                    const auto point=[&](int bone){return scene::transformPoint(source.skeleton.bones[bone].restGlobal,{});};const auto origin=point(arm.wrist),forward=point(arm.fingers[2][0])-origin,normal=scene::cross(point(arm.fingers[1][0])-origin,point(arm.fingers[4][0])-origin);
                    if(targetPalm&&scene::length(forward)>.001f&&scene::length(normal)>.001f){const auto sourcePalm=scene::imported::viewAnatomicalFrame(origin,origin+forward,normal);const auto animatedPalm=sourcePose[arm.wrist]*scene::inverseAffine(source.skeleton.bones[arm.wrist].restGlobal)*sourcePalm;
                        const auto worldPalm=scene::imported::world_grip::rigid(actorPose[derived->bone]*derived->transform*animatedPalm);
                        if(worldPalm){desiredSupportWrist=*worldPalm*scene::inverseAffine(*targetPalm)*actor.skeleton.bones[wrist].restGlobal;supportLocalContact=scene::transformPoint(scene::inverseAffine(actor.skeleton.bones[wrist].restGlobal),restContact);}
                    }
                }
                for(int side=0;side<2;++side){const auto& mount=(side||supportPrototype)?*derived:*live;weaponContacts[side][hand]=scene::transformPoint(actorPose[mount.bone]*mount.transform,sourceContact);std::cout<<"contact_side="<<side<<" hand="<<(hand?"trigger":"support")<<" error_cm="<<scene::length(weaponContacts[side][hand]-bodyContacts[side][hand])<<'\n';}
            }
        }
        if(supportPrototype&&contactsValid){const auto layout=scene::imported::bodyOrCodLayout(actor.skeleton);const auto oldWrist=scene::transformPoint(actorPose[layout.hand[0]],{});const auto goal=oldWrist+weaponContacts[1][0]-bodyContacts[1][0];
            const auto solve=diagnostic::placeSupport(actor.skeleton,comparisonPoses[1],goal);
            if(solve.applied)bodyContacts[1][0]=bodyContacts[1][0]+scene::transformPoint(comparisonPoses[1][layout.hand[0]],{})-oldWrist;
            std::cout<<"support_ik="<<solve.reason<<" upper_length_error="<<solve.upperError<<" lower_length_error="<<solve.lowerError<<" wrist_linear_error="<<solve.wristLinearError<<" unrelated_error="<<solve.unrelatedError<<" support_after_cm="<<scene::length(weaponContacts[1][0]-bodyContacts[1][0])<<'\n';
            if(!solve.applied)for(std::size_t b=0;b<actorPose.size();++b)for(int k=0;k<16;++k)if(comparisonPoses[1][b].v[k]!=actorPose[b].v[k])++failures;
            if(orientationPrototype&&desiredSupportWrist){comparisonPoses[0]=comparisonPoses[1];bodyContacts[0]=bodyContacts[1];comparisonPoses[1]=actorPose;
                const auto vector=scene::transformPoint(*desiredSupportWrist,supportLocalContact)-scene::transformPoint(*desiredSupportWrist,{});const auto orientedGoal=weaponContacts[1][0]-vector;
                const auto oriented=diagnostic::placeSupport(actor.skeleton,comparisonPoses[1],orientedGoal,&*desiredSupportWrist);
                if(oriented.applied)bodyContacts[1][0]=scene::transformPoint(comparisonPoses[1][layout.hand[0]],supportLocalContact);
                else{comparisonPoses[1]=comparisonPoses[0];bodyContacts[1]=bodyContacts[0];}
                float angle{};for(int axis=0;axis<3;++axis){const auto& a=comparisonPoses[0][layout.hand[0]];const auto& b=*desiredSupportWrist;const auto x=scene::normalize(scene::Vec3{a.v[axis*4],a.v[axis*4+1],a.v[axis*4+2]}),y=scene::normalize(scene::Vec3{b.v[axis*4],b.v[axis*4+1],b.v[axis*4+2]});angle=std::max(angle,std::acos(std::clamp(scene::dot(x,y),-1.f,1.f))*180.f/scene::kPi);}
                std::cout<<"support_orientation="<<oriented.reason<<" maximum_wrist_axis_change_deg="<<angle<<" support_after_cm="<<scene::length(weaponContacts[1][0]-bodyContacts[1][0])<<" upper_error="<<oriented.upperError<<" lower_error="<<oriented.lowerError<<" linear_error="<<oriented.wristLinearError<<" unrelated_error="<<oriented.unrelatedError<<'\n';
            }
        }
        const auto folder=std::filesystem::path(argv[7]);std::filesystem::create_directories(folder);
        if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(1000,750,"Convention comparison",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
        render::StageRenderer renderer;if(!renderer.initialize(error)){std::cerr<<error;return 2;}
        const auto checkGl=[&](const char* stage){for(GLenum code=glGetError();code!=GL_NO_ERROR;code=glGetError()){std::cerr<<"GL error "<<code<<" at "<<stage<<'\n';++failures;}};
        checkGl("initialize");
        renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
        const auto center=scene::transformPoint(actorPose[live->bone],{})+scene::Vec3{12,0,0};
        for(int angle=0;angle<(contactPrototype||fullGrasp?5:3);++angle){std::array<std::vector<std::uint8_t>,2> pixels;
            const auto focus=angle>=3?(contactsValid?bodyContacts[0][1]:scene::transformPoint(actorPose[live->bone],{})):center;
            const auto eye=focus+(angle==0?scene::Vec3{100,-140,65}:angle==1?scene::Vec3{100,140,65}:angle==2?scene::Vec3{20,-65,30}:angle==3?scene::Vec3{25,35,10}:scene::Vec3{-25,-35,10});
            const auto vp=scene::perspective(48*scene::kPi/180,1000.f/750.f,.1f,2000)*scene::lookAt(eye,focus,{0,0,1});
            for(int side=0;side<2;++side){if(!renderer.loadScene(actors[side],error)){std::cerr<<error;return 2;}renderer.render(actors[side],comparisonPoses[side],vp,1000,750,false,false,false);
                const auto muzzle=side?muzzle1:muzzle0;if(muzzle)renderer.renderDebugLine3D(*muzzle-scene::Vec3{0,0,3},*muzzle+scene::Vec3{0,0,3},{0,1,0,1},vp);
                if(contactsValid)for(int hand=0;hand<2;++hand){const auto a=bodyContacts[side][hand],b=weaponContacts[side][hand];renderer.renderDebugLine3D(a,b,{1,.3f,.1f,1},vp);renderer.renderDebugLine3D(a-scene::Vec3{0,0,1},a+scene::Vec3{0,0,1},{0,1,1,1},vp);renderer.renderDebugLine3D(b-scene::Vec3{0,1,0},b+scene::Vec3{0,1,0},{1,1,0,1},vp);}
                if(!renderer.saveColorPng(folder/((side?(fullGrasp?"full_grasp_":supportPrototype?"support_ik_":contactPrototype?"target_contact_":"compact_"):"live_")+std::to_string(angle)+".png"),error)||!renderer.readColorRgba(pixels[side],error)){std::cerr<<error;return 2;}
                checkGl("render/readback");
            }
            if(productionControl){if(!renderer.loadScene(*productionControl,error))return 2;renderer.render(*productionControl,actorPose,vp,1000,750,false,false,false);if(!renderer.saveColorPng(folder/("helper_call_"+std::to_string(angle)+".png"),error))return 2;checkGl("helper-call control (not runtime path)");}
            std::size_t changed{};int maximum{};std::uint64_t total{};
            if(pixels[0].size()!=1000*750*4||pixels[0].size()!=pixels[1].size()){++failures;continue;}
            for(std::size_t i=0;i<pixels[0].size();++i){const int d=std::abs(int(pixels[0][i])-int(pixels[1][i]));changed+=d>0;maximum=std::max(maximum,d);total+=d;}
            std::cout<<"render_angle="<<angle<<" changed_channels="<<changed<<" maximum_channel_delta="<<maximum<<" mean_channel_delta="<<double(total)/pixels[0].size()<<'\n';
        }
        if(contactPrototype){auto nativeSource=rawSource;auto handsPath=std::filesystem::path(argv[1]).parent_path()/"hands_shared.cast";
            if(const auto* explicitHands=std::getenv("CADENCE_AUDIT_NATIVE_HANDS"))handsPath=explicitHands;
            if(std::filesystem::is_regular_file(handsPath)){for(auto& m:nativeSource.meshes)m.viewmodelWeapon=true;const auto hands=scene::buildScene(cast::Document::load(handsPath));if(!scene::imported::fitForeignViewSkin(nativeSource,hands,error)){std::cerr<<"Native shared-hand reference unavailable: "<<error<<'\n';return 2;}std::cout<<"native_hand_reference="<<handsPath.string()<<'\n';}
            auto nativePose=rawPose;if(reflectedConstruction&&!diagnostic::reflectNativeConstruction(nativeSource,nativePose))return 2;
            const auto arms=scene::imported::viewLayout(source);int wrist=arms.arms[reflectedConstruction?0:1].wrist;int middle=arms.arms[reflectedConstruction?0:1].fingers[2][0];
            if(wrist>=0&&middle>=0){const auto focus=(scene::transformPoint(sourcePose[wrist],{})+scene::transformPoint(sourcePose[middle],{}))*.5f;const auto distance=std::max(10.f,scene::length(scene::transformPoint(sourcePose[wrist],{})-scene::transformPoint(sourcePose[middle],{}))*10.f);
                if(!renderer.loadScene(nativeSource,error)){std::cerr<<error;return 2;}
                const bool mirrorAudit=std::getenv("CADENCE_AUDIT_NATIVE_MIRROR")!=nullptr;
                auto reflection=scene::Mat4::identity();reflection.v[5]=-1.f;
                for(int angle=0;angle<(mirrorAudit?3:2);++angle){
                    const auto aim=angle==2?scene::Vec3{focus.x,0,focus.z}:focus;
                    const auto eye=angle==2?aim+scene::Vec3{-distance,0,0}:focus+scene::Vec3{distance*.5f,distance*(angle?1.f:-1.f),distance*.5f};
                    const auto vp=scene::perspective(48*scene::kPi/180,1000.f/750.f,.05f,2000)*scene::lookAt(eye,aim,{0,0,1});
                    std::array<std::vector<std::uint8_t>,2> nativePixels;
                    for(int mirrored=0;mirrored<(mirrorAudit?2:1);++mirrored){
                        // Diagnostic display transform of the WHOLE source,
                        // not a wrist correction or a production normalization.
                        // Fullbright only: this does not validate tangent shading.
                        const auto view=mirrored?vp*reflection:vp;
                        GLint winding{};glGetIntegerv(GL_FRONT_FACE,&winding);
                        if(mirrored)glFrontFace(winding==GL_CCW?GL_CW:GL_CCW);
                        renderer.render(nativeSource,nativePose,view,1000,750,false,false,false);
                        if(physicalContact){for(int side=0;side<2;++side){const auto& arm=arms.arms[side];if(arm.wrist>=0&&arm.fingers[2][0]>=0){const auto p=(scene::transformPoint(sourcePose[arm.wrist],{})+scene::transformPoint(sourcePose[arm.fingers[2][0]],{}))*.5f;renderer.renderDebugLine3D(p-scene::Vec3{0,0,.5f},p+scene::Vec3{0,0,.5f},side?scene::Vec4{0,1,1,1}:scene::Vec4{1,.3f,0,1},view);}}}
                        glFrontFace(winding);
                        renderer.renderDebugLine3D(focus-scene::Vec3{0,0,1},focus+scene::Vec3{0,0,1},{1,1,0,1},view);
                        const std::string prefix=mirrored?"native_mirrored_":"native_view_";
                        if(!renderer.saveColorPng(folder/(prefix+std::to_string(angle)+".png"),error)||!renderer.readColorRgba(nativePixels[mirrored],error)){std::cerr<<error;return 2;}
                        checkGl("native source handedness render");
                    }
                    if(mirrorAudit&&angle==2){std::uint64_t sum{};int maximum{};std::size_t changed{};
                        for(int y=0;y<750;++y)for(int x=0;x<1000;++x)for(int c=0;c<4;++c){const int d=std::abs(int(nativePixels[0][(y*1000+x)*4+c])-int(nativePixels[1][(y*1000+999-x)*4+c]));sum+=d;maximum=std::max(maximum,d);changed+=d>0;}
                        std::cout<<"whole_source_y_reflection determinant=-1 target_mount_unchanged=1 flipped_pixel_mean="<<double(sum)/(1000*750*4)<<" maximum="<<maximum<<" changed="<<changed<<'\n';
                    }
                }
            }
        }
        renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();
    }
    std::cout<<"maximum_matrix="<<maxMatrix<<" maximum_point_cm="<<maxPoint<<" failures="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
