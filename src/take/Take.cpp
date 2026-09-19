#include "take/Take.h"
#include "take/PoseInterpolation.h"
#include "take/AtomicFile.h"

#include <sstream>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <limits>
#include <type_traits>

namespace take {
namespace {

constexpr std::array<char,8> kMagic{'I','W','T','A','K','E','0','1'};
constexpr std::uint32_t kVersion=15;
constexpr std::uint32_t kMaxBones=4096;
constexpr std::uint32_t kMaxSamples=60u*60u*60u*4u;
constexpr std::uint32_t kMaxString=16u*1024u;
constexpr std::uint32_t kMaxModels=1024;
constexpr std::uint32_t kMaxActors=128;
constexpr std::uint32_t kMaxCameraKeyframes=256;
constexpr std::uint32_t kMaxCameraPaths=64;

float interpolateAngle(float a,float b,float t){
    float delta=std::fmod(b-a+scene::kPi,2.0f*scene::kPi);if(delta<0)delta+=2.0f*scene::kPi;delta-=scene::kPi;return a+delta*t;
}

template<class T> bool finiteValue(const T& value){
    if constexpr(std::is_floating_point_v<T>)return std::isfinite(value);
    else if constexpr(std::is_same_v<T,scene::Vec3>)return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);
    else if constexpr(std::is_same_v<T,scene::Vec4>)return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z)&&std::isfinite(value.w);
    else if constexpr(std::is_same_v<T,scene::Mat4>)return std::all_of(value.v.begin(),value.v.end(),[](float f){return std::isfinite(f);});
    else return true;
}
bool finitePose(const std::vector<scene::Mat4>& pose){return std::all_of(pose.begin(),pose.end(),[](const auto& m){return finiteValue(m);});}
bool bytesAvailable(std::ifstream& stream,std::uint64_t amount){
    const auto current=stream.tellg();if(current<0)return false;
    stream.seekg(0,std::ios::end);const auto end=stream.tellg();stream.seekg(current);
    return stream&&end>=current&&amount<=static_cast<std::uint64_t>(end-current);
}
bool readPose(std::ifstream& stream,std::vector<scene::Mat4>& pose,std::size_t bones){
    pose.resize(bones);stream.read(reinterpret_cast<char*>(pose.data()),static_cast<std::streamsize>(bones*sizeof(scene::Mat4)));
    return stream&&finitePose(pose);
}
template<class T> bool writeValue(std::ofstream& stream,const T& value){
    if(!finiteValue(value))return false;
    stream.write(reinterpret_cast<const char*>(&value),sizeof(value));return static_cast<bool>(stream);
}
template<class T> bool readValue(std::ifstream& stream,T& value){
    stream.read(reinterpret_cast<char*>(&value),sizeof(value));return static_cast<bool>(stream)&&finiteValue(value);
}
bool writeString(std::ofstream& stream,const std::string& value){
    if(value.size()>kMaxString)return false;const auto length=static_cast<std::uint32_t>(value.size());
    return writeValue(stream,length)&&(length==0||(stream.write(value.data(),length),static_cast<bool>(stream)));
}
bool readString(std::ifstream& stream,std::string& value){
    std::uint32_t length{};if(!readValue(stream,length)||length>kMaxString)return false;
    value.resize(length);return length==0||(stream.read(value.data(),length),static_cast<bool>(stream));
}

bool writeActorManifest(std::ofstream& stream,const ActorManifest& actor,std::uint32_t version){
    const auto rigCount=static_cast<std::uint32_t>(actor.rigModels.size()),attachmentCount=static_cast<std::uint32_t>(actor.attachedModels.size()),hiddenCount=static_cast<std::uint32_t>(actor.hiddenBones.size());
    const std::uint8_t viewmodelCamera=actor.viewmodelCamera?1u:0u;
    if(rigCount>kMaxModels||attachmentCount>kMaxModels||hiddenCount>kMaxBones||!writeString(stream,actor.baseModel)||!writeValue(stream,rigCount))return false;
    for(const auto& path:actor.rigModels)if(!writeString(stream,path))return false;
    if(!writeValue(stream,attachmentCount))return false;
    for(const auto& attachment:actor.attachedModels)if(!writeString(stream,attachment.path)||!writeString(stream,attachment.bone)||!writeValue(stream,attachment.position)||!writeValue(stream,attachment.rotationDegrees)||!writeValue(stream,attachment.scale))return false;
    if(!writeValue(stream,hiddenCount))return false;for(const auto& bone:actor.hiddenBones)if(!writeString(stream,bone))return false;
    if(!writeValue(stream,viewmodelCamera)||!writeValue(stream,actor.viewmodelFov)||!writeValue(stream,actor.viewmodelFovScale))return false;
    if(version>=14){
        const std::uint8_t replace=actor.replaceBaseMeshes?1:0;
        if(!writeValue(stream,replace)||(replace&&actor.rigModels.empty()))return false;
        for(std::size_t i=0;i<actor.rigModels.size();++i){const std::int32_t variant=i<actor.rigModelVariants.size()?actor.rigModelVariants[i]:-1;if(variant < -1||variant>=static_cast<std::int32_t>(kMaxActors)||!writeValue(stream,variant))return false;}
        for(const auto& a:actor.attachedModels)if(a.modelVariant < -1||a.modelVariant>=static_cast<std::int32_t>(kMaxActors)||!writeValue(stream,a.modelVariant))return false;
    }
    return true;
}

