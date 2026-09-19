#include "cast/CastDocument.h"
#include "scene/CastScene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
using scene::Mat4;using scene::Quat;using scene::Vec3;
struct Clip { const char* family;const char* file; };
Quat inverse(Quat value){return {-value.x,-value.y,-value.z,value.w};}
Quat rotationOf(const Mat4& value){Vec3 position,scale;Quat rotation;scene::decomposeAffine(value,position,rotation,scale);return rotation;}
Vec3 positionOf(const Mat4& value){return {value.v[12],value.v[13],value.v[14]};}
float angle(Vec3 a,Vec3 b){if(scene::length(a)<1e-6f||scene::length(b)<1e-6f)return 0;return std::acos(std::clamp(scene::dot(scene::normalize(a),scene::normalize(b)),-1.0f,1.0f))*180.0f/scene::kPi;}
float rotationError(Quat a,Quat b){a=scene::normalize(a);b=scene::normalize(b);const float value=std::clamp(std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w),0.0f,1.0f);return 2.0f*std::acos(value)*180.0f/scene::kPi;}
std::optional<std::size_t> mapped(const scene::Skeleton& source,const scene::Bone& target){if(const auto exact=source.boneByName.find(target.name);exact!=source.boneByName.end())return exact->second;if(const auto canonical=source.boneByCanonicalName.find(scene::canonicalName(target.name));canonical!=source.boneByCanonicalName.end())return canonical->second;return std::nullopt;}
bool finite(const Mat4& value){return std::all_of(value.v.begin(),value.v.end(),[](float item){return std::isfinite(item);});}
}

