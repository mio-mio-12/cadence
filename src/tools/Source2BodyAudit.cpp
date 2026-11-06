#include "cast/CastDocument.h"
#include "scene/ImportedBodyRetarget.h"
#include "scene/ImportedViewRetarget.h"
#include <iostream>
#include <map>
#include <cmath>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const std::filesystem::path root=argv[1];
    std::map<std::string,std::filesystem::path> clips;
    for(const auto& e:std::filesystem::directory_iterator(root/"animations")){
        const auto n=e.path().filename().string();if(!n.starts_with("pb_rifle_stand_"))continue;
        for(const auto* role:{"idle","walk","run","idle_fire","idle_reload","death"})
            if(n.starts_with(std::string("pb_rifle_stand_")+role+"_clip"))clips.try_emplace(role,e.path());
    }
    int failures=0,cases=0;std::shared_ptr<scene::CastScene> donor;
    for(const auto& e:std::filesystem::directory_iterator(root/"models"/"playermodels")){
        if(e.path().extension()!=".cast")continue;
        auto model=scene::buildScene(cast::Document::load(e.path()),false);
        if(!scene::imported::completeBodyLayout(scene::imported::bodyLayout(model.skeleton))){++failures;continue;}
        for(const auto& [role,path]:clips){
            model.animations.clear();scene::appendAnimations(cast::Document::load(path),model);
            if(model.animations.empty()){++failures;continue;}
            auto& clip=model.animations.front();clip.sourceGame="cs2";scene::imported::classifySourceBodyAnimation(clip);
            if(clip.tracks.empty()||clip.unmappedCurveCount){++failures;std::cout<<e.path().stem()<<" "<<role<<" unmapped="<<clip.unmappedCurveCount<<'\n';}
            for(float t:{0.f,.5f,1.f}){
                const auto pose=model.samplePose(0,t*clip.durationFrames);
                for(const auto& m:pose)for(float v:m.v)if(!std::isfinite(v))++failures;
                const auto layout=scene::imported::bodyLayout(model.skeleton);
                for(int side=0;side<2;++side){
                    const auto a=layout.elbow[side],b=layout.hand[side];
                    const auto rest=scene::length(scene::transformPoint(model.skeleton.bones[a].restGlobal,{})-scene::transformPoint(model.skeleton.bones[b].restGlobal,{}));
                    const auto animated=scene::length(scene::transformPoint(pose[a],{})-scene::transformPoint(pose[b],{}));
                    if(rest>.001f&&(animated/rest<.85f||animated/rest>1.15f)){++failures;std::cout<<e.path().stem()<<" "<<role<<" forearm ratio="<<animated/rest<<'\n';}
                }
            }
            ++cases;
        }
        if(!donor)donor=std::make_shared<scene::CastScene>(model);
        else if(!donor->animations.empty()){
            model.animations.clear();std::string error;
            if(!scene::imported::appendRetargetedBody(model,donor,0,error)){++failures;std::cout<<error<<'\n';}
            else for(const auto& m:model.samplePose(0,0))for(float v:m.v)if(!std::isfinite(v))++failures;
        }
    }
    int viewCases=0;
    for(const auto* family:{"ak47","usp_silencer","knife_talon"}){
        const auto folder=root/"animations"/"viewmodel"/family;
        for(const auto& entry:std::filesystem::directory_iterator(folder)){
            if(entry.path().extension()!=".cast"||!entry.path().filename().string().starts_with("idle"))continue;
            auto driver=scene::buildScene(cast::Document::load(entry.path()));
            if(driver.skeleton.bones.empty()||driver.animations.empty()||driver.animations[0].unmappedCurveCount){++failures;std::cout<<family<<" embedded hierarchy binding failed\n";break;}
            const auto skin=scene::buildScene(cast::Document::load(root/"models"/"viewhands_ctm_fbi_hands_model.cast"));std::string error;
            if(!scene::imported::fitPoseOnlyViewSkin(driver,skin,error)){++failures;std::cout<<family<<" "<<error<<'\n';break;}
            for(const auto& m:driver.samplePose(0,0))for(float v:m.v)if(!std::isfinite(v))++failures;
            ++viewCases;break;
        }
    }
    std::cout<<"Native body cases="<<cases<<" action families="<<clips.size()<<" embedded viewmodel fits="<<viewCases<<" failures="<<failures<<'\n';
    if(viewCases!=3)++failures;
    return failures||cases==0?1:0;
}
