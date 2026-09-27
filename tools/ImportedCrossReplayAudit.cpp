#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
static int auditAnyBotReplay(AppState& app,const std::filesystem::path& output){
    const auto require=[](bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);};
    std::string error;
    const auto pick=[&](std::string_view game,assets::Role role,std::string_view name){for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game==game&&a.role==role&&a.name.find(name)!=std::string::npos)return i;}return SIZE_MAX;};
    app.selectedBaseAsset=pick("bo2",assets::Role::ViewHands,"seal6");
    app.selectedWeaponAsset=pick("bo2",assets::Role::ViewWeapon,"an94");
    require(app.selectedBaseAsset!=SIZE_MAX&&app.selectedWeaponAsset!=SIZE_MAX,"BO2 recording fixture missing");
    equipViewWeapon(app,app.selectedWeaponAsset);
    app.enemyBotCount=8;app.botGame="cso2";app.botTeamSide=3;app.botAnimationGame="bo2";app.botSystemMode=0;app.botUseSalukiWeaponPool=false;
    app.botModelAsset=pick("cso2",assets::Role::PlayerModel,"ct_707");
    require(app.botModelAsset!=SIZE_MAX,"CSO2 bot fixture missing");
    rebuildBotActors(app);
    require(app.botActorScene&&app.bots.size()==8,"Any bot build failed: "+app.status);
    scene::AnimationQuery query;query.domain=scene::AnimationDomain::PlayerBody;query.motion=scene::MotionRole::Idle;query.weapon=scene::WeaponClass::Rifle;query.stance=scene::Stance::Stand;query.preferredGame="bo2";
    const auto clip=scene::findBestAnimation(*app.botActorScene,query);require(clip.has_value(),"No BO2 body idle");
    app.botActorPoses.clear();std::vector<int> variants;std::set<int> identities;
    for(std::size_t i=0;i<app.bots.size();++i){auto& bot=app.bots[i];variants.push_back(bot.modelVariant);identities.insert(bot.modelVariant);
        auto pose=app.botActorScene->samplePose(*clip,app.botActorScene->animations[*clip].durationFrames*.37f);
        scene::imported::suppressBodyRootMotion(app.botActorScene->skeleton,pose,false);
        const scene::Vec3 offset{float(i%4)*130-195,float(i/4)*210,0};
        for(auto& bone:pose){bone.v[12]+=offset.x;bone.v[13]+=offset.y;bone.v[14]+=offset.z;}
        app.botActorPoses.push_back(std::move(pose));bot.animation=*clip;
    }
    require(identities.size()>1,"Any bots all share one model");
    const auto live=*app.botActorScene;const auto poses=app.botActorPoses;
    app.hiddenWorldActor.reset();app.classWorldModelAsset=SIZE_MAX;
    toggleTakeRecording(app);captureTakeSample(app,.1f);toggleTakeRecording(app);
    require(app.recordedTake.samples.size()>=2&&app.recordedTake.botCount==8,"Production bot recording empty");
    const auto path=output/"any-cso2.c_dm";
    require(take::save(app.recordedTake,path,error),error);
    take::Take restoredTake;require(take::load(path,restoredTake,error),error);
    require(restoredTake.samples.front().bots.size()==variants.size(),"Bot samples missing after serialization");
    for(std::size_t i=0;i<variants.size();++i)require(restoredTake.samples.front().bots[i].modelVariant==variants[i],"Serialized bot identity changed");
    const scene::CastScene empty;
    const auto vp=scene::perspective(45*scene::kPi/180,960.f/540,.1f,3000)*scene::lookAt({550,-750,380},{0,100,85},{0,0,1});
    const auto render=[&](const scene::CastScene& actor,const std::vector<std::vector<scene::Mat4>>& matrices,const std::string& name){
        require(app.renderer.loadScene(empty,error),error);require(app.renderer.loadAuxiliaryScenes(nullptr,nullptr,&actor,error),error);
        app.renderer.setFirstPersonProjection(false);app.renderer.setViewmodelCapture(false,{.08f,.09f,.11f,1});app.renderer.setDebugView(1);
        app.renderer.render(empty,{},vp,960,540,false,false,false,&actor,&matrices,&variants,nullptr,nullptr,false);
        require(app.renderer.saveColorPng(output/(name+".png"),error),error);std::vector<std::uint8_t> pixels;require(app.renderer.readColorRgba(pixels,error),error);
        std::size_t nonBackground{};for(std::size_t i=4;i+3<pixels.size();i+=4)nonBackground+=pixels[i]!=pixels[0]||pixels[i+1]!=pixels[1]||pixels[i+2]!=pixels[2];
        require(nonBackground>5000,"Bot render has insufficient visible coverage");return pixels;
    };
    const auto before=render(live,poses,"any-bots-live");
    app.botActorScene.reset();app.bots.clear();app.botActorPoses.clear();
    require(restoreTakeBotActor(app,restoredTake.botActor,restoredTake.botBoneCount,error),error);
    const auto& replay=*app.botActorScene;
    require(live.meshes.size()==replay.meshes.size()&&live.attachments.size()==replay.attachments.size(),"Bot mesh/attachment counts changed");
    require(live.skeleton.bones.size()==replay.skeleton.bones.size(),"Bot skeleton size changed");
    float maximum{};std::size_t vertices{};
    for(std::size_t b=0;b<live.skeleton.bones.size();++b){require(live.skeleton.bones[b].name==replay.skeleton.bones[b].name,"Bot skeleton order changed");for(int k=0;k<16;++k)maximum=std::max(maximum,std::abs(live.skeleton.bones[b].inverseBind.v[k]-replay.skeleton.bones[b].inverseBind.v[k]));}
    // Live assembly interleaves each variant's body and weapon; restoration
    // reads all body parts before attachments. Compare identity, not draw order.
    std::vector<bool> used(replay.meshes.size());
    for(std::size_t m=0;m<live.meshes.size();++m){const auto& a=live.meshes[m];std::size_t match=replay.meshes.size();
        for(std::size_t n=0;n<replay.meshes.size();++n){const auto& candidate=replay.meshes[n];if(!used[n]&&a.name==candidate.name&&a.actorVariant==candidate.actorVariant&&a.attachmentIndex==candidate.attachmentIndex){match=n;break;}}
        require(match<replay.meshes.size(),"Bot mesh identity missing: "+a.name+" variant="+std::to_string(a.actorVariant));used[match]=true;const auto& b=replay.meshes[match];require(a.vertices.size()==b.vertices.size()&&a.indices==b.indices,"Bot variant geometry changed: "+a.name);
        for(std::size_t v=0;v<a.vertices.size();++v){++vertices;maximum=std::max(maximum,scene::length(a.vertices[v].position-b.vertices[v].position));require(a.vertices[v].bones==b.vertices[v].bones&&a.vertices[v].weights==b.vertices[v].weights&&a.vertices[v].uv.x==b.vertices[v].uv.x&&a.vertices[v].uv.y==b.vertices[v].uv.y&&scene::length(a.vertices[v].normal-b.vertices[v].normal)<.0001f,"Bot vertex binding/UV/normal changed");}
    }
    std::vector<std::vector<scene::Mat4>> replayPoses;for(const auto& bot:restoredTake.samples.front().bots)replayPoses.push_back(bot.pose);
    const auto after=render(replay,replayPoses,"any-bots-restored");
    require(maximum<.0001f&&before.size()==after.size(),"Bot reconstruction diverged");
    int pixelError{};for(std::size_t i=0;i<before.size();++i)pixelError=std::max(pixelError,std::abs(int(before[i])-int(after[i])));
    require(pixelError<=1,"Any bot replay render differs");
    std::ofstream report(output/"any-bots-results.txt");
    report<<"PASS actual Any CSO2 production recording/save/load/reconstruction\nBots 8; unique variants "<<identities.size()<<"; vertices "<<vertices<<"; bind/position max "<<maximum<<"; pixel max "<<pixelError<<"\n";
    for(const auto& part:live.rigParts)report<<"Part "<<part.name<<'\n';
    std::cout<<"PASS Any CSO2 bots replay: "<<identities.size()<<" variants, "<<vertices<<" vertices, pixel max "<<pixelError<<'\n';return 0;
}
int main(int argc,char** argv){
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(960,540,"Imported cross-rig replay audit",nullptr,nullptr);if(!window)return 2;
    glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;app.deferSceneUpload=true;
    std::string error;if(!app.renderer.initialize(error))return 2;
    app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
    const std::string legacyGame=std::getenv("CADENCE_AUDIT_POINTBLANK")?"pointblank":"bo2";
    for(const auto game:{"bo2","pointblank","eldewrito","cs1.6","cz","css","csnz","cso2"})if(std::filesystem::is_directory(app.defaultSalukiDirectory/game)&&!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error)){std::cerr<<error;return 2;}
    const auto choose=[&](std::string_view game,assets::Role role,std::string_view wanted){
        std::size_t best=app.assetCatalog.entries.size();
        for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game!=game||a.role!=role)continue;if(best==app.assetCatalog.entries.size())best=i;if(a.name.find(wanted)!=std::string::npos)return i;}return best;
    };
    const auto output=argc>1?std::filesystem::path(argv[1]):std::filesystem::path("diagnostics/imported_v230");
    std::filesystem::create_directories(output);int failures{};
    if(argc>2&&std::string_view(argv[2])=="--bots-only")try{return auditAnyBotReplay(app,output);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    for(const auto game:{"eldewrito","cs1.6","cz","css","csnz","cso2"})for(bool importedSkin:{false,true}){
        const auto skin=choose(importedSkin?std::string(game):legacyGame,assets::Role::ViewHands,importedSkin?(std::string_view(game)=="eldewrito"?"assault_rifle":"ak47"):(legacyGame=="bo2"?"seal6":"SWAT_Male_hands"));
        const auto gun=choose(importedSkin?legacyGame:std::string(game),assets::Role::ViewWeapon,importedSkin?(legacyGame=="bo2"?"an94":"ColtPython"):(std::string_view(game)=="eldewrito"?"assault_rifle":"ak47"));
        if(skin>=app.assetCatalog.entries.size()||gun>=app.assetCatalog.entries.size()){std::cerr<<"SKIP unavailable assets "<<game<<'\n';continue;}
        const auto label=std::string(game)+(importedSkin?"_hands_"+legacyGame+"_gun":"_gun_"+legacyGame+"_hands");
        std::cout<<"CASE "<<label<<" skin="<<app.assetCatalog.entries[skin].path.string()<<" weapon="<<app.assetCatalog.entries[gun].path.string()<<std::endl;
        app.selectedBaseAsset=skin;app.selectedWeaponAsset=gun;equipViewWeapon(app,gun);
        if(app.scene.viewHandsDriverGame.empty()||app.scene.animations.empty()){std::cerr<<"FAIL "<<label<<" missing conversion: "<<app.status<<'\n';++failures;continue;}
        const auto live=app.scene;captureTakeActorManifest(app);const auto manifest=app.recordedTake.actor;
        const auto index=std::min(app.animationIndex,live.animations.size()-1);const auto pose=live.samplePose(index,live.animations[index].durationFrames*.5f);
        app.renderer.loadScene(live,error);app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
        const auto vp=scene::perspective(75*scene::kPi/180,960.f/540.f,.02f,5000)*scene::lookAt({},{1,0,0},{0,0,1});
        app.renderer.render(live,pose,vp,960,540,false,false,false);if(!app.renderer.saveColorPng(output/(label+".png"),error)){std::cerr<<error;++failures;}
        app.scene={};app.classSlotRigs={};app.selectedWeaponAsset=app.assetCatalog.entries.size();
        if(!restoreTakeActor(app,manifest,live.skeleton.bones.size(),error)){std::cerr<<"FAIL "<<label<<" restore: "<<error<<'\n';++failures;continue;}
        float bindError{},meshError{};bool mismatch=app.scene.meshes.size()!=live.meshes.size()||app.scene.skeleton.bones.size()!=live.skeleton.bones.size();
        for(std::size_t b=0;b<std::min(live.skeleton.bones.size(),app.scene.skeleton.bones.size());++b){mismatch|=live.skeleton.bones[b].name!=app.scene.skeleton.bones[b].name;for(int k=0;k<16;++k)bindError=std::max(bindError,std::abs(live.skeleton.bones[b].inverseBind.v[k]-app.scene.skeleton.bones[b].inverseBind.v[k]));}
        for(std::size_t m=0;m<std::min(live.meshes.size(),app.scene.meshes.size());++m){const auto& a=live.meshes[m];const auto& b=app.scene.meshes[m];mismatch|=a.vertices.size()!=b.vertices.size();for(std::size_t v=0;v<std::min(a.vertices.size(),b.vertices.size());++v)meshError=std::max(meshError,scene::length(a.vertices[v].position-b.vertices[v].position));}
        const bool pass=!mismatch&&bindError<.002f&&meshError<.002f;
        std::cout<<(pass?"PASS ":"FAIL ")<<label<<" bones="<<live.skeleton.bones.size()<<" bind="<<bindError<<" mesh="<<meshError<<std::endl;failures+=!pass;
    }
    {
        ImportedBodyPreparationBatch batch;
        ImportedWorldGripBatch worldBatch;
        PointBlankPreparationBatch documentBatch;
        // Exercise the production default, not a diagnostic-only donor setup.
        app.gameReferenceSetups.erase("bo2");
        for(const auto game:{"eldewrito","cs1.6","cz","css","csnz","cso2"}){
            const auto model=choose(game,assets::Role::PlayerModel,std::string_view(game)=="eldewrito"?"masterchief":"gign");
            if(model>=app.assetCatalog.entries.size()){std::cerr<<"SKIP body unavailable "<<game<<'\n';continue;}
            app.classWorldModelAsset=model;
            std::cout<<"BODY "<<app.assetCatalog.entries[model].path.string()<<std::endl;
            for(const auto source:{std::string(game),std::string("bo2")}){
                auto actor=scene::buildScene(cast::Document::load(app.assetCatalog.entries[model].path),false);
                app.botSpAlertClips={999999};app.botSpWalkClips={999999};
                loadSpBotScenarios(app,actor,game);
                if(std::find(app.botSpAlertClips.begin(),app.botSpAlertClips.end(),999999)!=app.botSpAlertClips.end()||std::find(app.botSpWalkClips.begin(),app.botSpWalkClips.end(),999999)!=app.botSpWalkClips.end()){
                    std::cerr<<"FAIL stale SP clip indices on "<<game<<'\n';++failures;
                }
                appendClassBodyAnimations(app,source,actor);std::size_t bad{};
                for(std::size_t clip=0;clip<std::min<std::size_t>(actor.animations.size(),8);++clip)for(float phase:{0.f,.37f,1.f})for(const auto& m:actor.samplePose(clip,actor.animations[clip].durationFrames*phase))for(float v:m.v)bad+=!std::isfinite(v);
                const bool pass=!actor.animations.empty()&&!bad;
                std::cout<<(pass?"PASS ":"FAIL ")<<"body "<<game<<" source="<<source<<" clips="<<actor.animations.size()<<" nonfinite="<<bad<<std::endl;
                if(!pass)for(const auto& warning:actor.warnings)std::cerr<<warning<<'\n';failures+=!pass;
                const auto view=choose("bo2",assets::Role::ViewWeapon,"an94");
                if(source=="bo2"&&view<app.assetCatalog.entries.size()){
                    const auto world=matchingWorldWeapon(app,view);
                    const auto before=actor.attachments.size();
                    if(world<app.assetCatalog.entries.size())attachImportedWorldWeaponAndParts(app,cast::Document::load(app.assetCatalog.entries[world].path),view,world,actor,game);
                    const bool mounted=actor.attachments.size()>before;
                    std::cout<<(mounted?"PASS ":"FAIL ")<<"BO2 world mount on "<<game<<std::endl;failures+=!mounted;
                    if(!mounted)for(const auto& warning:actor.warnings)std::cerr<<warning<<'\n';
                }
                app.hiddenWorldActor=actor;captureTakeWorldActorManifest(app);
                // Render actual bound locomotion, rather than treating finite matrices
                // or a nonempty clip library as visual proof of cross-rig support.
                for(const auto motion:{scene::MotionRole::Idle,scene::MotionRole::Run}){
                    scene::AnimationQuery query;query.domain=scene::AnimationDomain::PlayerBody;
                    query.motion=motion;query.weapon=scene::WeaponClass::Rifle;query.stance=scene::Stance::Stand;query.preferredGame=source;
                    const auto clip=scene::findBestAnimation(actor,query);
                    if(!clip){std::cout<<"SKIP missing rendered motion "<<game<<" source="<<source<<std::endl;continue;}
                    auto sampled=actor.samplePose(*clip,actor.animations[*clip].durationFrames*.37f);
                    scene::imported::suppressBodyRootMotion(actor.skeleton,sampled,false);
                    const auto center=actor.bounds.center();const float radius=std::max(1.f,actor.bounds.radius());
                    const auto camera=scene::perspective(40.f*scene::kPi/180.f,960.f/540.f,.01f,radius*20.f)*scene::lookAt(center+scene::Vec3{radius*2.6f,-radius*2.6f,radius*.65f},center,{0,0,1});
                    if(!app.renderer.loadScene(actor,error)){std::cerr<<error;++failures;continue;}
                    app.renderer.setFirstPersonProjection(false);app.renderer.setViewmodelCapture(false,{.08f,.09f,.11f,1});app.renderer.setDebugView(1);
                    app.renderer.render(actor,sampled,camera,960,540,false,false,false);
                    const auto filename=std::string("body_")+game+"_"+source+(motion==scene::MotionRole::Idle?"_idle.png":"_run.png");
                    if(!app.renderer.saveColorPng(output/filename,error)){std::cerr<<error;++failures;}
                    std::cout<<"RENDER "<<filename<<" clip="<<actor.animations[*clip].sourceName<<std::endl;
                }
                const auto manifest=app.recordedTake.worldActor;app.hiddenWorldActor.reset();
                bool replayPass=restoreTakeWorldActor(app,manifest,actor.skeleton.bones.size(),error);
                float replayBindError{};
                if(replayPass)for(std::size_t b=0;b<actor.skeleton.bones.size();++b){
                    replayPass&=actor.skeleton.bones[b].name==app.hiddenWorldActor->skeleton.bones[b].name;
                    for(int k=0;k<16;++k)replayBindError=std::max(replayBindError,std::abs(actor.skeleton.bones[b].inverseBind.v[k]-app.hiddenWorldActor->skeleton.bones[b].inverseBind.v[k]));
                }
                replayPass&=replayBindError<.002f;
                if(replayPass){replayPass=actor.attachments.size()==app.hiddenWorldActor->attachments.size();
                    for(std::size_t a=0;replayPass&&a<actor.attachments.size();++a){const auto& live=actor.attachments[a];const auto& restored=app.hiddenWorldActor->attachments[a];replayPass&=live.boneIndex==restored.boneIndex&&scene::length(live.position-restored.position)<.002f&&scene::length(live.rotationDegrees-restored.rotationDegrees)<.002f&&scene::length(live.scale-restored.scale)<.002f;}}
                std::cout<<(replayPass?"PASS ":"FAIL ")<<"body replay "<<game<<" source="<<source<<" bind="<<replayBindError<<" "<<(replayPass?"":error)<<std::endl;failures+=!replayPass;
            }
        }
    }
    return failures?1:0;
}
