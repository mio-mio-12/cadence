#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main

// Calls production recording, playback, input handling and viewport rendering.
// Does not load or save any user settings, or interact with a running Cadence.
int main(int argc,char** argv){
    if(argc!=2||!glfwInit())return 1;
    const std::filesystem::path output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output);
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(960,720,"Replay flow audit",nullptr,nullptr);if(!window)return 2;
    glfwMakeContextCurrent(window);glfwSwapInterval(0);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;std::string error;
    if(!app.viewportAspectLocked||std::abs(app.viewportAspect-1.77f)>.0001f)return 57;
    if(!app.renderer.initialize(error))return 3;
    app.defaultSalukiDirectory="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
    if(!assets::appendScan(app.defaultSalukiDirectory/"bo2","bo2",app.assetCatalog,error))return 4;
    for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];
        if(a.name=="t6_wpn_ar_an94_view_LOD0")app.classPrimaryAsset=i;
        if(a.name=="t6_wpn_sniper_ballista_view_LOD0")app.classSecondaryAsset=i;
        if(a.role==assets::Role::ViewHands&&app.classViewhandsOverride>=app.assetCatalog.entries.size())app.classViewhandsOverride=i;
    }
    loadBothClassSlots(app);
    while(app.pendingClassFuture){processPendingClassLoad(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    if(!app.classGpuResident){std::cerr<<app.status;return 5;}
    // The real trigger must select both the empty clip AND its own duration.
    app.gameplayLogic=true;
    const auto reloadTestTiming=app.weaponTiming;
    app.weaponTiming.reloadTime=1.1f;app.weaponTiming.reloadEmptyTime=2.7f;app.weaponTiming.rechamberTime=6.f;
    for(int variant=0;variant<3;++variant){
        app.gameplayAction=scene::ActionRole::Reload;
        app.gameplayReloadEmpty=variant==1;app.gameplayRechamber=variant==2;
        triggerGameplayAction(app);
        const float expected=variant==0?1.1f:variant==1?2.7f:6.f;
        if(std::abs(app.actionDurationOverride-expected)>.0001f)return 50;
        const auto clip=app.actionOverlay?app.actionAnimationIndex:app.animationIndex;
        if(variant==1&&(!app.actionActive||clip>=app.scene.animations.size()||app.scene.animations[clip].sourceName.find("reload_empty")==std::string::npos))return 51;
        stopGameplayAction(app);
    }
    app.weaponTiming=reloadTestTiming;app.gameplayReloadEmpty=false;app.gameplayRechamber=false;app.gameplayLogic=false;
    app.actorMode=true;app.actorThirdPerson=false;app.actorFreecamRetainViewmodel=false;
    app.actorPosition=app.actorRenderPosition={100,200,0};app.actorYaw=.3f;app.cameraPitch=0;
    app.cameraTarget={150,220,95};app.cameraDistance=350;app.cameraYaw=scene::kPi;app.cameraPitch=-.12f;
    app.actorCaptureRequested=app.actorInputCaptured=true;
    toggleTakeRecording(app);
    for(int frame=1;frame<=60;++frame){
        if(frame==30)activateClassSlot(app,1);
        app.actorPosition=app.actorRenderPosition={100+float(frame)*2,200+float(frame),0};
        app.animationFrame=float(frame%20);captureTakeSample(app,float(frame)/30.f);
    }
    toggleTakeRecording(app);
    if(app.recordedTake.samples.size()!=61||app.takePreview)return 6;
    if(!app.hiddenWorldActor||app.recordedTake.samples[0].worldActor.pose.empty())return 18;
    if(!take::save(app.recordedTake,output/"fresh.c_dm",error))return 7;
    app.actorPosition=app.actorRenderPosition={10000,-5000,2000};
    app.actorViewCamera=scene::translation(app.actorPosition);app.actorViewCameraValid=true;
    const auto draw=[&](bool f2=false,bool fullUi=false){
        ImGui::GetIO().AddKeyEvent(ImGuiKey_F2,f2);
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
        handleTakeCameraInput(app,1.f/60);
        if(fullUi)drawUi(app);
        else {ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({960,720});
            ImGui::Begin("Replay audit",nullptr,ImGuiWindowFlags_NoDecoration);
            drawViewport(app,940,690);ImGui::End();}
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glFinish();
    };
    const auto checkCamera=[&](){
        const auto s=app.recordedTake.interpolatedSample(app.takeTime,{&app.scene.skeleton});
        const auto i=app.scene.skeleton.boneByCanonicalName.at("tag_camera");const auto& m=s.pose.at(i);
        return scene::length(app.lastRenderedCameraPosition-scene::Vec3{m.v[12],m.v[13],m.v[14]})<.01f;
    };
    std::ofstream report(output/"results.txt");
    for(int saved=0;saved<2;++saved){
        const auto liveClipCount=app.scene.animations.size();
        const auto liveWeapon=app.selectedWeaponAsset;
        if(saved){
            // Exercise the same restore/prewarm path as the actual Load button.
            app.actorCaptureRequested=true;app.freeCameraActive=true;
            app.pendingModelKind=1;
            if(openRecordedTake(app,output/"fresh.c_dm")||app.scene.animations.size()!=liveClipCount)return 61;
            app.pendingModelKind=0;
            if(!openRecordedTake(app,output/"fresh.c_dm"))return 8;
            draw();if(!app.takePreview||!app.takeFirstPersonView||!checkCamera()||app.actorCaptureRequested)return 52;
            draw(true);draw(false);draw(true);draw(false);
            if(!app.takeFirstPersonView||!checkCamera())return 53;
        }else{
            // F2 must enter replay immediately, without first pressing Play.
            draw(true);draw(false);
            if(!app.takePreview||!app.takeFirstPersonView||!checkCamera())return 54;
        }
        toggleTakePlayback(app);draw();
        if(!app.takePreview||!app.takeFirstPersonView||!app.takePlaying||app.actorInputCaptured||app.actorCaptureRequested||!checkCamera())return 9;
        for(float t:{0.f,.5f,.99f,1.f,1.5f,2.f,.2f}){
            app.takeTime=t;draw();if(!checkCamera()||!app.takePreview)return 10;
        }
        app.takeTime=.5f;draw();
        if(!app.renderer.saveColorPng(output/(saved?"loaded-first.png":"fresh-first.png"),error))return 11;
        draw(true);if(app.takeFirstPersonView||!app.takePreview||app.actorInputCaptured)return 12;
        if(!app.renderer.saveColorPng(output/(saved?"loaded-third.png":"fresh-third.png"),error))return 13;
        draw(false);draw(true);if(!app.takeFirstPersonView||!app.takePreview||!checkCamera())return 14;
        const auto livePosition=app.actorPosition;const auto liveClock=app.gameplayClock;
        for(int i=0;i<4;++i){draw(false,true);if(!app.takePreview||!checkCamera()||scene::length(app.actorPosition-livePosition)>.001f||app.gameplayClock!=liveClock)return 19;}
        draw(false);toggleTakePlayback(app);draw();if(app.takePlaying||!app.takePreview||!checkCamera())return 15;
        // Conflicting live flags must not replace the recorded camera or rig.
        app.actorThirdPerson=true;app.actorFreecamRetainViewmodel=true;app.navigationClickPlacement=true;
        draw();if(!checkCamera())return 16;
        app.actorThirdPerson=false;app.actorFreecamRetainViewmodel=false;app.navigationClickPlacement=false;
        report<<(saved?"saved-reopened":"fresh-recorded")<<": PASS Play Replay, both weapon slots, F2 twice, paused, scrub backwards, live flags isolated\n";report.flush();
        app.cameraEditMode=true;app.freeCameraActive=true;app.actorThirdPerson=true;app.navigationClickPlacement=true;
        returnToLiveGameplay(app);draw(false,true);
        if(app.takePreview||app.takeFirstPersonView||app.cameraEditMode||app.freeCameraActive||app.actorThirdPerson||app.navigationClickPlacement||!app.viewmodelCamera)return 55;
        if(saved&&(app.scene.animations.size()!=liveClipCount||app.selectedWeaponAsset!=liveWeapon||app.liveClassBeforeTake))return 60;
        draw();
        if(!app.renderer.saveColorPng(output/(saved?"loaded-return-live.png":"fresh-return-live.png"),error))return 56;
        report<<"PASS immediate F2 and stop/return live first-person state restoration\n";report.flush();
    }
    // Current presentation controls must work on a paused, already-loaded demo.
    app.takePreview=true;app.takeFirstPersonView=true;app.takePlaying=false;app.takeTime=.55f;
    draw();const auto recorded=app.recordedTake.interpolatedSample(app.takeTime,{&app.scene.skeleton});
    const auto camera=recorded.pose.at(app.scene.skeleton.boneByCanonicalName.at("tag_camera"));
    const scene::Vec3 eye{camera.v[12],camera.v[13],camera.v[14]},forward{camera.v[0],camera.v[1],camera.v[2]},side{camera.v[4],camera.v[5],camera.v[6]},up{camera.v[8],camera.v[9],camera.v[10]};
    take::ShotEvent shot;shot.time=.4f;shot.muzzlePos=eye+forward*150.f+side*40.f;shot.origin=eye;shot.direction=forward;shot.hitPos=shot.muzzlePos+forward*500.f+side*70.f;
    shot.smokeEnabled=false;shot.trailWidth=0;shot.muzzleFlashSize=0;app.recordedTake.shots={shot};
    if(!take::save(app.recordedTake,output/"before-visual.c_dm",error)){std::cerr<<error;return 20;}
    const auto pixels=[&](){draw();std::vector<std::uint8_t> p;if(!app.renderer.readColorRgba(p,error))std::abort();return p;};
    app.debugMuzzleFlash=false;app.smokeTrailEnabled=false;app.projectileTrailEnabled=false;app.sniperTrailEnabled=false;
    const auto baseline=pixels();
    app.debugMuzzleFlash=true;app.muzzleFlashDuration=.5f;app.muzzleFlashSize=2;app.muzzleFlashColor={1,.1f,.1f,1};
    const auto flash=pixels();if(flash==baseline)return 21;
    app.muzzleFlashSize=4;const auto flashBig=pixels();if(flash==flashBig)return 22;
    app.debugMuzzleFlash=false;app.smokeTrailEnabled=true;app.smokeTrailColor={.2f,1,.2f,1};app.smokeTrailStartWidth=20;app.smokeTrailEndWidth=50;
    const auto smoke=pixels();if(smoke==baseline||smoke!=pixels())return 23;
    app.takeTime=1.2f;pixels();app.takeTime=.55f;if(smoke!=pixels())return 24;
    app.smokeTrailBlastSpeed=150;if(smoke==pixels())return 25;
    if(!app.renderer.saveColorPng(output/"editable-smoke.png",error))return 37;
    app.smokeTrailEnabled=false;app.projectileTrailEnabled=true;app.projectileTrailSpeed=1500;app.projectileTrailLength=150;app.projectileTrailWidth=5;
    const auto projectile=pixels();if(projectile==baseline)return 26;
    app.projectileTrailWidth=12;if(projectile==pixels())return 27;
    app.projectileTrailEnabled=false;if(pixels()!=baseline)return 28;
    app.recordedTake.shots[0].sniperWeapon=true;app.sniperTrailEnabled=true;app.sniperTrailWidth=5;app.sniperTrailLifetime=.75f;
    const auto sniper=pixels();if(sniper==baseline)return 29;
    app.sniperTrailWidth=15;if(sniper==pixels())return 30;
    app.sniperTrailEnabled=false;if(pixels()!=baseline)return 31;
    app.recordedTake.shots[0].sniperWeapon=false;
    const float oldHip=app.viewmodelFov,oldScale=app.viewmodelFovScale;
    app.viewmodelFov=100;draw();if(std::abs(app.lastRenderedCameraFov-cadence::replay::presentationFov(app.recordedTake,recorded,100,oldScale))>.001f)return 32;
    app.takeTime=1.5f;draw();if(app.viewmodelFov!=100)return 33;
    app.takeTime=.55f;draw();if(app.viewmodelFov!=100)return 34;
    app.viewmodelFov=oldHip;app.viewmodelFovScale=oldScale;
    if(!take::save(app.recordedTake,output/"after-visual.c_dm",error))return 35;
    const auto read=[](const auto& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
    if(read(output/"before-visual.c_dm")!=read(output/"after-visual.c_dm"))return 36;
    report<<"PASS paused live-style flash/smoke/projectile/sniper pixel changes, toggles, smoke repeated/seek pixels, FOV edits across slots; take bytes unchanged\n";
    report.flush();
    // Render the actual timeline controls at wide and sidebar widths.
    const auto savedTiming=app.weaponTiming;
    app.weaponTiming.recoil.intensity=0;const auto withoutRecoil=pixels();const auto noKickCamera=app.lastRenderedCameraRotationDegrees;
    app.weaponTiming.hipKickPitchMin=app.weaponTiming.hipKickPitchMax=8;
    app.weaponTiming.adsKickPitchMin=app.weaponTiming.adsKickPitchMax=8;
    app.weaponTiming.hipKickCenterSpeed=app.weaponTiming.adsKickCenterSpeed=15;
    app.weaponTiming.recoil.duration=.4f;app.weaponTiming.recoil.intensity=1;
    const auto withRecoil=pixels();const auto kickCamera=app.lastRenderedCameraRotationDegrees;
    if(withRecoil==withoutRecoil||scene::length(kickCamera-noKickCamera)<.1f||withRecoil!=pixels())return 46;
    if(!app.renderer.saveColorPng(output/"recoil-enabled.png",error))return 47;
    app.takePlaybackSpeed=.1f;const auto slowPixels=pixels();
    if(withRecoil!=slowPixels){std::size_t differences=0;for(std::size_t i=0;i<std::min(withRecoil.size(),slowPixels.size());++i)differences+=withRecoil[i]!=slowPixels[i];std::cerr<<"Slow pixel differences="<<differences<<" sizes="<<withRecoil.size()<<','<<slowPixels.size()<<" cameraDelta="<<scene::length(app.lastRenderedCameraRotationDegrees-kickCamera)<<" time="<<app.takeTime<<'\n';(void)app.renderer.saveColorPng(output/"recoil-slow-mismatch.png",error);return 48;}
    app.takeTime=.1f;pixels();app.takeTime=.55f;if(withRecoil!=pixels())return 49;
    app.takeFirstPersonView=false;app.cameraEditMode=true;app.freeCameraActive=true;app.freeCameraPosition={400,0,170};app.freeCameraRotationDegrees={5,180,0};
    const auto freeRecoil=pixels();app.weaponTiming.recoil.intensity=0;if(freeRecoil!=pixels())return 50;
    app.cameraEditMode=false;app.freeCameraActive=false;app.takeFirstPersonView=true;app.weaponTiming.recoil.intensity=1;
    // Actual production ProRes pipes: all three passes at one tenth timescale.
    app.exportWidth=320;app.exportHeight=240;app.exportFps=30;app.exportStart=.44f;app.exportEnd=.48f;
    app.exportFrame=0;app.exportFrameCount=12;app.exportBeautyPass=true;app.exportDepthPass=true;app.exportViewmodelPass=true;app.exportNavigationPass=false;
    app.exportCameraData=true;app.exportCaptureReShade=false;app.exportDepthFormat=1;
    const auto movie=output/("recoil-slow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".mov");
    if(!startProResExport(app,movie))return 51;
    app.exportActive=true;app.takePlaying=false;app.takeTime=app.exportStart;
    for(int i=0;i<12;++i){const float time=app.takeTime;app.exportActive=false;draw();const auto previewCamera=app.lastRenderedCameraRotationDegrees;app.exportActive=true;app.takeTime=time;draw();if(scene::length(app.lastRenderedCameraRotationDegrees-previewCamera)>.0001f)return 52;}
    if(app.exportActive||app.exportFrame!=12)return 53;
    app.takeTime=.55f;app.takePlaybackSpeed=1;app.weaponTiming=savedTiming;
    if(!take::save(app.recordedTake,output/"after-recoil.c_dm",error)||read(output/"before-visual.c_dm")!=read(output/"after-recoil.c_dm"))return 54;
    report<<"PASS recoil pixel/camera changes, paused repeat, slow-time and backward seek equality, freecam exclusion, unchanged take bytes; 12-frame 0.1x ProRes beauty/depth/viewmodel cameras match preview\n";report.flush();
    for(int width:{940,330}){
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
        ImGui::SetNextWindowPos({10,10});ImGui::SetNextWindowSize({float(width),170});
        ImGui::Begin("Replay timeline",nullptr,ImGuiWindowFlags_NoResize);
        drawIWXMVMVisualTimeline(app,-1,56);ImGui::End();ImGui::Render();
        glViewport(0,0,960,720);glClearColor(.04f,.04f,.04f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glFinish();
        std::vector<std::uint8_t> uiPixels(960*720*4);glReadPixels(0,0,960,720,GL_RGBA,GL_UNSIGNED_BYTE,uiPixels.data());
        if(!app.renderer.savePixelsPng(output/("timeline-"+std::to_string(width)+".png"),960,720,uiPixels,error))return 38;
    }
    // A hidden, unfocused GLFW window must still export real frame images.
    if(glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE)return 39;
    app.exportFolder=output/("unfocused-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(app.exportFolder);
    app.exportWidth=320;app.exportHeight=240;app.exportStart=.55f;app.exportEnd=.6f;app.takeTime=.55f;
    app.exportFrame=0;app.exportFrameCount=2;app.exportFps=30;app.exportProRes=false;app.exportCameraOnly=false;
    app.exportCaptureReShade=false;app.exportDepthPass=false;app.exportCameraData=true;app.exportActive=true;
    app.cameraNearClipCm=.1f;app.cameraFarClipMeters=200.f;
    for(int i=0;i<2;++i){if(cadence::replay::pauseForInactiveWindow(false,false,app.exportActive))return 40;draw(false,true);}
    if(app.exportActive||app.exportFrame!=2||!std::filesystem::exists(app.exportFolder/"frame_000001.png"))return 41;
    report<<"PASS unfocused full-UI two-frame PNG + camera CSV export at 320x240\n";report.flush();
    // Broken data must pause replay instead of silently drawing a live pose.
    app.recordedTake.samples[0].pose.clear();app.takeTime=0;app.takePreview=true;app.takePlaying=true;draw();
    if(app.takePlaying)return 17;
    report<<"PASS incompatible sample pauses without live fallback\n";
    report.flush();
    if(!assets::appendScan(app.defaultSalukiDirectory/"mw3","mw3",app.assetCatalog,error))return 42;
    std::ofstream mw3Report(output/"mw3-world-pairs.txt");
    for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& asset=app.assetCatalog.entries[i];
        if(asset.game=="mw3"&&asset.role==assets::Role::ViewWeapon){const auto world=findWorldWeaponForViewWeapon(app,asset);mw3Report<<asset.name<<" -> "<<(world<app.assetCatalog.entries.size()?app.assetCatalog.entries[world].name:"MISSING")<<'\n';}
        if(asset.game=="mw3"&&asset.name=="viewmodel_model_1887_LOD0")app.classPrimaryAsset=i;
        if(asset.game=="bo2"&&asset.name=="t6_wpn_ar_an94_view_LOD0")app.classSecondaryAsset=i;
        if(asset.game=="bo2"&&asset.name=="c_usa_mp_isa_smg_viewhands_LOD0")app.classViewhandsOverride=i;
    }
    mw3Report.flush();app.takePreview=false;app.takePlaying=false;app.actorMode=true;app.actorThirdPerson=true;app.cameraEditMode=false;
    loadBothClassSlots(app);while(app.pendingClassFuture){processPendingClassLoad(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    mw3Report<<"Selected primary "<<app.assetCatalog.entries[app.classPrimaryAsset].name<<" world="<<app.classWorldWeaponAsset<<" attachments="<<(app.hiddenWorldActor?app.hiddenWorldActor->attachments.size():0)<<"\n";
    if(app.classWorldWeaponAsset>=app.assetCatalog.entries.size()||!app.hiddenWorldActor||app.hiddenWorldActor->attachments.empty())return 43;
    app.actorPosition=app.actorRenderPosition={0,0,0};app.actorYaw=0;app.cameraPitch=0;app.actorFollowCamera=true;app.actorShoulderCameraValid=false;
    draw();if(!app.renderer.saveColorPng(output/"mw3-1887-world.png",error))return 44;
    app.cameraEditMode=true;app.freeCameraActive=true;app.takeFirstPersonView=false;app.freeCameraPosition={180,-180,140};app.freeCameraRotationDegrees={4.62f,133.36f,0};app.freeCameraFov=50;
    draw();if(!app.renderer.saveColorPng(output/"mw3-1887-side.png",error))return 45;
    report<<"PASS MW3 1887 production class world assembly and render\n";
    // Exercise the real Class UI while hovering the moved third-weapon toggle.
    returnToLiveGameplay(app);
    for(int frame=0;frame<4;++frame){
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();
        ImGui::GetIO().AddMousePosEvent(30,20);ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({960,720});
        ImGui::Begin("Class controls audit",nullptr,ImGuiWindowFlags_NoDecoration);
        const auto thirdId=ImGui::GetID("Third weapon##enable_third_weapon");
        drawCreateAClass(app);
        if(frame==3&&ImGui::GetCurrentContext()->HoveredId!=thirdId)return 62;
        ImGui::End();ImGui::Render();
        if(ImGui::GetCurrentContext()->DebugDrawIdConflictsCount>0)return 58;
        glViewport(0,0,960,720);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glFinish();
        if(frame==3){std::vector<std::uint8_t> image(960*720*4);glReadPixels(0,0,960,720,GL_RGBA,GL_UNSIGNED_BYTE,image.data());
            if(!app.renderer.savePixelsPng(output/"class-controls.png",960,720,image,error))return 59;}
    }
    report<<"PASS Class control hover without ID conflicts; default aspect lock 1.77; reload/empty/rechamber timings separated\n";report.flush();
    app.renderer.shutdown();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
}
