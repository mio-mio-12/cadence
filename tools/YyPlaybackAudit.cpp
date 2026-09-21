#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main

// Real weapon assets, production selection/evaluation, isolated settings.
int main(int argc,char**argv){
    if(argc!=3||!glfwInit())return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* w=glfwCreateWindow(960,720,"YY playback audit",nullptr,nullptr);if(!w)return 2;
    glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& a=*state;a.window=w;a.defaultSalukiDirectory=argv[1];a.deferSceneUpload=true;
    const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);std::ofstream log(out/"results.txt");std::string error;
    if(!a.renderer.initialize(error))return 3;
    for(const auto* game:{"mw","bo2"})if(!assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error))return 4;
    int failures=0,tested=0;
    const auto check=[&](bool ok,const std::string& s){log<<(ok?"PASS ":"FAIL ")<<s<<std::endl;failures+=!ok;};
    const auto index=[&](std::string_view name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return SIZE_MAX;};
    const auto difference=[](const auto& x,const auto& y){if(x.size()!=y.size())return 1e9f;float e=0;for(size_t i=0;i<x.size();++i)for(int j=0;j<16;++j)e=std::max(e,std::abs(x[i].v[j]-y[i].v[j]));return e;};
    const auto tick=[&](float dt){
        advanceYyV4LiveBlend(a,dt);a.interruptPoseElapsed=std::min(a.interruptPoseDuration,a.interruptPoseElapsed+dt);
        const auto& c=a.scene.animations[a.animationIndex];const float authored=authoredClipDuration(a,a.animationIndex);
        advanceAnimationFrame(a,a.animationFrame,c,a.actionActive&&a.actionDurationOverride>0?dt*authored/a.actionDurationOverride:dt,!a.actionActive);
        if(a.transitioning){advanceAnimationFrame(a,a.previousAnimationFrame,a.scene.animations[a.previousAnimationIndex],dt);a.transitionElapsed+=dt;if(a.transitionElapsed>=a.activeTransitionDuration)a.transitioning=false;}
        a.actionElapsed+=dt;
    };
    for(const auto* name:{"viewmodel_ak47_mp_LOD0","t6_wpn_ar_an94_view_LOD0","t6_wpn_sniper_dsr50_view_LOD0","t6_wpn_sniper_ballista_view_LOD0"}){
        const auto weapon=index(name);check(weapon!=SIZE_MAX,std::string(name)+" asset present");if(weapon==SIZE_MAX)continue;
        const auto game=a.assetCatalog.entries[weapon].game;a.selectedBaseAsset=index(game=="mw"?"viewmodel_base_viewhands_LOD0":"c_usa_mp_isa_smg_viewhands_LOD0");
        equipViewWeapon(a,weapon);a.actorMode=false;a.gameplayLogic=true;a.playing=true;a.weaponSwitchAlgorithm=4;a.activeClassSlot=0;
        const auto idle=findViewmodelClip(a,"idle"),drop=findViewmodelClip(a,"putaway");check(idle&&drop,std::string(name)+" idle + putaway");if(!idle||!drop)continue;++tested;
        for(const auto* source:{"idle","fire","reload","ads_up"}){
            const auto sourceClip=findViewmodelClip(a,source);if(!sourceClip)continue;
            a.runtimeLayers.clear();a.actionActive=a.actionOverlay=a.transitioning=false;a.interruptPoseDuration=0;
            a.gameplayAds=a.viewmodelAdsEngaged=a.viewmodelAdsExiting=false;a.viewmodelAdsBaseAnimation=SIZE_MAX;a.jumpFeedbackAnimation=SIZE_MAX;
            a.animationIndex=*sourceClip;a.animationFrame=a.scene.animations[*sourceClip].durationFrames*.25f;
            if(std::string_view(source)!="idle"){a.actionActive=true;a.activeAction=std::string_view(source)=="reload"?scene::ActionRole::Reload:scene::ActionRole::Fire;a.actionDurationOverride=authoredClipDuration(a,*sourceClip);}
            if(std::string_view(source)=="fire"){
                a.animationIndex=*idle;a.animationFrame=0;a.actionOverlay=true;a.actionAnimationIndex=*sourceClip;
                a.actionFrame=a.scene.animations[*sourceClip].durationFrames*.25f;a.actionElapsed=.02f;
            }else if(std::string_view(source)=="ads_up"){
                a.actionActive=false;startViewmodelAimClip(a,*sourceClip);a.interruptPoseDuration=0;a.transitioning=false;
                a.viewmodelAdsBaseFrame=a.scene.animations[*sourceClip].durationFrames*.7f;a.viewmodelAdsTransitionElapsed=a.weaponTiming.adsIn*.7f;
            }
            a.weaponSwitchStage=1;a.weaponSwitchElapsed=0;a.gameplayAction=scene::ActionRole::Unequip;
            const auto before=evaluateCurrentPose(a);triggerGameplayAction(a);auto expected=before;scene::restoreT6MagazineAfterReload(a.scene,expected,*drop,a.animationFrame);
            check(difference(expected,evaluateCurrentPose(a))<.01f,std::string(name)+" "+source+" drop seam");
            for(int i=0;i<12;++i)tick(1.f/120.f);
            const auto beforeCancel=evaluateCurrentPose(a);cancelYyV4Drop(a);
            check(difference(beforeCancel,evaluateCurrentPose(a))<.01f,std::string(name)+" "+source+" cancel seam");
            check(a.weaponSwitchStage==0&&a.activeClassSlot==0&&!a.yyReverse&&!a.actionActive,std::string(name)+" ready immediately, no switch/reverse");
            check(a.yyLiveAnimation==*drop,std::string(name)+" outgoing putaway retained");
            const float oldFrame=a.yyLiveFrame;tick(.025f);
            check(a.yyLiveFrame>oldFrame,std::string(name)+" outgoing clip advances");
            const float frameAfter=a.yyLiveFrame;advanceYyV4LiveBlend(a,0);
            check(a.yyLiveFrame==frameAfter,std::string(name)+" paused playback stays paused");
            const float expectedSlowFrame=std::min(frameAfter+.00625f*a.scene.animations[a.yyLiveAnimation].framerate*a.yyLiveRate,static_cast<float>(a.scene.animations[a.yyLiveAnimation].durationFrames));
            advanceYyV4LiveBlend(a,.00625f);
            check(std::abs(a.yyLiveFrame-expectedSlowFrame)<.0001f,std::string(name)+" quarter-speed source clock");
            const auto paused=evaluateCurrentPose(a);evaluateCurrentPose(a);
            check(difference(paused,evaluateCurrentPose(a))<.001f,std::string(name)+" sampling never advances time");
            const auto live=evaluateCurrentPose(a);const auto liveIndex=a.yyLiveAnimation;a.yyLiveAnimation=SIZE_MAX;const auto frozen=evaluateCurrentPose(a);a.yyLiveAnimation=liveIndex;
            check(difference(live,frozen)>.001f,std::string(name)+" live result differs from frozen interpolation");
            // Rapid YYY/YYYY does not discard the still-visible drop trajectory.
            for(int press=0;press<10;++press){
                const auto p=evaluateCurrentPose(a);
                if(press%2==0){a.weaponSwitchStage=1;a.gameplayAction=scene::ActionRole::Unequip;triggerGameplayAction(a);}else cancelYyV4Drop(a);
                check(difference(p,evaluateCurrentPose(a))<.01f,std::string(name)+" chain seam "+std::to_string(press));
                tick(.0125f);for(const auto&m:evaluateCurrentPose(a))for(float v:m.v)if(!std::isfinite(v))++failures;
            }
            tick(1.f);const auto settled=evaluateCurrentPose(a);a.interruptPoseDuration=0;
            check(difference(settled,evaluateCurrentPose(a))<.001f,std::string(name)+" tail expires exactly");
        }
        // Capture a continuous, reproducible YY sequence; no personal config.
        if(!a.renderer.loadScene(a.scene,error))return 5;
        a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
        a.animationIndex=*idle;a.animationFrame=0;a.actionActive=a.actionOverlay=a.transitioning=false;a.interruptPoseDuration=0;
        a.actorMode=true;a.actorPosition=a.actorRenderPosition={};a.actorYaw=a.cameraPitch=0;a.actorViewHeight=0;
        take::Take recorded;recorded.sampleRate=120;recorded.boneCount=static_cast<unsigned>(a.scene.skeleton.bones.size());
        for(int f=0;f<90;++f){
            if(f==6||f==24||f==42){a.weaponSwitchStage=1;a.gameplayAction=scene::ActionRole::Unequip;triggerGameplayAction(a);}
            if(f==15||f==33||f==51)cancelYyV4Drop(a);
            tick(1.f/120.f);const auto p=evaluateCurrentPose(a);
            take::Sample sample;sample.time=f/120.f;sample.pose=p;recorded.samples.push_back(std::move(sample));
            const auto vp=scene::perspective(65*scene::kPi/180,4.f/3,.1f,4000)*scene::lookAt({0,0,0},{1,0,0},{0,0,1});
            a.renderer.render(a.scene,p,vp,640,480,false,false,false);
            std::ostringstream file;file<<name<<'_'<<std::setfill('0')<<std::setw(3)<<f<<".png";
            if(!a.renderer.saveColorPng(out/file.str(),error))return 6;
        }
        take::Take loaded;const auto file=out/(std::string(name)+".c_dm");
        check(take::save(recorded,file,error)&&take::load(file,loaded,error),std::string(name)+" replay round trip");
        if(loaded.samples.size()==recorded.samples.size()){for(size_t f=0;f<recorded.samples.size();++f)if(difference(recorded.samples[f].pose,loaded.samples[f].pose)>.001f)++failures;}
        else ++failures;
        for(const auto&m:loaded.interpolatedSample(.127f).pose)for(float v:m.v)if(!std::isfinite(v))++failures;
    }
    log<<"tested="<<tested<<" failures="<<failures<<std::endl;return failures?7:0;
}
