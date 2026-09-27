#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<error<<" "<<app.status<<'\n';return 1;}}while(false)

// Deterministic view/pose workload, not a recorded reproduction of an unknown
// user's camera path. Never writes settings, presets or source assets.
int main(int argc,char** argv)try{
    const bool profilePasses=argc==5&&std::string_view(argv[4])=="--gpu-passes";
    const bool shadowAb=argc==5&&std::string_view(argv[4])=="--shadow-ab";
    const bool cullAb=argc==5&&std::string_view(argv[4])=="--cull-ab";
    const bool decalAb=argc==5&&std::string_view(argv[4])=="--decal-ab";
    if(argc!=4&&!profilePasses&&!shadowAb&&!cullAb&&!decalAb){std::cerr<<"map visual-preset output-directory [--gpu-passes|--shadow-ab|--cull-ab|--decal-ab]\n";return 2;}
    const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
    if(!glfwInit())return 3;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(1920,1080,"Full scene audit",nullptr,nullptr);if(!window)return 4;
    glfwMakeContextCurrent(window);glfwSwapInterval(0);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    auto owner=std::make_unique<AppState>();auto& app=*owner;std::string error;app.window=window;
    CHECK(app.renderer.initialize(error));app.renderer.setGpuPassProfiling(profilePasses);
    app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
    for(auto game:{"bo2","csnz","cso2"})CHECK(assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error));
    const auto find=[&](std::string_view game,std::string_view name){for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game==game&&a.name==name)return i;}return SIZE_MAX;};
    CHECK(cadence::loadout_defaults::applyBo2TestingDefaults(app.assetCatalog,app.classPrimaryAsset,app.classSecondaryAsset,app.classViewhandsOverride,app.classFaction));
    app.experimentalThirdWeapon=true;app.classThirdAsset=find("csnz","viewmodel_v_as50");CHECK(app.classThirdAsset!=SIZE_MAX);
    app.autoPlayerModel=false;
    std::cout<<"Loading three-weapon class\n"<<std::flush;
    loadBothClassSlots(app);while(app.pendingClassFuture){processPendingClassLoad(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    CHECK(app.classGpuResident&&app.classSlotRigs[0]&&app.classSlotRigs[1]&&app.classSlotRigs[2]);
    app.enemyBotCount=8;app.botGame="cso2";app.botModelAsset=find("cso2","ct_707");CHECK(app.botModelAsset!=SIZE_MAX);
    app.botTeamSide=0;app.botFactionModelVariety=false;app.botAnimationGame="bo2";app.botSystemMode=0;
    rebuildBotActors(app);CHECK(app.botActorScene&&app.bots.size()==8);
    std::cout<<"Loading map\n"<<std::flush;
    app.loadedMap.emplace();CHECK(scene::glb::load(argv[1],*app.loadedMap,error,1,false));
    // Use the production resident-class upload: loadAuxiliaryScenes alone
    // deliberately drops slot residency and is not valid after class loading.
    CHECK(uploadResidentClassScenes(app));
    CHECK(loadVisualPreset(app,argv[2]));syncVisualRendererSettings(app);
    app.renderer.setHbao(app.hbao);app.renderer.setDepthOfField(app.dof);app.renderer.setVolumetricLighting(app.volumetric);
    app.renderer.setWater(app.water,2);app.renderer.setRain(app.rain,2,&*app.loadedMap);app.renderer.setWeather(app.weather,2,1);app.renderer.setWetSurfaces(app.wet);
    std::vector<float> xs,ys,zs;
    for(const auto& mesh:app.loadedMap->scene.meshes)if(!mesh.vertices.empty()){
        scene::Vec3 mean{};for(const auto& v:mesh.vertices)mean+=scene::transformPoint(mesh.modelTransform,v.position);mean=mean/float(mesh.vertices.size());xs.push_back(mean.x);ys.push_back(mean.y);zs.push_back(mean.z);
    }
    CHECK(!xs.empty());for(auto* v:{&xs,&ys,&zs})std::sort(v->begin(),v->end());
    const scene::Vec3 center{xs[xs.size()/2],ys[ys.size()/2],zs[zs.size()/3]+160};
    std::ofstream log(out/"timings.csv");log<<"view,slot,mode,completed_ms,pose_cpu_ms,submit_cpu_ms,gpu_ms,map_draws,shadow_draws,total_draws,viewmodel_meshes,bot_meshes\n";
    std::ofstream passLog;if(profilePasses){passLog.open(out/"gpu-passes.csv");passLog<<"view,slot,mode,frame,stage_index,stage,milliseconds\n";}
    std::ofstream shadowLog;if(shadowAb){shadowLog.open(out/"shadow-ab.csv");shadowLog<<"view,slot,case,elision,completed_ms,changed_channels,max_error\n";}
    std::ofstream cullLog;if(cullAb){cullLog.open(out/"cull-ab.csv");cullLog<<"view,slot,variant,iteration,enabled,completed_ms,submit_ms,queries,enable_writes,face_writes,draws,changed,max_error\n";}
    std::ofstream decalLog;if(decalAb){decalLog.open(out/"decal-ab.csv");decalLog<<"view,slot,iteration,enabled,completed_ms,draws,changed,max_error\n";}
    std::ofstream setup(out/"setup.txt");setup<<"Map: "<<argv[1]<<"\nPreset: "<<argv[2]<<"\n1920x1080; eight identical ct_707 bots, BO2 animations, AN94 world weapons. Three resident viewmodel slots. No quality changes.\n";
    setup<<"Camera center: "<<center.x<<','<<center.y<<','<<center.z<<"\n";
    const float az=app.sunAzimuth*scene::kPi/180,el=app.sunElevation*scene::kPi/180;
    for(int view=0;view<7;++view){
        const auto camera=view==6?center+scene::Vec3{0,-900,600}:center+scene::Vec3{view>=4?600.f:0.f,view>=4?-600.f:0.f,0};
        const float yaw=view==6?scene::kPi*.5f:view*scene::kPi*.5f;const auto forward=scene::normalize(scene::Vec3{std::cos(yaw),std::sin(yaw),view==6?-.65f:-.1f});
        const auto lateral=scene::normalize(scene::cross(scene::Vec3{0,0,1},forward));
        app.actorPosition=camera-scene::Vec3{0,0,app.actorViewHeight};app.actorRenderPosition=app.actorPosition;app.actorYaw=yaw;
        app.actorMode=true;app.actorFollowCamera=false;app.botFoundationMovement=false;app.botAllowEquipment=false;app.botAllowJumpDrop=false;app.botAllowSidequests=false;
        for(std::size_t i=0;i<app.bots.size();++i){auto& bot=app.bots[i];
            auto p=camera+forward*(450.f+float(i/4)*300.f)+lateral*((float(i%4)-1.5f)*140.f);
            p.z=actorGroundHeight(app,p.x,p.y,camera.z+500);if(p.z<-1e8f)p.z=camera.z-160;
            bot.position=bot.previousPhysicsPosition=bot.spawnPosition=p;bot.velocity={};bot.yaw=yaw+scene::kPi;bot.grounded=true;bot.input={};
        }
        updateBotActors(app,0);const auto fixedBots=app.bots;
        std::vector<int> variants;for(const auto& bot:app.bots)variants.push_back(bot.modelVariant);
        app.renderer.setCameraPosition(camera);app.renderer.setCameraDepthRange(1,100000);app.renderer.setEnvironmentCamera(forward,{0,0,1},75,1920.f/1080);
        app.renderer.setSun(app.sunLighting,app.sunShadows,{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),-std::sin(el)},app.sunIntensity,app.sunAmbient,app.sunColor,app.ambientColor,app.shadowResolution,app.shadowDistanceMeters*gameplay::iw::kMetersToUnits,app.shadowFadeStartMeters*gameplay::iw::kMetersToUnits,camera,app.farShadowEnabled,app.farShadowResolution,app.farShadowStartMeters*gameplay::iw::kMetersToUnits,app.farShadowDistanceMeters*gameplay::iw::kMetersToUnits,app.farShadowBlendMeters*gameplay::iw::kMetersToUnits);
        const auto cameraWorld=scene::inverseAffine(scene::lookAtDirection(camera,forward,{0,0,1}));
        const auto vp=scene::perspective(75*scene::kPi/180,1920.f/1080,1,100000)*scene::lookAtDirection(camera,forward,{0,0,1});
        for(int slot=0;slot<3;++slot){
            activateClassSlot(app,slot);app.actorMode=false;
            if(profilePasses||shadowAb||cullAb||decalAb){app.gameplayLogic=true;app.actionActive=app.actionOverlay=false;app.activeAction=scene::ActionRole::None;app.gameplayAds=false;app.movementSpeed=0;CHECK(resolveGameplayAnimation(app));app.transitioning=false;app.animationFrame=0;
                if(slot<2){const auto& clip=app.scene.animations[app.animationIndex].sourceName;CHECK(clip.find("gl_grenade")==std::string::npos&&clip.find("dw_left")==std::string::npos);}}
            auto pose=evaluateCurrentPose(app);app.actorMode=true;
            if(view==0)setup<<"Slot "<<slot<<": "<<app.assetCatalog.entries[app.selectedWeaponAsset].game<<'/'<<app.assetCatalog.entries[app.selectedWeaponAsset].name<<" clip="<<(app.animationIndex<app.scene.animations.size()?app.scene.animations[app.animationIndex].sourceName:"<rest>")<<" frame="<<app.animationFrame<<"\n";
            const auto cameraBone=app.scene.skeleton.boneByCanonicalName.find("tag_camera");CHECK(cameraBone!=app.scene.skeleton.boneByCanonicalName.end()&&cameraBone->second<pose.size());
            const auto rigCamera=pose[cameraBone->second];
            const scene::Vec3 rigOrigin{rigCamera.v[12],rigCamera.v[13],rigCamera.v[14]},rigForward{rigCamera.v[0],rigCamera.v[1],rigCamera.v[2]},rigUp{rigCamera.v[8],rigCamera.v[9],rigCamera.v[10]};
            const auto mount=cameraWorld*scene::lookAtDirection(rigOrigin,rigForward,rigUp);
            for(auto& bone:pose)bone=mount*bone;
            app.renderer.setFirstPersonProjection(true);app.renderer.setViewmodelProjection(app.weaponProfile.viewmodelFovMultiplier,app.weaponProfile.flipViewmodel);
            bool auditWire=false;
            const auto draw=[&]{app.renderer.render(app.scene,pose,vp,1920,1080,false,false,auditWire,&*app.botActorScene,&app.botActorPoses,&variants,nullptr,nullptr,true,&app.loadedMap->scene);};
            if(cullAb){
                #include "CullFullSceneAudit.inc"
                continue;
            }
            if(decalAb){
                #include "DecalFullSceneAudit.inc"
                continue;
            }
            std::vector<std::uint8_t> referencePixels;
            std::vector<std::vector<scene::Mat4>> referenceBotPoses;
            // mode0 freezes an evaluated pose, mode1 evaluates identical inputs
            // per frame. This separates pose cost without changing pixels.
            for(int mode=0;mode<2;++mode){
                // fixedBots was captured after the initial placement update.
                // Evaluate that same input for both sides, not initial input A
                // versus its rounded/canonicalized result B on the first slot.
                if(profilePasses&&mode==0){app.bots=fixedBots;updateBotActors(app,0);}
                for(int i=0;i<20;++i)draw();glFinish();double poseMs{},submitMs{};
                const auto minimumFrame=app.renderer.gpuPassTimings().frame+8;std::uint64_t lastFrame=minimumFrame;
                std::vector<render::GpuPassTimer::Result> passResults;passResults.reserve(100);
                const auto begin=std::chrono::steady_clock::now();
                for(int i=0;i<100;++i){
                    const auto p=std::chrono::steady_clock::now();if(mode){app.bots=fixedBots;updateBotActors(app,0);}
                    const auto s=std::chrono::steady_clock::now();poseMs+=std::chrono::duration<double,std::milli>(s-p).count();draw();
                    submitMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s).count();
                    if(profilePasses){const auto& result=app.renderer.gpuPassTimings();if(result.frame>lastFrame){passResults.push_back(result);lastFrame=result.frame;}}
                }glFinish();const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/100;
                if(profilePasses){CHECK(glGetError()==GL_NO_ERROR);CHECK(!passResults.empty());for(const auto& result:passResults)for(int stage=0;stage<result.count;++stage){const auto& sample=result.samples[stage];CHECK(std::isfinite(sample.milliseconds)&&sample.milliseconds>=0);passLog<<view<<','<<slot<<','<<mode<<','<<result.frame<<','<<stage<<','<<sample.name<<','<<sample.milliseconds<<'\n';}}
                const auto stats=app.renderer.renderStats(false);
                log<<view<<','<<slot<<','<<mode<<','<<ms<<','<<poseMs/100<<','<<submitMs/100<<','<<app.renderer.gpuFrameMilliseconds()<<','<<stats.lastVisibleMapMeshes<<','<<stats.lastShadowCasterDraws+stats.lastFarShadowCasterDraws<<','<<stats.lastTotalDrawCalls<<','<<app.scene.meshes.size()<<','<<app.botActorScene->meshes.size()<<std::endl;
                if(mode==0){CHECK(app.renderer.readColorRgba(referencePixels,error));if(profilePasses)referenceBotPoses=app.botActorPoses;}
                if(mode==1){
                    std::vector<std::uint8_t> pixels;CHECK(app.renderer.readColorRgba(pixels,error));CHECK(pixels.size()==referencePixels.size());
                    std::size_t changed{};int maximum{};for(std::size_t i=0;i<pixels.size();++i)if(pixels[i]!=referencePixels[i]){++changed;maximum=std::max(maximum,std::abs(int(pixels[i])-int(referencePixels[i])));}
                    setup<<"View "<<view<<" slot "<<slot<<" cached/evaluated changed channels="<<changed<<" max="<<maximum<<std::endl;
                    if(profilePasses){float poseDifference=0;CHECK(referenceBotPoses.size()==app.botActorPoses.size());for(std::size_t actor=0;actor<referenceBotPoses.size();++actor){CHECK(referenceBotPoses[actor].size()==app.botActorPoses[actor].size());for(std::size_t bone=0;bone<referenceBotPoses[actor].size();++bone)for(int element=0;element<16;++element)poseDifference=std::max(poseDifference,std::abs(referenceBotPoses[actor][bone].v[element]-app.botActorPoses[actor][bone].v[element]));}setup<<"Cached/evaluated global matrix maximum difference="<<poseDifference<<std::endl;}
                    CHECK(app.renderer.saveColorPng(out/("view_"+std::to_string(view)+"_slot_"+std::to_string(slot)+".png"),error));
                    if(profilePasses){app.renderer.setGpuPassProfiling(false);draw();std::vector<std::uint8_t> unprofiled;CHECK(app.renderer.readColorRgba(unprofiled,error));CHECK(unprofiled.size()==pixels.size());int largest=0;std::size_t changed=0;for(std::size_t i=0;i<pixels.size();++i){largest=std::max(largest,std::abs(int(pixels[i])-int(unprofiled[i])));changed+=pixels[i]!=unprofiled[i];}setup<<"Profiler on/off view "<<view<<" slot "<<slot<<" changed="<<changed<<" max="<<largest<<std::endl;CHECK(largest<=1);CHECK(glGetError()==GL_NO_ERROR);app.renderer.setGpuPassProfiling(true);}
                }
            }
            if(shadowAb){
                // All comparisons hold geometry/pose/time fixed. Case0 is the
                // actual preset; variants exercise exact zero contribution,
                // finite softness extremes and near/far/self-shadow policies.
                for(int variant=0;variant<8;++variant){
                    const float intensity=variant==1?0.f:variant==2?1.f:app.shadowIntensity;
                    const float softness=variant==3?0.f:variant==4?4.f:app.shadowSoftness;
                    const bool farEnabled=variant==5?false:app.farShadowEnabled;
                    const float nearDistance=variant==5?10.f:app.shadowDistanceMeters;
                    const float fadeStart=variant==5?5.f:app.shadowFadeStartMeters;
                    app.renderer.setShadowStyle(intensity,app.shadowBias,app.shadowNormalBias,softness,app.shadowContrast,app.shadowTint);
                    app.renderer.setViewmodelShadows(variant==6?false:variant==7?true:app.viewmodelSelfShadows,app.viewmodelShadowResolution);
                    app.renderer.setSun(app.sunLighting,app.sunShadows,{std::cos(az)*std::cos(el),std::sin(az)*std::cos(el),-std::sin(el)},app.sunIntensity,app.sunAmbient,app.sunColor,app.ambientColor,app.shadowResolution,nearDistance*gameplay::iw::kMetersToUnits,fadeStart*gameplay::iw::kMetersToUnits,camera,farEnabled,app.farShadowResolution,app.farShadowStartMeters*gameplay::iw::kMetersToUnits,app.farShadowDistanceMeters*gameplay::iw::kMetersToUnits,app.farShadowBlendMeters*gameplay::iw::kMetersToUnits);
                    std::vector<std::uint8_t> baseline;
                    for(int enabled=0;enabled<2;++enabled){
                        app.renderer.setShadowContributionElision(enabled!=0);for(int frame=0;frame<8;++frame)draw();glFinish();
                        const auto started=std::chrono::steady_clock::now();for(int frame=0;frame<40;++frame)draw();glFinish();
                        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/40;
                        CHECK(glGetError()==GL_NO_ERROR);std::vector<std::uint8_t> image;CHECK(app.renderer.readColorRgba(image,error));std::size_t changed{};int maximum{};
                        if(!enabled)baseline=image;else{CHECK(image.size()==baseline.size());for(size_t channel=0;channel<image.size();++channel){changed+=image[channel]!=baseline[channel];maximum=std::max(maximum,std::abs(int(image[channel])-int(baseline[channel])));}}
                        shadowLog<<view<<','<<slot<<','<<variant<<','<<enabled<<','<<elapsed<<','<<changed<<','<<maximum<<std::endl;
                        if(maximum>1){CHECK(app.renderer.saveColorPng(out/("shadow_failure_"+std::to_string(view)+"_"+std::to_string(slot)+"_"+std::to_string(variant)+".png"),error));}CHECK(maximum<=1);
                    }
                }
                app.renderer.setShadowContributionElision(false);app.renderer.setShadowStyle(app.shadowIntensity,app.shadowBias,app.shadowNormalBias,app.shadowSoftness,app.shadowContrast,app.shadowTint);app.renderer.setViewmodelShadows(app.viewmodelSelfShadows,app.viewmodelShadowResolution);
            }
            std::cout<<"view "<<view<<" slot "<<slot<<" complete\n"<<std::flush;
        }
    }
    owner.reset();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 10;}