int main(int argc,char** argv){
    const std::filesystem::path root=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::path{R"(D:\Editing\COD Resource\3D Rip\saluki\exported_files)"};
    const auto sourceDocument=cast::Document::load(root/R"(cs2\models\viewhands\viewhands_model.cast)");
    const auto targetDocument=cast::Document::load(root/R"(bo2\models\viewhands\seal6\c_usa_mp_seal6_longsleeve_viewhands\c_usa_mp_seal6_longsleeve_viewhands_LOD0.cast)");
    if(!sourceDocument.valid()||!targetDocument.valid()){std::cerr<<"Missing authoritative CS2 or BO2 viewhands\n";return 2;}
    auto sourceBuilt=scene::buildScene(sourceDocument);auto targetBuilt=scene::buildScene(targetDocument);scene::Skeleton sourceSkeleton=sourceBuilt.skeleton;scene::Skeleton targetSkeleton=targetBuilt.skeleton;
    static constexpr Clip clips[]={{"ak47","idle_ak.cast"},{"ak47","shoot1_ak.cast"},{"ak47","reload_ak.cast"},{"awp","idle_awp.cast"},{"awp","shoot1_awp.cast"},{"knife_karambit","idle1_karambit.cast"},{"knife_karambit","draw_karambit.cast"},{"knife_karambit","lookat01_karambit.cast"},{"knife_butterfly","idle1_butterfly.cast"},{"knife_butterfly","draw_butterfly.cast"},{"knife_talon","idle1_talon.cast"},{"knife_talon","draw_talon.cast"}};
    double directionTotal{},fingerDirectionTotal{},deformationTotal{},gripTotal{};float directionMaximum{},fingerDirectionMaximum{},deformationMaximum{},gripMaximum{};std::size_t directionCount{},fingerDirectionCount{},deformationCount{},gripCount{},sampledClips{},sampledFrames{};bool allFinite=true;
    for(const auto& request:clips){
        const auto animationDocument=cast::Document::load(root/"cs2"/"animations"/"viewmodel"/request.family/request.file);if(!animationDocument.valid()){std::cout<<"SKIP "<<request.file<<'\n';continue;}
        scene::CastScene source,target;source.skeleton=sourceSkeleton;target.skeleton=targetSkeleton;const auto sourceAnimation=source.animations.size(),targetAnimation=target.animations.size();scene::appendAnimations(animationDocument,source);scene::appendAnimations(animationDocument,target);if(sourceAnimation>=source.animations.size()||targetAnimation>=target.animations.size()||!scene::installRuntimePoseAdapter(target,targetAnimation,source,sourceAnimation)){std::cerr<<"Adapter installation failed for "<<request.file<<'\n';return 3;}
        const auto& adapter=target.runtimePoseAdapters.at(targetAnimation);const auto duration=std::max<std::uint32_t>(1,target.animations[targetAnimation].durationFrames);const auto step=std::max<std::uint32_t>(1,duration/20);
        for(std::uint32_t frame=0;frame<=duration;frame=std::min(duration,frame+step)){
            const auto sourcePose=source.samplePose(sourceAnimation,static_cast<float>(frame)),targetPose=target.samplePose(targetAnimation,static_cast<float>(frame));++sampledFrames;allFinite&=std::all_of(targetPose.begin(),targetPose.end(),finite);
            for(const char* side:{"le","ri"})for(const auto pair:{std::pair{std::string{"j_shoulder_"}+side,std::string{"j_elbow_"}+side},std::pair{std::string{"j_elbow_"}+side,std::string{"j_wrist_"}+side}}){const auto ta=target.skeleton.boneByCanonicalName.find(pair.first),tb=target.skeleton.boneByCanonicalName.find(pair.second);if(ta==target.skeleton.boneByCanonicalName.end()||tb==target.skeleton.boneByCanonicalName.end())continue;const auto sa=mapped(source.skeleton,target.skeleton.bones[ta->second]),sb=mapped(source.skeleton,target.skeleton.bones[tb->second]);if(!sa||!sb)continue;const auto error=angle(positionOf(sourcePose[*sb])-positionOf(sourcePose[*sa]),positionOf(targetPose[tb->second])-positionOf(targetPose[ta->second]));directionTotal+=error;directionMaximum=std::max(directionMaximum,error);++directionCount;}
            for(std::size_t bone=0;bone<target.skeleton.bones.size();++bone){const auto name=scene::canonicalName(target.skeleton.bones[bone].name);if(!(name.starts_with("j_thumb_")||name.starts_with("j_index_")||name.starts_with("j_mid_")||name.starts_with("j_ring_")||name.starts_with("j_pinky_")||name.starts_with("j_ringpalm_")||name.starts_with("j_pinkypalm_")))continue;for(std::size_t child=0;child<target.skeleton.bones.size();++child)if(target.skeleton.bones[child].parent==static_cast<std::int32_t>(bone)){const auto sourceBone=mapped(source.skeleton,target.skeleton.bones[bone]),sourceChild=mapped(source.skeleton,target.skeleton.bones[child]);if(!sourceBone||!sourceChild)continue;const auto error=angle(positionOf(sourcePose[*sourceChild])-positionOf(sourcePose[*sourceBone]),positionOf(targetPose[child])-positionOf(targetPose[bone]));fingerDirectionTotal+=error;fingerDirectionMaximum=std::max(fingerDirectionMaximum,error);++fingerDirectionCount;}}
            for(std::size_t bone=0;bone<target.skeleton.bones.size();++bone){if(bone>=adapter.adaptedBones.size()||!adapter.adaptedBones[bone]||!adapter.sourceBoneForTarget[bone])continue;const auto sourceBone=*adapter.sourceBoneForTarget[bone];const auto sourceDelta=scene::normalize(scene::multiply(rotationOf(sourcePose[sourceBone]),inverse(rotationOf(source.skeleton.bones[sourceBone].restGlobal)))),targetDelta=scene::normalize(scene::multiply(rotationOf(targetPose[bone]),inverse(adapter.alignedRestGlobalRotations[bone])));const auto error=rotationError(sourceDelta,targetDelta);deformationTotal+=error;deformationMaximum=std::max(deformationMaximum,error);++deformationCount;}
            const auto tw=target.skeleton.boneByCanonicalName.find("tag_weapon");if(tw!=target.skeleton.boneByCanonicalName.end()&&adapter.sourceBoneForTarget[tw->second])for(const char* side:{"le","ri"}){const auto wrist=target.skeleton.boneByCanonicalName.find(std::string{"j_wrist_"}+side);if(wrist==target.skeleton.boneByCanonicalName.end()||!adapter.sourceBoneForTarget[wrist->second])continue;const auto expected=(positionOf(sourcePose[*adapter.sourceBoneForTarget[wrist->second]])-positionOf(sourcePose[*adapter.sourceBoneForTarget[tw->second]]))*adapter.translationScale;const auto actual=positionOf(targetPose[wrist->second])-positionOf(targetPose[tw->second]);const auto error=scene::length(expected-actual);gripTotal+=error;gripMaximum=std::max(gripMaximum,error);++gripCount;}
            if(frame==duration)break;
        }
        std::cout<<"CLIP "<<request.file<<" frames="<<duration<<'\n';++sampledClips;
    }
    std::cout<<"POSE_ADAPTER_SUMMARY clips="<<sampledClips<<" frames="<<sampledFrames<<" arm_direction_avg="<<(directionCount?directionTotal/directionCount:0)<<" arm_direction_max="<<directionMaximum<<" finger_direction_avg="<<(fingerDirectionCount?fingerDirectionTotal/fingerDirectionCount:0)<<" finger_direction_max="<<fingerDirectionMaximum<<" deformation_avg="<<(deformationCount?deformationTotal/deformationCount:0)<<" deformation_max="<<deformationMaximum<<" grip_avg="<<(gripCount?gripTotal/gripCount:0)<<" grip_max="<<gripMaximum<<" finite="<<allFinite<<'\n';
    if(sampledClips<8||!allFinite||directionMaximum>1.0f||fingerDirectionMaximum>1.0f||deformationMaximum>.1f||gripMaximum>.05f)return 4;return 0;
}
