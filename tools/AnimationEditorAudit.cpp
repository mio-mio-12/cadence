#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#include "imgui_internal.h"

int main(int argc,char** argv){
    const std::filesystem::path output=argc>1?std::filesystem::u8path(argv[1]):"diagnostics/animation-editor";
    std::filesystem::create_directories(output);
    if(!glfwInit())return 2;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    auto* window=glfwCreateWindow(1280,800,"Animation editor audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().LogFilename=nullptr;ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window,false);ImGui_ImplOpenGL3_Init("#version 330");
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;gAppState=&app;app.workspaceMode=3;
    app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
    std::string error;int failed{};std::ofstream report(output/"audit.txt");
    const auto check=[&](bool value,const char* label){report<<label<<"="<<value<<'\n';if(!value)++failed;};
    for(const auto* game:{"bo2","cso2"})check(assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error),game);
    auto& editor=app.animationEditor;editor.initialized=true;editor.speed=0;editor.status="Animation preview";
    app.actorPosition={11,22,33};app.gameplayClock=123;app.takeTime=.75f;app.takePreview=true;app.takePlaying=true;
    const auto liveSet=app.animationSet;
    const auto dependencies=cadence::content::constructionDocuments();
    const auto bo2Reference=findGenericPlayermodelForGame(app,"bo2");
    if(bo2Reference<app.assetCatalog.entries.size())editor.draft.models["bo2"]=app.assetCatalog.entries[bo2Reference].name;
    const auto draftReferences=editor.draft.models;
    const auto runtimeReferenceCount=app.gameReferenceSetups.size();
    std::string selectedBo2Death,selectedCso2Death;
    const auto frame=[&](){
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();drawUi(app);ImGui::Render();
        int w{},h{};glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.055f,.065f,.08f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glFinish();
    };
    const auto screenshot=[&](const std::string& name,int width,int height){
        glfwSetWindowSize(window,width,height);for(int i=0;i<3;++i)frame();
        int w{},h{};glfwGetFramebufferSize(window,&w,&h);std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w)*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());check(app.actionPreviewRenderer.savePixelsPng(output/(name+".png"),w,h,pixels,error),name.c_str());
    };
    for(const auto& [source,target]:std::vector<std::pair<std::string,std::string>>{{"bo2","bo2"},{"cso2","cso2"},{"bo2","cso2"},{"cso2","bo2"}}){
        editor.sourceGame=source;editor.modelGame=target;editor.matchSource=false;editor.model=findGenericPlayermodelForGame(app,target);editor.pendingLoad=true;
        frame();check(app.actionPreviewReady,(source+"_on_"+target+"_ready").c_str());
        for(const auto& warning:app.actionPreviewScene.warnings)report<<source<<"_on_"<<target<<"_warning="<<warning<<'\n';
        std::size_t count{},firstAvailable=SIZE_MAX;for(std::size_t i=0;i<app.actionPreviewScene.animations.size();++i){const auto& clip=app.actionPreviewScene.animations[i];if(clip.action==scene::ActionRole::Death&&animationui::available(clip)){++count;if(firstAvailable==SIZE_MAX||clip.sourceName<app.actionPreviewScene.animations[firstAvailable].sourceName)firstAvailable=i;}}
        check(count>0,(source+"_on_"+target+"_deaths").c_str());
        if(firstAvailable!=SIZE_MAX)animationui::select(app,firstAvailable);
        if(source==target&&firstAvailable!=SIZE_MAX){if(source=="bo2")selectedBo2Death=app.actionPreviewScene.animations[firstAvailable].sourceName;else if(source=="cso2")selectedCso2Death=app.actionPreviewScene.animations[firstAvailable].sourceName;}
        if(app.actionPreviewClip<app.actionPreviewScene.animations.size())app.actionPreviewFrame=app.actionPreviewScene.animations[app.actionPreviewClip].durationFrames*.5f;
        screenshot(source+"_on_"+target+"_wide",1280,800);screenshot(source+"_on_"+target+"_narrow",960,640);
        const auto deathClip=app.actionPreviewClip;
        if(deathClip<app.actionPreviewScene.animations.size()){
            const auto& chosen=app.actionPreviewScene.animations[deathClip];editor.draft.set(source,chosen.action,chosen.sourceName,true);
            const auto layout=scene::imported::bodyOrCodLayout(app.actionPreviewScene.skeleton);
            for(const auto fraction:{0.f,.5f,1.f}){
                app.actionPreviewFrame=app.actionPreviewScene.animations[deathClip].durationFrames*fraction;animationui::framePose(app);
                screenshot(source+"_on_"+target+"_death_"+std::to_string(int(fraction*100)),1280,800);
                const auto pose=animationui::previewPose(app);report<<source<<"_on_"<<target<<" fraction="<<fraction;
                for(const auto bone:{layout.pelvis,layout.head,layout.foot[0],layout.foot[1]})if(bone>=0&&std::size_t(bone)<pose.size())report<<" bone"<<bone<<"="<<pose[bone].v[12]<<","<<pose[bone].v[13]<<","<<pose[bone].v[14];report<<'\n';
            }
            for(std::size_t i=0;i<app.actionPreviewScene.animations.size();++i){const auto& clip=app.actionPreviewScene.animations[i];if(clip.action==scene::ActionRole::None&&clip.motion==scene::MotionRole::Idle&&clip.domain==scene::AnimationDomain::PlayerBody&&!clip.tracks.empty()){editor.action=static_cast<int>(clip.action);app.actionPreviewClip=i;app.actionPreviewFrame=0;animationui::framePose(app);screenshot(source+"_on_"+target+"_idle",1280,800);break;}}
            editor.action=static_cast<int>(scene::ActionRole::Death);app.actionPreviewClip=deathClip;app.actionPreviewFrame=0;editor.pan={};editor.zoom=1;
        }
        for(std::size_t i=0;i<app.actionPreviewScene.animations.size();++i){const auto& clip=app.actionPreviewScene.animations[i];if(clip.action!=scene::ActionRole::Death||animationui::available(clip))continue;
            editor.draft.set(source,clip.action,clip.sourceName,true);std::snprintf(editor.search.data(),editor.search.size(),"%s",cadence::AnimationSet::file(clip.sourceName).c_str());animationui::select(app,i);frame();
            check(!app.actionPreviewPlaying&&app.actionPreviewFrame==0,"unavailable_stops_previous_motion");
            const auto pose=animationui::previewPose(app);bool bind=true;const float lift=-app.actionPreviewScene.bounds.minimum.z+gameplay::iw::worldUnits(.25f);for(std::size_t b=0;b<pose.size();++b){auto expected=app.actionPreviewScene.skeleton.bones[b].restGlobal;expected.v[14]+=lift;for(int k=0;k<16;++k)if(std::abs(pose[b].v[k]-expected.v[k])>1e-4f)bind=false;}check(bind,"unavailable_shows_bind_pose");
            screenshot(source+"_on_"+target+"_unavailable",1280,800);editor.search.fill(0);app.actionPreviewClip=deathClip;break;
        }
        const auto before=app.actionPreviewClip;const auto* documents=app.botAnimationCache[source].data();
        for(auto* candidate:ImGui::GetCurrentContext()->Windows)if(std::string(candidate->Name).find("animation_files")!=std::string::npos)ImGui::FocusWindow(candidate);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_DownArrow,true);frame();ImGui::GetIO().AddKeyEvent(ImGuiKey_DownArrow,false);frame();
        check(count<=1||app.actionPreviewClip!=before,(source+"_arrow_preview").c_str());
        check(app.botAnimationCache[source].data()==documents,(source+"_cached_arrow").c_str());
        for(const auto& m:app.actionPreviewScene.samplePose(app.actionPreviewClip,app.actionPreviewFrame))for(const auto f:m.v)if(!std::isfinite(f))++failed;
    }
    // Drive actual widgets without touching the user's preset/settings files.
    editor.saved=editor.draft;editor.dirty=false;app.actionPreviewPlaying=false;app.actionPreviewFrame=3;
    frame();
    ImGuiWindow* files{};for(auto* candidate:ImGui::GetCurrentContext()->Windows)if(std::string(candidate->Name).find("animation_files")!=std::string::npos)files=candidate;
    if(files){
        const auto oldClip=app.actionPreviewClip;const float oldFrame=app.actionPreviewFrame;
        const ImVec2 point{files->Pos.x+files->WindowPadding.x+8,files->Pos.y+files->WindowPadding.y+8};ImGui::GetIO().AddMousePosEvent(point.x,point.y);ImGui::GetIO().AddMouseButtonEvent(0,true);frame();ImGui::GetIO().AddMouseButtonEvent(0,false);frame();
        check(editor.dirty,"checkbox_marks_draft_dirty");check(app.actionPreviewClip==oldClip&&app.actionPreviewFrame==oldFrame,"checkbox_preserves_preview");check(app.animationSet==liveSet,"checkbox_preserves_runtime_set");
    }else check(false,"clip_list_exists");
    const auto beforeCancel=editor.draft;animationui::request(app,2);frame();frame();
    if(auto* popup=ImGui::FindWindowByName("Unsaved animation set")){
        const auto& style=ImGui::GetStyle();const float saveWidth=ImGui::CalcTextSize("Save").x+style.FramePadding.x*2,discardWidth=ImGui::CalcTextSize("Discard").x+style.FramePadding.x*2,cancelWidth=ImGui::CalcTextSize("Cancel").x+style.FramePadding.x*2;
        const ImVec2 point{popup->DC.CursorStartPos.x+saveWidth+discardWidth+style.ItemSpacing.x*2+cancelWidth*.5f,popup->DC.CursorStartPos.y+ImGui::GetTextLineHeightWithSpacing()+ImGui::GetFrameHeight()*.5f};
        ImGui::GetIO().AddMousePosEvent(point.x,point.y);ImGui::GetIO().AddMouseButtonEvent(0,true);frame();ImGui::GetIO().AddMouseButtonEvent(0,false);frame();
        check(editor.pendingOperation==0&&editor.draft==beforeCancel&&editor.dirty,"new_cancel_preserves_draft");
    }else check(false,"unsaved_guard_popup");
    check(app.actorPosition.x==11&&app.actorPosition.y==22&&app.actorPosition.z==33,"actor_unchanged");
    check(app.takePreview&&app.takePlaying&&app.takeTime==.75f,"replay_unchanged");check(app.gameplayClock==123,"gameplay_clock_unchanged");check(app.animationSet==liveSet,"runtime_set_unchanged");
    check(cadence::content::constructionDocuments()==dependencies,"demo_dependencies_unchanged");
    check(app.gameReferenceSetups.size()==runtimeReferenceCount,"runtime_references_restored");
    check(editor.draft.models==draftReferences,"browsing_preserves_source_references");
    cadence::AnimationSet roundTrip;check(editor.draft.save(output/"source-reference.cfg",&error)&&roundTrip.load(output/"source-reference.cfg",&error)&&roundTrip==editor.draft&&roundTrip.models==draftReferences,"mixed_game_source_reference_roundtrip");
    {
        cadence::content::ObservationPause auditAssets;
        if(bo2Reference>=app.assetCatalog.entries.size()){check(false,"bot_application_model_available");}
        else {
        auto isolated=std::make_unique<AppState>();isolated->assetCatalog=app.assetCatalog;isolated->defaultSalukiDirectory=app.defaultSalukiDirectory;isolated->botAnimationCache=app.botAnimationCache;
        auto actor=scene::buildScene(cast::Document::load(app.assetCatalog.entries[bo2Reference].path),false);
        PointBlankPreparationBatch pb;ImportedBodyPreparationBatch imported;
        for(const auto& doc:isolated->botAnimationCache["bo2"]){const auto before=actor.animations.size();if(!appendPointBlankWorldAnimation(*isolated,doc,actor,"bo2","bo2"))scene::appendAnimations(doc,actor);for(std::size_t i=before;i<actor.animations.size();++i)actor.animations[i].sourceGame="bo2";}
        std::vector<std::string> oldNames;for(const auto& clip:actor.animations)oldNames.push_back(clip.sourceName);
        isolated->animationSetForBots=true;isolated->animationSet.curatedDeaths=true;isolated->animationSet.models=draftReferences;
        isolated->animationSet.set("bo2",scene::ActionRole::Death,selectedBo2Death,true);isolated->animationSet.set("cso2",scene::ActionRole::Death,selectedCso2Death,true);
        applyBotAnimationSet(*isolated,actor,"bo2");
        check(isolated->animationSetMissing.empty(),"bot_application_no_missing_clips");
        std::set<std::pair<std::string,std::string>> enabled;
        for(std::size_t i=0;i<actor.animations.size();++i){const auto& clip=actor.animations[i];if(clip.action!=scene::ActionRole::Death||clip.tracks.empty())continue;enabled.insert({clip.sourceGame,cadence::AnimationSet::file(clip.sourceName)});for(const auto& m:actor.samplePose(i,clip.durationFrames*.5f))for(float f:m.v)if(!std::isfinite(f))++failed;}
        const std::set<std::pair<std::string,std::string>> expected={{"bo2",cadence::AnimationSet::file(selectedBo2Death)},{"cso2",cadence::AnimationSet::file(selectedCso2Death)}};
        check(!selectedBo2Death.empty()&&!selectedCso2Death.empty()&&enabled==expected,"bot_application_exact_mixed_game_deaths");
        bool indices=actor.animations.size()>=oldNames.size();for(std::size_t i=0;i<oldNames.size();++i)indices=indices&&actor.animations[i].sourceName==oldNames[i];check(indices,"bot_application_preserves_clip_indices");
        const auto choices=cadence::prepareBotDeaths(actor);bool eligible=true;for(const auto& choice:choices){const auto& clip=actor.animations[choice.animation];eligible=eligible&&!clip.tracks.empty()&&expected.contains({clip.sourceGame,cadence::AnimationSet::file(clip.sourceName)});}check(eligible&&choices.size()==2,"bot_application_disabled_clips_ineligible");
        bool adapters=true;for(const auto& [index,adapter]:actor.runtimePoseAdapters)adapters=adapters&&index<actor.animations.size()&&!actor.animations[index].tracks.empty();
        std::size_t appliedIndex=SIZE_MAX,previewIndex=SIZE_MAX;
        for(std::size_t i=0;i<actor.animations.size();++i)if(actor.animations[i].sourceGame=="cso2"&&cadence::AnimationSet::file(actor.animations[i].sourceName)==cadence::AnimationSet::file(selectedCso2Death))appliedIndex=i;
        for(std::size_t i=0;i<app.actionPreviewScene.animations.size();++i)if(app.actionPreviewScene.animations[i].sourceGame=="cso2"&&cadence::AnimationSet::file(app.actionPreviewScene.animations[i].sourceName)==cadence::AnimationSet::file(selectedCso2Death))previewIndex=i;
        // ImportedBodyRetarget attaches its independently sampled source through
        // Animation::coldWarWorldPose; runtimePoseAdapters is a different path.
        const bool crossAdapter=appliedIndex<actor.animations.size()&&bool(actor.animations[appliedIndex].coldWarWorldPose);
        check(adapters&&crossAdapter,"bot_application_preserves_cross_pose_adapter");
        bool equivalent=crossAdapter&&previewIndex<app.actionPreviewScene.animations.size();float maximumDifference{},motion{};std::vector<scene::Mat4> initialPose;
        if(equivalent)for(const auto fraction:{0.f,.25f,.5f,1.f}){
            const auto actual=actor.samplePose(appliedIndex,actor.animations[appliedIndex].durationFrames*fraction),reference=app.actionPreviewScene.samplePose(previewIndex,app.actionPreviewScene.animations[previewIndex].durationFrames*fraction);
            equivalent=equivalent&&actual.size()==reference.size();if(initialPose.empty())initialPose=actual;
            for(std::size_t b=0;b<std::min(actual.size(),reference.size());++b)for(int k=0;k<16;++k){maximumDifference=std::max(maximumDifference,std::abs(actual[b].v[k]-reference[b].v[k]));if(b<initialPose.size())motion=std::max(motion,std::abs(actual[b].v[k]-initialPose[b].v[k]));}
        }
        check(equivalent&&maximumDifference<=1e-4f&&motion>.01f,"bot_application_matches_prepared_cross_pose");report<<"bot_application_max_pose_difference="<<maximumDifference<<" motion="<<motion<<'\n';
        }
    }
    check(app.animationSet==liveSet&&cadence::content::constructionDocuments()==dependencies,"isolated_bot_application_preserves_live_state");
    {
        cadence::content::ObservationPause auditAssets;
        auto referenceState=std::make_unique<AppState>();auto& referenceApp=*referenceState;
        referenceApp.defaultSalukiDirectory=app.defaultSalukiDirectory;
        for(const auto* game:{"bo2","pointblank"})check(assets::appendScan(referenceApp.defaultSalukiDirectory/game,game,referenceApp.assetCatalog,error),"reference_fixture_catalog");
        for(const std::string source:{"bo2","pointblank"}){
            const std::string target=source=="bo2"?"pointblank":"bo2";
            const auto targetIndex=findGenericPlayermodelForGame(referenceApp,target);
            std::vector<std::size_t> donors;std::string firstBind;
            for(std::size_t i=0;i<referenceApp.assetCatalog.entries.size();++i){const auto& asset=referenceApp.assetCatalog.entries[i];if(asset.game!=source||asset.role!=assets::Role::PlayerModel)continue;
                auto document=cast::Document::load(asset.path);if(!document.valid())continue;auto candidate=scene::buildScene(document,false);
                if(candidate.skeleton.bones.empty()||(source=="pointblank"?!scene::pointblank::body(candidate.skeleton):!scene::imported::completeBodyLayout(scene::imported::bodyOrCodLayout(candidate.skeleton))))continue;const auto bind=cadence::prepared_animation::skeletonKey("",candidate.skeleton);
                if(donors.empty()){donors.push_back(i);firstBind=bind;}else if(bind!=firstBind){donors.push_back(i);break;}
            }
            check(donors.size()==2&&targetIndex<referenceApp.assetCatalog.entries.size(),(source+"_reference_distinct_binds").c_str());if(donors.size()!=2||targetIndex>=referenceApp.assetCatalog.entries.size())continue;
            ensureBotAnimationCache(referenceApp,source);std::string selectedFile;
            for(const auto donor:{donors[0],donors[1],donors[0]}){
                const auto donorName=referenceApp.assetCatalog.entries[donor].name;
                auto& state=referenceApp.animationEditor;state.sourceGame=source;state.modelGame=target;state.model=targetIndex;state.matchSource=false;
                const auto previousReference=state.draft.models;
                cadence::AnimationSet preset; preset.models[source]=donorName;
                const auto presetName="reference-"+source+"-"+std::to_string(donor);
                check(preset.save(output/(presetName+".cfg"),&error),"reference_preset_saved_in_audit_folder");
                state.pendingLoad=false;state.pendingOperation=1;state.pendingPreset=presetName;
                animationui::performPending(referenceApp,output);
                check(previousReference==state.draft.models||state.pendingLoad,(source+"_preset_reference_change_invalidates").c_str());
                if(previousReference!=state.draft.models)check(!referenceApp.actionPreviewReady&&!referenceApp.actionPreviewPlaying,(source+"_preset_reference_hides_stale_preview").c_str());
                if(state.pendingLoad)animationui::load(referenceApp);
                check(referenceApp.actionPreviewReady,(source+"_reference_preview_ready").c_str());
                auto& preview=referenceApp.actionPreviewScene;std::size_t selected=SIZE_MAX;
                for(std::size_t i=0;i<preview.animations.size();++i){const auto& clip=preview.animations[i];if(!animationui::available(clip)||clip.action!=scene::ActionRole::Death)continue;if(selectedFile.empty()||cadence::AnimationSet::file(clip.sourceName)==selectedFile){selected=i;if(selectedFile.empty())selectedFile=cadence::AnimationSet::file(clip.sourceName);break;}}
                check(selected<preview.animations.size(),(source+"_reference_death_available").c_str());if(selected>=preview.animations.size())continue;
                const auto savedReferences=referenceApp.gameReferenceSetups;referenceApp.gameReferenceSetups[source].playermodelAsset=donor;
                const auto expectedDonor=playerSkeletonForGame(referenceApp,source);referenceApp.gameReferenceSetups=savedReferences;
                const auto& adapter=preview.animations[selected].coldWarWorldPose;
                check(expectedDonor&&adapter&&adapter->source&&cadence::prepared_animation::skeletonKey("",*expectedDonor)==cadence::prepared_animation::skeletonKey("",adapter->source->skeleton),(source+"_reference_selected_bind_used").c_str());
                const auto& targetAsset=referenceApp.assetCatalog.entries[targetIndex];auto actor=scene::buildScene(cast::Document::load(targetAsset.path),false);
                for(const auto part:characterAssemblyParts(referenceApp,targetAsset)){const auto& a=referenceApp.assetCatalog.entries[part];const auto doc=cast::Document::load(a.path);if(doc.valid())scene::appendRigModel(doc,actor,a.name);}
                referenceApp.animationSet={};referenceApp.animationSetForBots=true;referenceApp.animationSet.models[source]=donorName;referenceApp.animationSet.set(source,scene::ActionRole::Death,selectedFile,true);
                applyBotAnimationSet(referenceApp,actor,target);std::size_t applied=SIZE_MAX;
                for(std::size_t i=0;i<actor.animations.size();++i)if(actor.animations[i].sourceGame==source&&cadence::AnimationSet::file(actor.animations[i].sourceName)==selectedFile)applied=i;
                bool same=applied<actor.animations.size();float delta{};
                if(same)for(float fraction:{0.f,.5f,1.f}){const auto actual=actor.samplePose(applied,actor.animations[applied].durationFrames*fraction),expected=preview.samplePose(selected,preview.animations[selected].durationFrames*fraction);same=actual.size()==expected.size();for(std::size_t b=0;b<std::min(actual.size(),expected.size());++b)for(int k=0;k<16;++k)delta=std::max(delta,std::abs(actual[b].v[k]-expected[b].v[k]));}
                check(same&&delta<=1e-4f&&referenceApp.animationSetMissing.empty(),(source+"_reference_preview_applied_parity").c_str());
                report<<"explicit_reference "<<source<<" donor="<<donorName<<" target="<<targetAsset.name<<" clip="<<selectedFile<<" pose_delta="<<delta<<'\n';
            }
            auto& state=referenceApp.animationEditor;
            const auto priorDraft=state.draft;const auto priorClip=referenceApp.actionPreviewClip;const auto priorReady=referenceApp.actionPreviewReady;const auto priorPlaying=referenceApp.actionPreviewPlaying;
            state.pendingLoad=false;state.pendingOperation=1;state.pendingPreset="definitely-absent-audit-preset";animationui::performPending(referenceApp,output);
            check(state.draft==priorDraft&&!state.pendingLoad&&referenceApp.actionPreviewClip==priorClip&&referenceApp.actionPreviewReady==priorReady&&referenceApp.actionPreviewPlaying==priorPlaying,(source+"_failed_preset_preserves_preview").c_str());
            auto ruleOnly=state.draft;ruleOnly.set(source,scene::ActionRole::Death,"audit_rule_only.cast",false);const auto rulePreset="rules-"+source;
            check(ruleOnly.save(output/(rulePreset+".cfg"),&error),"rule_only_preset_saved_in_audit_folder");state.pendingOperation=1;state.pendingPreset=rulePreset;animationui::performPending(referenceApp,output);
            check(!state.pendingLoad&&referenceApp.actionPreviewReady==priorReady,(source+"_rule_only_preset_keeps_preview").c_str());
            state.saved=state.draft;state.pendingLoad=false;state.draft.set(source,scene::ActionRole::Death,"audit_rule_only.cast",false);state.pendingOperation=3;
            animationui::performPending(referenceApp,output);check(!state.pendingLoad,(source+"_rule_only_discard_keeps_preview").c_str());
            state.draft.models["unused_audit_game"]="other_donor";state.pendingOperation=3;animationui::performPending(referenceApp,output);check(!state.pendingLoad,(source+"_unrelated_reference_keeps_preview").c_str());
            state.draft.models[source]=referenceApp.assetCatalog.entries[donors[1]].name;state.pendingOperation=3;
            animationui::performPending(referenceApp,output);check(state.pendingLoad,(source+"_reference_discard_invalidates").c_str());
            state.pendingLoad=false;state.pendingOperation=2;animationui::performPending(referenceApp,output);
            check(state.pendingLoad&&state.draft.models.empty(),(source+"_new_set_reference_removal_invalidates").c_str());
        }
        referenceApp.actionPreviewRenderer.shutdown();
    }
    report<<"failed="<<failed<<std::endl;std::cout<<"Animation editor audit failures: "<<failed<<std::endl;
    app.actionPreviewRenderer.shutdown();gAppState=nullptr;state.reset();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return failed?1:0;
}
