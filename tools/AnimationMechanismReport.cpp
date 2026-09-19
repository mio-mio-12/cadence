#include "cast/CastDocument.h"
#include "scene/CastScene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace {
float quatErrorDegrees(scene::Quat a,scene::Quat b){
    a=scene::normalize(a);b=scene::normalize(b);
    const float dot=std::clamp(std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w),0.0f,1.0f);
    return 2.0f*std::acos(dot)*180.0f/scene::kPi;
}
}

int main(int argc,char** argv){
    if(argc!=5){std::cerr<<"Usage: animation_mechanism_report <rig.cast> <weapon.cast> <animation.cast> <bone>\n";return 2;}
    const auto rig=cast::Document::load(std::filesystem::u8path(argv[1]));
    const auto weapon=cast::Document::load(std::filesystem::u8path(argv[2]));
    const auto animation=cast::Document::load(std::filesystem::u8path(argv[3]));
    if(!rig.valid()||!weapon.valid()||!animation.valid()){std::cerr<<"Could not load input\n";return 1;}
    auto value=scene::buildScene(rig);
    scene::appendRigModel(weapon,value,"diagnostic_weapon");
    const auto animationIndex=value.animations.size();scene::appendAnimations(animation,value);
    const auto bone=value.skeleton.boneByCanonicalName.find(scene::canonicalName(argv[4]));
    if(animationIndex>=value.animations.size()||bone==value.skeleton.boneByCanonicalName.end()){std::cerr<<"Animation or bone unavailable\n";return 1;}
    const auto& clip=value.animations[animationIndex];
    const auto final=value.sampleLocalPose(animationIndex,static_cast<float>(clip.durationFrames))[bone->second];
    std::cout<<std::fixed<<std::setprecision(6)<<"fps="<<clip.framerate<<" frames="<<clip.durationFrames<<" bone="<<value.skeleton.bones[bone->second].name<<'\n';
    std::cout<<"frame,time,x,y,z,position_to_final,rotation_to_final\n";
    for(std::uint32_t frame=0;frame<=clip.durationFrames;++frame){
        const auto pose=value.sampleLocalPose(animationIndex,static_cast<float>(frame))[bone->second];
        std::cout<<frame<<','<<frame/std::max(1.0f,clip.framerate)<<','<<pose.position.x<<','<<pose.position.y<<','<<pose.position.z<<','<<scene::length(pose.position-final.position)<<','<<quatErrorDegrees(pose.rotation,final.rotation)<<'\n';
    }
}
