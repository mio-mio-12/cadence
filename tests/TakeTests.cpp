#include "take/Take.h"
#include "take/DollyCamera.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>

namespace { bool expect(bool value,const char* message){if(!value)std::cerr<<"FAILED: "<<message<<'\n';return value;} }

int main(){
    int failures{};take::Take original;original.sampleRate=30;original.boneCount=1;original.actor.baseModel="models/viewhands.cast";original.actor.rigModels={"models/rifle.cast","models/mag.cast"};original.worldActor.baseModel="models/player.cast";original.worldBoneCount=1;original.actorSlots[0]=original.actor;original.actorSlots[1].baseModel="models/viewhands.cast";original.actorSlots[1].rigModels={"models/pistol.cast"};original.worldActorSlots[0]=original.worldActor;original.worldActorSlots[1].baseModel="models/player.cast";original.worldActorSlots[1].attachedModels.push_back({"models/pistol_world.cast","tag_weapon"});original.botActor.baseModel="models/enemy.cast";original.botBoneCount=1;original.botCount=2;
    take::AttachedModel attachment;attachment.path="models/silencer.cast";attachment.bone="tag_silencer";attachment.position={1,2,3};attachment.rotationDegrees={4,5,6};attachment.scale={0.5f,0.5f,0.5f};original.actor.attachedModels.push_back(attachment);original.actor.hiddenBones={"tag_sights"};original.actor.viewmodelCamera=true;original.actor.viewmodelFov=72;original.actor.viewmodelFovScale=1.1f;
    for(int i=0;i<3;++i){take::Sample sample;sample.time=i/30.0f;sample.weaponSlot=static_cast<std::uint8_t>(i==2?1:0);sample.camera.target={static_cast<float>(i),2,3};sample.camera.distance=5+i;sample.camera.fov=65.0f+i*5.0f;sample.camera.adsBlend=i*0.5f;
        sample.primaryClip="pb_stand_alert.cast";sample.primaryFrame=static_cast<float>(i);
        sample.pose.push_back(scene::trs({static_cast<float>(i),0,0},scene::fromEulerRadians({0,0,scene::kPi*i}),{1,1,1}));sample.worldActor.id=0;sample.worldActor.primaryClip="pb_run_rifle.cast";sample.worldActor.primaryFrame=static_cast<float>(i);sample.worldActor.pose.push_back(scene::translation({static_cast<float>(i*3),1,0}));sample.hiddenBones=i==1?std::vector<std::uint32_t>{0}:std::vector<std::uint32_t>{};for(std::uint32_t actor=0;actor<2;++actor){take::RecordedActorState bot;bot.id=actor+1;bot.health=100.0f-i*20.0f-actor;bot.alive=i<2;bot.primaryClip="pb_run.cast";bot.primaryFrame=static_cast<float>(i);bot.torsoClip=i==2?"pb_death_forward.cast":"";bot.pose.push_back(scene::translation({static_cast<float>(i*2+actor),static_cast<float>(actor),0}));sample.bots.push_back(std::move(bot));}original.samples.push_back(sample);}
    for(std::uint32_t i=0;i<4;++i){take::DollyCameraKeyframe camera;camera.tick=i*10;camera.position={static_cast<float>(i*i),static_cast<float>(i),5};camera.rotationDegrees={static_cast<float>(i*4),static_cast<float>(i*20),static_cast<float>(i)};camera.fov=80.0f-i*5.0f;original.dollyCamera.push_back(camera);}
    original.actorSlotBoneCounts={1,1};original.worldActorSlotBoneCounts={1,1};original.cameraPaths.push_back({"Opening",original.dollyCamera});
    take::ShotEvent testShot;testShot.time=0.033f;testShot.muzzlePos={10,20,30};testShot.hitPos={100,200,300};testShot.sniperWeapon=true;testShot.muzzleFlashDuration=0.06f;testShot.trailWidth=2.0f;testShot.smokeEnabled=true;original.shots.push_back(testShot);
    failures+=!expect(original.compatible(1)&&!original.compatible(2),"take validates skeleton compatibility");
    failures+=!expect(original.bonesForWorldSlot(0)==1&&original.bonesForWorldSlot(1)==1,"take provides world bone counts for both slots");
    failures+=!expect(original.sampleIndex(0.03f)==1,"timeline selects the nearest fixed-rate sample");
    const auto halfway=original.interpolatedSample(1.0f/60.0f);
    failures+=!expect(halfway.pose.size()==1&&std::abs(halfway.pose[0].v[12]-0.5f)<0.001f,"fractional take time interpolates bone translation");
    failures+=!expect(std::abs(halfway.pose[0].v[0])<0.001f&&std::abs(std::abs(halfway.pose[0].v[1])-1.0f)<0.001f,"fractional take time uses quaternion bone rotation interpolation");
    failures+=!expect(std::abs(halfway.camera.target.x-0.5f)<0.001f&&std::abs(halfway.camera.distance-5.5f)<0.001f,"fractional take time interpolates the camera");
    failures+=!expect(halfway.bots.size()==2&&std::abs(halfway.bots[0].pose[0].v[12]-1.0f)<0.001f&&std::abs(halfway.bots[0].health-90.0f)<0.001f,"fractional take time interpolates recorded bot pose and state");
    failures+=!expect(halfway.worldActor.pose.size()==1&&std::abs(halfway.worldActor.pose[0].v[12]-1.5f)<0.001f,"fractional take time interpolates the recorded player world proxy");
    failures+=!expect(halfway.weaponSlot==0,"take interpolation retains class-slot metadata within a weapon segment");
    const auto swapping=original.interpolatedSample(1.25f/30.0f);
    failures+=!expect(swapping.weaponSlot==0&&std::abs(swapping.pose[0].v[12]-1.0f)<.001f,"weapon swap keeps the selected rig discrete");
    failures+=!expect(std::abs(swapping.camera.target.x-1.25f)<.001f&&std::abs(swapping.bots[0].pose[0].v[12]-2.5f)<.001f,"weapon swap does not step camera or independent bot playback");
    auto metadataTake=original;
    auto& lowerActor=metadataTake.samples[0].worldActor;auto& upperActor=metadataTake.samples[1].worldActor;
    lowerActor.id=7;lowerActor.alive=true;lowerActor.health=100;lowerActor.primaryClip="lower";lowerActor.torsoClip="lower_torso";lowerActor.primaryFrame=2;lowerActor.torsoFrame=4;
    upperActor.id=7;upperActor.alive=false;upperActor.health=20;upperActor.primaryClip="upper";upperActor.torsoClip="upper_torso";upperActor.primaryFrame=10;upperActor.torsoFrame=12;
    for(float alpha:{0.25f,0.75f}){
        const auto interpolated=metadataTake.interpolatedSample(alpha/30.0f);const auto& actor=interpolated.worldActor;const auto& expected=alpha<0.5f?lowerActor:upperActor;
        failures+=!expect(actor.id==expected.id&&actor.alive==expected.alive&&actor.primaryClip==expected.primaryClip&&actor.torsoClip==expected.torsoClip,"world actor interpolation retains nearest discrete metadata");
        failures+=!expect(std::abs(actor.health-(100-80*alpha))<0.001f&&actor.primaryFrame==expected.primaryFrame&&actor.torsoFrame==expected.torsoFrame&&actor.pose.size()==1&&std::abs(actor.pose[0].v[12]-3*alpha)<0.001f,"world actor interpolation retains continuous state and nearest changed-clip frames");
    }
    upperActor.primaryClip=lowerActor.primaryClip;upperActor.torsoClip=lowerActor.torsoClip;
    const auto continuousActor=metadataTake.interpolatedSample(0.25f/30.0f).worldActor;
    failures+=!expect(std::abs(continuousActor.primaryFrame-4.0f)<0.001f&&std::abs(continuousActor.torsoFrame-6.0f)<0.001f,"world actor matching clips retain frame interpolation");
    const std::vector<take::DollyCameraKeyframe> linearNodes{original.dollyCamera[0],original.dollyCamera[1]};const auto linearCamera=take::interpolateDollyCamera(linearNodes,5.0f);failures+=!expect(std::abs(linearCamera.position.x-0.5f)<0.001f&&std::abs(linearCamera.fov-77.5f)<0.001f,"IWXMVM camera path uses linear channels below four nodes");const auto cubicNode=take::interpolateDollyCamera(original.dollyCamera,20.0f);failures+=!expect(std::abs(cubicNode.position.x-4.0f)<0.001f&&std::abs(cubicNode.rotationDegrees.y-40.0f)<0.001f,"IWXMVM cubic camera path passes through exact timeline nodes");
    const auto nearFreeze=original.interpolatedSample(0.000001f);
    failures+=!expect(nearFreeze.pose.size()==1&&nearFreeze.pose[0].v[12]>0.0f&&nearFreeze.pose[0].v[12]<0.0001f,"arbitrarily small positive take times remain interpolated rather than snapping");
    failures+=!expect(take::outputFrameCount(0,1,120,0.25f)==480,"offline frame count accounts for output FPS and timescale");
    failures+=!expect(std::abs(take::outputTakeTime(1,120,120,0.25f)-1.25f)<0.0001f,"offline frame clock advances deterministically");
    failures+=!expect(take::outputFrameCount(0,1,120,0.0f)==1&&take::outputTakeTime(0.5f,99,120,0.0f)==0.5f,"zero timescale exports one exact still frame");
    const auto path=std::filesystem::temp_directory_path()/"cast_stage_take_test.c_dm";std::string error;
    failures+=!expect(take::save(original,path,error),".c_dm take serializes");take::Take restored;
    {std::ifstream file(path,std::ios::binary);std::string magic(8,'\0');file.read(magic.data(),8);
        failures+=!expect(file.good()&&magic=="IWTAKE01",".c_dm extension preserves the existing binary payload header");}
    failures+=!expect(take::load(path,restored,error),".c_dm take deserializes");std::error_code ignored;std::filesystem::remove(path,ignored);
    failures+=!expect(restored.samples.size()==3&&restored.boneCount==1,"take dimensions round-trip");
    failures+=!expect(restored.samples[2].primaryClip=="pb_stand_alert.cast"&&std::abs(restored.samples[2].pose[0].v[12]-2.0f)<0.001f,"take metadata and pose round-trip");
    failures+=!expect(std::abs(restored.samples[2].camera.fov-75.0f)<0.001f&&std::abs(restored.samples[2].camera.adsBlend-1.0f)<0.001f,"take camera dynamic FOV and ADS blend round-trip");
    failures+=!expect(restored.actor.baseModel==original.actor.baseModel&&restored.actor.rigModels==original.actor.rigModels,"take actor base and merged rig models round-trip");
    failures+=!expect(restored.actor.attachedModels.size()==1&&restored.actor.attachedModels[0].bone=="tag_silencer"&&std::abs(restored.actor.attachedModels[0].position.z-3.0f)<0.001f,"take rigid attachment manifest round-trips");
    failures+=!expect(restored.actor.hiddenBones==original.actor.hiddenBones&&restored.actor.viewmodelCamera&&std::abs(restored.actor.viewmodelFov-72.0f)<0.001f,"take visibility and viewmodel camera settings round-trip");
    failures+=!expect(restored.botActor.baseModel==original.botActor.baseModel&&restored.botBoneCount==1&&restored.botCount==2,"take bot actor manifest round-trips");
    failures+=!expect(restored.worldActor.baseModel==original.worldActor.baseModel&&restored.worldBoneCount==1&&restored.samples[2].worldActor.primaryClip=="pb_run_rifle.cast"&&std::abs(restored.samples[2].worldActor.pose[0].v[12]-6.0f)<0.001f,"take player world proxy manifest and pose round-trip");
    failures+=!expect(restored.actorSlots[1].rigModels==std::vector<std::string>{"models/pistol.cast"}&&restored.worldActorSlots[1].attachedModels.size()==1&&restored.samples[2].weaponSlot==1,"per-slot first-person and world weapon manifests round-trip");
    failures+=!expect(restored.samples[2].bots.size()==2&&!restored.samples[2].bots[0].alive&&restored.samples[2].bots[0].torsoClip=="pb_death_forward.cast","take bot death and animation state round-trip");
    failures+=!expect(restored.samples[1].hiddenBones==std::vector<std::uint32_t>{0},"per-sample tag visibility round-trips");failures+=!expect(restored.dollyCamera.size()==4&&restored.dollyCamera[2].tick==20&&std::abs(restored.dollyCamera[2].fov-70.0f)<0.001f,"IWXMVM camera keyframes round-trip inside the take");
    failures+=!expect(restored.cameraPaths.size()==1&&restored.cameraPaths[0].name=="Opening"&&restored.cameraPaths[0].keyframes.size()==4,"named campaths round-trip inside the take");
    failures+=!expect(restored.shots.size()==1&&restored.shots[0].sniperWeapon&&std::abs(restored.shots[0].muzzlePos.x-10.0f)<0.001f&&std::abs(restored.shots[0].hitPos.z-300.0f)<0.001f,"take shot events round-trip");
    auto trimmed=restored;failures+=!expect(trimmed.trim(0.03f,0.07f)&&trimmed.samples.size()==2&&std::abs(trimmed.samples.front().time)<0.0001f&&trimmed.duration()>0.03f&&trimmed.shots.size()==1,"take trimming keeps the selected sample interval and rebases it to zero");
    const auto fileVersion=[&](){std::ifstream input(path,std::ios::binary);input.seekg(8);std::uint32_t value{};input.read(reinterpret_cast<char*>(&value),sizeof(value));return value;};
    failures+=!expect(take::save(original,path,error)&&fileVersion()==10,"two-slot recordings retain version 10 compatibility");
    failures+=!expect(take::load(path,restored,error)&&restored.actorSlots[2].empty()&&restored.actorSlotBoneCounts[2]==0,"old two-slot takes leave third slot empty");
    auto three=original;three.actorSlots[2].baseModel="models/third.cast";three.worldActorSlots[2].baseModel="models/third_world.cast";
    three.actorSlotBoneCounts[2]=3;three.worldActorSlotBoneCounts[2]=2;
    auto third=three.samples.back();third.time=0.1f;third.weaponSlot=2;third.pose.assign(3,scene::translation({9,0,0}));third.worldActor.pose.assign(2,scene::translation({4,0,0}));third.hiddenBones={2};three.samples.push_back(third);
    third.time=0.2f;for(auto& pose:third.pose)pose=scene::translation({11,0,0});three.samples.push_back(third);
    failures+=!expect(take::save(three,path,error)&&fileVersion()==11,"third-slot recordings use version 11");
    take::Take threeRead;const bool readThree=take::load(path,threeRead,error);failures+=!expect(readThree,"three-slot take loads");
    if(readThree){
        failures+=!expect(threeRead.actorSlots[2].baseModel=="models/third.cast"&&threeRead.bonesForSlot(2)==3&&threeRead.bonesForWorldSlot(2)==2,"third manifests and independent skeleton sizes round-trip");
        for(float time:{0.15f,0.1f,0.0f,0.15f}){
            const auto sample=threeRead.interpolatedSample(time);
            failures+=!expect(time==0?sample.weaponSlot==0:sample.weaponSlot==2&&sample.pose.size()==3&&sample.worldActor.pose.size()==2&&sample.hiddenBones==std::vector<std::uint32_t>{2},"third-slot forward and backward scrubbing retains correct skeleton and visibility");
            if(time==0.15f)failures+=!expect(std::abs(sample.pose[0].v[12]-10)<0.001f,"third-slot fractional pose interpolates");
        }
        failures+=!expect(threeRead.interpolatedSample(0.07f).weaponSlot==1&&threeRead.interpolatedSample(0.095f).weaponSlot==2,"switch boundaries do not blend unrelated weapon skeletons");
        failures+=!expect(threeRead.shots.size()==original.shots.size()&&threeRead.cameraPaths.size()==original.cameraPaths.size(),"third-slot recording preserves effects and camera paths");
        failures+=!expect(threeRead.trim(0.09f,0.21f)&&threeRead.samples.front().weaponSlot==2,"third-slot trim preserves slot identity");
    }
    auto curved=original;auto& shape=curved.shots[0].trailTaperCurve;shape.count=3;shape.points[0]={0,.2f};shape.points[1]={.4f,2.f};shape.points[2]={1,.1f};shape.sanitize();
    failures+=!expect(take::save(curved,path,error)&&fileVersion()==12,"custom taper uses two-slot curve format");
    failures+=!expect(take::load(path,restored,error)&&restored.shots[0].trailTaperCurve.sample(.4f)==2.f&&restored.actorSlots[2].empty(),"taper interior and slot count round-trip");
    three.shots[0].trailTaperCurve=shape;
    failures+=!expect(take::save(three,path,error)&&fileVersion()==13&&take::load(path,restored,error)&&restored.shots[0].trailTaperCurve.sample(1)==.1f,"three-slot curve round-trip");
    auto assembled=original;assembled.botActor.replaceBaseMeshes=true;
    assembled.botActor.rigModels={"body_a.cast","head_a.cast","body_b.cast","head_b.cast"};assembled.botActor.rigModelVariants={0,0,1,1};
    take::AttachedModel botOptic;botOptic.path="scope.cast";botOptic.bone="tag_weapon";botOptic.position={1,2,3};botOptic.modelVariant=1;assembled.botActor.attachedModels.push_back(botOptic);
    for(auto& sample:assembled.samples)for(std::size_t i=0;i<sample.bots.size();++i)sample.bots[i].modelVariant=static_cast<std::int32_t>(i);
    failures+=!expect(take::save(assembled,path,error)&&fileVersion()==14,"assembled bots use version14 without changing ordinary take format");
    failures+=!expect(take::load(path,restored,error)&&restored.botActor.replaceBaseMeshes&&restored.botActor.rigModels==assembled.botActor.rigModels&&restored.botActor.rigModelVariants==assembled.botActor.rigModelVariants,"all bot body/head parts and variant identities round-trip");
    failures+=!expect(take::save(restored,path,error)&&fileVersion()==14,"reopened two-slot v14 take can be saved again despite fallback third skeleton count");
    failures+=!expect(restored.samples[0].bots[1].modelVariant==1&&restored.botActor.attachedModels.back().modelVariant==1&&restored.botActor.attachedModels.back().position.z==3,"recorded bot and world optic variant/placement round-trip");
    failures+=!expect(restored.interpolatedSample(.01f).bots[1].modelVariant==1,"bot model variant survives pose interpolation");
    auto visible=assembled;
    visible.samples[0].visibility=take::VisibilityKnown;
    visible.samples[1].visibility=take::VisibilityKnown|take::HideViewmodel|take::ScopeOverlay;
    visible.samples[1].worldActor.visibility=take::VisibilityKnown|take::HideWeapon;
    visible.samples[1].bots[0].visibility=take::VisibilityKnown|take::HideWeapon;
    visible.samples[1].bots[1].visibility=take::VisibilityKnown;
    failures+=!expect(take::save(visible,path,error)&&fileVersion()==15&&take::load(path,restored,error),"actor visibility uses version15 and round-trips");
    failures+=!expect(restored.samples[0].visibility==take::VisibilityKnown&&restored.samples[1].visibility==visible.samples[1].visibility&&restored.samples[1].bots[0].visibility==visible.samples[1].bots[0].visibility&&restored.samples[1].worldActor.visibility==visible.samples[1].worldActor.visibility,"scope, equipment, and per-bot visibility stay independent");
    failures+=!expect(restored.botActor.rigModelVariants==visible.botActor.rigModelVariants&&restored.shots.size()==visible.shots.size(),"version15 preserves assembly and effects");
    failures+=!expect(take::save(restored,path,error)&&fileVersion()==15,"reopened two-slot v15 take can be saved again");
    std::filesystem::resize_file(path,40);
    failures+=!expect(!take::load(path,restored,error),"truncated new-format take rejected");
    visible.samples[0].visibility=255;
    failures+=!expect(!take::save(visible,path,error),"invalid visibility rejected when saving");
    assembled.botActor.rigModelVariants[0]=999;
    failures+=!expect(!take::save(assembled,path,error),"invalid bot mesh variant rejected");
    auto invalid=three;invalid.samples.back().weaponSlot=3;
    failures+=!expect(!take::save(invalid,path,error)&&three.bonesForSlot(3)==0,"out-of-range slots rejected instead of clamped");
    std::filesystem::remove(path,ignored);
    if(!failures)std::cout<<"All take tests passed.\n";return failures?1:0;
}
