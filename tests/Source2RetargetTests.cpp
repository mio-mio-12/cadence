#include "assets/LocalAssetPaths.h"
#include "cast/CastDocument.h"
#include "scene/CastScene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::size_t bone(const scene::Skeleton& skeleton,std::string name) {
    const auto has=[&](const char* n){return std::any_of(skeleton.bones.begin(),skeleton.bones.end(),[&](const auto& b){return b.name==n;});};
    if(!has("j_index_le_0")&&has("j_index_le_1"))for(const auto finger:{"index","mid","ring","pinky","thumb"}){
        if(name.starts_with("j_"+std::string(finger)+"_")&&name.back()>='0'&&name.back()<='3')++name.back();
    }
    for(std::size_t i=0;i<skeleton.bones.size();++i)
        if(skeleton.bones[i].name==name)return i;
    throw std::runtime_error("Missing actual bone: "+name);
}
bool descendantOf(const scene::Skeleton& skeleton,std::size_t child,std::size_t ancestor) {
    for(std::size_t depth=0;depth<skeleton.bones.size();++depth){
        const auto parent=skeleton.bones[child].parent;
        if(parent<0)return false;
        child=static_cast<std::size_t>(parent);
        if(child==ancestor)return true;
    }
    return false;
}
template<class T> bool bytesEqual(const T& a,const T& b) {
    return std::memcmp(&a,&b,sizeof(T))==0;
}
template<class T> bool vectorBytesEqual(const std::vector<T>& a,const std::vector<T>& b) {
    return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);
}
bool sameTrack(const scene::Track& a,const scene::Track& b) {
    return a.boneIndex==b.boneIndex&&a.property==b.property&&a.mode==b.mode&&
        bytesEqual(a.additiveWeight,b.additiveWeight)&&vectorBytesEqual(a.frames,b.frames)&&
        vectorBytesEqual(a.scalarValues,b.scalarValues)&&vectorBytesEqual(a.rotationValues,b.rotationValues);
}
bool sameTracks(const scene::Animation& a,const scene::Animation& b) {
    return a.tracks.size()==b.tracks.size()&&std::equal(a.tracks.begin(),a.tracks.end(),b.tracks.begin(),sameTrack);
}
bool sameBind(const scene::Skeleton& a,const scene::Skeleton& b) {
    if(a.bones.size()!=b.bones.size())return false;
    for(std::size_t i=0;i<a.bones.size();++i){
        const auto& x=a.bones[i];const auto& y=b.bones[i];
        if(x.name!=y.name||x.parent!=y.parent||!bytesEqual(x.restLocal.position,y.restLocal.position)||
           !bytesEqual(x.restLocal.rotation,y.restLocal.rotation)||!bytesEqual(x.restLocal.scale,y.restLocal.scale)||
           !bytesEqual(x.restGlobal,y.restGlobal)||!bytesEqual(x.inverseBind,y.inverseBind))return false;
    }
    return true;
}
scene::Vec3 position(const scene::Mat4& value){return {value.v[12],value.v[13],value.v[14]};}
scene::Quat rotation(const scene::Mat4& value){scene::Vec3 p,s;scene::Quat q;scene::decomposeAffine(value,p,q,s);return q;}
float quaternionError(scene::Quat a,scene::Quat b){
    a=scene::normalize(a);b=scene::normalize(b);
    return 1.0f-std::min(1.0f,std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w));
}
float armLength(const scene::Skeleton& skeleton,bool source){
    float length{};
    for(const std::string side:{"L","R"}){
        const auto targetSide=side=="L"?"le":"ri";
        length+=scene::length(skeleton.bones[bone(skeleton,source?"arm_lower_"+side:"j_elbow_"+std::string(targetSide))].restLocal.position);
        length+=scene::length(skeleton.bones[bone(skeleton,source?"hand_"+side:"j_wrist_"+std::string(targetSide))].restLocal.position);
    }
    return length;
}
cast::Document load(const std::filesystem::path& path){
    auto result=cast::Document::load(path);
    if(!result.valid())throw std::runtime_error("Required asset unavailable: "+path.string());
    return result;
}
void require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
struct Clip {const char* weapon;const char* file;};
}

