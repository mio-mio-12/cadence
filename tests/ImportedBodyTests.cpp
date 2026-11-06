#include "scene/ImportedBody.h"
#include "scene/ImportedBodyRetarget.h"
#include "scene/ImportedBodyMotion.h"
#include "gameplay/BodyPresentation.h"
#include "app/ImportedBodyDonorCompatibility.h"
#include <iostream>
#include <stdexcept>
#include <limits>
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void coveragePolicyChecks(){
    using namespace cadence::body_donor;
    for(const auto name:{"j_bridge","j_brow_in_le","j_lip_upper_out_ri","j_tongue"}){
        check(optionalStaticFace("bo2",name,1),"Static optional facial default");
        check(!optionalStaticFace("bo2",name,2),"Animated face must retain coverage check");
        check(!optionalStaticFace("blacksquad",name,1),"Other rig identity must retain coverage check");
    }
    for(const auto name:{"j_mainroot","j_spinelower","j_head","j_wrist_le","j_elbow_ri","j_knee_le","j_ankle_ri"}){
        check(!optionalStaticFace("bo2",name,1),"Body channel never optional");
        check(requiredBodyChannel("bo2",name),"Missing body channel rejects donor despite percentage");
    }
    check(!optionalStaticFace("bo2","j_bridge",0),"Malformed/empty curve not optional");
}
static std::size_t bone(scene::Skeleton& s,const char* name,int parent=-1){scene::Bone b;b.name=name;b.parent=parent;const auto index=s.bones.size();s.bones.push_back(b);s.boneByName[name]=index;return index;}
static scene::CastScene bodyFixture(bool halo,float scale,bool upper=false){
    scene::CastScene s;const auto add=[&](const char* h,const char* cod,int parent,scene::Vec3 p){const auto i=bone(s.skeleton,halo?h:cod,parent);auto& b=s.skeleton.bones[i];b.restLocal.position=p*scale;b.restGlobal=(parent>=0?s.skeleton.bones[parent].restGlobal:scene::Mat4::identity())*scene::translation(b.restLocal.position);b.inverseBind=scene::inverseAffine(b.restGlobal);s.skeleton.boneByCanonicalName[b.name]=i;return static_cast<int>(i);};
    add("pelvis","pelvis",-1,{0,0,2});add("spine","j_spinelower",0,{0,0,1});
    int neck=1;if(upper){const int chest=add("spine1","j_spineupper",1,{0,0,.3f});neck=add("neck","j_neck",chest,{0,0,.3f});}add("head","j_head",neck,{0,0,1});
    for(int side=0;side<2;++side){const float sign=side?-1.f:1.f;
        const int collar=upper?add(side?"r_clavicle":"l_clavicle",side?"j_clavicle_ri":"j_clavicle_le",neck,{0,sign*.2f,0}):1;
        const auto arm=add(side?"r_upperarm":"l_upperarm",side?"j_shoulder_ri":"j_shoulder_le",collar,{0,sign,0});const auto elbow=add(side?"r_forearm":"l_forearm",side?"j_elbow_ri":"j_elbow_le",arm,{1,0,0});add(side?"r_hand":"l_hand",side?"j_wrist_ri":"j_wrist_le",elbow,{1,0,0});const auto hip=add(side?"r_thigh":"l_thigh",side?"j_hip_ri":"j_hip_le",0,{0,sign,0});const auto knee=add(side?"r_calf":"l_calf",side?"j_knee_ri":"j_knee_le",hip,{0,0,-1});add(side?"r_foot":"l_foot",side?"j_ankle_ri":"j_ankle_le",knee,{0,0,-1});}
    scene::Animation a;a.domain=scene::AnimationDomain::PlayerBody;a.durationFrames=10;scene::Track tx;tx.boneIndex=0;tx.property=scene::TrackProperty::TranslationX;tx.frames={0,10};tx.scalarValues={0,scale};a.tracks.push_back(tx);s.animations.push_back(a);return s;
}
static void playerGaitTransportChecks(){
    using scene::MotionRole;using scene::ActionRole;
    for(const auto family:{scene::imported::BodyFamily::Halo,scene::imported::BodyFamily::Source,scene::imported::BodyFamily::DirtyBomb}){
        auto model=bodyFixture(true,1);
        if(family!=scene::imported::BodyFamily::Halo){
            const std::string prefix=family==scene::imported::BodyFamily::Source?"ValveBiped.Bip01_":"SkelBone_";
            model.skeleton.boneByName.clear();
            for(std::size_t i=0;i<model.skeleton.bones.size();++i){auto& b=model.skeleton.bones[i];
                for(const auto& names:{std::pair{"pelvis","Pelvis"},std::pair{"spine","Spine"},std::pair{"head","Head"},std::pair{"l_thigh","L_Thigh"},std::pair{"r_thigh","R_Thigh"}})
                    if(b.name==names.first){b.name=prefix+names.second;break;}
                model.skeleton.boneByName[b.name]=i;
            }
        }
        check(scene::imported::bodyLayout(model.skeleton).family==family,"player gait native fixture recognized");
        std::vector<scene::Mat4> original;for(const auto& b:model.skeleton.bones){auto pose=b.restGlobal*scene::rotation(scene::fromEulerRadians({.2f,-.1f,.4f}));pose.v[12]+=37;pose.v[13]-=21;pose.v[14]+=5;original.push_back(pose);}
        for(const auto motion:{MotionRole::Idle,MotionRole::Walk,MotionRole::Run,MotionRole::Sprint,MotionRole::Crawl,MotionRole::Jump,MotionRole::Land}){
            auto pose=original;
            check(gameplay::presentation::suppressPlayerGaitTransport(model.skeleton,pose,motion,ActionRole::None),"native player gait owns XY transport");
            for(std::size_t b=0;b<pose.size();++b)for(int component=0;component<16;++component){
                const float expected=original[b].v[component]-(component==12?37.f:component==13?-21.f:0.f);
                check(std::abs(pose[b].v[component]-expected)<.0001f,"gait suppression preserves native height, rotation, scale and relative bones");
            }
        }
        for(const auto motion:{MotionRole::Unknown,MotionRole::Climb,MotionRole::Slide,MotionRole::Ladder}){
            auto pose=original;check(!gameplay::presentation::suppressPlayerGaitTransport(model.skeleton,pose,motion,ActionRole::None),"non-gait traversal keeps transport ownership");
            for(std::size_t b=0;b<pose.size();++b)for(int k=0;k<16;++k)check(pose[b].v[k]==original[b].v[k],"non-gait native pose unchanged");
        }
        for(const auto action:{ActionRole::Death,ActionRole::Fire,ActionRole::Reload,ActionRole::Gesture}){
            auto pose=original;check(!gameplay::presentation::suppressPlayerGaitTransport(model.skeleton,pose,MotionRole::Run,action),"authored action keeps transport ownership");
            for(std::size_t b=0;b<pose.size();++b)for(int k=0;k<16;++k)check(pose[b].v[k]==original[b].v[k],"action native pose unchanged");
        }
    }
    const auto cod=bodyFixture(false,1);std::vector<scene::Mat4> pose;for(const auto& b:cod.skeleton.bones)pose.push_back(scene::translation({37,-21,5})*b.restGlobal);
    const auto before=pose;
    check(!gameplay::presentation::suppressPlayerGaitTransport(cod.skeleton,pose,MotionRole::Run,ActionRole::None),"canonical CoD retains existing root policy");
    for(std::size_t b=0;b<pose.size();++b)for(int k=0;k<16;++k)check(pose[b].v[k]==before[b].v[k],"canonical CoD pose unchanged");
}
static void presentationFacingChecks(){
    for(float authoredYaw:{0.f,-scene::kPi*.5f,scene::kPi*.5f}){
        auto model=bodyFixture(true,1);const auto rotation=scene::rotation(scene::fromEulerRadians({0,0,authoredYaw}));
        for(auto& b:model.skeleton.bones)b.restGlobal=rotation*b.restGlobal;
        const auto correction=scene::imported::bodyPresentationYaw(model.skeleton);
        check(std::abs(correction+authoredYaw)<1e-5f,"authored forward compensation wrong sign");
        const auto forward=scene::imported::bodyLayout(model.skeleton).authoredForward;
        for(float aiYaw:{0.f,.63f,-2.1f}){const auto world=scene::rotation(scene::fromEulerRadians({0,0,aiYaw+correction}));
            const auto actual=scene::transformPoint(world,forward);
            check(scene::length(actual-scene::Vec3{std::cos(aiYaw),std::sin(aiYaw),0})<1e-5f,"rendered body forward differs from AI yaw");
            // Preserve an authored turn inside the clip; only the static model
            // coordinate frame is removed, not its animated root rotation.
            const auto turned=world*scene::rotation(scene::fromEulerRadians({0,0,.25f}));
            check(scene::length(scene::transformPoint(turned,forward)-scene::Vec3{std::cos(aiYaw+.25f),std::sin(aiYaw+.25f),0})<1e-5f,"presentation cancelled authored root turn");
        }
        for(float heading:{0.f,.7f,-1.4f}){
            const scene::Vec3 simulation{10,20,30},display{8,19,29};
            const auto recorded=gameplay::presentation::bodyWorldTransform(model.skeleton,simulation,display,false,heading,2);
            const auto rendered=gameplay::presentation::bodyWorldTransform(model.skeleton,simulation,display,true,heading,2);
            check(scene::length(scene::transformPoint(recorded,{})-scene::Vec3{10,20,32})<1e-5f,"body record position changed");
            check(scene::length(scene::transformPoint(rendered,{})-scene::Vec3{8,19,31})<1e-5f,"body render interpolation changed");
            for(int i=0;i<12;++i)check(recorded.v[i]==rendered.v[i],"render sample changed body orientation");
            const auto forward=scene::imported::bodyLayout(model.skeleton).authoredForward;
            const auto direction=scene::transformPoint(recorded,forward)-scene::transformPoint(recorded,{});
            check(scene::length(direction-scene::Vec3{std::cos(heading),std::sin(heading),0})<1e-5f,"player world facing differs from controller heading");
            const auto local=scene::translation({1,2,3})*scene::rotation(scene::fromEulerRadians({.1f,.2f,.3f}));
            const auto restored=scene::inverseAffine(recorded)*(recorded*local);
            for(int i=0;i<16;++i)check(std::abs(restored.v[i]-local.v[i])<1e-4f,"blocked mantle local pose roundtrip changed");
        }
        model.meshes.resize(2);model.meshes[0].actorVariant=0;model.meshes[1].actorVariant=1;
        bone(model.skeleton,"weapon_attachment_root");model.skeleton.bones.back().restGlobal=scene::translation({999,999,999});
        check(scene::imported::bodyPresentationYaw(model.skeleton)==correction,"variant or weapon changed shared bind facing");
    }
    const auto cod=bodyFixture(false,1);check(scene::imported::bodyPresentationYaw(cod.skeleton)==0,"canonical CoD presentation changed");
}
static void retargetChecks(){
    for(bool splitParent:{false,true}){
        auto target=bodyFixture(false,1,true);target.animations.clear();
        if(splitParent){
            auto parent=target.skeleton.bones[0];parent.name="coincident_deform_parent";
            parent.restLocal.rotation=scene::fromEulerRadians({0,0,.17f});
            parent.restGlobal=scene::trs(parent.restLocal.position,parent.restLocal.rotation,parent.restLocal.scale);
            parent.inverseBind=scene::inverseAffine(parent.restGlobal);
            target.skeleton.bones.insert(target.skeleton.bones.begin(),parent);
            target.skeleton.boneByName.clear();target.skeleton.boneByCanonicalName.clear();
            for(std::size_t i=0;i<target.skeleton.bones.size();++i){auto& b=target.skeleton.bones[i];
                if(i){b.parent=b.parent<0?0:b.parent+1;
                    // The spine branches from the deform parent, not pelvis,
                    // matching CODM's Bip01 -> {b_Hips,b_Spine2} hierarchy.
                    if(b.name=="j_spinelower")b.parent=0;
                    const auto local=scene::inverseAffine(target.skeleton.bones[b.parent].restGlobal)*b.restGlobal;
                    scene::decomposeAffine(local,b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);
                }
                target.skeleton.boneByName[b.name]=i;target.skeleton.boneByCanonicalName[b.name]=i;
            }
        }
        const auto originalBind=target.skeleton;
        auto donor=std::make_shared<scene::CastScene>(bodyFixture(true,1,true));
        donor->animations[0].tracks[0].scalarValues={0,0};
        scene::Track yaw;yaw.boneIndex=0;yaw.property=scene::TrackProperty::Rotation;yaw.frames={0,10};
        yaw.rotationValues={scene::fromEulerRadians({0,0,.61f}),scene::fromEulerRadians({0,0,.61f})};
        donor->animations[0].tracks.push_back(yaw);
        auto torso=donor->animations[0];torso.domain=scene::AnimationDomain::PlayerTorso;torso.action=scene::ActionRole::Fire;
        auto spine=yaw;spine.boneIndex=donor->skeleton.boneByName.at("spine");
        spine.rotationValues={scene::fromEulerRadians({.1f,0,0}),scene::fromEulerRadians({.2f,0,0})};torso.tracks.push_back(spine);
        donor->animations.push_back(torso);
        auto reference=torso;reference.domain=scene::AnimationDomain::PlayerBody;donor->animations.push_back(reference);
        auto translated=torso;translated.tracks[0].scalarValues={100,200};donor->animations.push_back(translated);
        std::string error;
        for(std::size_t i=0;i<donor->animations.size();++i)check(scene::imported::appendRetargetedBody(target,donor,i,error),"split-parent torso fixture adapter installs");
        const auto layout=scene::imported::bodyOrCodLayout(target.skeleton);
        const auto upper=[&](std::size_t bone){int b=static_cast<int>(bone);while(b>=0&&b!=layout.spine)b=target.skeleton.bones[b].parent;return b==layout.spine;};
        for(auto index:{1u,3u})for(const auto& track:target.animations[index].tracks){
            check(track.property==scene::TrackProperty::Rotation,"torso emits no ancestor translation tracks");
            check(!track.ownsLayer||upper(track.boneIndex),"torso ancestor and leg rotations never own layer");
        }
        for(auto index:{1u,3u})for(bool translatedBone:target.animations[index].coldWarWorldPose->globalTranslation)
            check(!translatedBone,"torso adapter has no global translation ownership");
        for(float frame:{0.f,2.5f,5.f,9.5f}){
            std::vector<scene::PoseSlot> slots(1);slots[0].nodes.push_back({1,frame,1.f,scene::LayerMode::Override,true});
            const auto combined=target.samplePoseSlots(0,frame,slots),expected=target.samplePose(2,frame);
            const auto baseLocal=target.sampleLocalPose(0,frame),combinedLocal=target.sampleLocalPoseSlots(0,frame,slots);
            for(std::size_t b=0;b<combined.size();++b){
                if(upper(b))for(int k=0;k<16;++k)check(std::abs(combined[b].v[k]-expected[b].v[k])<.001f,"torso spine expressed in same parent frame as body base");
                else{const auto base=scene::trs(baseLocal[b].position,baseLocal[b].rotation,baseLocal[b].scale),actual=scene::trs(combinedLocal[b].position,combinedLocal[b].rotation,combinedLocal[b].scale);
                    for(int k=0;k<16;++k)check(std::abs(base.v[k]-actual.v[k])<.001f,"torso preserves base ancestor pelvis and leg transforms");}
            }
            slots[0].nodes[0].animation=3;const auto contaminated=target.samplePoseSlots(0,frame,slots);
            for(std::size_t b=0;b<combined.size();++b)for(int k=0;k<16;++k)check(std::abs(combined[b].v[k]-contaminated[b].v[k])<.001f,"source torso pelvis translation cannot leak through ancestors");
        }
        // Action pelvis motion must cancel in the local torso frame even when
        // the base is facing elsewhere, including partially blended layers.
        auto movingBase=donor->animations[0];
        movingBase.tracks[1].rotationValues={scene::fromEulerRadians({0,0,.35f}),scene::fromEulerRadians({0,0,.8f})};
        auto differentYaw=donor->animations[1];
        differentYaw.tracks[1].rotationValues={scene::fromEulerRadians({0,0,-.55f}),scene::fromEulerRadians({0,0,-.1f})};
        differentYaw.tracks[2].rotationValues={scene::fromEulerRadians({.12f,.08f,-.07f}),scene::fromEulerRadians({.28f,-.09f,.13f})};
        auto rebasedReference=differentYaw;rebasedReference.domain=scene::AnimationDomain::PlayerBody;
        rebasedReference.tracks[1]=movingBase.tracks[1];
        auto displacedAction=differentYaw;displacedAction.tracks[0].scalarValues={100,200};
        for(const auto& clip:{movingBase,differentYaw,rebasedReference,displacedAction})donor->animations.push_back(clip);
        for(std::size_t i=4;i<8;++i)check(scene::imported::appendRetargetedBody(target,donor,i,error),"different-yaw torso fixture adapter installs");
        for(float frame:{0.f,2.5f,5.f,9.5f})for(float weight:{.25f,.5f,.75f,1.f}){
            const auto base=target.sampleLocalPose(4,frame),referenceLocal=target.sampleLocalPose(6,frame);
            std::vector<scene::PoseSlot> slots(1);slots[0].nodes.push_back({5,frame,weight,scene::LayerMode::Override,true});
            const auto actual=target.sampleLocalPoseSlots(4,frame,slots);
            auto expectedLocal=base;
            for(std::size_t b=0;b<base.size();++b){
                if(upper(b))expectedLocal[b].rotation=scene::slerp(base[b].rotation,referenceLocal[b].rotation,weight);
                const auto expected=scene::trs(expectedLocal[b].position,expectedLocal[b].rotation,expectedLocal[b].scale);
                const auto observed=scene::trs(actual[b].position,actual[b].rotation,actual[b].scale);
                for(int k=0;k<16;++k)check(std::abs(expected.v[k]-observed.v[k])<.001f,"partial torso layer uses base parent frame without action pelvis leakage");
            }
            const auto actualGlobal=target.samplePoseSlots(4,frame,slots),expectedGlobal=target.globalPose(expectedLocal);
            for(std::size_t b=0;b<actualGlobal.size();++b)for(int k=0;k<16;++k)
                check(std::abs(actualGlobal[b].v[k]-expectedGlobal[b].v[k])<.001f,"different-yaw partial torso global pose matches rebased reference");
            slots[0].nodes[0].animation=7;const auto displaced=target.samplePoseSlots(4,frame,slots);
            for(std::size_t b=0;b<actualGlobal.size();++b)for(int k=0;k<16;++k)
                check(std::abs(actualGlobal[b].v[k]-displaced[b].v[k])<.001f,"partial torso ignores source pelvis translation with independent yaw");
        }
        for(std::size_t b=0;b<target.skeleton.bones.size();++b)for(int k=0;k<16;++k)
            check(target.skeleton.bones[b].restGlobal.v[k]==originalBind.bones[b].restGlobal.v[k]&&target.skeleton.bones[b].inverseBind.v[k]==originalBind.bones[b].inverseBind.v[k],"torso sampling preserves target bind matrices");
    }
    {auto target=bodyFixture(false,1);target.animations.clear();
     auto parent=target.skeleton.bones[0];parent.name="deform_pelvis_parent";
     target.skeleton.bones.insert(target.skeleton.bones.begin(),parent);
     target.skeleton.boneByName.clear();target.skeleton.boneByCanonicalName.clear();
     for(std::size_t i=0;i<target.skeleton.bones.size();++i){auto& b=target.skeleton.bones[i];
        if(i){b.parent=b.parent<0?0:b.parent+1;if(i==1)b.restLocal={};}
        target.skeleton.boneByName[b.name]=i;target.skeleton.boneByCanonicalName[b.name]=i;}
     const auto helper=bone(target.skeleton,"hip_accessory",0);auto& h=target.skeleton.bones[helper];h.restLocal.position={0,1,0};h.restGlobal=parent.restGlobal*scene::translation(h.restLocal.position);h.inverseBind=scene::inverseAffine(h.restGlobal);
     auto donor=std::make_shared<scene::CastScene>(bodyFixture(true,1));donor->animations[0].action=scene::ActionRole::Death;
     scene::Track tr;tr.boneIndex=0;tr.property=scene::TrackProperty::Rotation;tr.frames={0,10};tr.rotationValues={{0,0,0,1},scene::fromEulerRadians({1.2f,.4f,.7f})};donor->animations[0].tracks.push_back(tr);
     std::string error;check(scene::imported::appendRetargetedBody(target,donor,0,error),"coincident deform parent adapter installs");
     for(float frame:{0.f,2.5f,5.f,9.5f}){const auto p=target.samplePose(0,frame);for(int k=0;k<16;++k)check(std::abs(p[0].v[k]-p[1].v[k])<.001f,"deform parent follows pelvis during death");
        const auto expected=p[1]*scene::translation({0,1,0});for(int k=0;k<16;++k)check(std::abs(p[helper].v[k]-expected.v[k])<.001f,"hip accessory follows falling pelvis");}
    }
    {auto target=bodyFixture(false,1,true);target.animations.clear();auto donor=std::make_shared<scene::CastScene>(bodyFixture(true,1,true));
     for(const auto name:{"spine1","neck","l_clavicle"}){scene::Track tr;tr.boneIndex=donor->skeleton.boneByName.at(name);tr.property=scene::TrackProperty::Rotation;tr.frames={0,10};tr.rotationValues={{0,0,0,1},scene::fromEulerRadians({.4f,.2f,-.3f})};donor->animations[0].tracks.push_back(tr);}
     std::string error;check(scene::imported::appendRetargetedBody(target,donor,0,error),"upper torso adapter installs");
     for(float frame:{0.f,2.5f,5.f,9.5f}){const auto expected=donor->samplePose(0,frame),actual=target.samplePose(0,frame);
        for(std::size_t i=0;i<actual.size();++i)for(int k=0;k<16;++k)check(std::abs(actual[i].v[k]-expected[i].v[k])<.001f,"upper spine neck and collar motion reaches shoulders and wrists");}
    }
    {auto target=bodyFixture(false,1);target.animations.clear();
     scene::Bone root;root.name="tag_origin";target.skeleton.bones.insert(target.skeleton.bones.begin(),root);
     target.skeleton.boneByName.clear();target.skeleton.boneByCanonicalName.clear();
     for(std::size_t i=0;i<target.skeleton.bones.size();++i){auto& b=target.skeleton.bones[i];if(i)b.parent=b.parent<0?0:b.parent+1;target.skeleton.boneByName[b.name]=i;target.skeleton.boneByCanonicalName[b.name]=i;}
     const auto detached=bone(target.skeleton,"detached_cloth");
     auto donor=std::make_shared<scene::CastScene>(bodyFixture(true,1));donor->animations[0].looping=false;donor->animations[0].motion=scene::MotionRole::Run;
     std::string error;check(scene::imported::appendRetargetedBody(target,donor,0,error),"locomotion adapter installs on CoD root hierarchy");
     auto first=target.samplePose(0,0),last=target.samplePose(0,5);
     check(std::abs(last[0].v[12]-first[0].v[12]-.5f)<.001f,"target motion root receives imported gait travel");
     check(std::abs(last[detached].v[12]-first[detached].v[12]-.5f)<.001f,"detached roots follow same gait travel");
     scene::imported::suppressBodyRootMotion(target.skeleton,first);scene::imported::suppressBodyRootMotion(target.skeleton,last);
     for(std::size_t b=0;b<first.size();++b)check(scene::length(scene::transformPoint(first[b],{})-scene::transformPoint(last[b],{}))<.001f,"in-place evaluation removes travel consistently across all branches");}
    {auto native=bodyFixture(true,1);const auto helper=bone(native.skeleton,"detached_deform");scene::Mesh mesh;mesh.skinned=true;scene::Vertex vertex;vertex.bones[0]=static_cast<unsigned>(helper);mesh.vertices.push_back(vertex);native.meshes.push_back(mesh);
     auto& clip=native.animations[0];clip.looping=true;clip.motion=scene::MotionRole::Run;scene::Track track;track.boneIndex=helper;track.property=scene::TrackProperty::TranslationX;track.frames={0,5,10};track.scalarValues={0,57,100};clip.tracks.push_back(track);auto oneShot=clip;oneShot.looping=false;native.animations.push_back(oneShot);
     scene::imported::repairDetachedBodyRootLoops(native,0);const auto& values=native.animations[0].tracks.back().scalarValues;
     check(std::abs(values.back()-1.f)<.001f,"detached root loop endpoint matches main root travel");check(std::abs(values[1]-7.5f)<.001f,"detached root correction preserves nonlinear gait detail");check(native.animations[1].tracks.back().scalarValues.back()==100.f,"one-shot detached root motion untouched");}
    {auto native=bodyFixture(true,1);auto first=native.samplePose(0,0),last=native.samplePose(0,10);
     check(std::abs(scene::imported::bodyRootTravelSpeed(native.skeleton,first,last,1)-1.f)<.001f,"native root speed without CoD alias");
     last[0].v[14]+=3.f;const auto height=last[0].v[14];scene::imported::suppressBodyRootMotion(native.skeleton,last);
     check(std::abs(last[0].v[12]-first[0].v[12])<.001f&&last[0].v[14]==height,"native horizontal root suppressed while height preserved");
     scene::Skeleton legacy;bone(legacy,"tag_origin");legacy.boneByCanonicalName["tag_origin"]=0;std::vector<scene::Mat4> pose{scene::translation({2,3,4})};scene::imported::suppressBodyRootMotion(legacy,pose);check(pose[0].v[12]==0&&pose[0].v[13]==0&&pose[0].v[14]==0,"legacy root suppression unchanged");}
    {scene::Skeleton s;bone(s,"Root");bone(s,"Bridge",0);bone(s,"Pelvis",1);
     s.boneByCanonicalName["tag_origin"]=0;s.boneByCanonicalName["j_mainroot"]=2;
     for(auto& b:s.bones)b.restGlobal=scene::translation({0,0,94});
     std::vector<scene::Mat4> pose(3,scene::translation({12,8,20}));scene::imported::suppressBodyRootMotion(s,pose);
     for(const auto& p:pose)check(p.v[12]==0&&p.v[13]==0&&p.v[14]==20,"coincident anatomical root retains death collapse without XY travel");
     s.bones[0].restGlobal=scene::Mat4::identity();check(!scene::imported::bodyMotionRoot(s).preserveVertical,"distinct CoD motion origin still suppresses Z");
     s.bones[0].restGlobal=s.bones[2].restGlobal;s.bones[1].restGlobal=scene::Mat4::identity();check(!scene::imported::bodyMotionRoot(s).preserveVertical,"noncoincident intermediate ancestor is not a pelvis root");
     s.bones[1].restGlobal=s.bones[2].restGlobal;s.bones[1].parent=-1;check(!scene::imported::bodyMotionRoot(s).preserveVertical,"unrelated coincident root does not own pelvis height");
     s.boneByCanonicalName["tag_origin"]=2;check(scene::imported::bodyMotionRoot(s).preserveVertical,"pelvis itself retains height");}
    for(bool targetHalo:{false,true}){auto target=bodyFixture(targetHalo,2);auto source=std::make_shared<scene::CastScene>(bodyFixture(!targetHalo,1));const auto aliases=target.skeleton.boneByCanonicalName;std::string error;const auto index=target.animations.size();check(scene::imported::appendRetargetedBody(target,source,0,error),"bidirectional body adapter installation");
        const auto pose=target.sampleLocalPose(index,5);check(std::abs(pose[0].position.x-1.f)<.001f,"root translation scales at fractional frame");check(std::abs(pose[0].position.z-4.f)<.001f,"target pelvis bind height retained");
        for(std::size_t b=1;b<pose.size();++b)check(scene::length(pose[b].position-target.skeleton.bones[b].restLocal.position)<.001f,"target limb lengths retained");check(target.skeleton.boneByCanonicalName==aliases,"retarget does not expose aliases");check(std::abs(source->skeleton.bones[0].restGlobal.v[14]-2.f)<.001f,"source remains immutable");
    }
    auto target=bodyFixture(true,1);auto source=std::make_shared<scene::CastScene>(bodyFixture(false,1));source->animations[0].domain=scene::AnimationDomain::PlayerTorso;const auto count=target.animations.size();std::string error;check(scene::imported::appendRetargetedBody(target,source,0,error),"native torso adapter installs");
    const auto mask=scene::imported::nativeBodyTorsoMask(target.skeleton);bool upperOwned=false;for(const auto& track:target.animations[count].tracks){check(track.property==scene::TrackProperty::Rotation,"torso emits no root translation");check(!track.ownsLayer||mask[track.boneIndex],"torso ownership excludes pelvis and legs");upperOwned|=track.ownsLayer;}
    check(upperOwned&&!target.animations[count].coldWarWorldPose->globalTranslation[0],"torso has upper ownership without global pelvis motion");
}
static void sourceNormalizationChecks(){
    {scene::CastScene s;for(const auto* n:{"pelvis","spine_0","head_0","leg_upper_L","leg_upper_R"})bone(s.skeleton,n);
     s.importedTranslationScale=2.54f;scene::Animation a;scene::Track t;t.boneIndex=0;t.property=scene::TrackProperty::TranslationY;t.frames={0};t.scalarValues={10.f};a.tracks.push_back(t);s.animations.push_back(a);
     scene::imported::normalizeSourceBodyAnimations(s,0);check(std::abs(s.animations[0].tracks[0].scalarValues[0]-25.4f)<.001f,"CS2 body animation translations follow normalized mesh units");
     s.importedTranslationScale=1.f;s.animations[0].tracks[0].scalarValues[0]=10.f;scene::imported::normalizeSourceBodyAnimations(s,0);check(s.animations[0].tracks[0].scalarValues[0]==10.f,"CS2 viewmodel-sized skeleton is not body scaled");}
    {scene::CastScene gold;bone(gold.skeleton,"Bip01");bone(gold.skeleton,"Bip01 Pelvis",0);bone(gold.skeleton,"Bip01 L Thigh",1);bone(gold.skeleton,"Bip01 R Thigh",1);bone(gold.skeleton,"Bip01 Spine",1);bone(gold.skeleton,"Bip01 Head",4);bone(gold.skeleton,"Bip01 L knee");gold.skeleton.bones[0].restLocal.position.z=97.f;gold.importedTranslationScale=2.54f;
     scene::Animation clip;for(int b:{0,1,6}){scene::Track track;track.boneIndex=b;track.property=scene::TrackProperty::TranslationZ;track.mode=scene::TrackMode::Absolute;track.frames={0};track.scalarValues={-4.f};clip.tracks.push_back(track);}gold.animations.push_back(clip);
     scene::classifyAnimationName("pb_generic_crouch_run_forward",clip);gold.animations.push_back(clip);
     auto additive=clip;for(auto& track:additive.tracks)track.mode=scene::TrackMode::Additive;gold.animations.push_back(additive);
     scene::imported::normalizeSourceBodyAnimations(gold,0);
     check(std::abs(gold.animations[1].tracks[0].scalarValues[0]-41.12f)<.001f&&std::abs(gold.animations[1].tracks[2].scalarValues[0]-41.12f)<.001f,"GoldSrc crouch roots restore duck hull origin, including helper roots");
     check(std::abs(gold.animations[1].tracks[1].scalarValues[0]+10.16f)<.001f,"crouch origin correction does not change limb translations");
     check(std::abs(gold.animations[2].tracks[0].scalarValues[0]+10.16f)<.001f,"GoldSrc additive root deltas do not acquire a hull origin");
     check(std::abs(gold.animations[0].tracks[0].scalarValues[0]-86.84f)<.001f&&std::abs(gold.animations[0].tracks[2].scalarValues[0]-86.84f)<.001f,"GoldSrc animation roots restore common standing elevation");check(std::abs(gold.animations[0].tracks[1].scalarValues[0]+10.16f)<.001f,"GoldSrc child translations are not height-shifted");}
    auto s=bodyFixture(true,1);const auto q=scene::fromEulerRadians({0,0,-scene::kPi*.5f});const auto basis=scene::rotation(q);
    const auto rename=[](std::string name){if(name=="pelvis")return std::string("ValveBiped.Bip01_Pelvis");if(name=="spine")return std::string("ValveBiped.Bip01_Spine");if(name=="head")return std::string("ValveBiped.Bip01_Head1");const std::string prefix=name.starts_with("l_")?"ValveBiped.Bip01_L_":"ValveBiped.Bip01_R_";const auto part=name.substr(2);return prefix+(part=="upperarm"?"UpperArm":part=="forearm"?"Forearm":part=="hand"?"Hand":part=="thigh"?"Thigh":part=="calf"?"Calf":"Foot");};
    s.skeleton.boneByName.clear();s.skeleton.boneByCanonicalName.clear();for(std::size_t i=0;i<s.skeleton.bones.size();++i){auto& b=s.skeleton.bones[i];b.name=rename(b.name);b.restGlobal=basis*b.restGlobal;b.inverseBind=scene::inverseAffine(b.restGlobal);if(b.parent<0){b.restLocal.position=scene::transformPoint(basis,b.restLocal.position);b.restLocal.rotation=scene::multiply(q,b.restLocal.rotation);}s.skeleton.boneByName[b.name]=i;s.skeleton.boneByCanonicalName[b.name]=i;}
    s.animations[0].tracks[0].property=scene::TrackProperty::TranslationX;s.animations[0].tracks[0].scalarValues={0,1};
    scene::Track rootRotation;rootRotation.boneIndex=0;rootRotation.property=scene::TrackProperty::Rotation;rootRotation.frames={0};rootRotation.rotationValues={{0,0,0,1}};s.animations[0].tracks.push_back(rootRotation);
    check(scene::imported::normalizeSourceBody(s),"Source body normalizes once");check(!scene::imported::normalizeSourceBody(s),"Source body normalization idempotent");
    scene::imported::normalizeSourceBodyAnimations(s,0);const auto pose=s.sampleLocalPose(0,5);
    check(std::abs(pose[0].position.x-1.27f)<.001f&&std::abs(pose[0].position.y)<.001f,"Source exported +X root translation remains forward at fractional frame");check(std::abs(pose[0].position.z-5.08f)<.001f,"missing root z retains correctly normalized reference");check(std::abs(pose[0].rotation.w-1.f)<.001f,"Source exported root rotation is not normalized twice");
    check(scene::imported::bodyLayout(s.skeleton).authoredForward.x>.999f,"normalized Source body forward inferred without double rotation");
    auto other=s.skeleton;check(scene::imported::sharedBodyBindCompatible(s.skeleton,other),"identical Any body bind compatible");other.bones[1].restGlobal.v[14]+=.2f;check(!scene::imported::sharedBodyBindCompatible(s.skeleton,other),"different Any body proportions rejected");
    auto variant=s;bone(variant.skeleton,"outfit_mesh_container");
    check(scene::imported::sharedBodyAssemblyCompatible(s,variant),"Any permits unused export helper roots");
    check(cadence::body_donor::equivalent(s.skeleton,cadence::body_donor::requiredBones(s),variant.skeleton,cadence::body_donor::requiredBones(variant)),"Donor ignores unused export helper roots");
    {auto reordered=variant;const auto count=reordered.skeleton.bones.size();std::reverse(reordered.skeleton.bones.begin(),reordered.skeleton.bones.end());reordered.skeleton.boneByName.clear();
     for(std::size_t i=0;i<count;++i){auto& b=reordered.skeleton.bones[i];if(b.parent>=0)b.parent=static_cast<int>(count-1-static_cast<std::size_t>(b.parent));reordered.skeleton.boneByName[b.name]=i;}
     for(auto& mesh:reordered.meshes)for(auto& v:mesh.vertices)for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0)v.bones[k]=static_cast<unsigned>(count-1-v.bones[k]);
     check(cadence::body_donor::equivalent(variant.skeleton,cadence::body_donor::requiredBones(variant),reordered.skeleton,cadence::body_donor::requiredBones(reordered)),"Donor equivalence independent of export bone ordering");
     reordered.skeleton.bones.front().inverseBind.v[0]=std::numeric_limits<float>::quiet_NaN();
     check(!cadence::body_donor::equivalent(variant.skeleton,cadence::body_donor::requiredBones(variant),reordered.skeleton,cadence::body_donor::requiredBones(reordered)),"Donor rejects nonfinite shared bind");}
    variant.skeleton.bones[1].restGlobal.v[14]+=.2f;
    check(!scene::imported::sharedBodyAssemblyCompatible(s,variant),"Any still rejects incompatible shared binds");
    check(!cadence::body_donor::equivalent(s.skeleton,cadence::body_donor::requiredBones(s),variant.skeleton,cadence::body_donor::requiredBones(variant)),"Donor rejects incompatible shared binds");
    variant=s;const auto helper=bone(variant.skeleton,"outfit_mesh_container");
    scene::Mesh deform;deform.skinned=true;scene::Vertex vertex;vertex.bones[0]=static_cast<unsigned>(helper);deform.vertices.push_back(vertex);variant.meshes.push_back(deform);
    check(!scene::imported::sharedBodyAssemblyCompatible(s,variant),"Any rejects unmatched weighted bones");
    check(!cadence::body_donor::equivalent(s.skeleton,cadence::body_donor::requiredBones(s),variant.skeleton,cadence::body_donor::requiredBones(variant)),"Donor rejects unmatched weighted bones");
    auto descendant=bone(variant.skeleton,"shared_descendant",static_cast<int>(helper));bone(s.skeleton,"shared_descendant");variant.meshes.back().vertices[0].bones[0]=static_cast<unsigned>(descendant);
    check(!scene::imported::sharedBodyAssemblyCompatible(s,variant),"Any rejects unmatched deform ancestors");
    check(!cadence::body_donor::equivalent(s.skeleton,cadence::body_donor::requiredBones(s),variant.skeleton,cadence::body_donor::requiredBones(variant)),"Donor rejects unmatched deform ancestors");
    scene::Animation death;death.sourceName="pb_generic_stand_idle_death_clip123.cast";check(scene::imported::classifySourceBodyAnimation(death)&&death.action==scene::ActionRole::Death&&death.motion==scene::MotionRole::Unknown&&!death.looping,"Source death never becomes idle");
    scene::Animation shoot;shoot.sourceGame="cs1.6";shoot.sourceName="diagnostic_native_arctic_ref_shoot_ak47_blend4_clip123.cast";
    for(const auto label:{"combat:rifle:walk:front","combat:rifle:walk_front","combat:rifle:move_back","combat:rifle:idle"}){
        scene::Animation gait;gait.name=label;gait.sourceName="native.cast";gait.looping=false;
        const bool valid=scene::imported::classifyDewBodyAnimation(gait);
        if(std::string_view(label)=="combat:rifle:walk:front")check(!valid,"Unknown Halo graph is not guessed into locomotion");
        else check(valid&&gait.looping&&gait.action==scene::ActionRole::None,"Native Halo gait/idle loops despite missing export loop flag");
    }
    for(const auto label:{"combat:any:airborne_dead","combat:rifle:br:fire_1","combat:rifle:br:reload_1"}){
        scene::Animation action;action.name=label;action.sourceName="native.cast";action.looping=true;
        check(scene::imported::classifyDewBodyAnimation(action)&&!action.looping&&action.motion==scene::MotionRole::Unknown,"Halo one-shot actions are not cyclic idle locomotion");
    }
    scene::Animation hold;hold.sourceGame="cs1.6";hold.sourceName="diagnostic_native_arctic_ref_aim_ak47_blend4_clip123.cast";
    check(scene::imported::classifySourceBodyAnimation(hold)&&hold.domain==scene::AnimationDomain::PlayerTorso&&hold.action==scene::ActionRole::None&&hold.motion==scene::MotionRole::Idle&&hold.looping&&!hold.contextual,"Native center aim is looping hold, not ADS/action or body locomotion");
    check(scene::imported::classifySourceBodyAnimation(shoot)&&shoot.domain==scene::AnimationDomain::PlayerTorso&&shoot.action==scene::ActionRole::Fire&&shoot.motion==scene::MotionRole::Unknown&&!shoot.contextual&&!shoot.looping&&shoot.stance==scene::Stance::Stand,"Native shooting is non-contextual torso fire, not idle");
    scene::Animation reload;reload.sourceName="pb_rifle_crouch_idle_reload_clip456.cast";check(scene::imported::classifySourceBodyAnimation(reload)&&reload.domain==scene::AnimationDomain::PlayerTorso&&reload.action==scene::ActionRole::Reload&&reload.stance==scene::Stance::Crouch,"Source reload becomes native torso action");
}
static void palmRetargetChecks(){
    auto target=bodyFixture(true,2);auto source=std::make_shared<scene::CastScene>(bodyFixture(false,1));
    const auto addDigits=[](scene::CastScene& body,bool halo,bool rotate){const auto layout=scene::imported::bodyOrCodLayout(body.skeleton);
        for(int side=0;side<2;++side)for(const auto& digit:{std::pair{"index",scene::Vec3{1,.3f,.05f}},std::pair{"mid",scene::Vec3{1,0,0}},std::pair{"pinky",scene::Vec3{1,-.3f,0}},std::pair{"thumb",scene::Vec3{.5f,.5f,-.1f}}}){
            const auto name=halo?std::string(side?"r_":"l_")+(std::string(digit.first)=="mid"?"middle":digit.first)+"_low":"j_"+std::string(digit.first)+(side?"_ri_1":"_le_1");
            const auto b=bone(body.skeleton,name.c_str(),layout.hand[side]);auto p=digit.second;if(rotate)p={-p.y,p.x,p.z};body.skeleton.bones[b].restLocal.position=p;body.skeleton.bones[b].restGlobal=body.skeleton.bones[layout.hand[side]].restGlobal*scene::translation(p);body.skeleton.bones[b].inverseBind=scene::inverseAffine(body.skeleton.bones[b].restGlobal);
        }};
    addDigits(target,true,true);addDigits(*source,false,false);const auto index=target.animations.size();std::string error;check(scene::imported::appendRetargetedBody(target,source,0,error),"palm reference adapter installs");const auto pose=target.samplePose(index,5);const auto layout=scene::imported::bodyOrCodLayout(target.skeleton);
    for(int side=0;side<2;++side){const auto middle=target.skeleton.boneByName.at(side?"r_middle_low":"l_middle_low");const auto direction=scene::normalize(scene::transformPoint(pose[middle],{})-scene::transformPoint(pose[layout.hand[side]],{}));check(direction.x>.999f,"anatomical palm forward follows donor, not target wrist bind axes");}
    auto partial=bodyFixture(true,1);addDigits(partial,true,false);
    for(const auto* name:{"l_middle_low","r_middle_low","l_pinky_low","r_pinky_low"}){
        const auto b=partial.skeleton.boneByName.at(name);partial.skeleton.boneByName.erase(name);partial.skeleton.boneByCanonicalName.erase(scene::canonicalName(name));partial.skeleton.bones[b].name=std::string("unused_")+name;
    }
    const auto partialIndex=partial.animations.size();check(scene::imported::appendRetargetedBody(partial,source,0,error),"partial palm adapter installs");
    const auto partialPose=partial.samplePose(partialIndex,5),sourcePose=source->samplePose(0,5);
    const auto pl=scene::imported::bodyOrCodLayout(partial.skeleton),sl=scene::imported::bodyOrCodLayout(source->skeleton);
    for(int side=0;side<2;++side)for(int col=0;col<3;++col)for(int row=0;row<3;++row)
        check(std::abs(partialPose[pl.hand[side]].v[col*4+row]-sourcePose[sl.hand[side]].v[col*4+row])<.001f,"missing palm landmarks do not add wrist rotation to identical shared anatomy");
}
static void palmVariantChecks(){
    scene::Skeleton rig;bone(rig,"Bip01 Pelvis");bone(rig,"Bip01 L Thigh",0);bone(rig,"Bip01 R Thigh",0);bone(rig,"Bip01 Spine",0);bone(rig,"Bip01 Head",3);
    for(int side=0;side<2;++side){const std::string prefix=side?"Bip01 R ":"Bip01 L ";const int wrist=static_cast<int>(bone(rig,(prefix+"Hand").c_str(),3));
        for(int digit=0;digit<3;++digit){const auto i=bone(rig,(prefix+"Finger"+std::to_string(digit)+"_variant").c_str(),wrist);rig.bones[i].restGlobal=scene::translation({1.f,digit==0?.5f:digit==1?.2f:0.f,0.f});}
        const auto frame=scene::imported::bodyPalmRestFrame(rig,side);check(frame.has_value()&&frame->v[0]>.999f,"both sided variant finger roots resolve anatomical palm");
    }
    auto cod=bodyFixture(false,1);const auto layout=scene::imported::bodyOrCodLayout(cod.skeleton);
    for(int side=0;side<2;++side)for(const auto& digit:{std::pair{"index",.3f},std::pair{"mid",0.f},std::pair{"pinky",-.3f}})for(int joint=0;joint<2;++joint){const auto name="j_"+std::string(digit.first)+(side?"_ri_":"_le_")+std::to_string(joint);const auto i=bone(cod.skeleton,name.c_str(),layout.hand[side]);cod.skeleton.bones[i].restGlobal=cod.skeleton.bones[layout.hand[side]].restGlobal*scene::translation(joint?scene::Vec3{0,1,digit.second}:scene::Vec3{1,digit.second,0});}
    for(int side=0;side<2;++side){const auto frame=scene::imported::bodyPalmRestFrame(cod.skeleton,side);check(frame.has_value()&&frame->v[0]>.999f,"CoD digit0 root preferred over digit1 knuckle");}
}
static void fingerRetargetChecks(){
    const auto addDigits=[](scene::CastScene& body,bool halo,bool four,bool helper,bool oneBased=false){
        const auto layout=scene::imported::bodyOrCodLayout(body.skeleton);const std::array<const char*,5> names{"thumb","index","middle","ring","pinky"};
        for(int side=0;side<2;++side)for(int f=0;f<5;++f){if(four&&f==2)continue;int parent=layout.hand[side];
            for(int j=0;j<3;++j){if(helper&&j==1){const auto h=bone(body.skeleton,("digit_helper_"+std::to_string(side)+"_"+std::to_string(f)).c_str(),parent);body.skeleton.bones[h].restGlobal=body.skeleton.bones[parent].restGlobal;body.skeleton.bones[h].inverseBind=scene::inverseAffine(body.skeleton.bones[h].restGlobal);parent=int(h);}
                const auto name=halo?std::string(side?"r_":"l_")+names[f]+"_"+(j==0?"low":j==1?"mid":"tip"):"j_"+std::string(f==2?"mid":names[f])+(side?"_ri_":"_le_")+std::to_string(j+(oneBased?1:0));
                const auto index=bone(body.skeleton,name.c_str(),parent);auto& b=body.skeleton.bones[index];b.restLocal.position=j?scene::Vec3{.4f,0,0}:scene::Vec3{1.f,(2-f)*.25f,f==0?-.1f:0.f};b.restGlobal=body.skeleton.bones[parent].restGlobal*scene::translation(b.restLocal.position);b.inverseBind=scene::inverseAffine(b.restGlobal);body.skeleton.boneByCanonicalName[name]=index;
                scene::Track track;track.boneIndex=index;track.property=scene::TrackProperty::Rotation;track.mode=scene::TrackMode::Absolute;track.frames={0,10};track.rotationValues={{},scene::fromEulerRadians({0,.6f,0})};body.animations[0].tracks.push_back(track);parent=int(index);
            }
        }
    };
    auto native=bodyFixture(true,1);addDigits(native,true,false,true);
    // Exercise transported rest bases, not only identity-oriented fixture bones.
    const auto nativeLayout=scene::imported::bodyOrCodLayout(native.skeleton);
    const auto nativeDigits=scene::imported::bodyFingerChains(native.skeleton);
    for(int side=0;side<2;++side){native.skeleton.bones[nativeLayout.hand[side]].restLocal.rotation=scene::fromEulerRadians({.2f,-.3f,.4f});for(const auto& finger:nativeDigits[side])for(int b:finger)native.skeleton.bones[b].restLocal.rotation=scene::fromEulerRadians({.11f,.17f,-.23f});}
    for(auto& b:native.skeleton.bones){const auto local=scene::translation(b.restLocal.position)*scene::rotation(b.restLocal.rotation);b.restGlobal=b.parent>=0?native.skeleton.bones[b.parent].restGlobal*local:local;b.inverseBind=scene::inverseAffine(b.restGlobal);}
    for(auto& track:native.animations[0].tracks)if(track.property==scene::TrackProperty::Rotation){const auto restQ=native.skeleton.bones[track.boneIndex].restLocal.rotation;for(auto& q:track.rotationValues)q=scene::normalize(scene::multiply(restQ,q));}
    auto identity=native;identity.animations.clear();std::string error;check(scene::imported::appendRetargetedBody(identity,std::make_shared<scene::CastScene>(native),0,error),"same anatomy digit adapter installs");
    const auto chains=scene::imported::bodyFingerChains(native.skeleton);
    {auto aliased=bodyFixture(false,1);addDigits(aliased,false,false,false);
     const auto expected=scene::imported::bodyFingerChains(aliased.skeleton);
     const auto palm=scene::imported::bodyPalmRestFrame(aliased.skeleton,0);
     aliased.skeleton.boneByName.clear();
     for(size_t i=0;i<aliased.skeleton.bones.size();++i){auto& b=aliased.skeleton.bones[i];b.name="native_pb_joint_"+std::to_string(i);aliased.skeleton.boneByName[b.name]=i;}
     const auto aliases=aliased.skeleton.boneByCanonicalName;
     check(scene::imported::bodyFingerChains(aliased.skeleton)==expected,"semantic body fingers survive native name differences");
     const auto actual=scene::imported::bodyPalmRestFrame(aliased.skeleton,0);
     check(palm&&actual&&palm->v==actual->v,"semantic body palm frame preserved");
     check(aliases==aliased.skeleton.boneByCanonicalName,"semantic lookup does not change binder aliases");
     aliased.skeleton.boneByCanonicalName["invalid_joint"]=aliased.skeleton.bones.size();
     check(scene::imported::semanticBodyBone(aliased.skeleton,"invalid_joint")==-1,"invalid semantic index rejected");
    }
    for(float frame:{0.f,5.f,10.f}){const auto expected=native.samplePose(0,frame),actual=identity.samplePose(0,frame);for(const auto& hand:chains)for(const auto& finger:hand)for(int b:finger)for(int k=0;k<16;++k)check(std::abs(actual[b].v[k]-expected[b].v[k])<.001f,"identity and helper digit poses preserve native animation");}
    auto source=bodyFixture(false,1);addDigits(source,false,false,false,true);auto target=bodyFixture(true,2);addDigits(target,true,true,true);target.animations.clear();
    check(scene::imported::appendRetargetedBody(target,std::make_shared<scene::CastScene>(source),0,error),"five to four digit retarget installs");
    const auto rest=target.sampleLocalPose(0,0),animated=target.sampleLocalPose(0,10);const auto targetChains=scene::imported::bodyFingerChains(target.skeleton);
    for(const auto& hand:targetChains)for(const auto& finger:hand)for(int b:finger)if(b>=0){check(std::abs(rest[b].rotation.w-1.f)<.0001f,"digit rest delta preserves target bind");check(std::abs(animated[b].rotation.y)>.2f,"shared target digit receives animated curl");check(scene::length(animated[b].position-target.skeleton.bones[b].restLocal.position)<.0001f,"digit lengths remain target authored");}
    // Two-joint exports must animate in both directions without indexing -1 or
    // fabricating the absent tip. Remove aliases as well as native lookup names.
    for(bool shortSource:{false,true}){
        auto a=bodyFixture(false,1);addDigits(a,false,false,false);
        auto b=bodyFixture(true,1);addDigits(b,true,false,false);
        auto& shortRig=shortSource?a:b;const auto originalChains=scene::imported::bodyFingerChains(shortRig.skeleton);
        for(const auto& hand:originalChains)for(const auto& digit:hand){auto& tip=shortRig.skeleton.bones[digit[2]];shortRig.skeleton.boneByName.erase(tip.name);shortRig.skeleton.boneByCanonicalName.erase(tip.name);tip.name="unused_tip_"+std::to_string(digit[2]);}
        const auto shortened=scene::imported::bodyFingerChains(shortRig.skeleton);
        for(const auto& hand:shortened)for(const auto& digit:hand)check(digit[0]>=0&&digit[1]>=0&&digit[2]<0,"Valid two-joint chain retained");
        b.animations.clear();check(scene::imported::appendRetargetedBody(b,std::make_shared<scene::CastScene>(a),0,error),"Partial digit retarget installs both directions");
        const auto atEnd=b.sampleLocalPose(0,10);const auto bChains=scene::imported::bodyFingerChains(b.skeleton);
        for(const auto& hand:bChains)for(const auto& digit:hand){check(std::abs(atEnd[digit[1]].rotation.y)>.2f,"Two-joint curl reaches actual knuckle");if(shortSource&&digit[2]>=0)check(b.animations[0].coldWarWorldPose->sourceBones[digit[2]]<0,"Absent source tip not invented");}
    }
    auto four=bodyFixture(true,1);addDigits(four,true,true,false);auto five=bodyFixture(false,1);addDigits(five,false,false,false);five.animations.clear();
    check(scene::imported::appendRetargetedBody(five,std::make_shared<scene::CastScene>(four),0,error),"four to five preserves existing unmatched digit safely");
    const auto fiveChains=scene::imported::bodyFingerChains(five.skeleton);for(int side=0;side<2;++side)for(int b:fiveChains[side][2])check(five.animations[0].coldWarWorldPose->sourceBones[b]<0,"missing source middle digit never duplicates another digit");
    for(auto action:{scene::ActionRole::Death,scene::ActionRole::Reload}){auto clipSource=source;clipSource.animations[0].action=action;clipSource.animations[0].domain=action==scene::ActionRole::Reload?scene::AnimationDomain::PlayerTorso:scene::AnimationDomain::PlayerBody;auto clipTarget=bodyFixture(true,1);addDigits(clipTarget,true,false,false);clipTarget.animations.clear();check(scene::imported::appendRetargetedBody(clipTarget,std::make_shared<scene::CastScene>(clipSource),0,error),"death and reload digit adapters install");const auto adapter=clipTarget.animations[0].coldWarWorldPose;for(std::size_t b=0;b<adapter->fingerBindings.size();++b)if(adapter->fingerBindings[b].sourceParent>=0){check(!adapter->globalTranslation[b],"death/reload fingers do not pin global hand positions");if(action==scene::ActionRole::Reload){bool owned=false;for(const auto& track:clipTarget.animations[0].tracks)if(track.boneIndex==b&&track.ownsLayer)owned=true;check(owned,"torso reload owns mapped digit layer");}}}
}
static void supportArmChecks(){
    using namespace scene;
    Skeleton rig;std::vector<Transform> local;
    const auto add=[&](const char* name,int parent,Vec3 p){const auto i=bone(rig,name,parent);rig.bones[i].restLocal.position=p;local.push_back(rig.bones[i].restLocal);return int(i);};
    const int shoulder=add("shoulder",-1,{});add("upper_helper",shoulder,{});const int elbow=add("elbow",1,{2,0,0});add("lower_helper",elbow,{});const int wrist=add("wrist",3,{2,0,0});add("finger",wrist,{.5f,0,0});
    local[wrist].rotation=fromEulerRadians({.2f,.1f,.3f});
    const auto originalLocal=local;const auto rebuild=[&](){std::vector<Mat4> p;for(std::size_t i=0;i<local.size();++i)p.push_back((rig.bones[i].parent>=0?p[rig.bones[i].parent]:Mat4::identity())*trs(local[i].position,local[i].rotation,local[i].scale));return p;};
    auto globals=rebuild();const auto oldWrist=globals[wrist];
    imported::SupportArm arm;arm.shoulder=shoulder;arm.elbow=elbow;arm.wrist=wrist;for(std::size_t b=0;b<local.size();++b)arm.descendants.push_back(b);
    for(const auto target:{Vec3{2,2,0},Vec3{-1,2,1},Vec3{100,100,100}}){local=originalLocal;globals=rebuild();
        check(imported::fitSupportArm(arm,rig,local,globals,target),"support arm handles reachable and unreachable targets");
        const auto p=[&](int b){return transformPoint(globals[b],{});};
        check(std::abs(length(p(elbow)-p(shoulder))-2)<.001f&&std::abs(length(p(wrist)-p(elbow))-2)<.001f,"support solve preserves both arm segment lengths through helpers");
        if(length(target)<4)check(length(p(wrist)-target)<.001f,"reachable support hand reaches donor grip");else check(length(p(wrist))<=4.001f,"unreachable support target clamps without stretching");
        for(std::size_t b=0;b<local.size();++b)check(length(local[b].position-originalLocal[b].position)<1e-6f,"support solve never rewrites native local translations");
        for(int col=0;col<3;++col)for(int row=0;row<3;++row)check(std::abs(globals[wrist].v[col*4+row]-oldWrist.v[col*4+row])<.001f,"support solve retains animated wrist orientation");
    }
    Animation a;a.weapon=WeaponClass::Rifle;a.motion=MotionRole::Run;check(imported::supportedGripAction(a),"rifle run keeps two-handed contact");
    for(auto action:{ActionRole::Reload,ActionRole::Death,ActionRole::Melee,ActionRole::Throw}){a.action=action;check(!imported::supportedGripAction(a),"independent hand actions remain unconstrained");}
    a.action=ActionRole::None;a.motion=MotionRole::Climb;check(!imported::supportedGripAction(a),"mantle contact remains independent");
    a.motion=MotionRole::Run;a.weapon=WeaponClass::Any;check(!imported::supportedGripAction(a),"unidentified/unarmed clips remain unconstrained");
    auto source=bodyFixture(false,1);source.animations[0].weapon=WeaponClass::Rifle;source.animations[0].motion=MotionRole::Run;
    auto target=bodyFixture(true,2);target.animations.clear();std::string error;check(imported::appendRetargetedBody(target,std::make_shared<CastScene>(source),0,error),"support grip adapter installs");
    check(std::abs(target.animations[0].coldWarWorldPose->supportArm.sourceUnits-.5f)<1e-5f,"body height normalization does not resize weapon grip span");
}
int main(){try{
    {
        scene::CastScene cs2;auto& rig=cs2.skeleton;
        bone(rig,"pelvis");bone(rig,"spine_0",0);bone(rig,"head_0",1);
        for(const auto* side:{"L","R"}){
            const auto add=[&](const char* stem,int parent){return static_cast<int>(bone(rig,(std::string(stem)+"_"+side).c_str(),parent));};
            const int arm=add("arm_upper",1),elbow=add("arm_lower",arm),hand=add("hand",elbow);
            const int leg=add("leg_upper",0),knee=add("leg_lower",leg);add("ankle",knee);
            for(const auto* digit:{"thumb","index","middle","ring","pinky"}){int parent=hand;for(int j=0;j<3;++j)parent=add(("finger_"+std::string(digit)+"_"+std::to_string(j)).c_str(),parent);}
        }
        check(scene::imported::completeBodyLayout(scene::imported::bodyLayout(rig)),"CS2 complete body anatomy");
        check(scene::imported::bodyFingerChains(rig)[1][4][2]>=0,"CS2 articulated fingers");
        check(scene::imported::nativeSocket(rig,"tag_weapon_right").has_value(),"CS2 wrist socket");
        rig.bones[0].restLocal.position={1,0,1};rig.bones[0].restGlobal=scene::translation({1,0,1});
        check(scene::imported::normalizeSourceBody(cs2),"CS2 native units converted");
        check(std::abs(rig.bones[0].restGlobal.v[12]-2.54f)<1e-5f&&std::abs(rig.bones[0].restGlobal.v[13])<1e-5f,"CS2 +X does not get Source1 yaw");
        check(!scene::imported::normalizeSourceBody(cs2),"CS2 conversion not applied twice");
    }
    {scene::CastScene s;scene::Bone driver;driver.name="b_Transform";driver.parent=-1;driver.restLocal.position={0,0,10};s.skeleton.bones.push_back(driver);
     scene::Bone target=driver;target.name="body_variant_1_b_transform";target.restLocal.position={0,0,15};s.skeleton.bones.push_back(target);s.bodyVariantDrivers[1]=0;
     std::vector<scene::Transform> local{driver.restLocal,target.restLocal};local[0].position.x=5;local[0].rotation=scene::fromAxisAngle({0,0,1},.5f);
     const auto pose=s.globalPose(local);check(std::abs(pose[1].v[12]-5)<.0001f&&std::abs(pose[1].v[14]-15)<.0001f,"Variant retains native height with driver movement");
     check(std::abs(pose[1].v[0]-pose[0].v[0])<.0001f,"Variant follows rotation");
     scene::Bone arm;arm.name="arm";arm.parent=0;arm.restLocal.position={10,0,0};s.skeleton.bones.push_back(arm);
     auto variantArm=arm;variantArm.name="variant_arm";variantArm.parent=1;variantArm.restLocal.position={5,0,0};s.skeleton.bones.push_back(variantArm);s.bodyVariantDrivers[3]=2;
     local.push_back(arm.restLocal);local.push_back(variantArm.restLocal);local[2].position={99,0,0};
     const auto bent=s.globalPose(local);check(std::abs(scene::length(scene::transformPoint(bent[3],{})-scene::transformPoint(bent[1],{}))-5)<.0001f,"Variant limb length is not copied or stretched from donor");}
    {
        scene::CastScene s;bone(s.skeleton,"CSO2_BipM Pelvis");bone(s.skeleton,"CSO2_BipM L Thigh",0);bone(s.skeleton,"CSO2_BipM R Thigh",0);bone(s.skeleton,"CSO2_BipM L Foot",0);bone(s.skeleton,"CSO2_BipM R Foot",0);
        for(std::size_t i=0;i<s.skeleton.bones.size();++i){auto& b=s.skeleton.bones[i];b.restLocal.position=i==0?scene::Vec3{0,0,100}:i==1?scene::Vec3{0,10,0}:i==2?scene::Vec3{0,-10,0}:scene::Vec3{0,0,-100};b.restGlobal=(i? s.skeleton.bones[0].restGlobal:scene::Mat4::identity())*scene::translation(b.restLocal.position);b.inverseBind=scene::inverseAffine(b.restGlobal);}
        scene::Animation a;a.sourceGame="cso2";a.domain=scene::AnimationDomain::PlayerBody;a.motion=scene::MotionRole::Walk;a.durationFrames=20;
        for(int foot:{3,4}){scene::Track x;x.boneIndex=foot;x.property=scene::TrackProperty::TranslationX;x.frames={0,10,11,19,20};x.scalarValues={0,-10,-10,0,0};a.tracks.push_back(x);auto z=x;z.property=scene::TrackProperty::TranslationZ;z.scalarValues={-100,-100,-90,-90,-100};a.tracks.push_back(z);}
        s.animations.push_back(a);scene::imported::inferNativeWalkDirection(s,0);check(s.animations[0].direction==scene::Direction::Forward,"stance-foot motion infers forward gait");
        s.animations[0].direction=scene::Direction::Left;scene::imported::inferNativeWalkDirection(s,0);check(s.animations[0].direction==scene::Direction::Left,"authored direction preserved");
        s.animations[0]=a;for(auto& t:s.animations[0].tracks)if(t.property==scene::TrackProperty::TranslationX)std::fill(t.scalarValues.begin(),t.scalarValues.end(),0.f);
        scene::imported::inferNativeWalkDirection(s,0);check(s.animations[0].direction==scene::Direction::Any,"stationary feet remain unknown");
        s.animations[0]=a;
        for(auto& t:s.animations[0].tracks){t.frames={0,5,8,19,20};if(t.property==scene::TrackProperty::TranslationX)t.scalarValues={0,-5,2,-9,0};else t.scalarValues={-100,-100,-100,-100,-100};}
        scene::imported::inferNativeWalkDirection(s,0);check(s.animations[0].direction==scene::Direction::Forward,"low recovering foot does not cancel majority stance travel");
        for(auto& t:s.animations[0].tracks){t.frames={0,10,20};if(t.property==scene::TrackProperty::TranslationX)t.scalarValues={0,-10,0};else t.scalarValues={-100,-100,-100};}
        s.animations[0].direction=scene::Direction::Any;scene::imported::inferNativeWalkDirection(s,0);check(s.animations[0].direction==scene::Direction::Any,"balanced opposing foot motion remains ambiguous");
    }
    {
        scene::Skeleton s;bone(s,"CSO2_BipM Pelvis");bone(s,"CSO2_BipM Spine",0);bone(s,"CSO2_BipM L Thigh",0);bone(s,"CSO2_BipM R Thigh",0);
        scene::Animation a;a.sourceGame="cso2";a.domain=scene::AnimationDomain::PlayerBody;a.motion=scene::MotionRole::Walk;
        scene::Track t;t.boneIndex=1;a.tracks.push_back(t);
        scene::imported::classifyNativeGaitLayer(s,a);check(a.domain==scene::AnimationDomain::PlayerTorso,"upper-only gait is a torso layer");
        check(scene::imported::isNativeHoldingTorso(a),"gait layer preserves weapon posture");
        a.domain=scene::AnimationDomain::PlayerBody;a.motion=scene::MotionRole::Idle;
        scene::imported::classifyNativeGaitLayer(s,a);check(a.domain==scene::AnimationDomain::PlayerTorso,"upper-only idle cannot replace legs");
        check(scene::imported::isNativeHoldingTorso(a),"idle layer preserves weapon posture");
        a.domain=scene::AnimationDomain::PlayerBody;t.boneIndex=2;a.tracks.push_back(t);
        scene::imported::classifyNativeGaitLayer(s,a);check(a.domain==scene::AnimationDomain::PlayerBody,"leg-bearing gait remains body");
        a.tracks.resize(1);a.sourceGame="css";scene::imported::classifyNativeGaitLayer(s,a);check(a.domain==scene::AnimationDomain::PlayerBody,"other source sets unchanged");
    }
    coveragePolicyChecks();
    presentationFacingChecks();
    playerGaitTransportChecks();
    scene::Skeleton halo;bone(halo,"pelvis");bone(halo,"l_thigh",0);bone(halo,"r_thigh",0);bone(halo,"spine",0);bone(halo,"head",3);bone(halo,"r_hand",3);bone(halo,"dew2cast_socket__right_hand__0",5);
    const auto original=halo.boneByCanonicalName;
    const auto layout=scene::imported::bodyLayout(halo);check(layout.family==scene::imported::BodyFamily::Halo,"Halo body recognition");check(layout.authoredForward.x==1,"Halo forward");
    const auto mask=scene::imported::nativeBodyTorsoMask(halo);check(mask[3]&&mask[4]&&mask[5]&&!mask[0]&&!mask[1],"torso includes first spine, excludes hips/legs");
    check(scene::imported::nativeSocket(halo,"tag_weapon_right")==6,"native socket lookup");check(halo.boneByCanonicalName==original,"descriptor must not expose curve aliases");
    scene::Skeleton gun;bone(gun,"gun");bone(gun,"dew2cast_socket__left_hand__0",0);bone(gun,"dew2cast_socket__muzzle_flash__0",0);bone(gun,"dew2cast_socket__primary_ejection__0",0);
    check(!scene::imported::nativeSocket(gun,"tag_weapon_left"),"weapon support marker is not actor mount");check(scene::imported::nativeSocket(gun,"tag_flash")==2,"native muzzle marker");check(scene::imported::nativeSocket(gun,"tag_brass")==3,"native ejection marker");
    scene::Skeleton source;bone(source,"ValveBiped.Bip01_Pelvis");bone(source,"ValveBiped.Bip01_L_Thigh",0);bone(source,"ValveBiped.Bip01_R_Thigh",0);bone(source,"ValveBiped.Bip01_Spine",0);bone(source,"ValveBiped.Bip01_Head1",3);
    check(scene::imported::bodyLayout(source).family==scene::imported::BodyFamily::Source,"Source body recognition");check(scene::imported::bodyLayout(source).authoredForward.y==-1,"Source forward");
    scene::Skeleton view;bone(view,"l_hand");bone(view,"r_hand");check(!scene::imported::bodyLayout(view),"hands are not body");
    retargetChecks();sourceNormalizationChecks();palmRetargetChecks();palmVariantChecks();fingerRetargetChecks();supportArmChecks();std::cout<<"Imported body descriptors and retarget tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
