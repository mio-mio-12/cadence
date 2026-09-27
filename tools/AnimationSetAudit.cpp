#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
int main(int argc,char** argv){
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(640,480,"Action audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
    auto state=std::make_unique<AppState>();auto& a=*state;a.window=window;a.defaultSalukiDirectory=cadence::local_assets::exportPath("");std::string error;int failed{};
    for(const auto* game:{"bo2","cs1.6","css","csnz","cso2","eldewrito"}){if(argc>1&&std::string(argv[1])!=game)continue;
        if(!assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error)){std::cerr<<error<<'\n';++failed;continue;}
        const auto model=findGenericPlayermodelForGame(a,game);prepareActionPreview(a,game,model);std::size_t deaths{};
        for(std::size_t i=0;i<a.actionPreviewScene.animations.size();++i){const auto& clip=a.actionPreviewScene.animations[i];if(clip.action!=scene::ActionRole::Death||clip.tracks.empty())continue;++deaths;
            if(deaths==1){a.animationSet.set(game,clip.action,clip.sourceName,true);const auto pose=a.actionPreviewScene.samplePose(i,clip.durationFrames*.5f);for(const auto& m:pose)for(float f:m.v)if(!std::isfinite(f))++failed;
                const auto bounds=a.actionPreviewScene.bounds;const auto center=(bounds.minimum+bounds.maximum)*.5f;const float radius=std::max(30.f,scene::length(bounds.maximum-bounds.minimum)*.5f);
                const auto vp=scene::perspective(.7f,1.333f,.1f,radius*12)*scene::lookAt(center+scene::Vec3{radius*3,radius*2,radius},center,{0,0,1});a.actionPreviewRenderer.setDebugView(1);a.actionPreviewRenderer.render(a.actionPreviewScene,pose,vp,640,480,true,false,false);a.actionPreviewRenderer.saveColorPng(std::string("action_preview_")+game+".png",error);
            }
        }
        std::cout<<game<<" ready="<<a.actionPreviewReady<<" clips="<<a.actionPreviewScene.animations.size()<<" deaths="<<deaths<<" status="<<a.status<<std::endl;if(!a.actionPreviewReady)++failed;
    }
    const auto bo2=findGenericPlayermodelForGame(a,"bo2");
    if(bo2<a.assetCatalog.entries.size()){
        auto target=scene::buildScene(cast::Document::load(a.assetCatalog.entries[bo2].path),false);a.animationSetForBots=true;a.animationSet.curatedDeaths=true;applyBotAnimationSet(a,target,"bo2");
        for(const auto& [key,enabled]:a.animationSet.rules){bool found=false;for(std::size_t i=0;i<target.animations.size();++i){const auto& clip=target.animations[i];if(clip.sourceGame==std::get<0>(key)&&cadence::AnimationSet::file(clip.sourceName)==std::get<2>(key)&&!clip.tracks.empty()){found=true;for(const auto& matrix:target.samplePose(i,clip.durationFrames*.5f))for(const auto f:matrix.v)if(!std::isfinite(f))++failed;}}std::cout<<"MIXED_BO2 "<<std::get<0>(key)<<" loaded="<<found<<std::endl;if(!found)++failed;}
    }
    a.actionPreviewRenderer.shutdown();state.reset();glfwDestroyWindow(window);glfwTerminate();return failed?1:0;
}
