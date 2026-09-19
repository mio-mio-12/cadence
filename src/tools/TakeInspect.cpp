#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "take/Take.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>

namespace {
float determinant(const scene::Mat4& m) {
    return m.v[0]*(m.v[5]*m.v[10]-m.v[9]*m.v[6])
         - m.v[4]*(m.v[1]*m.v[10]-m.v[9]*m.v[2])
         + m.v[8]*(m.v[1]*m.v[6]-m.v[5]*m.v[2]);
}
}

int main(int argc,char** argv) {
    if(argc<2){std::cerr<<"usage: take_inspect <take>\n";return 2;}
    take::Take recorded;std::string error;const auto path=std::filesystem::u8path(argv[1]);
    if(!take::load(path,recorded,error)){std::cerr<<error<<'\n';return 1;}
    std::cout<<"TAKE "<<path.string()<<"\nrate "<<recorded.sampleRate<<" samples "<<recorded.samples.size()<<" duration "<<recorded.duration()<<" bones "<<recorded.boneCount<<"\nbase "<<recorded.actor.baseModel<<'\n';
    std::size_t cameraBone=static_cast<std::size_t>(-1),gunBone=static_cast<std::size_t>(-1),adsBone=static_cast<std::size_t>(-1);
    if(!recorded.actor.baseModel.empty()){
        const auto document=cast::Document::load(std::filesystem::u8path(recorded.actor.baseModel));
        if(document.valid()){auto rig=scene::buildScene(document);for(const auto& model:recorded.actor.rigModels){const auto part=cast::Document::load(std::filesystem::u8path(model));if(part.valid())scene::appendRigModel(part,rig,std::filesystem::path(model).stem().string());}const auto bone=[&](const char* name){if(const auto found=rig.skeleton.boneByCanonicalName.find(name);found!=rig.skeleton.boneByCanonicalName.end())return found->second;return static_cast<std::size_t>(-1);};cameraBone=bone("tag_camera");gunBone=bone("j_gun");adsBone=bone("tag_ads");}
    }
    std::cout<<"tag_camera "<<(cameraBone<recorded.boneCount?std::to_string(cameraBone):"unresolved")<<" j_gun "<<(gunBone<recorded.boneCount?std::to_string(gunBone):"unresolved")<<" tag_ads "<<(adsBone<recorded.boneCount?std::to_string(adsBone):"unresolved")<<'\n';
    std::set<std::string> clips;float minPitch=1e9f,maxPitch=-1e9f;
    for(const auto& sample:recorded.samples){clips.insert(sample.primaryClip);clips.insert(sample.torsoClip);minPitch=std::min(minPitch,sample.camera.pitch);maxPitch=std::max(maxPitch,sample.camera.pitch);}
    std::cout<<"pitch "<<minPitch<<".."<<maxPitch<<" clips";for(const auto& clip:clips)if(!clip.empty())std::cout<<" ["<<clip<<']';std::cout<<'\n';
    std::string previousPrimary,previousTorso;float previousDet{};bool first=true;
    for(std::size_t i=0;i<recorded.samples.size();++i){const auto& sample=recorded.samples[i];float det=0;const scene::Mat4* camera=nullptr;if(cameraBone<sample.pose.size()){camera=&sample.pose[cameraBone];det=determinant(*camera);}
        const bool clipChange=sample.primaryClip!=previousPrimary||sample.torsoClip!=previousTorso;
        const bool basisChange=!first&&((det<0)!=(previousDet<0));
        const bool periodic=i%std::max<std::size_t>(1,static_cast<std::size_t>(recorded.sampleRate/5))==0;
        if(clipChange||basisChange||periodic||i+1==recorded.samples.size()){
            std::cout<<std::fixed<<std::setprecision(5)<<"S "<<i<<" t "<<sample.time<<" yaw "<<sample.camera.yaw<<" pitch "<<sample.camera.pitch<<" slot "<<static_cast<int>(sample.weaponSlot)<<" base "<<sample.primaryClip<<" f "<<sample.primaryFrame<<" torso "<<sample.torsoClip<<" f "<<sample.torsoFrame;
            if(camera){std::cout<<" camP "<<camera->v[12]<<','<<camera->v[13]<<','<<camera->v[14]<<" camF "<<camera->v[0]<<','<<camera->v[1]<<','<<camera->v[2]<<" camU "<<camera->v[8]<<','<<camera->v[9]<<','<<camera->v[10]<<" det "<<det;const auto inverseCamera=scene::inverseAffine(*camera);const auto relative=[&](std::size_t bone,const char* label){if(bone<sample.pose.size()){const auto local=inverseCamera*sample.pose[bone];std::cout<<' '<<label<<' '<<local.v[12]<<','<<local.v[13]<<','<<local.v[14]<<" f "<<local.v[0]<<','<<local.v[1]<<','<<local.v[2]<<" u "<<local.v[8]<<','<<local.v[9]<<','<<local.v[10]<<" d "<<determinant(local);}};relative(gunBone,"gunL");relative(adsBone,"adsL");}
            std::cout<<'\n';
        }
        previousPrimary=sample.primaryClip;previousTorso=sample.torsoClip;previousDet=det;first=false;
    }
}
