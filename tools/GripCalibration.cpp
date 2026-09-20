#include "assets/LocalAssetPaths.h"
// Standalone reference collector. Never calls Cadence settings/profile writers.
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include "render/GlApi.h"
#include <fstream>
#include <iomanip>

namespace gripref {
using J=nlohmann::json;
struct Case {
    std::string id,clip,model,weapon,animationGame="bo2"; float frame{};size_t clipIndex{};
    scene::CastScene actor; std::vector<scene::Mat4> pose;
    scene::Attachment original,adjusted; std::array<char,1024> note{};
    std::vector<scene::Attachment> parts;J cameraReference;
    bool accepted{}; std::vector<scene::Attachment> undo;
};
J vec(scene::Vec3 v){return J::array({v.x,v.y,v.z});}
J mount(const scene::Attachment& a){return {{"position_cm",vec(a.position)},{"rotation_xyz_degrees",vec(a.rotationDegrees)},{"scale",vec(a.scale)},{"matrix_column_major",a.localMatrix().v}};}
scene::Vec3 readVec(const J& a){if(!a.is_array()||a.size()!=3)throw std::runtime_error("Invalid vector");scene::Vec3 v{a[0].get<float>(),a[1].get<float>(),a[2].get<float>()};if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))throw std::runtime_error("Nonfinite reference");return v;}
void setLocalMatrix(scene::Attachment& a,const scene::Mat4& m){
    scene::Quat q;scene::Vec3 scale;scene::decomposeAffine(m,a.position,q,scale);
    a.rotationDegrees=scene::Vec3{std::atan2(2*(q.w*q.x+q.y*q.z),1-2*(q.x*q.x+q.y*q.y)),std::asin(std::clamp(2*(q.w*q.y-q.z*q.x),-1.f,1.f)),std::atan2(2*(q.w*q.z+q.x*q.y),1-2*(q.y*q.y+q.z*q.z))}*(180/scene::kPi);
}
void moveWorld(Case& c,scene::Vec3 delta){const auto p=c.pose[c.adjusted.boneIndex];auto w=p*c.adjusted.localMatrix();w.v[12]+=delta.x;w.v[13]+=delta.y;w.v[14]+=delta.z;setLocalMatrix(c.adjusted,scene::inverseAffine(p)*w);}
void rotateWorld(Case& c,scene::Vec3 axis,float angle){const auto p=c.pose[c.adjusted.boneIndex];auto w=p*c.adjusted.localMatrix();const auto pivot=scene::transformPoint(w,{});auto r=scene::rotation(scene::fromAxisAngle(axis,angle));w=scene::translation(pivot)*r*scene::translation(pivot*-1.f)*w;setLocalMatrix(c.adjusted,scene::inverseAffine(p)*w);}
void applyMount(Case& c,bool original){c.actor.attachments=c.parts;if(original)return;const auto delta=c.adjusted.localMatrix()*scene::inverseAffine(c.original.localMatrix());for(auto& part:c.actor.attachments)if(part.boneIndex==c.original.boneIndex)setLocalMatrix(part,delta*part.localMatrix());c.actor.attachments[0]=c.adjusted;}
std::optional<ImVec2> project(scene::Vec3 p,const scene::Mat4& vp,ImVec2 pos,ImVec2 size){const auto& m=vp.v;const float w=m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15];if(w<=.001f)return {};return ImVec2{pos.x+size.x*.5f*(1+(m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12])/w),pos.y+size.y*.5f*(1-(m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13])/w)};}
float segmentDistance(ImVec2 p,ImVec2 a,ImVec2 b){const float x=b.x-a.x,y=b.y-a.y,l=x*x+y*y;const float t=l>0?std::clamp(((p.x-a.x)*x+(p.y-a.y)*y)/l,0.f,1.f):0;return std::hypot(p.x-a.x-x*t,p.y-a.y-y*t);}
struct Camera {float yaw=-.85f,pitch=.24f,distance=115;scene::Vec3 pan{};};
scene::Mat4 vpFor(const Case& c,const Camera& camera,float aspect){const auto center=scene::transformPoint(c.pose[c.adjusted.boneIndex],{})+camera.pan;const auto eye=center+scene::Vec3{std::cos(camera.yaw)*std::cos(camera.pitch),std::sin(camera.yaw)*std::cos(camera.pitch),std::sin(camera.pitch)}*camera.distance;return scene::perspective(45*scene::kPi/180,aspect,.5f,4000)*scene::lookAt(eye,center,{0,0,1});}
struct Gizmo {int axis=-1;scene::Vec3 direction{};ImVec2 tangent{};float pixelsPerUnit{};};
bool gizmo(Case& c,Gizmo& drag,int mode,bool local,const scene::Mat4& vp,ImVec2 origin,ImVec2 size,float distance,bool hovered){
    const auto world=c.pose[c.adjusted.boneIndex]*c.adjusted.localMatrix();const auto center=scene::transformPoint(world,{});const auto sc=project(center,vp,origin,size);if(!sc)return false;
    auto* dl=ImGui::GetWindowDrawList();const ImU32 colors[]={IM_COL32(255,90,80,255),IM_COL32(85,240,110,255),IM_COL32(85,155,255,255)};
    const float length=distance*.15f;float best=10;int hot=-1;scene::Vec3 chosen{};ImVec2 tangent{};float density=1;const auto mouse=ImGui::GetIO().MousePos;
    for(int axis=0;axis<3;++axis){scene::Vec3 dir{};(&dir.x)[axis]=1;if(local){auto r=world;r.v[12]=r.v[13]=r.v[14]=0;dir=scene::normalize(scene::transformPoint(r,dir));}
        if(mode==0){const auto end=project(center+dir*length,vp,origin,size);if(!end)continue;const float d=segmentDistance(mouse,*sc,*end);if(d<best){best=d;hot=axis;chosen=dir;tangent={end->x-sc->x,end->y-sc->y};density=std::hypot(tangent.x,tangent.y)/length;}dl->AddLine(*sc,*end,colors[axis],drag.axis==axis?5.f:3.f);dl->AddCircleFilled(*end,6,colors[axis]);dl->AddText({end->x+7,end->y},colors[axis],axis==0?"X":axis==1?"Y":"Z");}
        else{auto u=scene::cross(dir,scene::Vec3{0,0,1});if(scene::length(u)<.1f)u=scene::cross(dir,scene::Vec3{0,1,0});u=scene::normalize(u);const auto v=scene::cross(dir,u);constexpr int steps=96;for(int j=0;j<steps;++j){const float t=j*2*scene::kPi/steps,nt=(j+1)*2*scene::kPi/steps;auto a=project(center+(u*std::cos(t)+v*std::sin(t))*length,vp,origin,size),b=project(center+(u*std::cos(nt)+v*std::sin(nt))*length,vp,origin,size);if(!a||!b)continue;const float d=segmentDistance(mouse,*a,*b);if(d<best){best=d;hot=axis;chosen=dir;tangent={b->x-a->x,b->y-a->y};density=std::hypot(tangent.x,tangent.y)/(nt-t);}dl->AddLine(*a,*b,colors[axis],drag.axis==axis?4.f:2.f);}}
    }
    dl->AddCircleFilled(*sc,4,IM_COL32_WHITE);
    if(drag.axis<0&&hovered&&hot>=0&&ImGui::IsMouseClicked(0)&&density>1){drag.axis=hot;drag.direction=chosen;const float l=std::hypot(tangent.x,tangent.y);drag.tangent={tangent.x/l,tangent.y/l};drag.pixelsPerUnit=density;c.undo.push_back(c.adjusted);}
    if(drag.axis<0)return false;if(!ImGui::IsMouseDown(0)){drag.axis=-1;return false;}
    const auto delta=ImGui::GetIO().MouseDelta;const float amount=(delta.x*drag.tangent.x+delta.y*drag.tangent.y)/drag.pixelsPerUnit*(ImGui::GetIO().KeyShift?.15f:1.f);if(std::abs(amount)<1e-7f)return false;
    if(mode==0)moveWorld(c,drag.direction*amount);else rotateWorld(c,drag.direction,std::clamp(amount,-.2f,.2f));return true;
}
J caseJson(const Case& c){const auto parent=c.pose[c.adjusted.boneIndex];J bones=J::array();for(size_t i=0;i<c.pose.size();++i)bones.push_back({{"name",c.actor.skeleton.bones[i].name},{"global_matrix_column_major",c.pose[i].v}});return {{"case",c.id},{"body_cast",c.model},{"weapon_cast",c.weapon},{"clip",c.clip},{"frame",c.frame},{"animation_game",c.animationGame},{"camera",c.cameraReference},{"bone",c.actor.skeleton.bones[c.adjusted.boneIndex].name},{"parent_global",parent.v},{"original",mount(c.original)},{"adjusted",mount(c.adjusted)},{"local_delta_matrix",(c.adjusted.localMatrix()*scene::inverseAffine(c.original.localMatrix())).v},{"original_world",(parent*c.original.localMatrix()).v},{"adjusted_world",(parent*c.adjusted.localMatrix()).v},{"accepted_by_user",c.accepted},{"notes",c.note.data()},{"frozen_pose",bones}};}
std::filesystem::path outputDirectory(){
    return programDirectory()/"cadence weapon calibrator reference";
}
void writeReference(const std::filesystem::path& output,const std::vector<Case>& cases,const std::string& session,const std::string& event,int selected){
    J j={{"schema","cadence-grip-reference-v1"},{"reference_only",true},{"consumed_by_cadence",false},{"tool_build","v157"},{"session",session},{"event",event},{"selected_case",selected},{"units","centimetres"},{"rotation","XYZ degrees; matrix is authoritative"},{"cases",J::array()}};
    for(const auto& c:cases)j["cases"].push_back(caseJson(c));std::filesystem::create_directories(output);const auto temp=output/"latest.json.tmp",latest=output/"latest.json";
    {std::ofstream f(temp);if(!f||!(f<<j.dump(2)))throw std::runtime_error("Cannot write calibration reference");}
    if(!MoveFileExW(temp.c_str(),latest.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace calibration reference");
    std::ofstream history(output/(session+".jsonl"),std::ios::app);if(!history||!(history<<j.dump()<<'\n'))throw std::runtime_error("Cannot append reference history");
}
void restoreReference(const std::filesystem::path& output,std::vector<Case>& cases){
    std::ifstream f(output/"latest.json");if(!f)return;J j;f>>j;if(j.value("schema","")!="cadence-grip-reference-v1")throw std::runtime_error("Unknown previous reference format");
    for(auto& c:cases)for(const auto& old:j.at("cases"))if(old.value("case","")==c.id&&old.value("body_cast","")==c.model&&old.value("weapon_cast","")==c.weapon&&old.at("original")==mount(c.original)&&old.at("parent_global")==J(c.pose[c.adjusted.boneIndex].v)){
        auto a=c.adjusted;a.position=readVec(old.at("adjusted").at("position_cm"));a.rotationDegrees=readVec(old.at("adjusted").at("rotation_xyz_degrees"));c.adjusted=a;c.accepted=old.value("accepted_by_user",false);const auto note=old.value("notes","");std::snprintf(c.note.data(),c.note.size(),"%s",note.c_str());
    }
}
void selfTestMath(){Case c;c.pose={scene::trs({3,8,-2},scene::fromEulerRadians({.4f,-.3f,.7f}),{1,1,1})};c.adjusted.position={4,5,6};c.adjusted.rotationDegrees={14,-22,37};auto w=c.pose[0]*c.adjusted.localMatrix();moveWorld(c,{2,-3,1});auto after=c.pose[0]*c.adjusted.localMatrix();if(scene::length(scene::transformPoint(after,{})-scene::transformPoint(w,{})-scene::Vec3{2,-3,1})>.0001f)throw std::runtime_error("Translation test failed");w=after;rotateWorld(c,{0,0,1},.3f);after=c.pose[0]*c.adjusted.localMatrix();const auto p=scene::transformPoint(w,{});const auto wanted=scene::translation(p)*scene::rotation(scene::fromAxisAngle({0,0,1},.3f))*scene::translation(p*-1.f)*w;for(int i=0;i<16;++i)if(std::abs(after.v[i]-wanted.v[i])>.0001f)throw std::runtime_error("Rotation test failed");}
}

int main(int argc,char** argv){using namespace gripref;
    const bool smoke=argc>1&&std::string(argv[1])=="--self-test";
    try{selfTestMath();if(!glfwInit())throw std::runtime_error("GLFW initialization failed");glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);if(smoke)glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(1380,900,"cadence weapon calibrator",nullptr,nullptr);if(!window)throw std::runtime_error("Window creation failed");glfwMakeContextCurrent(window);glfwSwapInterval(1);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().LogFilename=nullptr;ImGui::StyleColorsDark();ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    const auto frameStart=[&](){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();};
    const auto frameEnd=[&](){ImGui::Render();int w,h;glfwGetFramebufferSize(window,&w,&h);glapi::BindFramebuffer(glapi::Framebuffer,0);glViewport(0,0,w,h);glClearColor(.07f,.08f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(window);};
    auto app=std::make_unique<AppState>();auto& a=*app;a.window=window;a.deferSceneUpload=true;a.defaultSalukiDirectory=cadence::local_assets::exportPath("");std::string error;if(!a.renderer.initialize(error))throw std::runtime_error(error);
    const std::vector<std::string> games={"bo2","pointblank","codm","mw","mw3","ghosts","aw","iw_sp","mwr","bocw_sp","bo2_sp","cs2"};
    for(const auto& game:games){if(!std::filesystem::exists(a.defaultSalukiDirectory/game))continue;frameStart();ImGui::Begin("Preparing reference poses");ImGui::Text("Loading %s...",game.c_str());ImGui::TextUnformatted("No Cadence settings are being changed.");ImGui::End();frameEnd();if(!assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error))throw std::runtime_error(error);}
    auto find=[&](std::string game,assets::Role role,std::string needle){for(size_t i=0;i<a.assetCatalog.entries.size();++i){const auto& e=a.assetCatalog.entries[i];if(e.game==game&&e.role==role&&lowerText(e.name).find(lowerText(needle))!=std::string::npos)return i;}throw std::runtime_error("Required reference asset not found: "+needle);};
    std::vector<Case> cases;
    const auto buildCase=[&](size_t body,size_t weapon,const std::string& game,std::string id,std::string requested,float frame){
        a.autoPlayerModel=false;a.actorMode=a.playing=a.gameplayLogic=true;a.actorPosition={};a.manualPlayerModelAsset=body;a.selectedWeaponAsset=a.classPrimaryAsset=weapon;a.playerWorldAnimationGame=game;configureClassActor(a);if(!a.hiddenWorldActor||a.hiddenWorldActor->attachments.empty())throw std::runtime_error("No world weapon assembled for this combination");
        Case c;c.id=id;c.animationGame=game;c.frame=frame;c.model=a.assetCatalog.entries[body].path.string();c.weapon=a.assetCatalog.entries[weapon].path.string();c.actor=*a.hiddenWorldActor;size_t clip=SIZE_MAX;
        for(size_t i=0;i<c.actor.animations.size();++i)if(std::filesystem::path(c.actor.animations[i].sourceName).filename()==requested){clip=i;break;}
        if(clip==SIZE_MAX)for(size_t i=0;i<c.actor.animations.size();++i){const auto& anim=c.actor.animations[i];if(anim.domain==scene::AnimationDomain::PlayerBody&&anim.motion==scene::MotionRole::Idle&&!anim.contextual){clip=i;break;}}
        if(clip==SIZE_MAX)throw std::runtime_error("No usable body pose in this animation set");c.clipIndex=clip;c.clip=std::filesystem::path(c.actor.animations[clip].sourceName).filename().string();c.frame=std::clamp(c.frame,0.f,float(c.actor.animations[clip].durationFrames));c.pose=c.actor.samplePose(clip,c.frame);c.original=c.adjusted=c.actor.attachments.front();c.parts=c.actor.attachments;return c;
    };
    for(int n=0;n<2;++n)cases.push_back(buildCase(find("codm",assets::Role::PlayerModel,"Charly_Sinister"),find("pointblank",assets::Role::ViewWeapon,n?"AK-47_DualMag":"ColtPython"),"bo2",n?"rifle_ak47":"pistol_python",n?"pb_stand_alert.cast":"pb_stand_alert_pistol.cast",n?5.f:80.f));
    auto output=outputDirectory();if(smoke)output/="self-test";const auto session="session_"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count());std::string status="Edits auto-save here for assistant reference only.";
    try{std::ifstream previous(output/"latest.json");if(previous){std::filesystem::copy_file(output/"latest.json",output/(session+"_previous.json"));J saved;previous>>saved;if(saved.value("schema","")!="cadence-grip-reference-v1")throw std::runtime_error("Unsupported previous reference");for(const auto& item:saved.at("cases")){
        const auto pathIndex=[&](const std::string& p){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].path.lexically_normal()==std::filesystem::path(p).lexically_normal())return i;throw std::runtime_error("Saved case asset missing: "+p);};
        auto restored=buildCase(pathIndex(item.at("body_cast")),pathIndex(item.at("weapon_cast")),item.value("animation_game","bo2"),item.at("case"),item.at("clip"),item.at("frame"));auto found=std::find_if(cases.begin(),cases.end(),[&](const auto& c){return c.id==restored.id;});if(found!=cases.end())*found=std::move(restored);else cases.push_back(std::move(restored));
    }}restoreReference(output,cases);}catch(const std::exception& e){status=std::string("Previous reference preserved; some cases not restored: ")+e.what();}
    Camera camera;Gizmo drag;int selected=0,uploaded=-1,mode=0;bool local=false,showGizmo=true,dirty=false,compare=false;double lastSave=0;bool saveShot=false;int frames=0;
    size_t pickedBody=find("codm",assets::Role::PlayerModel,"Charly_Sinister"),pickedWeapon=find("pointblank",assets::Role::ViewWeapon,"ColtPython");int pickedGame=0;char bodyFilter[128]{},weaponFilter[128]{};
    while(!glfwWindowShouldClose(window)){
        frameStart();auto& io=ImGui::GetIO();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);ImGui::Begin("Reference editor",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginChild("Controls",{350,0},ImGuiChildFlags_Borders);ImGui::TextUnformatted("cadence weapon calibrator");ImGui::TextDisabled("Reference only — never applied to Cadence");
        if(ImGui::BeginCombo("Reference case",cases[selected].id.c_str())){for(int i=0;i<int(cases.size());++i)if(ImGui::Selectable(cases[i].id.c_str(),selected==i)){selected=i;drag.axis=-1;camera={};}ImGui::EndCombo();}
        if(ImGui::CollapsingHeader("Other model combinations")){
            const auto picker=[&](const char* label,assets::Role role,size_t& choice,char* filter){ImGui::PushID(label);ImGui::InputText("Search",filter,128);if(ImGui::BeginCombo(label,a.assetCatalog.entries[choice].name.c_str())){for(size_t i=0;i<a.assetCatalog.entries.size();++i){const auto& e=a.assetCatalog.entries[i];if(e.role!=role||(!std::string(filter).empty()&&lowerText(e.game+" "+e.name).find(lowerText(filter))==std::string::npos))continue;ImGui::PushID(int(i));if(ImGui::Selectable(("["+e.game+"] "+e.name).c_str(),choice==i))choice=i;ImGui::PopID();}ImGui::EndCombo();}ImGui::PopID();};
            picker("Character",assets::Role::PlayerModel,pickedBody,bodyFilter);picker("Weapon",assets::Role::ViewWeapon,pickedWeapon,weaponFilter);
            if(ImGui::BeginCombo("Animation source",games[pickedGame].c_str())){for(int i=0;i<int(games.size());++i)if(ImGui::Selectable(games[i].c_str(),pickedGame==i))pickedGame=i;ImGui::EndCombo();}
            if(ImGui::Button("Add this combination")){try{const auto id="custom_"+std::to_string(cases.size()+1);const bool pistol=weapon::inferArchetype(a.assetCatalog.entries[pickedWeapon].name)==weapon::Archetype::Pistol;cases.push_back(buildCase(pickedBody,pickedWeapon,games[pickedGame],id,pistol?"pb_stand_alert_pistol.cast":"pb_stand_alert.cast",0));selected=int(cases.size()-1);camera={};uploaded=-1;dirty=true;status="Combination added; inspect the pose before approving.";}catch(const std::exception& e){status=e.what();}}
        }
        auto& c=cases[selected];
        if(ImGui::CollapsingHeader("Frozen animation pose")){bool poseChanged=false;if(ImGui::BeginCombo("Clip",c.clip.c_str())){for(size_t i=0;i<c.actor.animations.size();++i){const auto& anim=c.actor.animations[i];if(anim.domain!=scene::AnimationDomain::PlayerBody)continue;ImGui::PushID(int(i));if(ImGui::Selectable(anim.sourceName.c_str(),c.clipIndex==i)){c.clipIndex=i;c.clip=std::filesystem::path(anim.sourceName).filename().string();c.frame=0;poseChanged=true;}ImGui::PopID();}ImGui::EndCombo();}poseChanged|=ImGui::SliderFloat("Frame",&c.frame,0,float(c.actor.animations[c.clipIndex].durationFrames),"%.2f");if(poseChanged){c.pose=c.actor.samplePose(c.clipIndex,c.frame);c.accepted=false;dirty=true;}}
        ImGui::Separator();ImGui::RadioButton("Move [W]",&mode,0);ImGui::SameLine();ImGui::RadioButton("Rotate [E]",&mode,1);ImGui::Checkbox("Local gun axes",&local);ImGui::Checkbox("Show gizmo",&showGizmo);
        if(!io.WantTextInput&&drag.axis<0){if(ImGui::IsKeyPressed(ImGuiKey_W))mode=0;if(ImGui::IsKeyPressed(ImGuiKey_E))mode=1;}
        const auto before=c.adjusted;bool changed=ImGui::DragFloat3("Position (cm)",&c.adjusted.position.x,.05f,0,0,"%.3f");if(ImGui::IsItemActivated())c.undo.push_back(before);changed|=ImGui::DragFloat3("Rotation (deg)",&c.adjusted.rotationDegrees.x,.15f,0,0,"%.2f");if(ImGui::IsItemActivated())c.undo.push_back(before);
        if(ImGui::Button("Undo")&&!c.undo.empty()){c.adjusted=c.undo.back();c.undo.pop_back();changed=true;}ImGui::SameLine();if(ImGui::Button("Reset this pose")){c.undo.push_back(c.adjusted);c.adjusted=c.original;changed=true;}
        ImGui::Button("Hold to compare original",{-1,0});compare=ImGui::IsItemActive();
        ImGui::Separator();ImGui::TextUnformatted("Camera");if(ImGui::Button("Right side")){camera.yaw=-.85f;camera.pitch=.24f;camera.pan={};}ImGui::SameLine();if(ImGui::Button("Left side")){camera.yaw=.85f;camera.pitch=.24f;camera.pan={};}if(ImGui::Button("Front")){camera.yaw=0;camera.pitch=.1f;camera.pan={};}ImGui::SameLine();if(ImGui::Button("Close-up"))camera.distance=65;ImGui::SliderFloat("Zoom",&camera.distance,35,350,"%.0f cm");
        ImGui::TextWrapped("Right-drag: orbit. Middle-drag: pan. Wheel: zoom. Drag a colored axis or ring. Hold Shift for fine control.");
        ImGui::Separator();if(ImGui::InputTextMultiline("Notes",c.note.data(),c.note.size(),{-1,95}))dirty=true;
        if(ImGui::Button("Mark placement correct + capture",{-1,32})){c.accepted=true;dirty=true;saveShot=true;}
        if(ImGui::Button("Save draft + capture",{-1,0})){dirty=true;saveShot=true;}
        ImGui::TextColored(c.accepted?ImVec4{.3f,1,.5f,1}:ImVec4{1,.75f,.3f,1},"%s",c.accepted?"Marked correct by you":"Draft / not marked correct");ImGui::TextWrapped("%s",status.c_str());ImGui::TextWrapped("Saved to: %s",output.string().c_str());ImGui::TextDisabled("Close normally to save the final state.");ImGui::EndChild();ImGui::SameLine();
        ImGui::BeginChild("View",{0,0},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);const auto pos=ImGui::GetCursorScreenPos();auto size=ImGui::GetContentRegionAvail();size.x=std::max(size.x,32.f);size.y=std::max(size.y,32.f);
        if(uploaded!=selected){if(!a.renderer.loadScene(c.actor,error))throw std::runtime_error(error);a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.13f,.14f,.16f,1});uploaded=selected;}
        const auto vp=vpFor(c,camera,size.x/size.y);c.cameraReference={{"yaw_radians",camera.yaw},{"pitch_radians",camera.pitch},{"distance_cm",camera.distance},{"pan_cm",vec(camera.pan)},{"fov_degrees",45},{"viewport_pixels",J::array({int(size.x),int(size.y)})},{"view_projection_column_major",vp.v}};applyMount(c,compare);a.renderer.render(c.actor,c.pose,vp,int(size.x),int(size.y),false,false,false);applyMount(c,false);
        ImGui::Image((ImTextureID)a.renderer.colorTexture(),size,{0,1},{1,0});const bool hover=ImGui::IsItemHovered();
        if(hover&&drag.axis<0){if(ImGui::IsMouseDragging(1)){camera.yaw-=io.MouseDelta.x*.008f;camera.pitch=std::clamp(camera.pitch+io.MouseDelta.y*.008f,-1.4f,1.4f);}if(ImGui::IsMouseDragging(2)){const scene::Vec3 right{-std::sin(camera.yaw),std::cos(camera.yaw),0};camera.pan+=right*(-io.MouseDelta.x*camera.distance*.0008f)+scene::Vec3{0,0,io.MouseDelta.y*camera.distance*.0008f};}camera.distance=std::clamp(camera.distance*std::exp(-io.MouseWheel*.12f),25.f,500.f);}
        if(showGizmo&&!compare)changed|=gizmo(c,drag,mode,local,vp,pos,size,camera.distance,hover);if(changed){dirty=true;c.accepted=false;}
        if(saveShot){try{const auto folder=output/session;std::filesystem::create_directories(folder);const auto stamp=std::to_string(std::chrono::system_clock::now().time_since_epoch().count());const auto path=folder/(c.id+"_"+stamp+".png");c.actor.attachments[0]=c.adjusted;a.renderer.render(c.actor,c.pose,vp,int(size.x),int(size.y),false,false,false);if(!a.renderer.saveColorPng(path,error))throw std::runtime_error(error);status="Reference and screenshot saved.";}catch(const std::exception& e){status=e.what();}saveShot=false;}
        ImGui::EndChild();ImGui::End();frameEnd();
        if(smoke&&frames++==0){moveWorld(cases[0],{1,2,3});rotateWorld(cases[0],{0,0,1},.15f);dirty=true;saveShot=true;}
        if(dirty&&(glfwGetTime()-lastSave>.75||!ImGui::IsMouseDown(0))){try{writeReference(output,cases,session,"autosave",selected);dirty=false;lastSave=glfwGetTime();}catch(const std::exception& e){status=e.what();}}
        if(smoke&&frames>3)break;
    }
    writeReference(output,cases,session,"closed",selected);
    if(smoke){auto restored=cases;restored[0].adjusted=restored[0].original;restoreReference(output,restored);for(int i=0;i<16;++i)if(std::abs(restored[0].adjusted.localMatrix().v[i]-cases[0].adjusted.localMatrix().v[i])>1e-5f)throw std::runtime_error("Save/reopen transform mismatch");std::cout<<"Grip reference math, render and persistence PASS\n";}
    a.renderer.shutdown();app.reset();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(!smoke)MessageBoxA(nullptr,e.what(),"Grip Calibration error",MB_OK|MB_ICONERROR);return 1;}
}
