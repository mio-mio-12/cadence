#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
void requestIw4(AppState& app,int thirdRequest,bool swap=false){
    bool wheelSwap=false,wantsShot=true;
    auto trigger=[&](scene::ActionRole action){app.gameplayAction=action;triggerGameplayAction(app);};
#include "../src/app/Iw4Switch.inc"
}
void simulate(AppState& app,float deltaSeconds,float forward,bool jump){
    float side=0,landingSpeed=0;bool sprint=false,ads=false;
#include "../src/app/ReferenceMovement.inc"
}
int main(){
    {
        const auto root=std::filesystem::path(cadence::local_assets::exportPath("cs1.6"));
        auto shotApp=std::make_unique<AppState>();auto& shot=*shotApp;std::string error;
        CHECK(scene::imported::assemblePrepared(scene::buildScene(cast::Document::load(root/"models/viewmodel_rifle_v_ak47.cast"),false),scene::buildScene(cast::Document::load(root/"models/hands_shared.cast"),false),shot.scene,error));
        CHECK(scene::appendAnimations(cast::Document::load(root/"animations/viewmodel_rifle_v_ak47_fire.cast"),shot.scene));
        shot.takeRecording=true;shot.projectileTrailEnabled=false;shot.smokeTrailEnabled=false;
        shot.lastMuzzleFlashPosition={9999,9999,9999};
        for(float phase:{0.f,.5f,.95f}){
            shot.lastShotAnimProgress=phase;shot.animationFrame=shot.scene.animations[0].durationFrames*phase;
            const auto expected=scene::resolveMuzzlePosition(shot.scene,evaluateCurrentPose(shot));CHECK(expected);
            ++shot.recoilSequence;updateBotActors(shot,0.f);CHECK(!shot.recordedTake.shots.empty());
            CHECK(scene::length(shot.recordedTake.shots.back().muzzlePos-*expected)<.001f);
        }
        std::cout<<"PASS CS1.6 AK47 recorded trail origins match current muzzle at early/mid/late firing phases\n";
    }
    auto ptr=std::make_unique<AppState>();auto& app=*ptr;
    app.weaponSwitchAlgorithm=5;app.activeClassSlot=0;
    for(auto& slot:app.classSlotRigs)slot.emplace();
    app.experimentalThirdWeapon=true;
    requestIw4(app,1);CHECK(app.weaponSwitchStage==1&&app.weaponSwitchTarget==1);
    app.weaponSwitchElapsed=.12f;requestIw4(app,2);CHECK(app.weaponSwitchTarget==2&&app.weaponSwitchElapsed==.12f);
    requestIw4(app,0);CHECK(app.weaponSwitchStage==0&&!app.yyReverse&&!app.actionActive);
    app.actionActive=true;app.activeAction=scene::ActionRole::Fire;app.gameplayClock=1;app.nextFireTime=2;
    app.iw4FireCycle.shot(1,1,app.activeClassSlot,app.selectedWeaponAsset);
    requestIw4(app,1);CHECK(app.weaponSwitchStage==0&&app.iw4PendingSlot==1);
    app.gameplayClock=3;requestIw4(app,-1);CHECK(app.weaponSwitchStage==1&&app.iw4PendingSlot==-1);
    requestIw4(app,0);CHECK(app.weaponSwitchStage==0);
    app.actionActive=true;app.activeAction=scene::ActionRole::Reload;app.gameplayRechamber=true;
    requestIw4(app,1);CHECK(app.weaponSwitchStage==1&&!app.gameplayRechamber&&!app.pendingWeaponRechamber);
    requestIw4(app,0);app.weaponSwitchStage=2;requestIw4(app,1);CHECK(app.weaponSwitchStage==1);
    app.actorFollowCamera=false;app.actorPosition={0,0,10000};app.actorGrounded=false;
    for(int mode:{4,5}){
        app.movementAlgorithm=mode;
        const auto path=std::filesystem::path("diagnostics/v244")/(std::to_string(mode)+".cadencegame");
        CHECK(saveGameplayPreset(app,path));auto loaded=std::make_unique<AppState>();
        CHECK(loadGameplayPreset(*loaded,path));CHECK(loaded->movementAlgorithm==mode&&loaded->weaponSwitchAlgorithm==5);
        app.sourceFixedAccumulator=0;app.referenceMovement={};app.actorVelocity={};
        simulate(app,.032f,1,false);CHECK(app.actorVelocity.x>0&&app.actorVelocity.z<0);
        CHECK(std::isfinite(app.actorRenderPosition.z));
        auto other=std::make_unique<AppState>();auto& b=*other;
        b.movementAlgorithm=mode;b.actorFollowCamera=false;
        app.sourceFixedAccumulator=b.sourceFixedAccumulator=0;app.referenceMovement=b.referenceMovement={};
        app.actorPosition=b.actorPosition={.135f,0,10000};app.actorVelocity=b.actorVelocity={};
        for(int i=0;i<25;++i)simulate(app,.032f,1,false);
        for(int i=0;i<100;++i)simulate(b,.008f,1,false);
        CHECK(app.referenceMovement.clockMs==b.referenceMovement.clockMs);
        CHECK(app.actorPosition.x==b.actorPosition.x&&app.actorPosition.z==b.actorPosition.z);
        // Production fixed-step adapter: held Space survives frame grouping,
        // lands, then jumps again without synthesizing a fresh key press.
        auto floorMap=std::make_shared<scene::glb::Map>();
        const auto addTriangle=[&](scene::Vec3 a,scene::Vec3 b,scene::Vec3 c){
            scene::glb::CollisionTriangle t;t.a=a;t.b=b;t.c=c;t.normal={0,0,1};
            t.minimum={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),0};
            t.maximum={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),0};
            t.walkable=true;t.blocking=false;floorMap->collision.push_back(t);
        };
        addTriangle({-10000,-10000,0},{10000,-10000,0},{10000,10000,0});
        addTriangle({-10000,-10000,0},{10000,10000,0},{-10000,10000,0});
        app.loadedMap=b.loadedMap=*floorMap;
        app.sourceFixedAccumulator=b.sourceFixedAccumulator=0;app.referenceMovement=b.referenceMovement={};
        app.actorPosition=b.actorPosition={0,0,.01f};app.actorVelocity=b.actorVelocity={};
        for(int i=0;i<125;++i)simulate(app,.032f,0,true);
        int jumps=0;float previousVertical=0;
        for(int i=0;i<500;++i){simulate(b,.008f,0,true);if(previousVertical<=0&&b.actorVelocity.z>0)++jumps;previousVertical=b.actorVelocity.z;}
        CHECK(jumps>=4);
        CHECK(app.actorPosition.z==b.actorPosition.z&&app.actorVelocity.z==b.actorVelocity.z);
        CHECK(app.referenceMovement.clockMs==b.referenceMovement.clockMs);
        app.loadedMap.reset();b.loadedMap.reset();
    }
    std::cout<<"PASS production adapters: YY drop/cancel/retarget/fire queue/rechamber/raise and new movement preset round trips\n";
}