int main(int argc,char** argv){
    const std::filesystem::path root=argc>1?std::filesystem::u8path(argv[1]):
        std::filesystem::path{cadence::local_assets::exportPath("")};
    try {
        for(const auto weapon:{"ak47","awp","deagle","knife_karambit","knife_butterfly"}){
            const auto materialScene=scene::buildScene(load(root/"cs2"/"models"/"weapons"/weapon/(std::string(weapon)+"_model.cast")));
            bool checked=false;for(const auto& mesh:materialScene.meshes)if(!mesh.albedoPath.empty()){
                require(mesh.albedoPath.stem().string().ends_with("_color"),std::string(weapon)+": diffuse is not the color texture");
                require(std::filesystem::exists(mesh.metalnessPath),std::string(weapon)+": metalness map missing");
                require(std::filesystem::exists(mesh.roughnessPath),std::string(weapon)+": roughness map missing");
                require(mesh.ignoreAlbedoAlpha,std::string(weapon)+": opaque diffuse must ignore auxiliary alpha");checked=true;
            }require(checked,std::string(weapon)+": no textured material tested");
        }
        const auto targetDocument=load(argc>2?std::filesystem::path(argv[2]):root/R"(bo2\models\viewhands\seal6\c_usa_mp_seal6_longsleeve_viewhands\c_usa_mp_seal6_longsleeve_viewhands_LOD0.cast)");
        const auto sourceDocument=load(root/R"(cs2\models\viewhands\viewhands_model.cast)");
        const auto targetBase=scene::buildScene(targetDocument),sourceBase=scene::buildScene(sourceDocument);
        const auto targetOrigin=position(targetBase.skeleton.bones[bone(targetBase.skeleton,"tag_camera")].restGlobal);
        const float scale=armLength(targetBase.skeleton,false)/armLength(sourceBase.skeleton,true);
        const Clip clips[]={
            {"awp","idle_awp.cast"},{"awp","shoot1_awp.cast"},{"awp","reload_awp.cast"},
            {"knife_karambit","idle1_karambit.cast"},{"knife_karambit","draw_karambit.cast"},
            {"knife_karambit","lookat01_karambit.cast"},
            {"knife_butterfly","idle1_butterfly.cast"},{"knife_butterfly","draw_butterfly.cast"},
            {"knife_butterfly","lookat01_butterfly.cast"},{"knife_butterfly","light_miss1_butterfly.cast"}
        };
        std::size_t samples{},mechanismTracks{},weaponPoseChecks{};
        std::vector<bool> fingerMoved(30,false);
        for(const auto& clip:clips){
            const std::string label=clip.file;
            const auto animationDocument=load(root/"cs2"/"animations"/"viewmodel"/clip.weapon/clip.file);
            const auto weaponDocument=load(root/"cs2"/"models"/"weapons"/clip.weapon/(std::string(clip.weapon)+"_model.cast"));
            auto source=sourceBase,target=targetBase;
            scene::appendRigModel(weaponDocument,source,clip.weapon,scene::Mat4::identity(),1.0f);
            scene::appendRigModel(weaponDocument,target,clip.weapon,scene::Mat4::identity(),scale);
            std::vector<std::pair<std::size_t,std::size_t>> weaponBones;
            const auto targetSocket=bone(target.skeleton,"tag_weapon"),sourceSocket=bone(source.skeleton,"wpn");
            for(std::size_t t=0;t<target.skeleton.bones.size();++t){
                if(!descendantOf(target.skeleton,t,targetSocket))continue;
                // Match actual exported names; alias maps can identify unrelated hand bones.
                const auto& name=target.skeleton.bones[t].name;
                const auto found=std::find_if(source.skeleton.bones.begin(),source.skeleton.bones.end(),[&](const auto& b){return b.name==name;});
                if(found==source.skeleton.bones.end())continue;
                const auto s=static_cast<std::size_t>(found-source.skeleton.bones.begin());
                require(descendantOf(source.skeleton,s,sourceSocket),label+": common weapon bone has wrong native parent: "+name);
                weaponBones.emplace_back(t,s);
                // Uniform model scaling scales the inverse-bind translation, not its 3x3 basis.
                // The vertex is scaled separately before applying this inverse bind.
                for(std::size_t component=0;component<16;++component){
                    const float expected=found->inverseBind.v[component]*(component>=12&&component<=14?scale:1.0f);
                    require(std::abs(target.skeleton.bones[t].inverseBind.v[component]-expected)<0.0001f,label+": scaled inverse bind mismatch: "+name);
                }
            }
            require(!weaponBones.empty(),label+": no actual common weapon descendants tested");
            // A sentinel clip exercises a nonzero range offset and prefix preservation.
            scene::Animation sentinel;sentinel.name="untouched prefix";sentinel.durationFrames=7;
            target.animations.push_back(sentinel);source.animations.push_back(sentinel);
            const auto first=target.animations.size(),firstSource=source.animations.size();
            require(scene::appendAnimations(animationDocument,source)==1,label+": source import");
            require(scene::appendAnimations(animationDocument,target)==1,label+": target import");
            const auto before=target.animations[first];const auto bind=target.skeleton;
            auto native=source;
            require(!scene::retargetSource2AnimationRange(native,firstSource,source,firstSource),label+": native CS2 rejected");
            require(sameBind(native.skeleton,source.skeleton)&&sameTracks(native.animations[firstSource],source.animations[firstSource]),label+": native rejection leaves bind and animation unchanged");
            auto nativeBo2=target;
            require(!scene::retargetSource2AnimationRange(nativeBo2,first,target,first),label+": native BO2 rejected");
            require(sameBind(nativeBo2.skeleton,target.skeleton)&&sameTracks(nativeBo2.animations[first],before)&&
                nativeBo2.animations[first].durationFrames==before.durationFrames&&nativeBo2.animations[first].looping==before.looping&&
                nativeBo2.animations[first].framerate==before.framerate,label+": native BO2 rejection leaves bind and animation unchanged");
            require(scene::retargetSource2AnimationRange(target,first,source,firstSource),label+": retarget accepted");
            require(sameBind(bind,target.skeleton),label+": every target bind component remains byte identical");
            require(target.animations[0].name==sentinel.name&&target.animations[0].durationFrames==7&&target.animations[0].tracks.empty(),label+": prefix unchanged");
            const auto& output=target.animations[first];const auto& input=source.animations[firstSource];
            require(output.durationFrames==input.durationFrames&&output.framerate==input.framerate&&output.looping==input.looping,label+": timing preserved");
            // Every imported property must still be represented after hand tracks are replaced.
            for(const auto& old:before.tracks)require(std::any_of(output.tracks.begin(),output.tracks.end(),[&](const auto& t){return t.boneIndex==old.boneIndex&&t.property==old.property;}),label+": imported track disappeared");
            // Weapon mechanisms are not hand-retarget inputs; their original quaternion keys must survive exactly.
            for(const auto& old:before.tracks){
                const auto& name=target.skeleton.bones[old.boneIndex].name;
                if(old.property!=scene::TrackProperty::Rotation||name.starts_with("j_")||name.starts_with("tag_"))continue;
                const auto found=std::find_if(output.tracks.begin(),output.tracks.end(),[&](const auto& t){return t.boneIndex==old.boneIndex&&t.property==old.property;});
                require(found!=output.tracks.end()&&sameTrack(old,*found),label+": mechanism keys changed: "+name);
                const auto sourceBone=bone(source.skeleton,name);
                const auto sourceTrack=std::find_if(input.tracks.begin(),input.tracks.end(),[&](const auto& t){return t.boneIndex==sourceBone&&t.property==old.property;});
                require(sourceTrack!=input.tracks.end()&&vectorBytesEqual(found->frames,sourceTrack->frames)&&vectorBytesEqual(found->rotationValues,sourceTrack->rotationValues),label+": mechanism keys differ from source: "+name);
                ++mechanismTracks;
            }
            std::vector<std::size_t> fingers;
            for(const std::string side:{"le","ri"})for(const std::string finger:{"index","mid","ring","pinky","thumb"})for(int joint=0;joint<3;++joint){
                const auto index=bone(target.skeleton,"j_"+finger+"_"+side+"_"+std::to_string(joint));fingers.push_back(index);
                const auto found=std::find_if(output.tracks.begin(),output.tracks.end(),[&](const auto& t){return t.boneIndex==index&&t.property==scene::TrackProperty::Rotation;});
                require(found!=output.tracks.end()&&!found->frames.empty()&&!found->rotationValues.empty(),label+": missing finger rotation");
            }
            // Disable wrap only after checking metadata, so the last frame is evaluated literally.
            target.animations[first].looping=false;source.animations[firstSource].looping=false;
            const auto initial=target.sampleLocalPose(first,0);
            for(std::uint32_t frame=0;frame<=input.durationFrames;++frame){
                const auto pose=target.samplePose(first,static_cast<float>(frame));
                const auto sourcePose=source.samplePose(firstSource,static_cast<float>(frame));
                const auto local=target.sampleLocalPose(first,static_cast<float>(frame));
                for(const auto& matrix:pose)for(const auto value:matrix.v)require(std::isfinite(value),label+": nonfinite pose at "+std::to_string(frame));
                for(const auto& side:std::vector<std::pair<std::string,std::string>>{{"le","L"},{"ri","R"}}){
                    const auto tp=position(pose[bone(target.skeleton,"j_wrist_"+side.first)]);
                    const auto sp=position(sourcePose[bone(source.skeleton,"hand_"+side.second)])*scale+targetOrigin;
                    require(scene::length(tp-sp)<0.003f,label+": wrist trajectory mismatch at "+std::to_string(frame));
                }
                require(quaternionError(rotation(pose[bone(target.skeleton,"tag_weapon")]),rotation(sourcePose[bone(source.skeleton,"wpn")]))<0.00001f,label+": weapon socket rotation mismatch at "+std::to_string(frame));
                for(const auto& [t,s]:weaponBones){
                    const auto context=label+": "+target.skeleton.bones[t].name+" at frame "+std::to_string(frame);
                    require(scene::length(position(pose[t])-position(sourcePose[s])*scale-targetOrigin)<0.003f,context+": weapon pivot position mismatch");
                    require(quaternionError(rotation(pose[t]),rotation(sourcePose[s]))<0.00001f,context+": weapon orientation mismatch");
                    const auto targetSkin=pose[t]*target.skeleton.bones[t].inverseBind;
                    const auto sourceSkin=sourcePose[s]*source.skeleton.bones[s].inverseBind;
                    // Origin plus non-collinear points exercise both the pivot and geometry basis.
                    for(const scene::Vec3 point: {scene::Vec3{0,0,0},scene::Vec3{1,2,3},scene::Vec3{-3,1,-2}})
                        require(scene::length(scene::transformPoint(targetSkin,point*scale)-scene::transformPoint(sourceSkin,point)*scale-targetOrigin)<0.005f,context+": scaled skinned geometry mismatch");
                    ++weaponPoseChecks;
                }
                for(std::size_t i=0;i<fingers.size();++i)if(quaternionError(local[fingers[i]].rotation,initial[fingers[i]].rotation)>0.000001f)fingerMoved[i]=true;
                ++samples;
            }
            std::cout<<"PASS "<<label<<" ("<<input.durationFrames+1<<" frames)\n";
        }
        require(mechanismTracks>0,"No weapon mechanism rotation tracks were exercised");
        for(std::size_t i=0;i<fingerMoved.size();++i)require(fingerMoved[i],"Finger joint never animated across action clips: "+std::to_string(i));
        std::cout<<"All Source 2 retarget tests passed: 10 clips, "<<samples<<" full-frame samples, "<<mechanismTracks<<" mechanism tracks, "<<weaponPoseChecks<<" weapon descendant poses.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}
}
