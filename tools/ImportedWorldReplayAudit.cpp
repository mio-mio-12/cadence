#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>

int main(int argc,char** argv){try{
    const auto require=[](bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);};
    require(glfwInit()!=0,"GLFW initialization failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(960,640,"World attachment replay audit",nullptr,nullptr);require(window,"Window failed");
    glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;app.deferSceneUpload=true;
    std::string error;require(app.renderer.initialize(error),error);
    app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
    for(const auto game:{"bo2","cs1.6","css","csnz","cso2","eldewrito"})require(assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error),error);
    const auto pick=[&](std::string_view game,assets::Role role,std::string_view wanted){
        for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game==game&&a.role==role&&a.name==wanted)return i;}
        return SIZE_MAX;
    };
    const auto out=argc>1?std::filesystem::path(argv[1]):std::filesystem::path("diagnostics/world-replay");std::filesystem::create_directories(out);
    std::ofstream report(out/"results.txt");int failures{};
    struct Case{const char* game;const char* name;};
    const bool eldControls=std::getenv("CADENCE_AUDIT_ELD_WORLD_CONTROLS")!=nullptr;
    const bool explicitGripControl=std::getenv("CADENCE_AUDIT_EXPLICIT_WORLD_GRIP")!=nullptr;
    std::vector<Case> cases={{"cs1.6","viewmodel_rifle_v_ak47"},{"css","viewmodel_rifle_v_rif_ak47"},{"csnz","viewmodel_v_luger"},{"cso2","viewmodel_rifle_v_ak47"},{"eldewrito","assault_rifle_3467_fp_0_20976_weapon"}};
    if(eldControls)cases={{"eldewrito","assault_rifle_3467_fp_0_20976_weapon"},{"eldewrito","battle_rifle_7748_fp_0_20976_weapon"},{"eldewrito","covenant_carbine_1622_fp_0_20976_weapon"},{"eldewrito","magnum_7952_fp_0_20976_weapon"}};
    for(const auto& fixture:cases)for(const auto bodyGame:{"bo2","cso2"})for(bool forcePlaceholder:{false,true}){
        const std::string label=std::string(eldControls?fixture.name:fixture.game)+"_on_"+bodyGame+(forcePlaceholder?"_placeholder":"_selected");
        try{
            const auto viewIndex=pick(fixture.game,assets::Role::ViewWeapon,fixture.name);
            const auto bodyIndex=pick(bodyGame,assets::Role::PlayerModel,std::string_view(bodyGame)=="bo2"?"c_chn_mp_pla_assault_fb_LOD0":"ct_sas");
            require(viewIndex!=SIZE_MAX&&bodyIndex!=SIZE_MAX,"Exact fixture asset not in catalog");
            const auto& view=app.assetCatalog.entries[viewIndex];const auto& body=app.assetCatalog.entries[bodyIndex];
            auto live=scene::buildScene(cast::Document::load(body.path));
            const auto selected=forcePlaceholder?viewIndex:findWorldWeaponForViewWeapon(app,view);require(selected<app.assetCatalog.entries.size(),"No selected world/fallback asset");
            const auto& selectedAsset=app.assetCatalog.entries[selected];
            require(attachImportedWorldWeapon(app,cast::Document::load(selectedAsset.path),viewIndex,live,bodyGame),"Imported world route not handled");
            for(const auto& warning:live.warnings)report<<"  warning "<<warning<<'\n';
            require(live.attachments.size()==1,"Expected one prepared attachment");
            auto& attachment=live.attachments.front();
            const auto expectedPath=attachment.name==view.name?view.path:selectedAsset.path;
            const bool authored=expectedPath!=view.path;
            if(eldControls){
                // Diagnostic only: preserve application geometry and pose, change
                // only the source frame used by its already computed mount.
                const auto world=scene::buildScene(cast::Document::load(expectedPath),false);
                const auto root=scene::imported::world_grip::explicitGunAnchor(world.skeleton);
                const auto marker=scene::imported::world_grip::bone(world.skeleton,{"dew2cast_socket__right_hand__0"});
                const auto original=attachment.localMatrix();float delta{};
                const bool eligible=authored&&fixture.game==std::string_view("eldewrito")&&root>=0&&marker>=0&&world.importedTranslationScale==304.8f;
                if(eligible){
                    const auto rootFrame=scene::imported::world_grip::rigid(world.skeleton.bones[root].restGlobal);
                    const auto gripFrame=scene::imported::world_grip::rigid(world.skeleton.bones[marker].restGlobal);
                    require(rootFrame&&gripFrame,"Invalid explicit weapon grip frame");
                    const auto offset=scene::transformPoint(scene::inverseAffine(*rootFrame)*(*gripFrame),{});
                    report<<"  explicit_world_grip root_relative_cm="<<offset.x<<','<<offset.y<<','<<offset.z<<'\n';
                    // v258 production already selects a valid explicit marker.
                    // Retain the old opt-in for older builds, never apply twice.
                    const auto selectedFrame=scene::imported::world_grip::nativeSocket("eldewrito",world,world.skeleton).weapon;
                    if(explicitGripControl&&selectedFrame!=marker)cadence::world_weapon::setTransform(attachment,original*(*rootFrame)*scene::inverseAffine(*gripFrame));
                }
                const auto corrected=attachment.localMatrix();for(int k=0;k<16;++k)delta=std::max(delta,std::abs(corrected.v[k]-original.v[k]));
                require(eligible&&explicitGripControl||delta==0,"No-marker/control attachment changed");
                report<<"  diagnostic_explicit_grip="<<(eligible&&explicitGripControl)<<" mount_delta="<<delta<<'\n';
            }
            // Use an actual held pose for both families, then record its final
            // matrices. The restored actor must not resample/reinterpret it.
            const bool pistol=weapon::inferArchetype(view.name)==weapon::Archetype::Pistol;
            const auto hold=app.defaultSalukiDirectory/(pistol?"bo2/animations/pb/stand/pistol/pb_stand_alert_pistol.cast":"bo2/animations/pb/stand/generic/pb_stand_alert.cast");
            if(std::string_view(bodyGame)=="bo2")scene::appendAnimations(cast::Document::load(hold),live);
            else{
                require(appendPointBlankWorldAnimation(app,cast::Document::load(hold),live,"bo2",bodyGame),"Production held-pose route not handled");
            }
            require(!live.animations.empty()&&!live.animations.back().tracks.empty(),"Held-pose clip did not bind");
            const auto pose=live.animations.empty()?scene::imported::world_grip::restPose(live.skeleton):live.samplePose(live.animations.size()-1,0);
            float heldPoseDelta{};for(std::size_t i=0;i<pose.size();++i)for(int k=0;k<16;++k)heldPoseDelta=std::max(heldPoseDelta,std::abs(pose[i].v[k]-live.skeleton.bones[i].restGlobal.v[k]));require(heldPoseDelta>.01f,"Held pose remained bind pose");
            report<<"  bound_hold="<<live.animations.back().sourceName<<" bind_pose_delta="<<heldPoseDelta<<'\n';
            take::ActorManifest manifest;manifest.baseModel=takePathString(body.path);captureCharacterAttachments(app,live,manifest);
            require(manifest.attachedModels.size()==1,"Captured attachment missing");
            const bool pathMatches=manifest.attachedModels.front().path==takePathString(expectedPath);
            report<<label<<" selected="<<selectedAsset.path.string()<<" actual="<<(authored?"authored":"placeholder")<<" expected="<<expectedPath.string()<<" captured="<<manifest.attachedModels.front().path<<" path_match="<<pathMatches<<'\n';
            if(!pathMatches)++failures;
            take::Take recording;recording.actor=manifest;recording.worldActor=manifest;recording.boneCount=recording.worldBoneCount=pose.size();recording.samples.resize(1);recording.samples[0].pose=pose;recording.samples[0].worldActor.pose=pose;
            require(take::save(recording,out/(label+".c_dm"),error),error);take::Take loaded;require(take::load(out/(label+".c_dm"),loaded,error),error);
            require(restoreTakeWorldActor(app,loaded.worldActor,loaded.worldBoneCount,error),error);const auto& restored=*app.hiddenWorldActor;
            require(live.meshes.size()==restored.meshes.size()&&live.skeleton.bones.size()==restored.skeleton.bones.size()&&live.attachments.size()==restored.attachments.size(),"Rebuilt geometry counts differ");
            float vertexError{},bindError{},mountError{},normalError{};std::size_t vertices{};
            for(std::size_t b=0;b<live.skeleton.bones.size();++b){require(live.skeleton.bones[b].name==restored.skeleton.bones[b].name,"Bone order changed");for(int k=0;k<16;++k)bindError=std::max(bindError,std::abs(live.skeleton.bones[b].inverseBind.v[k]-restored.skeleton.bones[b].inverseBind.v[k]));}
            for(std::size_t m=0;m<live.meshes.size();++m){const auto& a=live.meshes[m];const auto& b=restored.meshes[m];require(a.vertices.size()==b.vertices.size()&&a.indices==b.indices,"Mesh topology changed");
                require(a.albedoPath==b.albedoPath&&a.normalPath==b.normalPath,"Material paths changed");
                for(int k=0;k<16;++k)mountError=std::max(mountError,std::abs(a.modelTransform.v[k]-b.modelTransform.v[k]));
                for(std::size_t i=0;i<a.vertices.size();++i){++vertices;vertexError=std::max(vertexError,scene::length(a.vertices[i].position-b.vertices[i].position));normalError=std::max(normalError,scene::length(a.vertices[i].normal-b.vertices[i].normal));require(a.vertices[i].bones==b.vertices[i].bones&&a.vertices[i].weights==b.vertices[i].weights&&a.vertices[i].uv.x==b.vertices[i].uv.x&&a.vertices[i].uv.y==b.vertices[i].uv.y,"Weights or UV changed");}
            }
            const auto ma=attachment.localMatrix(),mb=restored.attachments.front().localMatrix();for(int k=0;k<16;++k)mountError=std::max(mountError,std::abs(ma.v[k]-mb.v[k]));
            const auto muzzle=scene::resolveMuzzlePosition(live,pose),replayMuzzle=scene::resolveMuzzlePosition(restored,loaded.samples.front().worldActor.pose);
            require(muzzle&&replayMuzzle,"Prepared/replayed muzzle missing");const auto muzzleError=scene::length(*muzzle-*replayMuzzle);
            report<<"  vertices="<<vertices<<" vertex="<<vertexError<<" normal="<<normalError<<" bind="<<bindError<<" mount="<<mountError<<" muzzle="<<muzzleError<<'\n';
            require(std::max({vertexError,normalError,bindError,mountError,muzzleError})<.0001f,"Reconstruction numerical mismatch");
            const auto focus=scene::transformPoint(pose[attachment.boneIndex],{});const auto vp=scene::perspective(48*scene::kPi/180,960.f/640,.1f,3000)*scene::lookAt(focus+scene::Vec3{85,-135,50},focus,{0,0,1});
            const auto render=[&](const scene::CastScene& rig,const std::string& suffix){require(app.renderer.loadScene(rig,error),error);app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});app.renderer.render(rig,pose,vp,960,640,false,false,false);app.renderer.renderDebugLine3D(*muzzle-scene::Vec3{0,0,2},*muzzle+scene::Vec3{0,0,2},{0,1,0,1},vp);require(app.renderer.saveColorPng(out/(label+suffix+".png"),error),error);std::vector<std::uint8_t> pixels;require(app.renderer.readColorRgba(pixels,error),error);return pixels;};
            const auto before=render(live,"_live"),after=render(restored,"_replay");require(before.size()==960*640*4&&before.size()==after.size(),"Render dimensions differ");int maximum{};std::size_t visible{};for(std::size_t i=0;i<before.size();++i)maximum=std::max(maximum,std::abs(int(before[i])-int(after[i])));for(std::size_t i=4;i<before.size();i+=4)visible+=before[i]!=before[0]||before[i+1]!=before[1]||before[i+2]!=before[2];require(visible>5000,"Render contains insufficient model coverage");require(maximum<=1,"Replay render differs");require(glGetError()==GL_NO_ERROR,"GL error");
            report<<"  PASS pixel_max="<<maximum<<'\n';std::cout<<"PASS "<<label<<std::endl;
            if(eldControls){
                // Additional unobscured opposite-side view; not a substitute
                // for the exact live/replay pixel assertion above.
                const auto opposite=scene::perspective(48*scene::kPi/180,960.f/640,.1f,3000)*scene::lookAt(focus+scene::Vec3{100,210,65},focus,{0,0,1});
                require(app.renderer.loadScene(live,error),error);app.renderer.render(live,pose,opposite,960,640,false,false,false);
                require(app.renderer.saveColorPng(out/(label+"_opposite.png"),error),error);require(glGetError()==GL_NO_ERROR,"Opposite control GL error");
            }
            if(const auto* priorFolder=std::getenv("CADENCE_AUDIT_PRIOR_WORLD_TAKES")){
                take::Take prior;require(take::load(std::filesystem::path(priorFolder)/(label+".c_dm"),prior,error),error);
                require(!prior.samples.empty()&&prior.worldActor.attachedModels.size()==1,"Prior recording missing actor");
                require(restoreTakeWorldActor(app,prior.worldActor,prior.worldBoneCount,error),error);
                const auto& priorRig=*app.hiddenWorldActor;const auto& saved=prior.worldActor.attachedModels.front();const auto& mounted=priorRig.attachments.front();
                const auto exact=[](scene::Vec3 a,scene::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
                require(exact(mounted.position,saved.position)&&exact(mounted.rotationDegrees,saved.rotationDegrees)&&exact(mounted.scale,saved.scale),"Prior saved mount was reinterpreted");
                const auto& priorPose=prior.samples.front().worldActor.pose;const auto priorMuzzle=scene::resolveMuzzlePosition(priorRig,priorPose);require(priorMuzzle.has_value(),"Prior muzzle missing");
                require(app.renderer.loadScene(priorRig,error),error);app.renderer.render(priorRig,priorPose,vp,960,640,false,false,false);
                app.renderer.renderDebugLine3D(*priorMuzzle-scene::Vec3{0,0,2},*priorMuzzle+scene::Vec3{0,0,2},{0,1,0,1},vp);
                require(app.renderer.saveColorPng(out/(label+"_prior_replay.png"),error),error);require(glGetError()==GL_NO_ERROR,"Prior replay GL error");
                report<<"  prior_saved_mount_preserved=1\n";
            }
        }catch(const std::exception& e){++failures;report<<"FAIL "<<label<<": "<<e.what()<<'\n';std::cerr<<"FAIL "<<label<<": "<<e.what()<<std::endl;}
        report.flush();
    }
    app.renderer.shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();report<<"failures="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