bool readActorManifest(std::ifstream& stream,ActorManifest& actor,std::uint32_t version){
    std::uint32_t rigCount{},attachmentCount{},hiddenCount{};std::uint8_t viewmodelCamera{};
    if(!readString(stream,actor.baseModel)||!readValue(stream,rigCount)||rigCount>kMaxModels)return false;actor.rigModels.resize(rigCount);for(auto& path:actor.rigModels)if(!readString(stream,path))return false;
    if(!readValue(stream,attachmentCount)||attachmentCount>kMaxModels)return false;actor.attachedModels.resize(attachmentCount);for(auto& attachment:actor.attachedModels)if(!readString(stream,attachment.path)||!readString(stream,attachment.bone)||!readValue(stream,attachment.position)||!readValue(stream,attachment.rotationDegrees)||!readValue(stream,attachment.scale))return false;
    if(!readValue(stream,hiddenCount)||hiddenCount>kMaxBones)return false;actor.hiddenBones.resize(hiddenCount);for(auto& bone:actor.hiddenBones)if(!readString(stream,bone))return false;
    if(!readValue(stream,viewmodelCamera)||viewmodelCamera>1||!readValue(stream,actor.viewmodelFov)||!readValue(stream,actor.viewmodelFovScale))return false;
    if(version>=14){
        std::uint8_t replace{};if(!readValue(stream,replace)||replace>1||(replace&&actor.rigModels.empty()))return false;actor.replaceBaseMeshes=replace!=0;
        actor.rigModelVariants.resize(rigCount);
        for(auto& variant:actor.rigModelVariants)if(!readValue(stream,variant)||variant < -1||variant>=static_cast<std::int32_t>(kMaxActors))return false;
        for(auto& a:actor.attachedModels)if(!readValue(stream,a.modelVariant)||a.modelVariant < -1||a.modelVariant>=static_cast<std::int32_t>(kMaxActors))return false;
    }
    actor.viewmodelCamera=viewmodelCamera!=0;return std::isfinite(actor.viewmodelFov)&&std::isfinite(actor.viewmodelFovScale)&&actor.viewmodelFov>0&&actor.viewmodelFovScale>0;
}

bool writeCameraKeys(std::ofstream& stream,const std::vector<DollyCameraKeyframe>& keys){
    const auto count=static_cast<std::uint32_t>(keys.size());if(count>kMaxCameraKeyframes||!writeValue(stream,count))return false;
    for(const auto& key:keys)if(!writeValue(stream,key.tick)||!writeValue(stream,key.position)||!writeValue(stream,key.rotationDegrees)||!writeValue(stream,key.fov))return false;return true;
}
bool readCameraKeys(std::ifstream& stream,std::vector<DollyCameraKeyframe>& keys){
    std::uint32_t count{};if(!readValue(stream,count)||count>kMaxCameraKeyframes)return false;keys.resize(count);std::uint32_t previous{};
    for(std::size_t i=0;i<keys.size();++i){auto& key=keys[i];if(!readValue(stream,key.tick)||!readValue(stream,key.position)||!readValue(stream,key.rotationDegrees)||!readValue(stream,key.fov)||(i&&key.tick<=previous)||!std::isfinite(key.fov)||key.fov<=0||key.fov>=180)return false;previous=key.tick;}return true;
}

} // namespace

