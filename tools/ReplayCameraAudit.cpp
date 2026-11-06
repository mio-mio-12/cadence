#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc,char** argv){
    if(argc<2||argc>4)return 2;
    auto owner=std::make_unique<AppState>();auto& app=*owner;app.deferSceneUpload=true;
    app.defaultSalukiDirectory="exported_files";
    take::Take input;std::string error;if(!take::load(argv[1],input,error)){std::cerr<<error;return 1;}
    if(argc>=3){
        if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto* window=glfwCreateWindow(960,720,"Take camera verification",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
        ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
        app.window=window;app.deferSceneUpload=false;if(!app.renderer.initialize(error)){std::cerr<<error;return 2;}
        if(argc==4){
            app.deferSceneUpload=true;
            if(!restoreTakeBotActor(app,input.botActor,input.botBoneCount,error)){std::cerr<<error;return 3;}
            auto actor=*app.botActorScene;
            std::erase_if(actor.meshes,[](const auto& m){return m.actorVariant>=0&&m.actorVariant!=4;});
            if(!app.renderer.loadScene(actor,error)){std::cerr<<error;return 3;}
            app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.13f,.14f,.16f,1});
            const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
            for(float t:{15.f,16.f,16.5f,16.7f}){
                const auto sample=input.interpolatedSample(t);
                for(const auto& b:sample.bots)if(b.id==5){
                    const auto wrist=actor.skeleton.boneByName.at("R Hand");const auto center=scene::transformPoint(b.pose[wrist],{});
                    for(int side=0;side<2;++side){const auto eye=center+(side?scene::Vec3{45,50,20}:scene::Vec3{45,-50,20});
                        app.renderer.setCameraPosition(eye);app.renderer.render(actor,b.pose,scene::perspective(40*scene::kPi/180,960.f/720,.1f,2000)*scene::lookAt(eye,center,{0,0,1}),960,720,false,false,false);
                        (void)app.renderer.saveColorPng(output/("bot5_"+std::to_string(t)+"_"+std::to_string(side)+".png"),error);
                    }
                    std::cout<<t<<" "<<b.primaryClip<<" / "<<b.torsoClip<<std::endl;
                }
            }
            app.renderer.shutdown();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
        }
        app.mapTextureResolution=256;openSetDemo(app,argv[1]);
        while(app.takePreparationFuture||app.pendingMapFuture||!app.demoLibrary.pendingDemo.empty()){
            processPendingTakeOpen(app);processPendingMapLoad(app);processDemoLibrary(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if(!app.takePreview||!app.experimentalThirdWeapon){std::cerr<<app.status<<" / "<<app.takeLoadError;return 3;}
        const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
        app.killer7Deaths=std::getenv("CADENCE_AUDIT_DEATH_FX")!=nullptr;
        for(float time:{0.f,3.5f,6.f,8.5f,16.5f}){
            app.takeTime=time;app.takeFirstPersonView=true;
            ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({960,720});
            ImGui::Begin("Replay audit",nullptr,ImGuiWindowFlags_NoDecoration);drawViewport(app,940,690);ImGui::End();ImGui::Render();glFinish();
            if(!app.renderer.saveColorPng(output/(std::to_string(time)+".png"),error)){std::cerr<<error;return 4;}
            std::cout<<"RENDER t="<<time<<" camera="<<app.lastRenderedCameraPosition.x<<","<<app.lastRenderedCameraPosition.y<<","<<app.lastRenderedCameraPosition.z<<std::endl;
        }
        if(app.killer7Deaths){
            float death=-1;bool alive=false;
            for(const auto& sample:app.recordedTake.samples){for(const auto& bot:sample.bots)if(bot.id==5){if(alive&&!bot.alive){death=sample.time;break;}alive=bot.alive;}if(death>=0)break;}
            if(death<0){std::cerr<<"No bot 5 death transition";return 5;}
            std::vector<std::uint8_t> first,again;
            for(float offset:{.2f,.8f,.2f}){
                app.takeTime=death+offset;
                ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({960,720});
                ImGui::Begin("Replay audit",nullptr,ImGuiWindowFlags_NoDecoration);drawViewport(app,940,690);ImGui::End();ImGui::Render();glFinish();
                if(!app.renderer.readColorRgba(again,error))return 6;
                if(offset==.2f){if(first.empty())first=again;else if(first!=again){std::cerr<<"Death seek mismatch";return 7;}}
                if(!app.renderer.saveColorPng(output/("death_"+std::to_string(offset)+".png"),error))return 8;
            }
            std::cout<<"DEATH_SEEK deterministic time="<<death<<std::endl;
        }
        app.renderer.shutdown();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
    }
    for(unsigned slot=0;slot<3;++slot){
        const auto& actor=input.actorSlots[slot].empty()?input.actor:input.actorSlots[slot];
        std::cout<<"SLOT "<<slot<<" base="<<actor.baseModel<<" camera="<<actor.viewmodelCamera<<" fov="<<actor.viewmodelFov<<" scale="<<actor.viewmodelFovScale<<std::endl;
        for(const auto& p:actor.rigModels)std::cout<<"rig="<<p<<std::endl;
        if(!restoreTakeActorInternal(app,actor,input.bonesForSlot(slot),false,error)){std::cerr<<error<<std::endl;continue;}
        const auto camera=app.scene.skeleton.boneByCanonicalName.find("tag_camera");
        if(camera==app.scene.skeleton.boneByCanonicalName.end())std::cout<<"ROOT CAMERA FALLBACK\n";
        else std::cout<<"CAMERA "<<camera->second<<" "<<app.scene.skeleton.bones[camera->second].name<<std::endl;
        std::string previous;
        for(const auto& s:input.samples)if(s.weaponSlot==slot&&s.primaryClip!=previous){
            previous=s.primaryClip;const auto recovered=cadence::replay::poseCamera(app.scene.skeleton,s.pose);if(!recovered){std::cout<<"NO CAMERA\n";continue;}const auto& m=*recovered;
            const scene::Vec3 f{m.v[0],m.v[1],m.v[2]},u{m.v[8],m.v[9],m.v[10]};
            std::cout<<"t="<<s.time<<" clip="<<s.primaryClip<<" cam="<<m.v[12]<<","<<m.v[13]<<","<<m.v[14]<<" forward="<<f.x<<","<<f.y<<","<<f.z<<" up="<<u.x<<","<<u.y<<","<<u.z<<" cross="<<scene::length(scene::cross(f,u))<<" fov="<<s.camera.fov<<" ads="<<s.camera.adsBlend<<std::endl;
        }
    }
}
