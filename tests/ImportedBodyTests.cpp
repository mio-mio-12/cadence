#include "scene/ImportedBody.h"
#include "scene/ImportedBodyRetarget.h"
#include "scene/ImportedBodyMotion.h"
#include "app/ImportedBodyDonorCompatibility.h"
#include <iostream>
#include <stdexcept>
#include <limits>
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::size_t bone(scene::Skeleton& s,const char* name,int parent=-1){scene::Bone b;b.name=name;b.parent=parent;const auto index=s.bones.size();s.bones.push_back(b);s.boneByName[name]=index;return index;}
static scene::CastScene bodyFixture(bool halo,float scale){
    scene::CastScene s;const auto add=[&](const char* h,const char* cod,int parent,scene::Vec3 p){const auto i=bone(s.skeleton,halo?h:cod,parent);auto& b=s.skeleton.bones[i];b.restLocal.position=p*scale;b.restGlobal=(parent>=0?s.skeleton.bones[parent].restGlobal:scene::Mat4::identity())*scene::translation(b.restLocal.position);b.inverseBind=scene::inverseAffine(b.restGlobal);s.skeleton.boneByCanonicalName[b.name]=i;return static_cast<int>(i);};
    add("pelvis","pelvis",-1,{0,0,2});add("spine","j_spinelower",0,{0,0,1});add("head","j_head",1,{0,0,1});
    for(int side=0;side<2;++side){const float sign=side?-1.f:1.f;const auto arm=add(side?"r_upperarm":"l_upperarm",side?"j_shoulder_ri":"j_shoulder_le",1,{0,sign,0});const auto elbow=add(side?"r_forearm":"l_forearm",side?"j_elbow_ri":"j_elbow_le",arm,{1,0,0});add(side?"r_hand":"l_hand",side?"j_wrist_ri":"j_wrist_le",elbow,{1,0,0});const auto hip=add(side?"r_thigh":"l_thigh",side?"j_hip_ri":"j_hip_le",0,{0,sign,0});const auto knee=add(side?"r_calf":"l_calf",side?"j_knee_ri":"j_knee_le",hip,{0,0,-1});add(side?"r_foot":"l_foot",side?"j_ankle_ri":"j_ankle_le",knee,{0,0,-1});}
    scene::Animation a;a.domain=scene::AnimationDomain::PlayerBody;a.durationFrames=10;scene::Track tx;tx.boneIndex=0;tx.property=scene::TrackProperty::TranslationX;tx.frames={0,10};tx.scalarValues={0,scale};a.tracks.push_back(tx);s.animations.push_back(a);return s;
}
static void retargetChecks(){
    {auto native=bodyFixture(true,1);const auto helper=bone(native.skeleton,"detached_deform");scene::Mesh mesh;mesh.skinned=true;scene::Vertex vertex;vertex.bones[0]=static_cast<unsigned>(helper);mesh.vertices.push_back(vertex);native.meshes.push_back(mesh);
     auto& clip=native.animations[0];clip.looping=true;clip.motion=scene::MotionRole::Run;scene::Track track;track.boneIndex=helper;track.property=scene::TrackProperty::TranslationX;track.frames={0,5,10};track.scalarValues={0,57,100};clip.tracks.push_back(track);auto oneShot=clip;oneShot.looping=false;native.animations.push_back(oneShot);
     scene::imported::repairDetachedBodyRootLoops(native,0);const auto& values=native.animations[0].tracks.back().scalarValues;
     check(std::abs(values.back()-1.f)<.001f,"detached root loop endpoint matches main root travel");check(std::abs(values[1]-7.5f)<.001f,"detached root correction preserves nonlinear gait detail");check(native.animations[1].tracks.back().scalarValues.back()==100.f,"one-shot detached root motion untouched");}
    {auto native=bodyFixture(true,1);auto first=native.samplePose(0,0),last=native.samplePose(0,10);
     check(std::abs(scene::imported::bodyRootTravelSpeed(native.skeleton,first,last,1)-1.f)<.001f,"native root speed without CoD alias");
     last[0].v[14]+=3.f;const auto height=last[0].v[14];scene::imported::suppressBodyRootMotion(native.skeleton,last);
     check(std::abs(last[0].v[12]-first[0].v[12])<.001f&&last[0].v[14]==height,"native horizontal root suppressed while height preserved");
     scene::Skeleton legacy;bone(legacy,"tag_origin");legacy.boneByCanonicalName["tag_origin"]=0;std::vector<scene::Mat4> pose{scene::translation({2,3,4})};scene::imported::suppressBodyRootMotion(legacy,pose);check(pose[0].v[12]==0&&pose[0].v[13]==0&&pose[0].v[14]==0,"legacy root suppression unchanged");}
    for(bool targetHalo:{false,true}){auto target=bodyFixture(targetHalo,2);auto source=std::make_shared<scene::CastScene>(bodyFixture(!targetHalo,1));const auto aliases=target.skeleton.boneByCanonicalName;std::string error;const auto index=target.animations.size();check(scene::imported::appendRetargetedBody(target,source,0,error),"bidirectional body adapter installation");
        const auto pose=target.sampleLocalPose(index,5);check(std::abs(pose[0].position.x-1.f)<.001f,"root translation scales at fractional frame");check(std::abs(pose[0].position.z-4.f)<.001f,"target pelvis bind height retained");
        for(std::size_t b=1;b<pose.size();++b)check(scene::length(pose[b].position-target.skeleton.bones[b].restLocal.position)<.001f,"target limb lengths retained");check(target.skeleton.boneByCanonicalName==aliases,"retarget does not expose aliases");check(std::abs(source->skeleton.bones[0].restGlobal.v[14]-2.f)<.001f,"source remains immutable");
    }
    auto target=bodyFixture(true,1);auto source=std::make_shared<scene::CastScene>(bodyFixture(false,1));source->animations[0].domain=scene::AnimationDomain::PlayerTorso;const auto count=target.animations.size();std::string error;check(scene::imported::appendRetargetedBody(target,source,0,error),"native torso adapter installs");
    const auto mask=scene::imported::nativeBodyTorsoMask(target.skeleton);bool upperOwned=false;for(const auto& track:target.animations[count].tracks){check(track.property==scene::TrackProperty::Rotation,"torso emits no root translation");check(!track.ownsLayer||mask[track.boneIndex],"torso ownership excludes pelvis and legs");upperOwned|=track.ownsLayer;}
    check(upperOwned&&!target.animations[count].coldWarWorldPose->globalTranslation[0],"torso has upper ownership without global pelvis motion");
}
static void sourceNormalizationChecks(){
    {scene::CastScene gold;bone(gold.skeleton,"Bip01");bone(gold.skeleton,"Bip01 Pelvis",0);bone(gold.skeleton,"Bip01 L Thigh",1);bone(gold.skeleton,"Bip01 R Thigh",1);bone(gold.skeleton,"Bip01 Spine",1);bone(gold.skeleton,"Bip01 Head",4);bone(gold.skeleton,"Bip01 L knee");gold.skeleton.bones[0].restLocal.position.z=97.f;gold.importedTranslationScale=2.54f;
     scene::Animation clip;for(int b:{0,1,6}){scene::Track track;track.boneIndex=b;track.property=scene::TrackProperty::TranslationZ;track.mode=scene::TrackMode::Absolute;track.frames={0};track.scalarValues={-4.f};clip.tracks.push_back(track);}gold.animations.push_back(clip);scene::imported::normalizeSourceBodyAnimations(gold,0);
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
    for(float frame:{0.f,5.f,10.f}){const auto expected=native.samplePose(0,frame),actual=identity.samplePose(0,frame);for(const auto& hand:chains)for(const auto& finger:hand)for(int b:finger)for(int k=0;k<16;++k)check(std::abs(actual[b].v[k]-expected[b].v[k])<.001f,"identity and helper digit poses preserve native animation");}
    auto source=bodyFixture(false,1);addDigits(source,false,false,false,true);auto target=bodyFixture(true,2);addDigits(target,true,true,true);target.animations.clear();
    check(scene::imported::appendRetargetedBody(target,std::make_shared<scene::CastScene>(source),0,error),"five to four digit retarget installs");
    const auto rest=target.sampleLocalPose(0,0),animated=target.sampleLocalPose(0,10);const auto targetChains=scene::imported::bodyFingerChains(target.skeleton);
    for(const auto& hand:targetChains)for(const auto& finger:hand)for(int b:finger)if(b>=0){check(std::abs(rest[b].rotation.w-1.f)<.0001f,"digit rest delta preserves target bind");check(std::abs(animated[b].rotation.y)>.2f,"shared target digit receives animated curl");check(scene::length(animated[b].position-target.skeleton.bones[b].restLocal.position)<.0001f,"digit lengths remain target authored");}
    auto four=bodyFixture(true,1);addDigits(four,true,true,false);auto five=bodyFixture(false,1);addDigits(five,false,false,false);five.animations.clear();
    check(scene::imported::appendRetargetedBody(five,std::make_shared<scene::CastScene>(four),0,error),"four to five preserves existing unmatched digit safely");
    const auto fiveChains=scene::imported::bodyFingerChains(five.skeleton);for(int side=0;side<2;++side)for(int b:fiveChains[side][2])check(five.animations[0].coldWarWorldPose->sourceBones[b]<0,"missing source middle digit never duplicates another digit");
    for(auto action:{scene::ActionRole::Death,scene::ActionRole::Reload}){auto clipSource=source;clipSource.animations[0].action=action;clipSource.animations[0].domain=action==scene::ActionRole::Reload?scene::AnimationDomain::PlayerTorso:scene::AnimationDomain::PlayerBody;auto clipTarget=bodyFixture(true,1);addDigits(clipTarget,true,false,false);clipTarget.animations.clear();check(scene::imported::appendRetargetedBody(clipTarget,std::make_shared<scene::CastScene>(clipSource),0,error),"death and reload digit adapters install");const auto adapter=clipTarget.animations[0].coldWarWorldPose;for(std::size_t b=0;b<adapter->fingerBindings.size();++b)if(adapter->fingerBindings[b].sourceParent>=0){check(!adapter->globalTranslation[b],"death/reload fingers do not pin global hand positions");if(action==scene::ActionRole::Reload){bool owned=false;for(const auto& track:clipTarget.animations[0].tracks)if(track.boneIndex==b&&track.ownsLayer)owned=true;check(owned,"torso reload owns mapped digit layer");}}}
}
int main(){try{
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
    retargetChecks();sourceNormalizationChecks();palmRetargetChecks();palmVariantChecks();fingerRetargetChecks();std::cout<<"Imported body descriptors and retarget tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
