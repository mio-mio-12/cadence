#include <GLFW/glfw3.h>
#include <array>
static std::array<int,GLFW_KEY_LAST+1> auditKeys{};
static std::array<int,GLFW_MOUSE_BUTTON_LAST+1> auditMouse{};
static int auditGetKey(GLFWwindow*,int key){return key>=0&&key<=GLFW_KEY_LAST?auditKeys[key]:GLFW_RELEASE;}
static int auditGetMouseButton(GLFWwindow*,int key){return key>=0&&key<=GLFW_MOUSE_BUTTON_LAST?auditMouse[key]:GLFW_RELEASE;}
static int auditGetWindowAttrib(GLFWwindow* window,int attribute){return attribute==GLFW_FOCUSED?GLFW_TRUE:glfwGetWindowAttrib(window,attribute);}
#define glfwGetKey auditGetKey
#define glfwGetMouseButton auditGetMouseButton
#define glfwGetWindowAttrib auditGetWindowAttrib
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#undef glfwGetKey
#undef glfwGetMouseButton
#undef glfwGetWindowAttrib
int main(){
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,540,"Import audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& a=*state;a.window=window;a.deferSceneUpload=true;std::string error;if(!a.renderer.initialize(error))return 2;
    a.defaultSalukiDirectory="exported_files";int failures{};std::size_t tested{};
    const std::filesystem::path auditOutput=std::getenv("CADENCE_AUDIT_OUTPUT")?std::getenv("CADENCE_AUDIT_OUTPUT"):"diagnostics/v277/runtime-imports";
    std::filesystem::create_directories(auditOutput);
    for(const auto* game:{"eldewrito","cs1.6","cz","css","csnz","cso2","gitsfa","dirtybomb","blacksquad","iw_sp"}){
        if(std::string_view(game)=="iw_sp"&&!std::getenv("CADENCE_AUDIT_GAME"))continue;
        if(const auto* only=std::getenv("CADENCE_AUDIT_GAME");only&&std::string_view(only)!=game)continue;
        a.assetCatalog.clear();if(!assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error)){std::cerr<<error;return 2;}
        std::size_t hands=a.assetCatalog.entries.size();for(std::size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].role==assets::Role::ViewHands){hands=i;break;}
        if(std::getenv("CADENCE_AUDIT_BO2_HANDS")){
            if(!assets::appendScan(a.defaultSalukiDirectory/"bo2","bo2",a.assetCatalog,error))return 2;
            hands=a.assetCatalog.entries.size();for(std::size_t i=0;i<a.assetCatalog.entries.size();++i){const auto& h=a.assetCatalog.entries[i];if(h.game=="bo2"&&h.role==assets::Role::ViewHands&&h.name.starts_with("c_usa_mp_seal6_longsleeve_viewhands")){hands=i;break;}}
            if(hands==a.assetCatalog.entries.size()){std::cerr<<"Missing BO2 reference hands\n";return 2;}
        }
        if(const auto* handGame=std::getenv("CADENCE_AUDIT_HANDS_GAME")){
            if(std::string_view(handGame)!=game&&!assets::appendScan(a.defaultSalukiDirectory/handGame,handGame,a.assetCatalog,error))return 2;
            hands=a.assetCatalog.entries.size();
            const auto* pattern=std::getenv("CADENCE_AUDIT_HANDS");
            for(std::size_t i=0;i<a.assetCatalog.entries.size();++i){const auto& h=a.assetCatalog.entries[i];if(h.game==handGame&&h.role==assets::Role::ViewHands&&(!pattern||h.name.find(pattern)!=std::string::npos)){hands=i;break;}}
            if(hands==a.assetCatalog.entries.size()){std::cerr<<"Missing requested hands\n";return 2;}
        }
        std::size_t count{};
        for(std::size_t i=0;i<a.assetCatalog.entries.size();++i){const auto asset=a.assetCatalog.entries[i];if(asset.role!=assets::Role::ViewWeapon||asset.game!=game)continue;
            if(const auto* only=std::getenv("CADENCE_AUDIT_WEAPON");only&&asset.name.find(only)==std::string::npos)continue;
            // Class loading sets both selections before assembling. Several
            // gameplay alias policies use the selected weapon's game.
            ++tested;a.selectedBaseAsset=hands;a.selectedWeaponAsset=i;try{equipViewWeapon(a,i);
                if(std::getenv("CADENCE_AUDIT_NATIVE_PAIR")){
                    scene::CastScene direct;
                    if(!scene::imported::assemble(cast::Document::load(asset.path),cast::Document::load(a.assetCatalog.entries[hands].path),direct,error)){std::cerr<<error;return 2;}
                    direct.animations=a.scene.animations;
                    for(auto& clip:direct.animations)for(auto& track:clip.tracks){
                        const auto& name=a.scene.skeleton.bones.at(track.boneIndex).name;
                        const auto found=direct.skeleton.boneByName.find(name);
                        if(found==direct.skeleton.boneByName.end()){std::cerr<<"Native comparison missing bone "<<name;return 2;}
                        track.boneIndex=found->second;
                    }
                    a.scene=std::move(direct);
                    std::cout<<"NATIVE_PAIR direct exported hand rig; existing selected action clocks retained"<<std::endl;
                }
                const auto idle=findViewmodelClip(a,"idle");
                std::cout<<game<<','<<asset.name<<",bones="<<a.scene.skeleton.bones.size()<<",clips="<<a.scene.animations.size()<<",idle="<<(idle?a.scene.animations[*idle].sourceName:"MISSING")<<std::endl;
                if(a.scene.animations.empty()||!findViewmodelClip(a,"idle")){std::cerr<<"LOAD_FAILURE "<<asset.name<<": "<<a.status<<std::endl;for(const auto& warning:a.scene.warnings)std::cerr<<warning<<std::endl;++failures;continue;}
                std::cout<<"HANDS "<<a.assetCatalog.entries[a.selectedBaseAsset].name<<std::endl;
                if(std::string_view(game)=="iw_sp"){
                    bool nativeReference=false;
                    for(const auto& h:a.assetCatalog.entries)if(h.game==game&&h.role==assets::Role::ViewHands&&preferredIwReferenceHands(game,lowerText(h.name)))nativeReference=true;
                    if(!nativeReference)throw std::runtime_error("Missing detailed IW7 native reference fixture");
                    std::cout<<"REFERENCE detailed IW7 selected by production reference policy\n";
                }
                if(isKnifeWeapon(a)){
                    a.gameplayLogic=true;a.gameplayAction=scene::ActionRole::Melee;triggerGameplayAction(a);
                    const auto attack=findViewmodelClip(a,"melee");
                    std::cout<<"ATTACK active="<<a.actionActive<<" mapped="<<bool(attack)<<" variants="<<findViewmodelClips(a,"melee").size()<<" status="<<a.gameplayStatus<<std::endl;
                    if(!attack||!a.actionActive)++failures;
                    if(attack){const auto& clip=a.scene.animations[*attack];const auto p0=a.scene.samplePose(*attack,0),p1=a.scene.samplePose(*attack,clip.durationFrames*.4f);float delta{};for(std::size_t b=0;b<p0.size();++b)for(int k=0;k<16;++k)delta=std::max(delta,std::abs(p0[b].v[k]-p1[b].v[k]));std::cout<<"ATTACK_MOTION "<<clip.sourceName<<" delta="<<delta<<std::endl;if(delta<.001f)++failures;}
                    for(const auto variant:findViewmodelClips(a,"melee")){const auto& clip=a.scene.animations[variant];std::cout<<"ATTACK_VARIANT "<<clip.sourceName<<" frames="<<clip.durationFrames<<std::endl;if(!clip.durationFrames)++failures;}
                    stopGameplayAction(a);
                }
                const auto ai=*findViewmodelClip(a,"idle");
                if(!a.renderer.loadScene(a.scene,error)){std::cerr<<error<<std::endl;++failures;continue;}
                a.renderer.setDebugView(std::getenv("CADENCE_AUDIT_LIT")?0:1);a.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
                const auto configureCaptureLighting=[&](scene::Vec3 camera){
                    a.renderer.setCameraPosition(camera);
                    if(!std::getenv("CADENCE_AUDIT_LIT"))return;
                    const bool shadows=std::getenv("CADENCE_AUDIT_SHADOWS")!=nullptr;
                    // Live rendering focuses the small viewmodel shadow map
                    // at the rendered camera, not at the world origin.
                    a.renderer.setSun(true,shadows,{-.4f,.6f,-.7f},1,.2f,{1,1,1},{1,1,1},1024,2000,1500,camera,false,1024,2000,5000,500);
                    a.renderer.setViewmodelShadows(shadows,1024);
                };
                if(std::getenv("CADENCE_AUDIT_LIT"))std::cout<<"LIGHTING sun=1 self_shadows="<<(std::getenv("CADENCE_AUDIT_SHADOWS")!=nullptr)<<" focus=pose_camera\n";
                if(game==std::string_view("blacksquad")){std::size_t scalar{};for(const auto& m:a.scene.meshes)if(m.specularRedChannel&&!m.specularPath.empty())++scalar;std::cout<<"MATERIAL scalar_specular_meshes="<<scalar<<std::endl;}
                const auto pose=a.scene.samplePose(ai,0);const scene::Vec3 forward{1,0,0};
                if(count<2){scene::Vec3 low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f};
                    for(const auto& m:a.scene.meshes)for(const auto& v:m.vertices){scene::Vec3 p{};float weight{};for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){p=p+scene::transformPoint(pose[v.bones[k]]*a.scene.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];weight+=v.weights[k];}if(weight==0)p=v.position;
                        low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
                    std::cout<<"POSE_BOUNDS "<<low.x<<','<<low.y<<','<<low.z<<" -> "<<high.x<<','<<high.y<<','<<high.z<<std::endl;
                }
                for(const auto* action:{"idle","pullout","putaway","first_raise","fire","ads_fire","reload","rechamber","sprint_in","sprint_loop","sprint_out","inspect","melee"})if(const auto clip=findViewmodelClip(a,action)){
                    const auto& anim=a.scene.animations[*clip];std::cout<<"ACTION "<<action<<" "<<anim.sourceName<<" frames="<<anim.durationFrames<<" tracks="<<anim.tracks.size()<<" unmapped="<<anim.unmappedCurveCount<<std::endl;
                    std::vector<float> phases{0.f,.5f,1.f};
                    if(std::getenv("CADENCE_AUDIT_DENSE")){phases.clear();for(int frame=0;frame<=20;++frame)phases.push_back(frame/20.f);}
                    for(float fraction:phases){
                        const auto actionPose=a.scene.samplePose(*clip,anim.durationFrames*fraction);
                        scene::Vec3 origin{},direction{1,0,0},up{0,0,1};
                        if(const auto camera=cadence::replay::poseCamera(a.scene.skeleton,actionPose)){origin={camera->v[12],camera->v[13],camera->v[14]};direction={camera->v[0],camera->v[1],camera->v[2]};up={camera->v[8],camera->v[9],camera->v[10]};}
                        const auto vp=scene::perspective(75*scene::kPi/180,960.f/540.f,.05f,5000)*scene::lookAtDirection(origin,direction,up);
                        configureCaptureLighting(origin);
                        a.renderer.render(a.scene,actionPose,vp,960,540,false,false,false);
                        if(std::getenv("CADENCE_AUDIT_MUZZLE"))if(const auto muzzle=scene::resolveMuzzlePosition(a.scene,actionPose)){
                            a.renderer.renderDebugLine3D(*muzzle-scene::Vec3{0,1,0},*muzzle+scene::Vec3{0,1,0},{0,1,0,1},vp);
                            a.renderer.renderDebugLine3D(*muzzle-scene::Vec3{0,0,1},*muzzle+scene::Vec3{0,0,1},{0,1,0,1},vp);
                        }
                        (void)a.renderer.saveColorPng(auditOutput/(std::string(game)+"_"+asset.name+"_"+action+"_"+std::to_string(fraction)+".png"),error);
                    }
                }
                if(std::getenv("CADENCE_AUDIT_ACTIONS")){
                    a.gameplayLogic=true;a.playing=true;a.weaponSwitchStage=0;a.pendingWeaponRechamber=false;
                    if(findViewmodelClip(a,"fire")&&!isKnifeWeapon(a)){
                        a.weaponTiming.firePlaybackScale=1;a.gameplayAction=scene::ActionRole::Fire;triggerGameplayAction(a);
                        const float normal=a.actionDurationOverride;stopGameplayAction(a);
                        a.weaponTiming.firePlaybackScale=2;a.gameplayAction=scene::ActionRole::Fire;triggerGameplayAction(a);
                        const float fast=a.actionDurationOverride;stopGameplayAction(a);a.weaponTiming.firePlaybackScale=1;
                        std::cout<<"FIRE_SPEED normal="<<normal<<" double="<<fast<<std::endl;
                        if(normal<=0||std::abs(normal-2*fast)>.001f)++failures;
                    }
                    a.gameplayRechamber=false;a.gameplayAction=scene::ActionRole::Reload;triggerGameplayAction(a);
                    if(findViewmodelClip(a,"reload")){
                        const auto expected=a.actionDurationOverride;const bool started=a.actionActive;
                        a.viewmodelAdsEngaged=true;releaseViewmodelAim(a);
                        bool retained=a.actionActive&&a.activeAction==scene::ActionRole::Reload;
                        for(float t=0;t+.04f<expected;t+=.02f){a.gameplayClock+=.02f;updateGameplay(a,.02f);retained&=a.actionActive&&a.activeAction==scene::ActionRole::Reload;}
                        std::cout<<"RELOAD_CHECK started="<<started<<" retained="<<retained<<" duration="<<expected<<std::endl;
                        if(!started||!retained)++failures;stopGameplayAction(a);
                    }
                    if(const auto loop=findViewmodelClip(a,"sprint_loop")){const auto& c=a.scene.animations[*loop];const auto first=a.scene.samplePose(*loop,.25f*c.durationFrames),repeat=a.scene.samplePose(*loop,2.25f*c.durationFrames);float difference{};for(size_t j=0;j<first.size();++j)for(int k=0;k<16;++k)difference=std::max(difference,std::abs(first[j].v[k]-repeat[j].v[k]));std::cout<<"LOOP_CHECK looping="<<c.looping<<" difference="<<difference<<std::endl;if(!c.looping||difference>.002f)++failures;}
                    if(const auto loop=findViewmodelClip(a,"sprint_loop")){
                        stopGameplayAction(a);a.actorMode=true;a.actorNoclip=true;a.actorInputCaptured=true;a.movementAlgorithm=0;
                        a.previousSprint=false;a.gameplayAds=false;a.adsSuppressedUntilRelease=false;a.weaponSwitchStage=0;
                        auditKeys.fill(GLFW_RELEASE);auditMouse.fill(GLFW_RELEASE);
                        if(std::getenv("CADENCE_AUDIT_TRANSITIONS")){
                            // Noclip becomes airborne and intentionally selects
                            // idle instead of sprint. Use the unobstructed course
                            // floor for a genuine sustained ground sprint.
                            a.actorNoclip=false;a.actorPosition={-3000,-3000,scene::course::groundHeight(-3000,-3000)};
                            a.actorVelocity={};a.actorGrounded=true;a.actorGroundGrace=.1f;
                        }
                        a.gameplayAction=scene::ActionRole::Fire;triggerGameplayAction(a);
                        a.actionFrame=std::min(2.f,float(a.scene.animations[a.actionAnimationIndex].durationFrames));
                        // Opt-in sequential production-pose capture. Keep source
                        // assets unchanged; use the real controller/playback clocks.
                        const bool captureTransition=std::getenv("CADENCE_AUDIT_TRANSITIONS")!=nullptr;
                        std::ofstream joints;
                        if(captureTransition){joints.open(auditOutput/"transition_joints.csv");joints<<"frame,bone,rawX,rawY,rawZ,viewX,viewY,viewZ,adapterSource,adapterError\n";
                            std::cout<<"ADAPTER present="<<bool(a.scene.codmRigAdapter)<<"\n";}
                        const auto captureFrame=[&](int frame){
                            if(!captureTransition)return;
                            const auto current=evaluateCurrentPose(a);
                            const auto raw=a.transitioning?a.scene.sampleBlendedPose(a.previousAnimationIndex,a.previousAnimationFrame,a.animationIndex,a.animationFrame,a.transitionElapsed/std::max(.001f,a.activeTransitionDuration)):a.scene.samplePose(a.animationIndex,a.animationFrame);
                            const auto invCamera=[&]{if(const auto camera=cadence::replay::poseCamera(a.scene.skeleton,current))return scene::inverseAffine(*camera);return scene::Mat4::identity();}();
                            for(std::size_t b=0;b<current.size();++b){const auto& name=a.scene.skeleton.bones[b].name;
                                if(name.find("wrist_le")==std::string::npos&&name.find("elbow_le")==std::string::npos&&name!="j_gun"&&name!="tag_weapon")continue;
                                const auto view=invCamera*current[b];std::size_t source=b;float error=0;
                                if(const auto& adapter=a.scene.codmRigAdapter;adapter&&b>=adapter->firstBone&&b-adapter->firstBone<adapter->bindings.size()){
                                    const auto& bind=adapter->bindings[b-adapter->firstBone];source=bind.source;
                                    const auto expected=current[source]*bind.offset;for(int k=0;k<16;++k)error=std::max(error,std::abs(current[b].v[k]-expected.v[k]));
                                }
                                joints<<frame<<','<<name<<','<<raw[b].v[12]<<','<<raw[b].v[13]<<','<<raw[b].v[14]<<','<<view.v[12]<<','<<view.v[13]<<','<<view.v[14]<<','<<source<<','<<error<<'\n';
                            }
                            scene::Vec3 origin{},direction{1,0,0},up{0,0,1};
                            if(const auto camera=cadence::replay::poseCamera(a.scene.skeleton,current)){
                                origin={camera->v[12],camera->v[13],camera->v[14]};direction={camera->v[0],camera->v[1],camera->v[2]};up={camera->v[8],camera->v[9],camera->v[10]};
                            }
                            const auto vp=scene::perspective(75*scene::kPi/180,960.f/540.f,.05f,5000)*scene::lookAtDirection(origin,direction,up);
                            configureCaptureLighting(origin);
                            a.renderer.render(a.scene,current,vp,960,540,false,false,false);
                            if(!a.renderer.saveColorPng(auditOutput/("transition_fire_sprint_"+std::to_string(frame)+".png"),error))throw std::runtime_error(error);
                        };
                        captureFrame(0);
                        auditKeys[GLFW_KEY_W]=auditKeys[GLFW_KEY_LEFT_SHIFT]=GLFW_PRESS;
                        updateActorController(a,.001f);
                        const bool entering=a.viewmodelSprintEntering||a.animationIndex==*loop;
                        const bool blending=!a.interruptPoseSource.empty()&&a.interruptPoseDuration>0&&a.interruptPoseElapsed<a.interruptPoseDuration;
                        std::cout<<"FIRE_SPRINT selected="<<entering<<" blend="<<blending<<" sprint="<<a.actorSprinting<<std::endl;
                        if(!entering||!blending)++failures;
                        if(captureTransition){
                            std::ofstream trace(auditOutput/"transition_fire_sprint.csv");
                            trace<<"frame,sprinting,grounded,motion,animation,animationFrame,blendElapsed,blendDuration,source,transitioning,transitionElapsed,transitionDuration,previousAnimation,previousFrame,previousLooping\n";
                            for(int frame=1;frame<=61;++frame){
                                if(frame>1){
                                    if(frame==32)auditKeys.fill(GLFW_RELEASE);
                                    constexpr float dt=1.f/60.f;a.gameplayClock+=dt;
                                    updateActorController(a,dt);updateGameplay(a,dt);advanceLiveAnimationPlayback(a,dt);
                                }
                                captureFrame(frame);
                                if(frame==32&&!activeWeaponIsCs2(a))if(const auto out=findViewmodelClip(a,"sprint_out")){
                                    const bool retained=a.viewmodelSprintExiting&&a.animationIndex==*out;
                                    std::cout<<"SPRINT_EXIT_RETAINED "<<retained<<'\n';if(!retained)++failures;
                                }
                                trace<<frame<<','<<a.actorSprinting<<','<<a.actorGrounded<<','<<scene::motionRoleName(desiredMotion(a))<<','<<a.animationIndex<<','<<a.animationFrame<<','<<a.interruptPoseElapsed<<','<<a.interruptPoseDuration<<','<<a.scene.animations[a.animationIndex].sourceName<<','<<a.transitioning<<','<<a.transitionElapsed<<','<<a.activeTransitionDuration<<','<<a.previousAnimationIndex<<','<<a.previousAnimationFrame<<','<<a.scene.animations[a.previousAnimationIndex].looping<<'\n';
                            }
                            std::cout<<"TRANSITION_CAPTURE frames=62 rate=60 release_frame=32\n";
                        }
                        auditKeys.fill(GLFW_RELEASE);auditMouse.fill(GLFW_RELEASE);a.previousSprint=false;
                        stopGameplayAction(a);a.actorInputCaptured=false;
                    }
                    if(std::getenv("CADENCE_AUDIT_ACTION_TRANSITIONS")){
                        // Exercise action clocks and production blending, not
                        // normalized raw clip samples. This does not simulate
                        // a class-slot swap; equip/unequip are explicit actions.
                        auditKeys.fill(GLFW_RELEASE);auditMouse.fill(GLFW_RELEASE);
                        a.actorInputCaptured=false;a.actorSprinting=false;a.previousSprint=false;
                        a.viewmodelSprintEntering=false;a.viewmodelSprintExiting=false;
                        a.gameplayAds=false;a.viewmodelAdsEngaged=false;a.weaponSwitchStage=0;
                        a.experimentalWeaponTurning=false;
                        for(const auto role:{scene::ActionRole::Equip,scene::ActionRole::Reload,scene::ActionRole::Unequip}){
                            const std::string name=role==scene::ActionRole::Equip?"pullout":role==scene::ActionRole::Reload?"reload":"putaway";
                            if(!findViewmodelClip(a,name.c_str())){std::cout<<"ACTION_TRANSITION missing="<<name<<'\n';continue;}
                            stopGameplayAction(a);resolveGameplayAnimation(a);
                            for(int warm=0;warm<30;++warm){a.gameplayClock+=1.f/60;updateGameplay(a,1.f/60);advanceLiveAnimationPlayback(a,1.f/60);}
                            a.gameplayAction=role;triggerGameplayAction(a);
                            const bool started=a.actionActive;const float duration=a.actionDurationOverride;
                            const int frames=std::clamp(int(std::ceil(std::max(duration,.1f)*60))+31,37,1200);
                            std::ofstream trace(auditOutput/("controller_"+name+".csv"));
                            trace<<"frame,active,actionFrame,animationFrame,transitioning\n";
                            for(int frame=0;frame<frames;++frame){
                                const auto current=evaluateCurrentPose(a);
                                scene::Vec3 origin{},direction{1,0,0},up{0,0,1};
                                if(const auto camera=cadence::replay::poseCamera(a.scene.skeleton,current)){
                                    origin={camera->v[12],camera->v[13],camera->v[14]};direction={camera->v[0],camera->v[1],camera->v[2]};up={camera->v[8],camera->v[9],camera->v[10]};
                                }
                                const auto vp=scene::perspective(75*scene::kPi/180,960.f/540.f,.05f,5000)*scene::lookAtDirection(origin,direction,up);
                                configureCaptureLighting(origin);
                                a.renderer.render(a.scene,current,vp,960,540,false,false,false);
                                if(!a.renderer.saveColorPng(auditOutput/("controller_"+name+"_"+std::to_string(frame)+".png"),error))throw std::runtime_error(error);
                                trace<<frame<<','<<a.actionActive<<','<<a.actionFrame<<','<<a.animationFrame<<','<<a.transitioning<<'\n';
                                a.gameplayClock+=1.f/60;updateGameplay(a,1.f/60);advanceLiveAnimationPlayback(a,1.f/60);
                            }
                            std::cout<<"ACTION_TRANSITION "<<name<<" started="<<started<<" completed="<<!a.actionActive<<" frames="<<frames<<" duration="<<duration<<'\n';
                            if(!started||a.actionActive)++failures;
                        }
                    }
                    a.actorMode=true;a.actorViewHeight=152.4f;a.experimentalWeaponTurning=false;const auto base=evaluateCurrentPose(a);
                    a.experimentalWeaponTurning=true;a.weaponTurn.update(0,0,0,.016f);a.weaponTurn.update(0,10,0,.016f);const auto turned=evaluateCurrentPose(a);
                    float weaponDelta{};for(std::size_t j=0;j<base.size();++j)for(int k=0;k<16;++k)weaponDelta=std::max(weaponDelta,std::abs(base[j].v[k]-turned[j].v[k]));
                    std::cout<<"TURN_WEAPON delta="<<weaponDelta<<std::endl;if(weaponDelta<.001f)++failures;
                    if(const auto camera=a.scene.skeleton.boneByCanonicalName.find("tag_camera");camera!=a.scene.skeleton.boneByCanonicalName.end()){float delta{};for(int k=0;k<16;++k)delta=std::max(delta,std::abs(base[camera->second].v[k]-turned[camera->second].v[k]));std::cout<<"TURN_CAMERA delta="<<delta<<std::endl;if(delta>.0001f)++failures;}
                    a.experimentalWeaponTurning=false;a.actorMode=false;
                    a.weaponTiming.instantScope=true;
                    if(!importedCameraScope(a)){std::cerr<<"INSTANT_SCOPE policy failed\n";++failures;}
                    a.actorMode=true;a.actorNoclip=true;a.actorInputCaptured=true;a.adsSuppressedUntilRelease=false;a.weaponSwitchStage=0;
                    auditMouse[GLFW_MOUSE_BUTTON_RIGHT]=GLFW_PRESS;updateActorController(a,.001f);
                    const bool scopeOn=a.adsCameraBlend==1&&a.viewmodelAdsEngaged&&(livePlayerVisibility(a)&take::HideViewmodel)&&(livePlayerVisibility(a)&take::ScopeOverlay);
                    std::cout<<"SCOPE_STATE blend="<<a.adsCameraBlend<<" engaged="<<a.viewmodelAdsEngaged<<" ads="<<a.gameplayAds<<" visibility="<<int(livePlayerVisibility(a))<<" captured="<<a.actorInputCaptured<<" status="<<a.gameplayStatus<<std::endl;
                    auditMouse[GLFW_MOUSE_BUTTON_RIGHT]=GLFW_RELEASE;updateActorController(a,.001f);
                    const bool scopeOff=a.adsCameraBlend==0&&!a.viewmodelAdsEngaged&&!(livePlayerVisibility(a)&(take::HideViewmodel|take::ScopeOverlay));
                    std::cout<<"INSTANT_SCOPE input_on="<<scopeOn<<" input_off="<<scopeOff<<std::endl;
                    if(!scopeOn||!scopeOff)++failures;
                    a.actorInputCaptured=false;a.actorNoclip=false;a.actorMode=false;
                    a.weaponTiming.instantScope=false;
                    if(!isKnifeWeapon(a)){
                        stopGameplayAction(a);a.actorMode=true;a.takeRecording=true;a.recordedTake.shots.clear();
                        const auto expected=scene::resolveMuzzlePosition(a.scene,evaluateCurrentPose(a));
                        ++a.recoilSequence;updateBotActors(a,0);
                        const float error=expected&&!a.recordedTake.shots.empty()?scene::length(a.recordedTake.shots.back().muzzlePos-*expected):1e9f;
                        std::cout<<"TRAIL_MUZZLE error="<<error<<std::endl;if(error>.001f)++failures;
                        a.takeRecording=false;a.actorMode=false;
                    }
                    if(findViewmodelClip(a,"fire")&&!isKnifeWeapon(a)){
                        CachedClassRig previous;previous.scene=a.scene;previous.profile=a.weaponProfile;previous.profile.stats=a.weaponTiming;
                        previous.weaponAsset=i;previous.animationPrefix=a.viewmodelAnimationPrefix;previous.idleAnimation=ai;
                        a.classSlotRigs[0]=std::move(previous);a.classSlotRigs[1].emplace();a.activeClassSlot=1;a.lastClassSlot=0;
                        a.weaponTiming.grenade=true;a.gameplayAction=scene::ActionRole::Fire;triggerGameplayAction(a);
                        const float duration=a.actionDurationOverride;
                        updateGameplay(a,std::max(.001f,duration-.01f));const bool held=a.activeClassSlot==1;
                        updateGameplay(a,.03f);const bool returned=a.activeClassSlot==0;
                        std::cout<<"GRENADE_RETURN held_until_end="<<held<<" returned="<<returned<<std::endl;
                        if(!held||!returned)++failures;
                        a.classSlotRigs={};a.activeClassSlot=-1;a.lastClassSlot=-1;a.weaponSwitchStage=0;stopGameplayAction(a);
                        a.weaponTiming.grenade=true;a.gameplayAction=scene::ActionRole::Fire;triggerGameplayAction(a);
                        updateGameplay(a,std::max(.1f,a.actionDurationOverride+.02f));
                        if(a.activeClassSlot!=-1||a.actionActive){std::cerr<<"GRENADE invalid prior slot failed\n";++failures;}
                        a.weaponTiming.grenade=false;
                        CachedClassRig third;third.scene=a.scene;third.profile=a.weaponProfile;third.profile.stats=a.weaponTiming;third.weaponAsset=i;third.animationPrefix=a.viewmodelAnimationPrefix;third.idleAnimation=ai;
                        a.classSlotRigs[0]=third;a.classSlotRigs[2]=std::move(third);a.activeClassSlot=0;a.lastClassSlot=0;
                        a.experimentalThirdWeapon=true;a.actorMode=true;a.actorNoclip=true;a.actorInputCaptured=true;a.weaponSwitchStage=0;a.previousThirdWeaponKey=false;
                        auditKeys[GLFW_KEY_V]=GLFW_PRESS;updateActorController(a,.016f);auditKeys[GLFW_KEY_V]=GLFW_RELEASE;
                        for(int tick=0;tick<300;++tick){a.gameplayClock+=.016f;updateActorController(a,.016f);updateGameplay(a,.016f);}
                        const bool thirdHeld=a.activeClassSlot==2&&a.weaponSwitchStage==0;
                        std::cout<<"THIRD_WEAPON retained_after_release="<<thirdHeld<<std::endl;if(!thirdHeld)++failures;
                        a.actorInputCaptured=false;a.actorNoclip=false;a.actorMode=false;a.classSlotRigs={};a.activeClassSlot=-1;a.lastClassSlot=-1;
                    }
                }
            }catch(const std::exception& e){std::cerr<<game<<'/'<<asset.name<<": "<<e.what()<<std::endl;++failures;}
        }
    }
    std::cout<<"AUDIT tested="<<tested<<" failures="<<failures<<std::endl;
    if(!tested){std::cerr<<"No weapons matched the requested filter\n";return 2;}
    return failures?1:0;
}
