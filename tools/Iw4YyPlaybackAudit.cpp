#define NOMINMAX
#include <Windows.h>
#include <GLFW/glfw3.h>
static bool controlledInput=false,controlledFire=false;
static int controlledKey=-1;
static int yyAuditKey(GLFWwindow* w,int k){return controlledInput?(k==controlledKey?GLFW_PRESS:GLFW_RELEASE):glfwGetKey(w,k);}
static int yyAuditMouse(GLFWwindow* w,int b){return controlledInput?(controlledFire&&b==GLFW_MOUSE_BUTTON_LEFT?GLFW_PRESS:GLFW_RELEASE):glfwGetMouseButton(w,b);}
static int yyAuditWindow(GLFWwindow* w,int a){return controlledInput?GLFW_TRUE:glfwGetWindowAttrib(w,a);}
static int yyAuditInput(GLFWwindow* w,int m){return controlledInput?GLFW_CURSOR_DISABLED:glfwGetInputMode(w,m);}
static void yyAuditCursor(GLFWwindow* w,double* x,double* y){if(controlledInput)*x=*y=0;else glfwGetCursorPos(w,x,y);}
static SHORT yyAuditAsync(int k){return controlledInput?0:GetAsyncKeyState(k);}
#define glfwGetKey yyAuditKey
#define glfwGetMouseButton yyAuditMouse
#define glfwGetWindowAttrib yyAuditWindow
#define glfwGetInputMode yyAuditInput
#define glfwGetCursorPos yyAuditCursor
#define GetAsyncKeyState yyAuditAsync
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#undef glfwGetKey
#undef glfwGetMouseButton
#undef glfwGetWindowAttrib
#undef glfwGetInputMode
#undef glfwGetCursorPos
#undef GetAsyncKeyState
#include <iostream>

