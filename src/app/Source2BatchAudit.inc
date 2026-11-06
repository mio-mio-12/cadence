int source2BatchAudit(const std::filesystem::path& batch,const std::filesystem::path& library,const std::filesystem::path& out){
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(640,360,"CS2 batch audit",nullptr,nullptr);if(!window)return 2;
    glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;app.deferSceneUpload=true;app.assetFirstPerson=true;
    std::string error;if(!assets::appendScan(batch,"cs2",app.assetCatalog,error))return 3;
    for(const auto* game:{"bo2","mwr"})if(!assets::appendScan(library/game,game,app.assetCatalog,error))return 3;
    app.defaultSalukiDirectory=library.string();if(!app.renderer.initialize(error))return 4;
    std::filesystem::create_directories(out);std::ofstream report(out/"production.txt");
    std::vector<std::size_t> hands,weapons;
    for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];
        if(a.game=="cs2"&&a.role==assets::Role::ViewWeapon)weapons.push_back(i);
        if(a.role==assets::Role::ViewHands&&(a.game=="cs2"||a.name=="c_usa_mp_seal6_longsleeve_viewhands_LOD0"||a.name=="viewhands_h1_usmc_desert_LOD0"))hands.push_back(i);
    }
    if(const auto* filter=std::getenv("CADENCE_AUDIT_HAND"))std::erase_if(hands,[&](auto i){return app.assetCatalog.entries[i].name!=filter;});
    if(const auto* filter=std::getenv("CADENCE_AUDIT_WEAPON"))std::erase_if(weapons,[&](auto i){return app.assetCatalog.entries[i].name!=filter;});
    int failures{};std::size_t cases{};
    for(const auto hand:hands)for(const auto weapon:weapons){
        app.selectedBaseAsset=app.classViewhandsOverride=hand;app.selectedWeaponAsset=weapon;
        equipViewWeapon(app,weapon);const auto& a=app.assetCatalog.entries[weapon];
        const auto id=app.assetCatalog.entries[hand].game+"_"+a.name+"_"+assets::source2::skin(a);
        if(std::getenv("CADENCE_AUDIT_WORLD")){
            auto actor=scene::buildScene(cast::Document::load(batch/"models/playermodels/ctm_fbi.cast"));
            const bool pistol=weapon::inferArchetype(a.name)==weapon::Archetype::Pistol;
            for(const auto& e:std::filesystem::directory_iterator(batch/"animations"))if(e.path().filename().string().starts_with(pistol?"pb_pistol_stand_idle_clip":"pb_rifle_stand_idle_clip"))appendImportedWorldAnimation(app,cast::Document::load(e.path()),actor,"cs2","cs2");
            scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Idle;q.weapon=pistol?scene::WeaponClass::Pistol:scene::WeaponClass::Rifle;q.stance=scene::Stance::Stand;
            const auto clip=scene::findBestAnimation(actor,q);if(!clip){++failures;continue;}
            const auto document=cast::Document::load(a.path);const auto before=actor.attachments.size();scene::appendAttachment(document,actor,preferredAttachmentBone(actor.skeleton),a.name);configureCs2WorldWeaponAttachment(actor,document,before);
            if(!app.renderer.loadScene(actor,error)){++failures;continue;}
            app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});app.renderer.setFirstPersonProjection(false);
            const auto pose=actor.samplePose(*clip,0);
            const auto vp=scene::perspective(50*scene::kPi/180,640.f/360,1,2000)*scene::lookAt({230,-230,160},{0,0,105},{0,0,1});
            app.renderer.render(actor,pose,vp,640,360,false,false,false);if(!app.renderer.saveColorPng(out/(id+"_world.png"),error))++failures;
            report<<id<<" body="<<actor.animations[*clip].sourceName<<" mount="<<actor.skeleton.bones[actor.attachments.back().boneIndex].name<<'\n';++cases;continue;
        }
        report<<id<<" clips="<<app.scene.animations.size()<<" driver="<<app.scene.viewHandsDriverGame<<" status="<<app.status<<'\n';
        if(app.scene.meshes.empty()||app.scene.viewHandsDriverGame!="cs2"){++failures;continue;}
        if(!app.renderer.loadScene(app.scene,error)){report<<error<<'\n';++failures;continue;}
        app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
        for(const auto* slot:{"idle","pullout","fire","reload"}){
            const auto clip=findViewmodelClip(app,slot);if(!clip){report<<" missing "<<slot<<'\n';if(std::string_view(slot)!="reload"||!app.weaponProfile.meleeWeapon)++failures;continue;}
            const auto& c=app.scene.animations[*clip];const auto pose=app.scene.samplePose(*clip,c.durationFrames*.5f);
            for(const auto& m:pose)for(const float v:m.v)if(!std::isfinite(v))++failures;
            const auto cameraBone=app.scene.skeleton.boneByCanonicalName.find("tag_camera");
            const auto camera=cameraBone!=app.scene.skeleton.boneByCanonicalName.end()&&cameraBone->second<pose.size()?pose[cameraBone->second]:scene::Mat4::identity();
            const scene::Vec3 origin{camera.v[12],camera.v[13],camera.v[14]},forward{camera.v[0],camera.v[1],camera.v[2]},up{camera.v[8],camera.v[9],camera.v[10]};
            app.renderer.setFirstPersonProjection(true);
            const auto vp=scene::perspective(75*scene::kPi/180,640.f/360,.02f,5000)*scene::lookAtDirection(origin,forward,up);
            app.renderer.render(app.scene,pose,vp,640,360,false,false,false);
            if(!app.renderer.saveColorPng(out/(id+"_"+slot+".png"),error))++failures;
            report<<" "<<slot<<" "<<c.sourceName<<'\n';
        }++cases;
    }
    report<<"cases="<<cases<<" failures="<<failures<<'\n';std::cout<<"CS2 production cases="<<cases<<" failures="<<failures<<'\n';
    app.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return failures?1:0;
}