float Take::duration() const{return samples.empty()?0.0f:samples.back().time;}
bool Take::compatible(std::size_t bones) const{return !samples.empty()&&boneCount==bones;}
std::size_t Take::bonesForSlot(std::uint8_t slot) const{if(slot>=actorSlotBoneCounts.size())return 0;const auto value=actorSlotBoneCounts[slot];return value?value:boneCount;}
std::size_t Take::bonesForWorldSlot(std::uint8_t slot) const{if(slot>=worldActorSlotBoneCounts.size())return 0;const auto value=worldActorSlotBoneCounts[slot];return value?value:worldBoneCount;}
bool Take::compatibleSlot(std::size_t bones,std::uint8_t slot) const{return !samples.empty()&&bonesForSlot(slot)==bones;}
bool Take::botsCompatible(std::size_t bones) const{return botCount==0||(botBoneCount==bones&&!botActor.empty());}
std::size_t Take::sampleIndex(float time) const{
    if(samples.empty())return 0;
    const auto found=std::lower_bound(samples.begin(),samples.end(),time,[](const Sample& sample,float value){return sample.time<value;});
    if(found==samples.begin())return 0;if(found==samples.end())return samples.size()-1;
    const auto upper=static_cast<std::size_t>(found-samples.begin()),lower=upper-1;
    return time-samples[lower].time<samples[upper].time-time?lower:upper;
}
const Sample* Take::sampleAt(float time) const{return samples.empty()?nullptr:&samples[sampleIndex(time)];}
Sample Take::interpolatedSample(float time,InterpolationRigs rigs) const{
    if(samples.empty())return {};
    const auto found=std::lower_bound(samples.begin(),samples.end(),time,[](const Sample& sample,float value){return sample.time<value;});
    if(found==samples.begin())return samples.front();if(found==samples.end())return samples.back();
    if(found->time==time)return *found;
    const auto& upper=*found;const auto& lower=*(found-1);const float span=upper.time-lower.time;
    const float alpha=span>1e-8f?std::clamp((time-lower.time)/span,0.0f,1.0f):0.0f;Sample result;
    const bool sameWeapon=lower.weaponSlot==upper.weaponSlot;
    result.time=time;result.camera.target=scene::lerp(lower.camera.target,upper.camera.target,alpha);
    result.camera.yaw=interpolateAngle(lower.camera.yaw,upper.camera.yaw,alpha);result.camera.pitch=interpolateAngle(lower.camera.pitch,upper.camera.pitch,alpha);
    result.camera.distance=lower.camera.distance+(upper.camera.distance-lower.camera.distance)*alpha;
    result.camera.fov=lower.camera.fov+(upper.camera.fov-lower.camera.fov)*alpha;
    result.camera.adsBlend=lower.camera.adsBlend+(upper.camera.adsBlend-lower.camera.adsBlend)*alpha;
    const bool samePrimary=sameWeapon&&lower.primaryClip==upper.primaryClip,sameTorso=sameWeapon&&lower.torsoClip==upper.torsoClip;
    const auto& metadata=alpha<0.5f?lower:upper;result.primaryClip=metadata.primaryClip;result.torsoClip=metadata.torsoClip;
    result.hiddenBones=metadata.hiddenBones;result.weaponSlot=metadata.weaponSlot;result.visibility=metadata.visibility;
    result.primaryFrame=samePrimary?lower.primaryFrame+(upper.primaryFrame-lower.primaryFrame)*alpha:metadata.primaryFrame;
    result.torsoFrame=sameTorso?lower.torsoFrame+(upper.torsoFrame-lower.torsoFrame)*alpha:metadata.torsoFrame;
    // A slot change selects a discrete weapon rig, not a discrete camera/bot frame.
    result.pose=sameWeapon?interpolatePose(lower.pose,upper.pose,alpha,rigs.view):metadata.pose;
    result.worldActor=metadata.worldActor;
    if(sameWeapon&&lower.worldActor.id==upper.worldActor.id&&!lower.worldActor.pose.empty()&&!upper.worldActor.pose.empty()){
        const auto& actorMetadata=alpha<0.5f?lower.worldActor:upper.worldActor;
        // Copy discrete metadata without copying the pose that interpolation replaces.
        result.worldActor.id=actorMetadata.id;result.worldActor.alive=actorMetadata.alive;result.worldActor.visibility=actorMetadata.visibility;
        result.worldActor.primaryClip=actorMetadata.primaryClip;result.worldActor.torsoClip=actorMetadata.torsoClip;
        result.worldActor.health=lower.worldActor.health+(upper.worldActor.health-lower.worldActor.health)*alpha;result.worldActor.primaryFrame=lower.worldActor.primaryClip==upper.worldActor.primaryClip?lower.worldActor.primaryFrame+(upper.worldActor.primaryFrame-lower.worldActor.primaryFrame)*alpha:actorMetadata.primaryFrame;result.worldActor.torsoFrame=lower.worldActor.torsoClip==upper.worldActor.torsoClip?lower.worldActor.torsoFrame+(upper.worldActor.torsoFrame-lower.worldActor.torsoFrame)*alpha:actorMetadata.torsoFrame;result.worldActor.pose=interpolatePose(lower.worldActor.pose,upper.worldActor.pose,alpha,rigs.world);
        if(!lower.worldActor.alive&&upper.worldActor.alive)result.worldActor=lower.worldActor;
    }
    const auto recordedBotCount=std::min(lower.bots.size(),upper.bots.size());result.bots.reserve(recordedBotCount);
    for(std::size_t actorIndex=0;actorIndex<recordedBotCount;++actorIndex){const auto& lowerActor=lower.bots[actorIndex];const auto& upperActor=upper.bots[actorIndex];
        // A respawn is a lifecycle boundary, never a journey from the corpse to spawn.
        if(!lowerActor.alive&&upperActor.alive){result.bots.push_back(lowerActor);continue;}
        if(lowerActor.id!=upperActor.id||lowerActor.modelVariant!=upperActor.modelVariant){result.bots.push_back(alpha<0.5f?lowerActor:upperActor);continue;}
        RecordedActorState recordedActor;recordedActor.id=lowerActor.id;recordedActor.health=lowerActor.health+(upperActor.health-lowerActor.health)*alpha;const auto& actorMetadata=alpha<0.5f?lowerActor:upperActor;recordedActor.alive=actorMetadata.alive;recordedActor.primaryClip=actorMetadata.primaryClip;recordedActor.torsoClip=actorMetadata.torsoClip;recordedActor.visibility=actorMetadata.visibility;
        recordedActor.primaryFrame=lowerActor.primaryClip==upperActor.primaryClip?lowerActor.primaryFrame+(upperActor.primaryFrame-lowerActor.primaryFrame)*alpha:actorMetadata.primaryFrame;
        recordedActor.modelVariant=actorMetadata.modelVariant;
        recordedActor.torsoFrame=lowerActor.torsoClip==upperActor.torsoClip?lowerActor.torsoFrame+(upperActor.torsoFrame-lowerActor.torsoFrame)*alpha:actorMetadata.torsoFrame;
        recordedActor.pose=interpolatePose(lowerActor.pose,upperActor.pose,alpha,rigs.bots);result.bots.push_back(std::move(recordedActor));
    }
    return result;
}
bool Take::trim(float start,float end){
    if(!std::isfinite(start)||!std::isfinite(end))return false;
    if(samples.empty())return false;start=std::clamp(start,0.0f,duration());end=std::clamp(end,start,duration());
    auto first=std::lower_bound(samples.begin(),samples.end(),start,[](const Sample& s,float t){return s.time<t;});auto last=std::upper_bound(samples.begin(),samples.end(),end,[](float t,const Sample& s){return t<s.time;});if(first==last)return false;
    const float offset=first->time;
    const auto count=static_cast<std::size_t>(last-first);
    if(first!=samples.begin())std::move(first,last,samples.begin());
    samples.erase(samples.begin()+static_cast<std::ptrdiff_t>(count),samples.end());
    for(auto& sample:samples)sample.time-=offset;
    const auto trimKeys=[&](std::vector<DollyCameraKeyframe>& keys){const auto firstTick=static_cast<std::uint32_t>(std::lround(offset*sampleRate)),lastTick=static_cast<std::uint32_t>(std::lround(end*sampleRate));keys.erase(std::remove_if(keys.begin(),keys.end(),[&](const auto& k){return k.tick<firstTick||k.tick>lastTick;}),keys.end());for(auto& key:keys)key.tick-=firstTick;};trimKeys(dollyCamera);for(auto& path:cameraPaths)trimKeys(path.keyframes);
    shots.erase(std::remove_if(shots.begin(),shots.end(),[start,end](const ShotEvent& s){return s.time<start||s.time>end;}),shots.end());
    for(auto& s:shots)s.time-=offset;
    return true;
}
void Take::clear(){samples.clear();shots.clear();boneCount=0;actor={};worldActor={};actorSlots={};worldActorSlots={};actorSlotBoneCounts={};worldActorSlotBoneCounts={};worldBoneCount=0;botActor={};botBoneCount=0;botCount=0;dollyCamera.clear();cameraPaths.clear();}