// Production mode-5 request branch, real resident class rigs, no user settings
// writes. State/pose regression coverage, not retail xanim visual certification.
int main(int argc,char** argv)try{
    if(argc!=3||!glfwInit())return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(640,480,"IW4 YY audit",nullptr,nullptr);if(!window)return 2;
    glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);std::ofstream log(out/"results.txt");
    int failures=0;const auto check=[&](bool ok,const std::string& name){log<<(ok?"PASS ":"FAIL ")<<name<<std::endl;failures+=!ok;};
    const auto difference=[](const auto& a,const auto& b){if(a.size()!=b.size())return 1e9f;float d=0;for(size_t i=0;i<a.size();++i)for(int j=0;j<16;++j)d=std::max(d,std::abs(a[i].v[j]-b[i].v[j]));return d;};
    for(const auto* game:{"mw","bo2"}){
        auto owner=std::make_unique<AppState>();auto& app=*owner;app.window=window;app.defaultSalukiDirectory=argv[1];std::string error;
        if(!app.renderer.initialize(error)||!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error)){log<<error;return 3;}
        const auto find=[&](std::string_view name){for(size_t i=0;i<app.assetCatalog.entries.size();++i)if(app.assetCatalog.entries[i].name==name)return i;return SIZE_MAX;};
        const bool mw=std::string_view(game)=="mw";
        app.classPrimaryAsset=find(mw?"viewmodel_ak47_mp_LOD0":"t6_wpn_ar_an94_view_LOD0");
        app.classSecondaryAsset=find(mw?"viewmodel_desert_eagle_LOD0":"t6_wpn_pistol_fnp45_view_LOD0");
        app.classViewhandsOverride=find(mw?"viewmodel_base_viewhands_LOD0":"c_usa_mp_isa_smg_viewhands_LOD0");
        if(app.classPrimaryAsset==SIZE_MAX||app.classSecondaryAsset==SIZE_MAX||app.classViewhandsOverride==SIZE_MAX)return 4;
        app.autoPlayerModel=false;app.experimentalThirdWeapon=false;
        loadBothClassSlots(app);while(app.pendingClassFuture){processPendingClassLoad(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        if(!app.classSlotRigs[0]||!app.classSlotRigs[1]){log<<app.status;return 5;}
        app.weaponSwitchAlgorithm=5;app.gameplayLogic=app.playing=true;app.actorMode=false;app.actionBlendTime=.073f;app.weaponTiming.yyReturnScale=2.5f;
        const auto reset=[&](int slot){
            app.iw4FireCycle.clear();controlledInput=controlledFire=false;controlledKey=-1;
            activateClassSlot(app,slot);app.weaponSwitchStage=0;app.weaponSwitchElapsed=0;app.iw4PendingSlot=-1;
            app.actionActive=app.actionOverlay=app.transitioning=false;app.activeAction=scene::ActionRole::None;
            app.gameplayAds=app.viewmodelAdsEngaged=app.viewmodelAdsExiting=app.viewmodelAdsPoseHold=false;
            app.viewmodelAdsBaseAnimation=SIZE_MAX;app.yyLiveAnimation=SIZE_MAX;app.interruptPoseDuration=0;
            app.yyReturning=app.yyTowardPutaway=app.yyReverse=false;app.runtimeLayers.clear();
            app.pendingWeaponRechamber=app.gameplayRechamber=false;app.nextFireTime=app.gameplayClock;resolveGameplayAnimation(app);
        };
        const auto dispatchRequest=[&](int desired,bool key=false){
            int thirdRequest=desired;const bool swap=key,wheelSwap=false;app.previousSwap=false;bool wantsShot=false;
            const auto trigger=[&](scene::ActionRole role){app.gameplayAction=role;triggerGameplayAction(app);};
            #include "../src/app/Iw4Switch.inc"
        };
        const auto tick=[&](float dt){
            app.gameplayClock+=dt;advanceYyV4LiveBlend(app,dt);app.interruptPoseElapsed+=dt;
            if(app.animationIndex<app.scene.animations.size()){
                const auto& c=app.scene.animations[app.animationIndex];const auto authored=authoredClipDuration(app,app.animationIndex);
                advanceAnimationFrame(app,app.animationFrame,c,app.actionActive&&app.actionDurationOverride>0?dt*authored/app.actionDurationOverride:dt,!app.actionActive);
            }
            updateGameplay(app,dt);
        };
        const std::string label=std::string(game)+" ";
        // Exercise production Equip timing and stage deadline together, rather
        // than only testing the scalar selector. Keep the real rig/clip loaded.
        for(float quickTime:{.12f,0.f,-1.f})for(float normalTime:{.4f,0.f,-1.f}){
            reset(0);const auto saved=app.weaponTiming;
            app.weaponTiming.quickRaiseTime=quickTime;app.weaponTiming.raiseTime=normalTime;
            app.weaponSwitchQuickAnimation=true;app.weaponSwitchStage=2;
            app.gameplayAction=scene::ActionRole::Equip;triggerGameplayAction(app);
            const float expected=quickTime>0?quickTime:normalTime>0?normalTime:.001f;
            check(app.actionActive&&std::abs(app.actionDurationOverride-expected)<.000001f,label+"effective raise presentation");
            updateGameplay(app,expected*.5f);check(app.weaponSwitchStage==2,label+"effective raise not early");
            updateGameplay(app,expected*.5f+.00001f);check(app.weaponSwitchStage==0,label+"effective raise completes");
            app.weaponTiming=saved;
        }
        for(int slot:{0,1}){
            reset(slot);const auto expectedQuick=useQuickSwapAnimation(app,slot,1-slot);dispatchRequest(1-slot);
            check(app.weaponSwitchStage==1&&app.weaponSwitchQuickAnimation==expectedQuick,label+"begin drop "+std::to_string(slot));
            const float duration=expectedQuick?app.weaponTiming.quickDropTime:app.weaponTiming.dropTime;
            updateGameplay(app,std::max(.001f,duration)+.0001f);
            check(app.activeClassSlot==1-slot&&app.weaponSwitchStage==2,label+"resident handoff "+std::to_string(slot));
            check(app.weaponSwitchQuickAnimation==expectedQuick,label+"retain quick state "+std::to_string(slot));
            const float raise=expectedQuick?app.weaponTiming.quickRaiseTime:app.weaponTiming.raiseTime;
            check(std::abs(app.actionDurationOverride-raise)<.0001f,label+"raise timing "+std::to_string(slot));
            const auto selected=findViewmodelGameplayAnimation(app);
            check(selected&&*selected==app.animationIndex,label+"raise clip selection "+std::to_string(slot));
            if(selected)log<<"CLIP "<<app.scene.animations[*selected].sourceName<<" quick="<<expectedQuick<<" duration="<<raise<<std::endl;
            updateGameplay(app,std::max(.001f,raise)+.0001f);check(app.weaponSwitchStage==0,label+"raise completes");
        }
        for(float portion:{.001f,.5f,.99f}){
            reset(0);const auto before=evaluateCurrentPose(app);dispatchRequest(1);
            check(difference(before,evaluateCurrentPose(app))<.01f,label+"drop seam");
            tick(portion*std::max(.001f,app.weaponSwitchQuickAnimation?app.weaponTiming.quickDropTime:app.weaponTiming.dropTime));
            const auto p=evaluateCurrentPose(app);dispatchRequest(0);
            check(app.activeClassSlot==0&&app.weaponSwitchStage==0&&!app.actionActive,label+"cancel stays ready");
            check(difference(p,evaluateCurrentPose(app))<.01f,label+"cancel seam");
            check(std::abs(app.interruptPoseDuration-app.actionBlendTime)<.0001f,label+"mode5 blend independent of v4 scale");
        }
        reset(0);
        for(int press=0;press<10;++press){const auto p=evaluateCurrentPose(app);dispatchRequest(-1,true);check(difference(p,evaluateCurrentPose(app))<.01f,label+"rapid key seam "+std::to_string(press));tick(.005f);}
        for(auto role:{scene::ActionRole::Fire,scene::ActionRole::Reload,scene::ActionRole::Melee,scene::ActionRole::Throw}){
            reset(0);app.actionActive=true;app.activeAction=role;app.nextFireTime=app.gameplayClock+1;
            if(role==scene::ActionRole::Fire)app.iw4FireCycle.shot(app.gameplayClock,1,app.activeClassSlot,app.selectedWeaponAsset);
            dispatchRequest(1);const bool allowed=role==scene::ActionRole::Reload;
            check((app.weaponSwitchStage==1)==allowed,label+"production admission "+std::to_string(int(role)));
            if(role==scene::ActionRole::Fire){check(app.iw4PendingSlot==1,label+"fire queues target");app.gameplayClock+=1;dispatchRequest(-1);check(app.weaponSwitchStage==1,label+"fire target admits on timer");}
        }
        reset(0);app.actionActive=true;app.activeAction=scene::ActionRole::Reload;app.gameplayRechamber=true;dispatchRequest(1);check(app.weaponSwitchStage==1,label+"rechamber admission");
        const auto actualShot=[&](float interval,int burst=1,float burstDelay=0.f){
            reset(0);app.weaponTiming.fireTime=interval;app.weaponTiming.burstCount=burst;app.weaponTiming.burstDelay=burstDelay;app.weaponTiming.fullAuto=false;app.weaponTiming.boltAction=false;
            app.burstShotsRemaining=0;app.previousFire=false;app.previousAds=false;app.previousReload=false;app.actorMode=true;app.actorNoclip=true;app.actorInputCaptured=true;
            const auto serial=app.acceptedShotSerial;controlledInput=controlledFire=true;updateActorController(app,0);controlledFire=false;
            check(app.acceptedShotSerial==serial+1,label+"actual shot accepted");
            check(app.iw4FireCycle.active,label+"accepted shot starts combat lifetime");
        };
        actualShot(10);const double shotStart=app.gameplayClock;updateGameplay(app,2);
        check(!app.actionActive||app.activeAction!=scene::ActionRole::Fire,label+"short firing clip completes");dispatchRequest(1);
        check(app.weaponSwitchStage==0&&app.iw4PendingSlot==1,label+"clip completion retains firing gate");
        app.gameplayClock=shotStart+10;dispatchRequest(-1);check(app.weaponSwitchStage==1,label+"queued target admitted at fire deadline");
        actualShot(.01f);check(app.actionActive&&app.activeAction==scene::ActionRole::Fire,label+"long firing presentation still active");app.gameplayClock+=.011;dispatchRequest(1);check(app.weaponSwitchStage==1,label+"long presentation does not extend expired firing gate");
        actualShot(.05f,2,3);app.gameplayClock+=.051;updateActorController(app,0);
        check(app.burstShotsRemaining==0&&app.nextFireTime>app.gameplayClock+2.9,label+"final burst has Ready cooldown");
        dispatchRequest(1);check(app.weaponSwitchStage==0,label+"final burst per-shot interval blocks");
        app.gameplayClock+=.051;dispatchRequest(-1);check(app.weaponSwitchStage==1,label+"Ready burst cooldown allows change");
        reset(0);app.nextFireTime=app.gameplayClock+10;dispatchRequest(1);check(app.weaponSwitchStage==1,label+"Ready timer without accepted fire is not busy");
        actualShot(10);app.gameplayAction=scene::ActionRole::Reload;triggerGameplayAction(app);check(!app.iw4FireCycle.active,label+"accepted reload supersedes firing");dispatchRequest(1);check(app.weaponSwitchStage==1,label+"reload permits change");
        actualShot(10);resetWeaponAimState(app);check(!app.iw4FireCycle.active,label+"slot reset clears combat lifetime");
        actualShot(10);app.weaponSwitchAlgorithm=4;dispatchRequest(1);check(!app.iw4FireCycle.active,label+"leaving mode5 clears combat lifetime");app.weaponSwitchAlgorithm=5;
        controlledInput=controlledFire=false;app.actorInputCaptured=false;
    }
    log<<"failures="<<failures<<std::endl;return failures?6:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 7;}
