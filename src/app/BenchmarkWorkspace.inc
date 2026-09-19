// Included after the application loaders. All mutable scenes/renderers belong to this workspace.
#include "BenchmarkWorldLoader.inc"

namespace benchui {
constexpr std::size_t missing=static_cast<std::size_t>(-1);
struct Selection {std::string game;std::size_t body{missing},hands{missing},rifle{missing},pistol{missing};};
struct Case {std::string label;std::vector<std::size_t> clips;};
struct Lane {
    std::unique_ptr<AppState> app;
    std::string game,rig,weapon,error;
    std::vector<Case> cases;
    std::vector<scene::Mat4> pose;
    double sampleMs{},renderMs{};
    int width{},height{};
    float sampledTime{};
    std::string sampledRequest,sampledClip;
    std::size_t baseClip{missing};
};
struct Workspace {
    std::vector<Selection> selections;
    std::vector<Lane> worlds;
    std::optional<Lane> firstPerson;
    std::optional<Lane> nativeReference;
    bool compareNative{true},codmSideView{},skeletonOverlay{};
    int mode{},seed{1729},phase{},fpIndex{};
    bool running{},loading{},heavy{true},measurement{},repeat{},advancePending{};
    std::size_t loadIndex{},caseIndex{};
    float time{},speed{1};
    std::vector<std::string> worldCases;
    std::filesystem::path reportDir;
    std::string status;
    std::array<char,2048> description{};
    int issueLane{},issueNumber{};
    std::vector<double> cpuSamples;
    std::uint64_t sampleNumber{};
};
std::unique_ptr<Workspace> workspace;
std::string bodyKey(const scene::Animation& a){return std::string(scene::animationDomainName(a.domain))+" / "+scene::weaponClassName(a.weapon)+" / "+scene::motionRoleName(a.motion)+" / "+scene::stanceName(a.stance)+" / "+scene::directionName(a.direction)+" / "+scene::actionRoleName(a.action);}
void append(Workspace& b,const std::string& message){if(b.reportDir.empty())return;std::ofstream f(b.reportDir/"benchmark.txt",std::ios::app);f<<message<<'\n';if(!f)b.status="Could not append benchmark report";}
void discover(Workspace& b,const AppState& owner){
    b.selections.clear();std::set<std::string> games;for(const auto& a:owner.assetCatalog.entries)if(!a.game.empty())games.insert(a.game);
    for(const auto& game:games)b.selections.push_back({game,findGenericPlayermodelForGame(owner,game),findGenericViewhandsForGame(owner,game),benchmark::selectWeapon(owner.assetCatalog,game,false),benchmark::selectWeapon(owner.assetCatalog,game,true)});
}
std::unique_ptr<AppState> isolated(const AppState& owner){
    auto a=std::make_unique<AppState>();a->assetCatalog=owner.assetCatalog;a->defaultSalukiDirectory=owner.defaultSalukiDirectory;a->gameReferenceSetups=owner.gameReferenceSetups;
    a->deferSceneUpload=true;a->playing=false;a->actorMode=false;a->takePreview=false;
    if(!a->renderer.initialize(a->rendererError))return a;
    return a;
}
void assetCombo(const char* label,std::size_t& selected,const AppState& owner,assets::Role role,const std::string& game,bool cross){
    const auto& entries=owner.assetCatalog.entries;const std::string preview=selected<entries.size()?"["+entries[selected].game+"] "+entries[selected].name:"Missing / select";
    if(ImGui::BeginCombo(label,preview.c_str())){if(ImGui::Selectable("Missing",selected==missing))selected=missing;
        for(std::size_t i=0;i<entries.size();++i){const auto& a=entries[i];if(a.role!=role||(!cross&&a.game!=game)||benchmark::isPlaceholder(a.game,a.name))continue;ImGui::PushID(static_cast<int>(i));if(ImGui::Selectable(("["+a.game+"] "+a.name).c_str(),i==selected))selected=i;ImGui::PopID();}ImGui::EndCombo();}
}
void assignRigs(Workspace& b,const AppState& owner){
    if(b.mode==1)return;
    std::vector<std::size_t> bodies,hands;for(const auto& s:b.selections){const auto body=findGenericPlayermodelForGame(owner,s.game),hand=findGenericViewhandsForGame(owner,s.game);if(body!=missing)bodies.push_back(body);if(hand!=missing)hands.push_back(hand);}
    std::mt19937 random(static_cast<unsigned>(b.seed));
    auto t6=[&](assets::Role role){for(std::size_t i=0;i<owner.assetCatalog.entries.size();++i){const auto& a=owner.assetCatalog.entries[i];const auto g=lowerText(a.game);if(a.role==role&&(g=="t6"||g=="bo2"))return role==assets::Role::PlayerModel?findGenericPlayermodelForGame(owner,a.game):findGenericViewhandsForGame(owner,a.game);}return missing;};
    for(auto& s:b.selections){if(b.mode==0){s.body=findGenericPlayermodelForGame(owner,s.game);s.hands=findGenericViewhandsForGame(owner,s.game);}else if(b.mode==2){s.body=bodies.empty()?missing:bodies[random()%bodies.size()];s.hands=hands.empty()?missing:hands[random()%hands.size()];}else{s.body=t6(assets::Role::PlayerModel);s.hands=t6(assets::Role::ViewHands);}}
}
void start(Workspace& b,const AppState& owner){
    b.worlds.clear();b.firstPerson.reset();b.worldCases.clear();b.phase=0;b.fpIndex=0;b.caseIndex=0;b.time=0;b.loadIndex=0;b.running=false;b.loading=true;b.cpuSamples.clear();b.sampleNumber=0;b.issueNumber=0;
    const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::filesystem::path root=std::filesystem::current_path()/"benchmark_runs";
    std::error_code ec;std::filesystem::create_directories(root,ec);int suffix=0;do{b.reportDir=root/("run_"+std::to_string(stamp)+"_"+std::to_string(suffix++));}while(!ec&&!std::filesystem::create_directory(b.reportDir,ec));
    if(ec){b.loading=false;b.status="Cannot create benchmark report: "+ec.message();return;}
    append(b,"Cadence v72 benchmark schema 1\nAppend-only run log; do not erase prior issues. Raw animation/pose coverage, NOT a gameplay-controller or visual-correctness certification.\nStart epoch milliseconds: "+std::to_string(stamp)+"\nRig mode: "+std::to_string(b.mode)+"; random seed: "+std::to_string(b.seed)+"\nHeavy preset: "+std::to_string(b.heavy)+"; MSAA, sun shadows 2048, viewmodel shadows 2048, bloom, 8 Kawase samples, dual-lobe specular. No environment map synthesized.\nCPU measurements exclude UI/present/loading; no GPU timings. Dimensions and timescale logged per case. Warm-up: first 30 samples per case.");
    for(const auto& s:b.selections){std::string line="Selection: "+s.game;for(const auto i:{s.body,s.hands,s.rifle,s.pistol})line+=" | "+(i<owner.assetCatalog.entries.size()?owner.assetCatalog.entries[i].path.string():"MISSING");append(b,line);}
    b.status="Preparing world rigs one at a time...";
}
void loadWorld(Workspace& b,const AppState& owner){
    if(b.loadIndex<b.selections.size()){
        const auto& s=b.selections[b.loadIndex++];Lane lane;lane.game=s.game;lane.app=isolated(owner);
        if(s.body>=owner.assetCatalog.entries.size())lane.error="Missing selected player rig";
        else if(!lane.app->rendererError.empty())lane.error=lane.app->rendererError;
        else{lane.rig=owner.assetCatalog.entries[s.body].path.string();prepareBenchmarkWorld(*lane.app,owner,s.game,s.body);if(lane.app->scene.meshes.empty()||!lane.app->rendererError.empty()||lane.app->status.find("missing source skeleton")!=std::string::npos)lane.error=lane.app->status;
            lane.app->cameraPitch=-.12f;
            scene::AnimationQuery idle;idle.preferredGame=s.game;idle.weapon=scene::WeaponClass::Rifle;idle.motion=scene::MotionRole::Idle;idle.stance=scene::Stance::Stand;if(const auto base=botLocomotionAnimation(lane.app->scene,idle,nullptr))lane.baseClip=*base;
            std::map<std::string,std::vector<std::size_t>> grouped;for(std::size_t i=0;i<lane.app->scene.animations.size();++i){const auto& a=lane.app->scene.animations[i];if(lowerText(a.sourceGame)!=lowerText(s.game)||(a.domain!=scene::AnimationDomain::PlayerBody&&a.domain!=scene::AnimationDomain::PlayerTorso)||(a.motion==scene::MotionRole::Unknown&&a.action==scene::ActionRole::None))continue;grouped[bodyKey(a)].push_back(i);}
            for(const auto& [key,clips]:grouped)for(std::size_t v=0;v<clips.size();++v){const auto label=key+" / variant "+std::to_string(v+1);lane.cases.push_back({label,{clips[v]}});b.worldCases.push_back(label);}
            // Exercise the production resolver as well as individual authored clips.
            for(auto stance:{scene::Stance::Stand,scene::Stance::Crouch,scene::Stance::Prone})for(auto motion:{scene::MotionRole::Idle,scene::MotionRole::Walk,scene::MotionRole::Run,scene::MotionRole::Sprint})for(auto direction:{scene::Direction::Forward,scene::Direction::Backward,scene::Direction::Left,scene::Direction::Right}){
                scene::AnimationQuery q;q.preferredGame=s.game;q.weapon=scene::WeaponClass::Rifle;q.stance=stance;q.motion=motion;q.direction=direction;
                Case c{"Resolver: "+std::string(scene::motionRoleName(motion))+" / "+scene::stanceName(stance)+" / "+scene::directionName(direction),{}};
                if(const auto selected=botLocomotionAnimation(lane.app->scene,q,nullptr);selected&&lowerText(lane.app->scene.animations[*selected].sourceGame)==lowerText(s.game))c.clips.push_back(*selected);
                b.worldCases.push_back(c.label);lane.cases.push_back(std::move(c));
            }
        }append(b,"World load: "+lane.game+" | "+lane.rig+" | "+lane.error+" | clips="+std::to_string(lane.cases.size()));b.worlds.push_back(std::move(lane));return;
    }
    // Include expected basic locomotion even if no game resolves it.
    for(auto motion:{scene::MotionRole::Idle,scene::MotionRole::Walk,scene::MotionRole::Run,scene::MotionRole::Sprint}){scene::Animation a;a.domain=scene::AnimationDomain::PlayerBody;a.weapon=scene::WeaponClass::Rifle;a.motion=motion;a.stance=scene::Stance::Stand;a.direction=scene::Direction::Forward;b.worldCases.push_back(bodyKey(a)+" / variant 1");}
    std::sort(b.worldCases.begin(),b.worldCases.end(),[](const auto& a,const auto& c){if(a.starts_with("Resolver:")!=c.starts_with("Resolver:"))return a.starts_with("Resolver:");return a<c;});b.worldCases.erase(std::unique(b.worldCases.begin(),b.worldCases.end()),b.worldCases.end());b.loading=false;b.running=true;b.status="World locomotion lineup ready";
}
void loadFirstPerson(Workspace& b,const AppState& owner){
    b.firstPerson.reset();b.nativeReference.reset();if(b.fpIndex>=static_cast<int>(b.selections.size()*2)){b.running=false;b.phase=2;b.status="Run complete; reports retained";append(b,"Run complete");return;}
    const auto& s=b.selections[b.fpIndex/2];const auto weapon=b.fpIndex%2?s.pistol:s.rifle;Lane lane;lane.game=s.game;lane.app=isolated(owner);
    if(s.hands>=owner.assetCatalog.entries.size()||weapon>=owner.assetCatalog.entries.size())lane.error="Missing selected native/mixed hands or weapon";
    else if(!lane.app->rendererError.empty())lane.error=lane.app->rendererError;
    else{auto& a=*lane.app;lane.rig=owner.assetCatalog.entries[s.hands].path.string();lane.weapon=owner.assetCatalog.entries[weapon].path.string();a.selectedBaseAsset=a.classViewhandsOverride=s.hands;a.selectedWeaponAsset=weapon;a.assetFirstPerson=true;equipViewWeapon(a,weapon);a.deferSceneUpload=false;if(!uploadMainScene(a))lane.error=a.rendererError;a.viewmodelCamera=true;
        std::set<std::size_t> used;
        for(const char* slot:{"idle","first_raise","pullout","putaway","fire","ads_up","ads_fire","ads_down","reload","reload_empty","rechamber","ads_rechamber","sprint_in","sprint_loop","sprint_out","inspect","melee"}){auto clip=findViewmodelClip(a,slot);Case c{slot,{}};if(clip){c.clips.push_back(*clip);used.insert(*clip);}lane.cases.push_back(std::move(c));}
        for(std::size_t i=0;i<a.scene.animations.size();++i){const auto& clip=a.scene.animations[i];if(clip.domain==scene::AnimationDomain::ViewModel&&!used.contains(i)&&lowerText(clip.sourceGame)==lowerText(s.game))lane.cases.push_back({"Additional variant: "+clip.sourceName,{i}});}
    }
    if(lane.app->scene.codmRigAdapter){
        Lane ref;ref.app=isolated(owner);ref.game="CODM native reference";ref.weapon=lane.weapon;
        for(size_t i=0;i<owner.assetCatalog.entries.size();++i)if(lowerText(owner.assetCatalog.entries[i].name)=="codm_viewhands_c_m_ghost_default"&&lowerText(owner.assetCatalog.entries[i].game)=="codm"){
            ref.app->selectedBaseAsset=i;ref.rig=owner.assetCatalog.entries[i].path.string();equipViewWeapon(*ref.app,weapon);ref.app->deferSceneUpload=false;if(!uploadMainScene(*ref.app))ref.error=ref.app->rendererError;
            for(const auto& c:lane.cases){Case mapped{c.label,{}};for(auto index:c.clips){const auto& name=lane.app->scene.animations[index].sourceName;for(size_t j=0;j<ref.app->scene.animations.size();++j)if(ref.app->scene.animations[j].sourceName==name){mapped.clips.push_back(j);break;}}ref.cases.push_back(std::move(mapped));}break;
        }
        if(ref.rig.empty())ref.error="Native Ghost reference unavailable";b.nativeReference.emplace(std::move(ref));append(b,"CODM synchronized native/converted comparison; global fit="+std::to_string(lane.app->scene.codmTranslationFactor/100.f)+"; adapter="+lane.app->scene.codmRigAdapter->identity);
    }
    if(lane.cases.empty())lane.cases.push_back({"Unavailable",{}});append(b,"First person: "+lane.game+" | "+lane.rig+" | "+lane.weapon+" | "+lane.error);b.firstPerson.emplace(std::move(lane));b.caseIndex=0;b.time=0;b.sampleNumber=0;
}
const Case* currentCase(const Workspace& b,const Lane& lane){if(b.phase==1)return b.caseIndex<lane.cases.size()?&lane.cases[b.caseIndex]:nullptr;if(b.caseIndex>=b.worldCases.size())return nullptr;for(const auto& c:lane.cases)if(c.label==b.worldCases[b.caseIndex])return &c;return nullptr;}
float duration(const Workspace& b,const Lane& lane){const auto* c=currentCase(b,lane);if(!c||c->clips.empty())return 1.5f;const auto& a=lane.app->scene.animations[c->clips[0]];return std::max(1.0f,a.durationFrames/std::max(1.0f,a.framerate));}
void summarize(Workspace& b){if(b.cpuSamples.empty())return;append(b,"Timing phase="+std::to_string(b.phase)+" case="+std::to_string(b.caseIndex)+" CPU sample+render median_ms="+std::to_string(benchmark::percentile(b.cpuSamples,50))+" p95_ms="+std::to_string(benchmark::percentile(b.cpuSamples,95))+" samples="+std::to_string(b.cpuSamples.size()));b.cpuSamples.clear();}
void next(Workspace& b,const AppState& owner){summarize(b);b.time=0;b.sampleNumber=0;++b.caseIndex;if(b.phase==0&&b.caseIndex>=b.worldCases.size()){b.phase=1;b.caseIndex=0;loadFirstPerson(b,owner);}else if(b.phase==1&&b.firstPerson&&b.caseIndex>=b.firstPerson->cases.size()){++b.fpIndex;loadFirstPerson(b,owner);}}
void renderLane(Workspace& b,Lane& lane,float w,float h){
    ImGui::TextWrapped("%s | %s",lane.game.c_str(),std::filesystem::path(lane.rig).filename().string().c_str());
    if(!lane.error.empty()){ImGui::TextWrapped("LOAD ERROR: %s",lane.error.c_str());return;}
    auto& a=*lane.app;const auto* c=currentCase(b,lane);const bool valid=c&&!c->clips.empty()&&c->clips[0]<a.scene.animations.size()&&!a.scene.animations[c->clips[0]].tracks.empty();
    const std::string actual=valid?a.scene.animations[c->clips[0]].sourceName:"MISSING / UNMAPPED - bind pose";ImGui::TextWrapped("Actual: %s",actual.c_str());
    lane.sampledTime=b.time;lane.sampledClip=actual;lane.sampledRequest=b.phase==0?(b.caseIndex<b.worldCases.size()?b.worldCases[b.caseIndex]:"Unavailable"):(c?c->label:"Missing");
    if(valid){const auto& clip=a.scene.animations[c->clips[0]];ImGui::Text("Mapped tracks: %zu; unmapped curves: %zu",clip.tracks.size(),clip.unmappedCurveCount);}
    const auto begin=std::chrono::steady_clock::now();
    if(valid){a.animationIndex=c->clips[0];a.animationFrame=std::min(b.time*a.scene.animations[a.animationIndex].framerate,static_cast<float>(a.scene.animations[a.animationIndex].durationFrames));lane.pose=a.scene.samplePose(a.animationIndex,a.animationFrame);}else{lane.pose.clear();for(const auto& bone:a.scene.skeleton.bones)lane.pose.push_back(bone.restGlobal);}
    if(valid&&b.phase==0&&a.scene.animations[a.animationIndex].domain==scene::AnimationDomain::PlayerTorso&&lane.baseClip<a.scene.animations.size())lane.pose=a.scene.sampleLayeredPose(lane.baseClip,0,a.animationIndex,a.animationFrame,1,scene::LayerMode::Override,true);
    lane.sampleMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    scene::Vec3 target=a.cameraTarget;const float cp=std::cos(a.cameraPitch);scene::Vec3 forward{cp*std::cos(a.cameraYaw),cp*std::sin(a.cameraYaw),std::sin(a.cameraPitch)},up{0,0,1},eye=target-forward*a.cameraDistance;float fov=55;
    if(b.phase==1){const auto found=a.scene.skeleton.boneByCanonicalName.find("tag_camera");if(found!=a.scene.skeleton.boneByCanonicalName.end()&&found->second<lane.pose.size()){const auto& m=lane.pose[found->second];eye={m.v[12],m.v[13],m.v[14]};forward=scene::normalize(scene::Vec3{m.v[0],m.v[1],m.v[2]});up=scene::normalize(scene::Vec3{m.v[8],m.v[9],m.v[10]});}else{eye={0,0,0};forward={1,0,0};}fov=a.viewmodelFov*a.viewmodelFovScale;}
    if(b.phase==1&&b.codmSideView&&a.scene.codmNativeCentimetres){eye={125,-38,-15};forward={-1,0,0};up={0,0,1};fov=65;}
    lane.width=std::max(64,static_cast<int>(w));lane.height=std::max(64,static_cast<int>(h));auto& r=a.renderer;r.setCameraPosition(eye);r.setCameraDepthRange(.05f,10000);r.setAntialiasing(b.heavy);r.setViewmodelShadows(b.heavy,2048);r.setSun(true,b.heavy,{.3f,.4f,-.85f},1.4f,.35f,{1,.96f,.9f},{.7f,.8f,1},2048,500,400,target,false,1024,400,1000,100);r.setKawaseSamples(8,8);r.setLensTweaks(b.heavy,1,.5f,0,1,0,.15f,false,0,1,1);
    const auto renderStart=std::chrono::steady_clock::now();r.render(a.scene,lane.pose,scene::perspective(fov*scene::kPi/180,float(lane.width)/lane.height,.05f,10000)*scene::lookAtDirection(eye,forward,up),lane.width,lane.height,b.phase==0,b.skeletonOverlay,false);
    lane.renderMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-renderStart).count();
    ImGui::Image(static_cast<ImTextureID>(r.colorTexture()),ImVec2(w,h),ImVec2(0,1),ImVec2(1,0));
    if(b.sampleNumber==0)append(b,"Case phase="+std::to_string(b.phase)+" index="+std::to_string(b.caseIndex)+" game="+lane.game+" requested="+(b.phase==0?b.worldCases[b.caseIndex]:(c?c->label:"Missing"))+" actual="+actual+" size="+std::to_string(lane.width)+"x"+std::to_string(lane.height)+" scale="+std::to_string(b.measurement?1:b.speed));
}
void issue(Workspace& b){
    Lane* lane=b.phase==1&&b.firstPerson?&*b.firstPerson:(b.issueLane>=0&&b.issueLane<static_cast<int>(b.worlds.size())?&b.worlds[b.issueLane]:nullptr);if(!lane){b.status="No loaded case to annotate";return;}
    const auto id=++b.issueNumber;const auto stem="issue_"+std::to_string(id);std::string error;const bool captured=lane->app->renderer.saveColorPng(b.reportDir/(stem+".png"),error);
    if(b.phase==1&&b.nativeReference&&b.compareNative){std::string nativeError;const bool saved=b.nativeReference->app->renderer.saveColorPng(b.reportDir/(stem+"_native.png"),nativeError);append(b,"Paired native reference: "+(saved?stem+"_native.png":nativeError)+"; side_view="+std::to_string(b.codmSideView));}
    const auto* c=currentCase(b,*lane);const auto clip=c&&!c->clips.empty()?c->clips[0]:missing;std::ostringstream entry;entry<<"ISSUE "<<id<<" epoch_ms="<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()<<" phase="<<b.phase<<" case="<<b.caseIndex<<" time_seconds="<<lane->sampledTime<<" scale="<<(b.measurement?1:b.speed)<<"\nGame: "<<lane->game<<"\nRig: "<<lane->rig<<"\nWeapon: "<<lane->weapon<<"\nRequested: "<<lane->sampledRequest<<"\nActual: "<<lane->sampledClip<<"\nDescription: "<<std::quoted(b.description.data())<<"\nScreenshot: "<<(captured?stem+".png":"FAILED "+error)<<"\nPose: "<<stem<<"_bones.csv";append(b,entry.str());
    std::ofstream poses(b.reportDir/(stem+"_bones.csv"));poses<<"bone,parent,m00,m01,m02,m03,m10,m11,m12,m13,m20,m21,m22,m23,m30,m31,m32,m33\n";for(std::size_t i=0;i<lane->pose.size();++i){poses<<std::quoted(lane->app->scene.skeleton.bones[i].name)<<','<<lane->app->scene.skeleton.bones[i].parent;for(float value:lane->pose[i].v)poses<<','<<std::setprecision(9)<<value;poses<<'\n';}b.status=poses?"Issue appended with screenshot/pose diagnostics":"Issue saved, but pose diagnostic write failed";b.description.fill(0);
}
void draw(AppState& owner){
    if(!workspace)workspace=std::make_unique<Workspace>();auto& b=*workspace;
    if(b.advancePending){b.advancePending=false;next(b,owner);}
    ImGui::TextWrapped("Independent animation benchmark. Play/Replay is suspended and untouched. Cross-game compatibility is under test, not guaranteed. Raw clips do not certify gameplay action transitions.");
    ImGui::BeginChild("Benchmark controls",ImVec2(370,0),ImGuiChildFlags_Borders);
    const bool active=!b.reportDir.empty()&&b.phase<2;
    ImGui::BeginDisabled(active||b.loading);if(ImGui::Button("Discover loaded games"))discover(b,owner);ImGui::EndDisabled();ImGui::SameLine();ImGui::Text("%zu games",b.selections.size());
    if(active&&ImGui::Button("End run / edit setup")){summarize(b);append(b,"Run ended by user");b.phase=2;b.running=false;}
    ImGui::BeginDisabled(b.loading||active);const char* modes[]{"Native rigs","Manual cross-game rigs","Random rigs (seeded)","T6 / BO2 rigs only"};if(ImGui::Combo("Rig mode",&b.mode,modes,4))assignRigs(b,owner);ImGui::InputInt("Random seed",&b.seed);if(ImGui::Button("Apply rig mode / shuffle"))assignRigs(b,owner);ImGui::Checkbox("Heavy visuals",&b.heavy);
    if(ImGui::CollapsingHeader("Selected assets (verify before run)"))for(auto& s:b.selections){ImGui::PushID(s.game.c_str());if(ImGui::TreeNode(s.game.c_str())){assetCombo("Player rig",s.body,owner,assets::Role::PlayerModel,s.game,b.mode!=0);assetCombo("Viewhands",s.hands,owner,assets::Role::ViewHands,s.game,b.mode!=0);assetCombo("Rifle",s.rifle,owner,assets::Role::ViewWeapon,s.game,false);assetCombo("Pistol",s.pistol,owner,assets::Role::ViewWeapon,s.game,false);ImGui::TreePop();}ImGui::PopID();}
    if(ImGui::Button("Start new run")&&!b.selections.empty())start(b,owner);ImGui::EndDisabled();
    if(b.loading){ImGui::Text("Loading %zu / %zu world rigs...",b.loadIndex,b.selections.size());if(ImGui::Button("Cancel loading")){b.loading=false;b.phase=2;append(b,"Loading cancelled");}else loadWorld(b,owner);ImGui::EndChild();return;}
    ImGui::SameLine();if(ImGui::Button(b.running?"Pause":"Resume")&&b.phase<2&&!b.reportDir.empty())b.running=!b.running;
    ImGui::SameLine();if(ImGui::Button("Next case")&&!b.reportDir.empty()&&b.phase<2)next(b,owner);
    ImGui::SameLine();if(ImGui::Button("Previous case")&&b.caseIndex>0){summarize(b);--b.caseIndex;b.time=0;b.sampleNumber=0;b.running=false;}
    if(ImGui::Button("First-person phase")&&!b.selections.empty()&&!b.reportDir.empty()){summarize(b);b.phase=1;b.fpIndex=0;loadFirstPerson(b,owner);}
    ImGui::SameLine();if(ImGui::Button("Frame step")&&!b.running){float fps=60;if(b.phase==1&&b.firstPerson&&b.firstPerson->app->scene.codmNativeCentimetres){const auto* c=currentCase(b,*b.firstPerson);if(c&&!c->clips.empty())fps=std::max(1.f,b.firstPerson->app->scene.animations[c->clips[0]].framerate);}b.time+=1.f/fps;}ImGui::Checkbox("Fixed 1x measurement",&b.measurement);ImGui::BeginDisabled(b.measurement);ImGui::SliderFloat("Timescale",&b.speed,0,2,"%.2fx");ImGui::EndDisabled();
    ImGui::TextWrapped("%s",b.status.c_str());if(!b.reportDir.empty()){ImGui::TextWrapped("Report: %s",b.reportDir.string().c_str());if(ImGui::Button("Copy report folder"))ImGui::SetClipboardText(b.reportDir.string().c_str());}if(b.reportDir.empty()||b.phase==2){ImGui::EndChild();return;}
    ImGui::Checkbox("Repeat case",&b.repeat);
    if(b.phase==1&&b.firstPerson&&b.firstPerson->app->scene.codmNativeCentimetres){ImGui::Checkbox("Compare native CODM hands",&b.compareNative);ImGui::Checkbox("CODM side view",&b.codmSideView);}
    ImGui::Checkbox("Skeleton overlay",&b.skeletonOverlay);
    const auto label=b.phase==0?(b.caseIndex<b.worldCases.size()?b.worldCases[b.caseIndex]:"Preparing"):(b.firstPerson&&b.caseIndex<b.firstPerson->cases.size()?b.firstPerson->cases[b.caseIndex].label:"Missing");ImGui::TextWrapped("Requested: %s | %.2f seconds",label.c_str(),b.time);
    if(ImGui::BeginCombo("Jump to case",label.c_str())){const auto count=b.phase==0?b.worldCases.size():(b.firstPerson?b.firstPerson->cases.size():0);for(std::size_t i=0;i<count;++i){const auto& name=b.phase==0?b.worldCases[i]:b.firstPerson->cases[i].label;if(ImGui::Selectable(name.c_str(),i==b.caseIndex)){summarize(b);b.caseIndex=i;b.time=0;b.sampleNumber=0;b.running=false;}}ImGui::EndCombo();}
    float scrubEnd=1.5f;if(b.phase==0){for(const auto& lane:b.worlds)scrubEnd=std::max(scrubEnd,duration(b,lane));}else if(b.firstPerson)scrubEnd=duration(b,*b.firstPerson);
    if(ImGui::SliderFloat("Case time",&b.time,0,scrubEnd,"%.3f s")){b.running=false;summarize(b);b.sampleNumber=0;}
    if(b.phase==1){if(ImGui::Button("Previous weapon")&&b.fpIndex>0){--b.fpIndex;loadFirstPerson(b,owner);}ImGui::SameLine();if(ImGui::Button("Next weapon")){++b.fpIndex;loadFirstPerson(b,owner);}if(b.phase==2){ImGui::EndChild();return;}}
    ImGui::BeginDisabled(b.phase==1&&(!b.firstPerson||b.firstPerson->sampledRequest.empty()));if(ImGui::Button("Mark incorrect")){b.running=false;ImGui::OpenPopup("Describe benchmark issue");}ImGui::EndDisabled();
    if(ImGui::BeginPopupModal("Describe benchmark issue",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){if(b.phase==0&&!b.worlds.empty()){if(ImGui::BeginCombo("Game",b.worlds[std::clamp(b.issueLane,0,static_cast<int>(b.worlds.size()-1))].game.c_str())){for(int i=0;i<static_cast<int>(b.worlds.size());++i)if(ImGui::Selectable(b.worlds[i].game.c_str(),i==b.issueLane))b.issueLane=i;ImGui::EndCombo();}}ImGui::InputTextMultiline("Description",b.description.data(),b.description.size(),ImVec2(500,120));ImGui::BeginDisabled(!b.description[0]);if(ImGui::Button("Append issue")){issue(b);ImGui::CloseCurrentPopup();}ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
    double cpu=0;float end=1.5f;
    ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("Benchmark previews",ImVec2(0,0),ImGuiChildFlags_Borders);
    if(b.phase==0&&!b.worlds.empty()){const int columns=b.worlds.size()>1?2:1;const float w=std::max(128.0f,(ImGui::GetContentRegionAvail().x-16)/columns);if(ImGui::BeginTable("Simultaneous game rigs",columns)){for(auto& lane:b.worlds){ImGui::TableNextColumn();ImGui::PushID(lane.game.c_str());renderLane(b,lane,w,std::max(140.0f,w*.6f));ImGui::PopID();cpu+=lane.sampleMs+lane.renderMs;end=std::max(end,duration(b,lane));}ImGui::EndTable();}}
    else if(b.firstPerson){const auto size=ImGui::GetContentRegionAvail();const bool paired=b.compareNative&&b.nativeReference.has_value();const float width=std::max(128.0f,paired?(size.x-12)*.5f:size.x),height=std::max(128.0f,size.y-90);if(paired&&ImGui::BeginTable("Native CODM comparison",2)){ImGui::TableNextColumn();ImGui::PushID("converted");renderLane(b,*b.firstPerson,width,height);ImGui::PopID();ImGui::TableNextColumn();ImGui::PushID("native");renderLane(b,*b.nativeReference,width,height);ImGui::PopID();ImGui::EndTable();cpu=b.nativeReference->sampleMs+b.nativeReference->renderMs;}else if(!paired)renderLane(b,*b.firstPerson,width,height);cpu+=b.firstPerson->sampleMs+b.firstPerson->renderMs;end=duration(b,*b.firstPerson);}
    ImGui::EndChild();
    if(b.running){if(b.sampleNumber++>=30&&b.cpuSamples.size()<10000)b.cpuSamples.push_back(cpu);b.time+=std::min(ImGui::GetIO().DeltaTime,.1f)*(b.measurement?1:b.speed);if(b.time>end+.25f){if(b.repeat){summarize(b);b.time=0;b.sampleNumber=0;}else b.advancePending=true;}}else if(b.sampleNumber==0)++b.sampleNumber;
}
int diagnostic(AppState& owner,const std::filesystem::path& gameRoot,const std::string& game,int mode=0){
    std::string error;if(!assets::appendScan(gameRoot,game,owner.assetCatalog,error)){std::cerr<<error;return 1;}
    if(mode==3&&game!="bo2")if(!assets::appendScan(gameRoot.parent_path()/"bo2","bo2",owner.assetCatalog,error))return 1;
    Workspace b;discover(b,owner);b.selections.erase(std::remove_if(b.selections.begin(),b.selections.end(),[&](const auto& s){return s.game!=game;}),b.selections.end());b.mode=mode;assignRigs(b,owner);start(b,owner);while(b.loading)loadWorld(b,owner);b.running=false;
    auto capture=[&](const std::string& name){
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::Begin("Benchmark diagnostic");
        Lane* lane=b.phase==0?(b.worlds.empty()?nullptr:&b.worlds.front()):(b.firstPerson?&*b.firstPerson:nullptr);
        if(lane)renderLane(b,*lane,640,360);ImGui::End();ImGui::Render();
        if(!lane||!lane->error.empty()||lane->pose.empty()){std::cerr<<"Missing diagnostic scene "<<name<<'\n';return false;}
        for(const auto& matrix:lane->pose)for(float value:matrix.v)if(!std::isfinite(value))return false;
        std::size_t tested=0,unmapped=0;for(std::size_t i=0;i<lane->app->scene.animations.size();++i){const auto& clip=lane->app->scene.animations[i];if(clip.tracks.empty()){++unmapped;continue;}for(float fraction:{0.0f,.5f,1.0f}){const auto pose=lane->app->scene.samplePose(i,clip.durationFrames*fraction);for(const auto& matrix:pose)for(float value:matrix.v)if(!std::isfinite(value))return false;}++tested;}append(b,"Finite-pose sampling "+name+": "+std::to_string(tested)+" clips at start/middle/end; unmapped="+std::to_string(unmapped));
        std::string captureError;const bool saved=lane->app->renderer.saveColorPng(b.reportDir/(name+".png"),captureError);const auto glError=glGetError();
        append(b,"Diagnostic "+name+" GL="+std::to_string(glError)+" pose_bones="+std::to_string(lane->pose.size()));return saved&&glError==GL_NO_ERROR;
    };
    bool ok=true;if(!b.worlds.empty()&&!b.worlds.front().cases.empty()&&!b.worlds.front().app->scene.animations.empty()){const auto& key=b.worlds.front().cases.front().label;const auto found=std::find(b.worldCases.begin(),b.worldCases.end(),key);b.caseIndex=static_cast<std::size_t>(found-b.worldCases.begin());ok=capture("world")&&ok;}else append(b,"Diagnostic world SKIPPED: unavailable native player/animation assets; not a coverage pass");
    b.phase=1;b.fpIndex=0;loadFirstPerson(b,owner);ok=capture("rifle")&&ok;
    b.fpIndex=1;loadFirstPerson(b,owner);b.time=.2f;ok=capture("pistol")&&ok;
    std::snprintf(b.description.data(),b.description.size(),"Automatic diagnostic annotation: verify screenshot/report/bone dump linkage.");issue(b);
    std::cout<<"Benchmark diagnostic "<<(ok?"PASS":"FAIL")<<" "<<b.reportDir.string()<<'\n';workspace=std::make_unique<Workspace>(std::move(b));owner.workspaceMode=2;return ok?0:1;
}
} // namespace benchui