std::uint64_t outputFrameCount(float start,float end,std::uint32_t fps,float timescale){
    if(!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(timescale)||fps==0||end<=start||timescale<=0)return 1;
    const double frames=std::ceil((static_cast<double>(end)-start)*fps/timescale);
    if(frames>=static_cast<double>(std::numeric_limits<std::uint64_t>::max()))return std::numeric_limits<std::uint64_t>::max();
    return static_cast<std::uint64_t>(std::max(frames,1.0));
}
float outputTakeTime(float start,std::uint64_t frame,std::uint32_t fps,float timescale){
    if(!std::isfinite(start))return 0;
    if(!std::isfinite(timescale)||fps==0||timescale<=0)return start;return start+static_cast<float>(static_cast<double>(frame)*timescale/fps);
}

static bool saveStaged(const Take& take,const std::filesystem::path& path,std::string& error){
    error.clear();
    if(take.samples.empty()||take.boneCount==0){error="The take has no recorded samples";return false;}
    if(!std::isfinite(take.sampleRate)||take.sampleRate<1||take.sampleRate>240){error="Invalid take sample rate";return false;}
    if(take.boneCount>kMaxBones||take.worldBoneCount>kMaxBones||take.botBoneCount>kMaxBones||take.botCount>kMaxActors||take.samples.size()>kMaxSamples||take.dollyCamera.size()>kMaxCameraKeyframes||take.cameraPaths.size()>kMaxCameraPaths){error="The take exceeds the supported size";return false;}
    // Ordinary recordings retain the byte-compatible v10 format.
    // v14+ stores three skeleton sizes even for a two-weapon take. Those
    // fallback counts alone do not mean a third weapon was recorded.
    const bool thirdSlot=!take.actorSlots[2].empty()||!take.worldActorSlots[2].empty()||std::any_of(take.samples.begin(),take.samples.end(),[](const Sample& sample){return sample.weaponSlot==2;});
    const bool customCurve=std::any_of(take.shots.begin(),take.shots.end(),[](const ShotEvent& shot){const auto& c=shot.trailTaperCurve;return c.count>2||c.sample(0)!=shot.trailStartTaper||c.sample(1)!=shot.trailEndTaper||(c.smoothness!=0&&shot.trailStartTaper!=shot.trailEndTaper);});
    const bool assembledBots=take.botActor.replaceBaseMeshes||!take.botActor.rigModelVariants.empty();
    const bool recordedVisibility=std::any_of(take.samples.begin(),take.samples.end(),[](const Sample& s){return s.visibility||s.worldActor.visibility||std::any_of(s.bots.begin(),s.bots.end(),[](const RecordedActorState& b){return b.visibility!=0;});});
    const std::uint32_t outputVersion=recordedVisibility?15u:assembledBots?14u:customCurve?(thirdSlot?13u:12u):(thirdSlot?11u:10u);
    const std::size_t slotCount=(thirdSlot||outputVersion>=14)?3u:2u;
    for(const auto& sample:take.samples)if(sample.weaponSlot>=slotCount){error="Recorded weapon slot is outside supported limits";return false;}
    if(thirdSlot&&take.actorSlots[2].empty()){error="Third weapon slot has no actor manifest";return false;}
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);if(!stream){error="Could not create the take file";return false;}
    stream.write(kMagic.data(),kMagic.size());const auto bones=static_cast<std::uint32_t>(take.boneCount),count=static_cast<std::uint32_t>(take.samples.size());
    if(!writeValue(stream,outputVersion)||!writeValue(stream,take.sampleRate)||!writeValue(stream,bones)||!writeValue(stream,count)){error="Could not write the take header";return false;}
    if(!writeActorManifest(stream,take.actor,outputVersion)){error="Could not write the take actor manifest";return false;}
    const auto botBones=static_cast<std::uint32_t>(take.botBoneCount),botCount=static_cast<std::uint32_t>(take.botCount);
    if(!writeActorManifest(stream,take.botActor,outputVersion)||!writeValue(stream,botBones)||!writeValue(stream,botCount)){error="Could not write the bot actor manifest";return false;}
    const auto cameraKeyframeCount=static_cast<std::uint32_t>(take.dollyCamera.size());if(!writeValue(stream,cameraKeyframeCount)){error="Could not write dolly camera header";return false;}for(const auto& keyframe:take.dollyCamera)if(!writeValue(stream,keyframe.tick)||!writeValue(stream,keyframe.position)||!writeValue(stream,keyframe.rotationDegrees)||!writeValue(stream,keyframe.fov)){error="Could not write dolly camera keyframes";return false;}
    const auto worldBones=static_cast<std::uint32_t>(take.worldBoneCount);if(!writeActorManifest(stream,take.worldActor,outputVersion)||!writeValue(stream,worldBones)){error="Could not write the world actor manifest";return false;}
    for(std::size_t i=0;i<slotCount;++i)if(!writeActorManifest(stream,take.actorSlots[i],outputVersion)){error="Could not write a class viewmodel manifest";return false;}
    for(std::size_t i=0;i<slotCount;++i)if(!writeActorManifest(stream,take.worldActorSlots[i],outputVersion)){error="Could not write a class world-model manifest";return false;}
    for(std::size_t i=0;i<slotCount;++i){const auto slotBones=static_cast<std::uint32_t>(take.bonesForSlot(static_cast<std::uint8_t>(i)));if(slotBones==0||slotBones>kMaxBones||!writeValue(stream,slotBones)){error="Could not write class skeleton sizes";return false;}}
    const auto pathCount=static_cast<std::uint32_t>(take.cameraPaths.size());if(!writeValue(stream,pathCount)){error="Could not write campath collection";return false;}for(const auto& pathSet:take.cameraPaths)if(!writeString(stream,pathSet.name)||!writeCameraKeys(stream,pathSet.keyframes)){error="Could not write a named campath";return false;}
    for(std::size_t i=0;i<slotCount;++i){const auto slotWorldBones=static_cast<std::uint32_t>(take.bonesForWorldSlot(static_cast<std::uint8_t>(i)));if(!writeValue(stream,slotWorldBones)){error="Could not write class world skeleton sizes";return false;}}
    float previousTime=-1;
    for(const auto& sample:take.samples){
        if(!std::isfinite(sample.time)||sample.time<0||sample.time<previousTime||!finitePose(sample.pose)||!finitePose(sample.worldActor.pose)||
            !std::all_of(sample.bots.begin(),sample.bots.end(),[](const auto& b){return finitePose(b.pose);})){error="Non-finite or unordered take sample";return false;}
        previousTime=sample.time;
        const auto sampleBones=take.bonesForSlot(sample.weaponSlot);
        const auto sampleWorldBones=take.bonesForWorldSlot(sample.weaponSlot);
        if(sample.pose.size()!=sampleBones||sample.bots.size()!=take.botCount||(sampleWorldBones>0&&!sample.worldActor.pose.empty()&&sample.worldActor.pose.size()!=sampleWorldBones)){error="A recorded sample has the wrong actor count or skeleton size";return false;}
        if(!writeValue(stream,sample.time)||!writeValue(stream,sample.camera.target)||!writeValue(stream,sample.camera.yaw)||!writeValue(stream,sample.camera.pitch)||
           !writeValue(stream,sample.camera.distance)||!writeValue(stream,sample.camera.fov)||!writeValue(stream,sample.camera.adsBlend)||!writeValue(stream,sample.primaryFrame)||!writeValue(stream,sample.torsoFrame)||
           !writeString(stream,sample.primaryClip)||!writeString(stream,sample.torsoClip)||sample.weaponSlot>=slotCount||!writeValue(stream,sample.weaponSlot)){error="Could not write take metadata";return false;}
        stream.write(reinterpret_cast<const char*>(sample.pose.data()),static_cast<std::streamsize>(sample.pose.size()*sizeof(scene::Mat4)));
        if(!stream){error="Could not write take pose data";return false;}
        if(sampleWorldBones>0){const auto& actor=sample.worldActor;const std::uint8_t alive=actor.alive?1u:0u;if(!writeValue(stream,actor.id)||!writeValue(stream,actor.health)||!writeValue(stream,alive)||!writeValue(stream,actor.primaryFrame)||!writeValue(stream,actor.torsoFrame)||!writeString(stream,actor.primaryClip)||!writeString(stream,actor.torsoClip)){error="Could not write recorded world actor metadata";return false;}
            if(actor.pose.size()==sampleWorldBones)stream.write(reinterpret_cast<const char*>(actor.pose.data()),static_cast<std::streamsize>(actor.pose.size()*sizeof(scene::Mat4)));
            else{std::vector<scene::Mat4> fallbackPose(sampleWorldBones,scene::Mat4::identity());stream.write(reinterpret_cast<const char*>(fallbackPose.data()),static_cast<std::streamsize>(fallbackPose.size()*sizeof(scene::Mat4)));}
            if(!stream){error="Could not write recorded world actor pose data";return false;}}
        for(const auto& actor:sample.bots){const std::uint8_t alive=actor.alive?1u:0u;if(actor.pose.size()!=take.botBoneCount||!writeValue(stream,actor.id)||!writeValue(stream,actor.health)||!writeValue(stream,alive)||!writeValue(stream,actor.primaryFrame)||!writeValue(stream,actor.torsoFrame)||!writeString(stream,actor.primaryClip)||!writeString(stream,actor.torsoClip)){error="Could not write recorded bot metadata";return false;}if(outputVersion>=14&&(actor.modelVariant<0||actor.modelVariant>=static_cast<std::int32_t>(kMaxActors)||!writeValue(stream,actor.modelVariant))){error="Invalid recorded bot model variant";return false;}stream.write(reinterpret_cast<const char*>(actor.pose.data()),static_cast<std::streamsize>(actor.pose.size()*sizeof(scene::Mat4)));if(!stream){error="Could not write recorded bot pose data";return false;}}
        const auto hiddenCount=static_cast<std::uint32_t>(sample.hiddenBones.size());if(hiddenCount>kMaxBones||!writeValue(stream,hiddenCount)){error="Could not write recorded visibility";return false;}for(const auto bone:sample.hiddenBones)if(bone>=sampleBones||!writeValue(stream,bone)){error="Recorded visibility contains an invalid bone";return false;}
        if(outputVersion>=15){
            if(!validVisibility(sample.visibility)||!validVisibility(sample.worldActor.visibility)||!writeValue(stream,sample.visibility)||!writeValue(stream,sample.worldActor.visibility)){error="Invalid recorded actor visibility";return false;}
            for(const auto& bot:sample.bots)if(!validVisibility(bot.visibility)||!writeValue(stream,bot.visibility)){error="Invalid recorded bot visibility";return false;}
        }
    }
    const auto shotCount=static_cast<std::uint32_t>(take.shots.size());
    if(!writeValue(stream,shotCount)){error="Could not write shot event count";return false;}
    for(const auto& shot:take.shots){
        if(!writeValue(stream,shot.time)||!writeValue(stream,shot.origin)||!writeValue(stream,shot.direction)||!writeValue(stream,shot.muzzlePos)||!writeValue(stream,shot.hitPos))return false;
        const std::uint8_t hitFound=shot.hitFound?1u:0u,sniper=shot.sniperWeapon?1u:0u,trailEmissive=shot.trailEmissive?1u:0u,trailSprite=shot.trailSprite?1u:0u,smokeEnabled=shot.smokeEnabled?1u:0u;
        if(!writeValue(stream,hitFound)||!writeValue(stream,sniper)||!writeValue(stream,shot.muzzleFlashDuration)||!writeValue(stream,shot.muzzleFlashSize)||!writeValue(stream,shot.muzzleFlashRotation)||!writeValue(stream,shot.muzzleFlashColor)||!writeValue(stream,shot.muzzleFlashCurvePower)||!writeValue(stream,trailEmissive)||!writeValue(stream,shot.trailStartTaper)||!writeValue(stream,shot.trailEndTaper)||!writeValue(stream,trailSprite)||!writeValue(stream,shot.trailLifetime)||!writeValue(stream,shot.trailWidth)||!writeValue(stream,shot.trailColor)||!writeValue(stream,shot.trailEmissiveIntensity)||!writeValue(stream,shot.trailFeathering)||!writeValue(stream,shot.trailSpeed)||!writeValue(stream,shot.trailLength)||!writeValue(stream,smokeEnabled)||!writeValue(stream,shot.smokeEmissionDuration)||!writeValue(stream,shot.smokeLifetime)||!writeValue(stream,shot.smokeStartWidth)||!writeValue(stream,shot.smokeEndWidth)||!writeValue(stream,shot.smokeTaperingExp)||!writeValue(stream,shot.smokeBlastSpeed)||!writeValue(stream,shot.smokeRiseSpeed)||!writeValue(stream,shot.smokeDispersion)||!writeValue(stream,shot.smokeColor)){error="Could not write shot event data";return false;}
        if(outputVersion>=12){std::ostringstream curve;curve<<shot.trailTaperCurve;if(!writeString(stream,curve.str())){error="Could not write trail curve";return false;}}
    }
    stream.flush();if(!stream){error="Could not flush the take file";return false;}
    stream.close();if(!stream){error="Could not close the take file";return false;}
    return true;
}

