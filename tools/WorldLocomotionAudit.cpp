#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main

int main(int argc,char**argv){
    if(argc!=3||!glfwInit())return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* w=glfwCreateWindow(640,480,"Locomotion audit",nullptr,nullptr);if(!w)return 2;
    glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;app.defaultSalukiDirectory=argv[1];app.deferSceneUpload=true;
    const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");std::string error;
    if(!app.renderer.initialize(error))return 3;
    for(const auto* game:{"mw","bo2","pointblank","aw"})if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))return 4;
    int failures=0,cases=0;PointBlankPreparationBatch batch;
    for(const auto* game:{"mw","bo2","pointblank"}){
        const auto index=findGenericPlayermodelForGame(app,game);if(index>=app.assetCatalog.entries.size())return 5;
        app.classWorldModelAsset=index;auto actor=scene::buildScene(cast::Document::load(app.assetCatalog.entries[index].path));
        for(auto part:characterAssemblyParts(app,app.assetCatalog.entries[index]))scene::appendRigModel(pointBlankReferenceDocument(app.assetCatalog.entries[part].path),actor,app.assetCatalog.entries[part].name);
        appendClassBodyAnimations(app,game,actor);app.playerWorldAnimationGame=game;
        std::cout<<game<<" clips="<<actor.animations.size()<<std::endl;
        report<<"LIBRARY "<<game<<" clips="<<actor.animations.size()<<'\n';
        app.hiddenWorldActor=std::move(actor);auto& rig=*app.hiddenWorldActor;
        for(const auto weapon:{scene::WeaponClass::Rifle,scene::WeaponClass::Pistol,scene::WeaponClass::Sniper,scene::WeaponClass::Knife,scene::WeaponClass::DualWield})
        for(const auto stance:{scene::Stance::Stand,scene::Stance::Crouch,scene::Stance::Prone})
        for(const bool ads:{false,true})for(const float forward:{-1.f,1.f}){
            app.gameplayWeapon=weapon;app.weaponProfile.meleeWeapon=weapon==scene::WeaponClass::Knife;app.gameplayStance=stance;app.gameplayAds=ads;
            app.actorMode=true;app.actorGrounded=true;app.actorMantling=app.actorSliding=app.actorSprinting=app.actorOnLadder=false;
            app.actorYaw=app.cameraPitch=0;app.actorPosition=app.actorRenderPosition={};app.actorWorldFacingOffset=0;
            app.actorVelocity=app.actorWishVelocity={forward*100,0,0};app.actorMoveInputForward=forward;app.actorMoveInputSide=0;
            app.actorWorldPresentationTime=-1;app.worldSelectionKey.reset();app.actorCurrLocoClip=app.actorPrevLocoClip=SIZE_MAX;app.actorWorldActionBlend={};
            app.actionActive=false;std::string selected;std::vector<scene::Mat4> first,last;
            for(int frame=0;frame<45;++frame){app.gameplayClock=frame/60.f;last=evaluateHiddenWorldActorPose(app,&selected);if(frame==15)first=last;}
            const auto chosen=std::find_if(rig.animations.begin(),rig.animations.end(),[&](const auto& c){return c.sourceName==selected;});
            float legMotion=0;for(const auto* name:{"j_ankle_le","j_ankle_ri"})if(auto it=rig.skeleton.boneByCanonicalName.find(name);it!=rig.skeleton.boneByCanonicalName.end()&&it->second<first.size())legMotion+=scene::length(scene::transformPoint(last[it->second],{})-scene::transformPoint(first[it->second],{}));
            const bool good=chosen!=rig.animations.end()&&(chosen->stance==stance||chosen->stance==scene::Stance::Any)&&chosen->motion!=scene::MotionRole::Idle&&chosen->weapon!=scene::WeaponClass::Launcher&&std::abs(app.actorWorldFacingOffset)<.001f&&legMotion>.01f;
            if(!good)++failures;++cases;
            report<<(good?"PASS ":"FAIL ")<<game<<" weapon="<<int(weapon)<<" stance="<<int(stance)<<" ads="<<ads<<" forward="<<forward<<" clip="<<selected<<" yaw="<<app.actorWorldFacingOffset<<" legMotion="<<legMotion<<'\n';report.flush();
            for(const auto&m:last)for(float f:m.v)if(!std::isfinite(f))++failures;
            if(stance==scene::Stance::Stand&&weapon==scene::WeaponClass::Rifle){
                if(!app.renderer.loadScene(rig,error))return 6;app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.12f,.13f,.15f,1});
                const auto vp=scene::perspective(45*scene::kPi/180,1,1,4000)*scene::lookAt({270,-290,175},{0,0,90},{0,0,1});
                app.renderer.render(rig,last,vp,480,480,false,false,false);std::vector<std::uint8_t> pixels;if(!app.renderer.readColorRgba(pixels,error))return 7;
                app.renderer.savePixelsPng(out/(std::string(game)+"_ads"+std::to_string(ads)+"_back"+std::to_string(forward<0)+".png"),480,480,pixels,error);
            }
        }
    }
    const auto body=findGenericPlayermodelForGame(app,"aw");if(body>=app.assetCatalog.entries.size())return 8;
    app.gameReferenceSetups["aw"].headAsset=std::size_t(-2);
    const auto isHead=[&](auto i){return assets::character::awPart(app.assetCatalog.entries[i].name)==assets::character::Part::Head;};
    const auto playerParts=characterAssemblyParts(app,app.assetCatalog.entries[body]);const auto botParts=characterAssemblyParts(app,app.assetCatalog.entries[body],true);
    const bool noHead=std::none_of(playerParts.begin(),playerParts.end(),isHead)&&std::any_of(botParts.begin(),botParts.end(),isHead);
    if(!noHead)++failures;report<<"AW no head player / headed bot="<<noHead<<'\n';
    report<<"cases="<<cases<<" failures="<<failures<<'\n';return failures?9:0;
}