bool save(const Take& take,const std::filesystem::path& path,std::string& error){
    try{return detail::atomicFile(path,error,[&](const auto& staged){return saveStaged(take,staged,error);});}
    catch(const std::exception& e){error=std::string("Take save failed; original preserved: ")+e.what();return false;}
}

static bool loadChecked(const std::filesystem::path& path,Take& take,std::string& error){
    error.clear();std::ifstream stream(path,std::ios::binary);if(!stream){error="Could not open the take file";return false;}
    std::array<char,8> magic{};stream.read(magic.data(),magic.size());std::uint32_t version{},bones{},count{};float sampleRate{};
    if(!stream||magic!=kMagic||!readValue(stream,version)||(version<1||version>kVersion)||!readValue(stream,sampleRate)||!readValue(stream,bones)||!readValue(stream,count)){
        error="Invalid or unsupported take header";return false;
    }
    if(bones==0||bones>kMaxBones||count==0||count>kMaxSamples||sampleRate<1.0f||sampleRate>240.0f){error="Take dimensions are outside supported limits";return false;}
    const std::size_t slotCount=(version==11||version>=13)?3u:2u;
    Take imported;imported.sampleRate=sampleRate;imported.boneCount=bones;if(version>=2&&!readActorManifest(stream,imported.actor,version)){error="Take actor manifest is malformed";return false;}if(version>=3){std::uint32_t botBones{},botCount{};if(!readActorManifest(stream,imported.botActor,version)||!readValue(stream,botBones)||!readValue(stream,botCount)||botBones>kMaxBones||botCount>kMaxActors||(botCount>0&&(botBones==0||imported.botActor.empty()))){error="Take bot actor manifest is malformed";return false;}imported.botBoneCount=botBones;imported.botCount=botCount;}if(version>=4&&!readCameraKeys(stream,imported.dollyCamera)){error="Take dolly camera keyframes are malformed";return false;}if(version>=5){std::uint32_t worldBones{};if(!readActorManifest(stream,imported.worldActor,version)||!readValue(stream,worldBones)||worldBones>kMaxBones||(worldBones>0&&imported.worldActor.empty())){error="Take world actor manifest is malformed";return false;}imported.worldBoneCount=worldBones;}if(version>=6){for(std::size_t i=0;i<slotCount;++i)if(!readActorManifest(stream,imported.actorSlots[i],version)){error="Take class viewmodel manifest is malformed";return false;}for(std::size_t i=0;i<slotCount;++i)if(!readActorManifest(stream,imported.worldActorSlots[i],version)){error="Take class world-model manifest is malformed";return false;}}else{imported.actorSlots[0]=imported.actor;imported.worldActorSlots[0]=imported.worldActor;}if(version>=7){for(std::size_t slot=0;slot<slotCount;++slot){auto& slotBones=imported.actorSlotBoneCounts[slot];std::uint32_t value{};if(!readValue(stream,value)||value==0||value>kMaxBones){error="Take class skeleton sizes are malformed";return false;}slotBones=value;}std::uint32_t pathCount{};if(!readValue(stream,pathCount)||pathCount>kMaxCameraPaths){error="Take campath collection is malformed";return false;}imported.cameraPaths.resize(pathCount);for(auto& pathSet:imported.cameraPaths)if(!readString(stream,pathSet.name)||!readCameraKeys(stream,pathSet.keyframes)){error="A named campath is malformed";return false;}}else imported.actorSlotBoneCounts={bones,bones};
    if(version>=9){for(std::size_t slot=0;slot<slotCount;++slot){auto& slotBones=imported.worldActorSlotBoneCounts[slot];std::uint32_t value{};if(!readValue(stream,value)||value>kMaxBones){error="Take class world skeleton sizes are malformed";return false;}slotBones=value;}}else imported.worldActorSlotBoneCounts={imported.worldBoneCount,imported.worldBoneCount};
    std::uint64_t minimumBones=kMaxBones;
    for(std::size_t slot=0;slot<slotCount;++slot)minimumBones=std::min(minimumBones,static_cast<std::uint64_t>(imported.bonesForSlot(static_cast<std::uint8_t>(slot))));
    // A lower bound valid for all versions; actual records also contain strings/world poses.
    const std::uint64_t minimumSampleBytes=44u+(minimumBones+static_cast<std::uint64_t>(imported.botCount)*imported.botBoneCount)*sizeof(scene::Mat4);
    if(!bytesAvailable(stream,minimumSampleBytes*count)){error="Take sample count exceeds the data available";return false;}
    imported.samples.reserve(std::min(count,4096u));float previous=-1.0f;
    for(std::uint32_t i=0;i<count;++i){
        Sample sample;
        if(!readValue(stream,sample.time)||!readValue(stream,sample.camera.target)||!readValue(stream,sample.camera.yaw)||!readValue(stream,sample.camera.pitch)||
           !readValue(stream,sample.camera.distance)){error="Take sample metadata is malformed";return false;}
        if(version>=8){
            if(!readValue(stream,sample.camera.fov)||!readValue(stream,sample.camera.adsBlend)){error="Take camera FOV metadata is malformed";return false;}
        }
        if(!readValue(stream,sample.primaryFrame)||!readValue(stream,sample.torsoFrame)||
           !readString(stream,sample.primaryClip)||!readString(stream,sample.torsoClip)||sample.time<previous||(version>=7&&(!readValue(stream,sample.weaponSlot)||sample.weaponSlot>=slotCount))){error="Take sample metadata is malformed";return false;}
        if(sample.time<0||!readPose(stream,sample.pose,imported.bonesForSlot(sample.weaponSlot))){error="Take pose data is truncated or non-finite";return false;}
        const auto expectedWorldBones=imported.bonesForWorldSlot(sample.weaponSlot);
        if(version>=5&&expectedWorldBones>0){auto& actor=sample.worldActor;std::uint8_t alive{};if(!readValue(stream,actor.id)||!readValue(stream,actor.health)||!readValue(stream,alive)||alive>1||!readValue(stream,actor.primaryFrame)||!readValue(stream,actor.torsoFrame)||!readString(stream,actor.primaryClip)||!readString(stream,actor.torsoClip)){error="Take world actor metadata is malformed";return false;}actor.alive=alive!=0;if(!readPose(stream,actor.pose,expectedWorldBones)){error="Take world actor pose data is truncated";return false;}}
        if(version>=3){sample.bots.resize(imported.botCount);for(auto& actor:sample.bots){std::uint8_t alive{};if(!readValue(stream,actor.id)||!readValue(stream,actor.health)||!readValue(stream,alive)||alive>1||!readValue(stream,actor.primaryFrame)||!readValue(stream,actor.torsoFrame)||!readString(stream,actor.primaryClip)||!readString(stream,actor.torsoClip)){error="Take bot metadata is malformed";return false;}if(version>=14&&(!readValue(stream,actor.modelVariant)||actor.modelVariant<0||actor.modelVariant>=static_cast<std::int32_t>(kMaxActors))){error="Take bot variant is malformed";return false;}actor.alive=alive!=0;if(!readPose(stream,actor.pose,imported.botBoneCount)){error="Take bot pose data is truncated";return false;}}}
        if(version>=4){std::uint32_t hiddenCount{};if(!readValue(stream,hiddenCount)||hiddenCount>kMaxBones){error="Take visibility data is malformed";return false;}sample.hiddenBones.resize(hiddenCount);for(auto& bone:sample.hiddenBones)if(!readValue(stream,bone)||bone>=sample.pose.size()){error="Take visibility data contains an invalid bone";return false;}}if(version>=6&&version<7&&(!readValue(stream,sample.weaponSlot)||sample.weaponSlot>=slotCount)){error="Take class-slot data is malformed";return false;}
        if(version>=15){
            if(!readValue(stream,sample.visibility)||!validVisibility(sample.visibility)||!readValue(stream,sample.worldActor.visibility)||!validVisibility(sample.worldActor.visibility)){error="Malformed actor visibility";return false;}
            for(auto& bot:sample.bots)if(!readValue(stream,bot.visibility)||!validVisibility(bot.visibility)){error="Malformed bot visibility";return false;}
        }
        previous=sample.time;imported.samples.push_back(std::move(sample));
    }
    if(version>=10){
        std::uint32_t shotCount{};
        if(!readValue(stream,shotCount)||shotCount>100000u){error="Take shot event count is malformed";return false;}
        imported.shots.reserve(shotCount);
        for(std::uint32_t s=0;s<shotCount;++s){
            ShotEvent shot;
            std::uint8_t hitFound{},sniper{},trailEmissive{},trailSprite{},smokeEnabled{};
            if(!readValue(stream,shot.time)||!readValue(stream,shot.origin)||!readValue(stream,shot.direction)||!readValue(stream,shot.muzzlePos)||!readValue(stream,shot.hitPos)||!readValue(stream,hitFound)||!readValue(stream,sniper)||!readValue(stream,shot.muzzleFlashDuration)||!readValue(stream,shot.muzzleFlashSize)||!readValue(stream,shot.muzzleFlashRotation)||!readValue(stream,shot.muzzleFlashColor)||!readValue(stream,shot.muzzleFlashCurvePower)||!readValue(stream,trailEmissive)||!readValue(stream,shot.trailStartTaper)||!readValue(stream,shot.trailEndTaper)||!readValue(stream,trailSprite)||!readValue(stream,shot.trailLifetime)||!readValue(stream,shot.trailWidth)||!readValue(stream,shot.trailColor)||!readValue(stream,shot.trailEmissiveIntensity)||!readValue(stream,shot.trailFeathering)||!readValue(stream,shot.trailSpeed)||!readValue(stream,shot.trailLength)||!readValue(stream,smokeEnabled)||!readValue(stream,shot.smokeEmissionDuration)||!readValue(stream,shot.smokeLifetime)||!readValue(stream,shot.smokeStartWidth)||!readValue(stream,shot.smokeEndWidth)||!readValue(stream,shot.smokeTaperingExp)||!readValue(stream,shot.smokeBlastSpeed)||!readValue(stream,shot.smokeRiseSpeed)||!readValue(stream,shot.smokeDispersion)||!readValue(stream,shot.smokeColor)){error="Take shot event data is truncated or malformed";return false;}
            shot.hitFound=(hitFound!=0);shot.sniperWeapon=(sniper!=0);shot.trailEmissive=(trailEmissive!=0);shot.trailSprite=(trailSprite!=0);shot.smokeEnabled=(smokeEnabled!=0);
            shot.trailTaperCurve=gameplay::view::ResponseCurve::linear(shot.trailStartTaper,shot.trailEndTaper);
            if(version>=12){std::string curve;if(!readString(stream,curve)){error="Missing trail curve";return false;}std::istringstream input(curve);if(!(input>>shot.trailTaperCurve)){error="Malformed trail curve";return false;}}
            imported.shots.push_back(shot);
        }
    }
    take=std::move(imported);return true;
}

bool load(const std::filesystem::path& path,Take& take,std::string& error){
    try{return loadChecked(path,take,error);}
    catch(const std::exception& e){error=std::string("Take load failed; current take preserved: ")+e.what();return false;}
}

} // namespace take
