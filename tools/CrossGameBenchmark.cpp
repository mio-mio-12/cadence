#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#include <numeric>
#include <iomanip>
#include <limits>
#include "render/NativeViewhandSkinning.h"
#include "render/GlApi.h"
#include "scene/SleeveClearance.h"
#include "scene/ArmSkinning.h"
#include "scene/ProximalSupportFit.h"
#include "scene/ArmComponentSupport.h"
#include "scene/ProximalContinuation.h"
#include "scene/WristTransitionDiagnostic.h"
#include "scene/NativeProximalDiagnostic.h"
#include "scene/BindBakedBodyDelta.h"

namespace crossbench {
using Clock=std::chrono::steady_clock;
constexpr int Width=960,Height=540;
decltype(glapi::UniformMatrix4fv) reviewMatrixUpload{};
decltype(glapi::DrawElements) reviewDrawElements{};
void APIENTRY traceReviewDraw(GLenum mode,GLsizei count,GLenum type,const void* indices){
    static unsigned remaining=24;
    if(remaining){
        --remaining;GLint program{};glGetIntegerv(0x8B8D,&program);
        using ReadUniform=void(APIENTRY*)(GLuint,GLint,GLfloat*);
        const auto read=reinterpret_cast<ReadUniform>(glfwGetProcAddress("glGetUniformfv"));
        const auto loc=glapi::GetUniformLocation(program,"uViewProjection");
        GLfloat m[16]{};if(read&&loc>=0)read(program,loc,m);
        std::cerr<<"REVIEW_DRAW program="<<program<<" loc="<<loc<<" count="<<count<<" x="<<m[0]<<','<<m[4]<<','<<m[8]<<','<<m[12]<<'\n';
    }
    reviewDrawElements(mode,count,type,indices);
}
void APIENTRY traceReviewMatrix(GLint location,GLsizei count,GLboolean transpose,const GLfloat* value){
    static unsigned remaining=160;
    if(remaining&&count==1&&value){--remaining;std::cerr<<"REVIEW_MATRIX loc="<<location<<" x="<<value[0]<<','<<value[4]<<','<<value[8]<<','<<value[12]<<" w="<<value[3]<<','<<value[7]<<','<<value[11]<<','<<value[15]<<'\n';}
    reviewMatrixUpload(location,count,transpose,value);
}
void configureReviewLighting(AppState& app){
    // Process-local verification controls; never modify saved user presets.
    if(std::getenv("CADENCE_REVIEW_MATRIX_TRACE")&&!reviewMatrixUpload){reviewMatrixUpload=glapi::UniformMatrix4fv;glapi::UniformMatrix4fv=traceReviewMatrix;reviewDrawElements=glapi::DrawElements;glapi::DrawElements=traceReviewDraw;}
    app.renderer.setPoseUploadCacheEnabled(std::getenv("CADENCE_REVIEW_UNCACHED_POSES")==nullptr);
    if(const auto* flip=std::getenv("CADENCE_REVIEW_FLIP"))app.renderer.setViewmodelProjection(app.weaponProfile.viewmodelFovMultiplier,std::string_view(flip)!="0");
    const bool lit=std::getenv("CADENCE_CROSS_LIT")!=nullptr;
    app.renderer.setDebugView(lit?0:1);
    if(lit)app.renderer.setSun(true,false,{-.4f,.6f,-.7f},1,.2f,{1,1,1},{1,1,1},1024,2000,1500,{},false,1024,2000,5000,500);
}
const std::vector<std::string> requiredGames={"aw","blacksquad","bo2","bo2_sp","bocw_sp","codm","cs1.6","cs2","csnz","cso2","css","dirtybomb","eldewrito","ghosts","gitsfa","iw_sp","mw","mw3","mwr","pointblank","bo","mw2","cz"};
double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
std::string quoted(std::string s){std::string r="\"";for(char c:s){if(c=='\"')r+='\"';if(c=='\n'||c=='\r')r+=' ';else r+=c;}return r+'\"';}
void require(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
bool finite(const std::vector<scene::Mat4>& pose){for(const auto& m:pose)for(float f:m.v)if(!std::isfinite(f))return false;return true;}
std::vector<scene::Mat4> evaluatedBodyPose(const scene::CastScene& s,std::size_t clip,float frame){
    auto p=s.samplePose(clip,frame);
#ifdef CADENCE_CROSS_CURRENT_REVIEW
    const auto& locomotion=s.animations[clip];
    if(scene::imported::usesNativeHoldingTorso(locomotion.sourceGame)&&locomotion.action==scene::ActionRole::None&&locomotion.domain==scene::AnimationDomain::PlayerBody){
        scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerTorso;q.motion=scene::imported::holdingTorsoMotion(locomotion.sourceGame,locomotion.motion);q.weapon=scene::WeaponClass::Rifle;q.stance=locomotion.stance==scene::Stance::Any?scene::Stance::Stand:locomotion.stance;q.preferredGame=locomotion.sourceGame;
        if(auto hold=scene::findBestAnimation(s,q);hold&&scene::imported::isNativeHoldingTorso(s.animations[*hold])){
            const auto& h=s.animations[*hold];const float heldFrame=locomotion.sourceGame=="cso2"&&h.durationFrames&&locomotion.framerate>0?std::fmod(frame/locomotion.framerate*h.framerate,static_cast<float>(h.durationFrames)):0;
            std::vector<scene::PoseSlot> slots(2);slots[1].nodes.push_back({*hold,heldFrame,1.f,scene::LayerMode::Override,true});p=s.samplePoseSlots(clip,frame,slots);
        }
    }
    if(s.animations[clip].domain==scene::AnimationDomain::PlayerTorso){
        const auto& a=s.animations[clip];scene::AnimationQuery q;
        q.motion=scene::MotionRole::Idle;q.weapon=a.weapon;q.stance=a.stance==scene::Stance::Any?scene::Stance::Stand:a.stance;
        if(std::getenv("CADENCE_DIAGNOSTIC_BODY_RUN"))q.motion=scene::MotionRole::Run;
        const auto base=botLocomotionAnimation(s,q,nullptr);
        require(base.has_value(),"No posture-matched base for torso action: "+a.sourceName);
        std::vector<scene::PoseSlot> slots(2);slots[1].nodes.push_back({clip,frame,1.f,scene::LayerMode::Override,true});
        const auto& idle=s.animations[*base];const float seconds=frame/std::max(1.f,a.framerate);
        const float baseFrame=idle.durationFrames?std::fmod(seconds*idle.framerate,float(idle.durationFrames)):0.f;
        p=s.samplePoseSlots(*base,baseFrame,slots);
    }
    static std::set<std::pair<const scene::CastScene*,std::size_t>> logged;
    if(logged.emplace(&s,clip).second){
        const auto& a=s.animations[clip];const auto root=scene::imported::bodyMotionRoot(s.skeleton);
        std::cout<<"BODY_ROOT clip="<<a.sourceName<<" loop="<<a.looping<<" motion="<<int(a.motion)<<" root="<<root.bone
                 <<" adapter="<<bool(a.coldWarWorldPose)<<" transport="<<(a.coldWarWorldPose?a.coldWarWorldPose->locomotionSourceRoot:-1)<<'\n';
    }
#endif
    scene::imported::suppressBodyRootMotion(s.skeleton,p,s.animations[clip].sourceGame=="pointblank");
    return p;
}
double privateBytes(){PROCESS_MEMORY_COUNTERS m{};return K32GetProcessMemoryInfo(GetCurrentProcess(),&m,sizeof(m))?double(m.PagefileUsage):0;}
std::size_t vertices(const scene::CastScene& s){std::size_t n{};for(const auto& m:s.meshes)n+=m.vertices.size();return n;}

struct Fixture {std::string game;std::size_t hand=SIZE_MAX,body=SIZE_MAX,weapon=SIZE_MAX;std::string handError,bodyError,weaponError;};
int preference(const assets::Asset& a){
    const auto n=lowerText(a.name);int score{};
    if(a.role==assets::Role::ViewHands){
        for(const auto token:{"seal6_longsleeve","viewmodel_base_viewhands","viewhands_usmc","hands_shared","hand_707","masterchief_fp_fp","ghost_1p","swat_male_hands"})if(n.find(token)!=std::string::npos)score+=100;
    }else if(a.role==assets::Role::ViewWeapon){
        const auto category=lowerText(a.category);
        if(category.find("rifle")!=std::string::npos)score+=300;
        else if(category.find("pistol")!=std::string::npos||category=="smg")score+=200;
        else if(category.find("knife")!=std::string::npos||category=="melee")score+=100;
        for(const auto token:{"an94_view","ar_ak47","rifle_v_ak47","rif_ak47","rif_m4a1","ar_04","m4a1_1st","m4a1_1p","assault_rifle_"})if(n.find(token)!=std::string::npos)score+=150;
        for(const auto token:{"_dual","_hands","_hand_","_scope","_silencer","_mag","_reload","_1_"})if(n.find(token)!=std::string::npos)score-=75;
        if(n.find("_fp_0_")!=std::string::npos)score+=30;
    }else if(a.role==assets::Role::PlayerModel){
        for(const auto token:{"isa_smg_fb","ct_707","masterchief","playermode_swat_male","body_usmc","char_01","sk_pm"})if(n.find(token)!=std::string::npos)score+=100;
        for(const auto token:{"_head","_helmet","_exo","_zombie","elite","_lod1"})if(n.find(token)!=std::string::npos)score-=75;
    }
    return score;
}
bool weaponFixtureCandidate(const assets::Asset& a){
    // Catalog role alone is insufficient: some legacy attachments use a
    // viewmodel_ prefix. Exclude part folders without changing app discovery.
    for(const auto& part:a.path){const auto p=lowerText(part.string());if(p=="attachments"||p=="attachment")return false;}
    const auto category=lowerText(a.category);
    return category!="devices"&&category!="equipment";
}
std::size_t pick(AppState& app,const std::string& game,assets::Role role,std::string& error){
    std::vector<std::size_t> candidates;
    for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game==game&&a.role==role&&(role!=assets::Role::ViewWeapon||weaponFixtureCandidate(a)))candidates.push_back(i);}
    std::sort(candidates.begin(),candidates.end(),[&](auto i,auto j){const auto& a=app.assetCatalog.entries[i];const auto& b=app.assetCatalog.entries[j];const auto sa=preference(a),sb=preference(b);return sa==sb?a.path.native()<b.path.native():sa>sb;});
    for(auto i:candidates)try{
        auto s=scene::buildScene(cast::Document::load(app.assetCatalog.entries[i].path),false);
        if(s.meshes.empty()||!vertices(s))continue;
        if(role==assets::Role::ViewHands&&!scene::imported::viewLayout(s).usable())continue;
        if(role==assets::Role::PlayerModel){
            for(const auto part:characterAssemblyParts(app,app.assetCatalog.entries[i],true)){const auto& a=app.assetCatalog.entries[part];scene::appendRigModel(cast::Document::load(a.path),s,a.name);}
            int codSegments{};for(const auto* n:{"j_hip_le","j_hip_ri","j_knee_le","j_knee_ri","j_ankle_le","j_ankle_ri","j_elbow_le","j_elbow_ri","j_wrist_le","j_wrist_ri"})codSegments+=s.skeleton.boneByCanonicalName.contains(n);
            if(codSegments!=10&&!scene::imported::completeBodyLayout(scene::imported::bodyOrCodLayout(s.skeleton)))continue;
        }
        error.clear();return i;
    }catch(const std::exception& e){error=e.what();}
    if(error.empty())error=candidates.empty()?"No exported asset with this role":"No usable geometry/anatomy in exported candidates";
    return SIZE_MAX;
}
Fixture fixture(AppState& app,const std::string& game){Fixture f;f.game=game;f.hand=pick(app,game,assets::Role::ViewHands,f.handError);f.body=pick(app,game,assets::Role::PlayerModel,f.bodyError);f.weapon=pick(app,game,assets::Role::ViewWeapon,f.weaponError);return f;}
Fixture frozenFixture(AppState& app,const std::string& game,const std::filesystem::path& manifest){
    Fixture f;f.game=game;std::ifstream in(manifest);std::string line;
    while(std::getline(in,line)){
        // Frozen paths are tab-separated; CSV quoting is reserved for reports.
        const auto first=line.find('\t'),second=line.find('\t',first==std::string::npos?0:first+1);
        if(first==std::string::npos||second==std::string::npos||line.substr(0,first)!=game)continue;
        const auto role=line.substr(first+1,second-first-1);auto stored=line.substr(second+1);if(!stored.empty()&&stored.back()=='\r')stored.pop_back();
        const auto path=std::filesystem::path(stored);auto index=SIZE_MAX;
        if(!stored.empty())for(std::size_t i=0;i<app.assetCatalog.entries.size();++i)if(app.assetCatalog.entries[i].game==game&&app.assetCatalog.entries[i].path.lexically_normal()==path.lexically_normal()){index=i;break;}
        if(role=="hand"){f.hand=index;f.handError="Frozen viewhands fixture unavailable";}
        else if(role=="body"){f.body=index;f.bodyError="Frozen playermodel fixture unavailable";}
        else if(role=="weapon"){f.weapon=index;f.weaponError="Frozen weapon fixture unavailable";}
    }
    return f;
}
std::vector<std::string> scan(AppState& app,const std::vector<std::string>& selected={}){
    auto games=requiredGames;for(const auto& d:std::filesystem::directory_iterator(app.defaultSalukiDirectory))if(d.is_directory()){
        const auto name=d.path().filename().string();if(name.empty()||name.front()=='_')continue;
        if(std::find(games.begin(),games.end(),name)==games.end())games.push_back(name);
    }
    std::string error;
    for(const auto& game:games)if((selected.empty()||std::find(selected.begin(),selected.end(),game)!=selected.end())&&std::filesystem::is_directory(app.defaultSalukiDirectory/game))require(assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error),"Scan "+game+": "+error);
    return games;
}
void inventory(AppState& app,const std::filesystem::path& out,const std::vector<std::string>& games){
    std::ofstream rows(out/"fixtures.csv"),matrix(out/"matrix.csv"),assetsOut(out/"assets.csv"),frozen(out/"fixtures.tsv");
    rows<<"game,present,viewhands,playermodel,weapon,viewhands_reason,playermodel_reason,weapon_reason,body_animation_exports,models,hands,weapons\n";
    assetsOut<<"game,role,name,path,bytes,modified\n";
    for(const auto& a:app.assetCatalog.entries){std::error_code ec;assetsOut<<quoted(a.game)<<','<<quoted(assets::roleName(a.role))<<','<<quoted(a.name)<<','<<quoted(a.path.string())<<','<<std::filesystem::file_size(a.path,ec)<<','<<std::filesystem::last_write_time(a.path,ec).time_since_epoch().count()<<'\n';}
    for(const auto& g:games){const auto f=fixture(app,g);ensureBotAnimationCache(app,g);std::size_t b{},h{},w{};for(const auto& a:app.assetCatalog.entries)if(a.game==g){b+=a.role==assets::Role::PlayerModel;h+=a.role==assets::Role::ViewHands;w+=a.role==assets::Role::ViewWeapon;}
        const auto path=[&](auto i){return i<app.assetCatalog.entries.size()?app.assetCatalog.entries[i].path.string():std::string{};};
        frozen<<g<<"\thand\t"<<path(f.hand)<<'\n'<<g<<"\tbody\t"<<path(f.body)<<'\n'<<g<<"\tweapon\t"<<path(f.weapon)<<'\n';
        rows<<quoted(g)<<','<<std::filesystem::is_directory(app.defaultSalukiDirectory/g)<<','<<quoted(path(f.hand))<<','<<quoted(path(f.body))<<','<<quoted(path(f.weapon))<<','<<quoted(f.handError)<<','<<quoted(f.bodyError)<<','<<quoted(f.weaponError)<<','<<app.botAnimationCache[g].size()<<','<<b<<','<<h<<','<<w<<std::endl;
        std::cout<<"FIXTURE "<<g<<" body="<<b<<" hands="<<h<<" weapons="<<w<<" body_clips="<<app.botAnimationCache[g].size()<<std::endl;
        app.botAnimationCache.clear();
    }
    matrix<<"case,model_game,driver_game,lane\n";
    for(const auto& m:games)for(const auto& d:games)for(const auto* lane:{"body","viewmodel"})matrix<<quoted(m+"__"+d)<<','<<quoted(m)<<','<<quoted(d)<<','<<lane<<'\n';
}

struct Lane {
    std::string name,status="BLOCKED",reason,actualGame;bool ready{};
    double loadMs{},uploadMs{},maxLengthError{},maxMotion{},maxReplayError{},memoryStart{},memoryEnd{};
    std::vector<std::size_t> schedule;
};
void fail(Lane& lane,const std::string& why){lane.status="FAIL";if(!lane.reason.empty())lane.reason+="; ";lane.reason+=why;}
void geometry(const scene::CastScene& s){
    require(!s.meshes.empty()&&vertices(s)>0,"No mesh geometry");
    for(const auto& m:s.meshes){for(auto i:m.indices)require(i<m.vertices.size(),"Mesh index outside vertices: "+m.name);
        for(const auto& v:m.vertices){require(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z)&&std::isfinite(v.uv.x)&&std::isfinite(v.uv.y),"Nonfinite vertex/UV");
            float sum{};for(std::size_t k=0;k<4;++k){require(std::isfinite(v.weights[k])&&v.weights[k]>=0,"Invalid skin weight");if(m.skinned&&v.weights[k]>0)require(v.bones[k]<s.skeleton.bones.size(),"Skin index outside skeleton");sum+=v.weights[k];}if(m.skinned)require(sum>.001f,"Zero-weight skinned vertex");}
    }
}
double geometryDifference(const scene::CastScene& a,const scene::CastScene& b){
    require(a.skeleton.bones.size()==b.skeleton.bones.size()&&a.meshes.size()==b.meshes.size(),"Replay geometry/skeleton counts differ");double maximum{};std::string field;
    const auto delta=[&](double value,const std::string& label){if(value>maximum){maximum=value;field=label;}};
    for(std::size_t i=0;i<a.skeleton.bones.size();++i){require(a.skeleton.bones[i].name==b.skeleton.bones[i].name,"Replay bone order differs");for(int k=0;k<16;++k)delta(double(std::abs(a.skeleton.bones[i].inverseBind.v[k]-b.skeleton.bones[i].inverseBind.v[k])),"inverse bind "+a.skeleton.bones[i].name+" ["+std::to_string(k)+"]");}
    std::vector<bool> used(b.meshes.size());
    for(const auto& m:a.meshes){bool found{};for(std::size_t n=0;n<b.meshes.size()&&!found;++n){const auto& r=b.meshes[n];if(used[n]||m.name!=r.name||m.vertices.size()!=r.vertices.size()||m.indices!=r.indices||m.actorVariant!=r.actorVariant)continue;
        require(m.albedoPath==r.albedoPath&&m.normalPath==r.normalPath&&m.specularPath==r.specularPath&&m.metalnessPath==r.metalnessPath&&m.roughnessPath==r.roughnessPath,"Replay material connections differ");
        for(std::size_t v=0;v<m.vertices.size();++v){const auto label=m.name+" vertex "+std::to_string(v);require(m.vertices[v].bones==r.vertices[v].bones&&m.vertices[v].weights==r.vertices[v].weights,"Replay weights/bind indices differ: "+label);delta(double(scene::length(m.vertices[v].position-r.vertices[v].position)),label+" position");delta(double(scene::length(m.vertices[v].normal-r.vertices[v].normal)),label+" normal");delta(double(std::abs(m.vertices[v].uv.x-r.vertices[v].uv.x)),label+" UV.x");delta(double(std::abs(m.vertices[v].uv.y-r.vertices[v].uv.y)),label+" UV.y");}used[n]=true;found=true;
    }require(found,"Replay mesh missing: "+m.name);}
    require(a.attachments.size()==b.attachments.size(),"Replay attachment count differs");
    for(std::size_t i=0;i<a.attachments.size();++i){require(a.attachments[i].boneIndex==b.attachments[i].boneIndex,"Replay attachment bone differs");const auto x=a.attachments[i].localMatrix(),y=b.attachments[i].localMatrix();for(int k=0;k<16;++k)delta(double(std::abs(x.v[k]-y.v[k])),"attachment "+std::to_string(i)+" ["+std::to_string(k)+"]");}
    require(maximum<=.002,"Replay geometry/binds exceed tolerance: "+field+" delta="+std::to_string(maximum));return maximum;
}
void inspectClips(const scene::CastScene& s,Lane& lane,std::ofstream& report){
    geometry(s);require(!s.animations.empty(),"No bound animation clips");
    for(std::size_t i=0;i<s.animations.size();++i){const auto& a=s.animations[i];
        const unsigned steps=std::max(2u,unsigned(std::ceil(std::max(1u,a.durationFrames)/std::max(1.f,a.framerate)*120.f)));
        require(steps<120000,"Implausibly long clip");
        report<<"CLIP,"<<quoted(lane.name)<<','<<i<<','<<quoted(a.sourceName)<<','<<quoted(a.sourceGame)<<','<<a.durationFrames<<','<<a.framerate<<','<<a.tracks.size()<<','<<a.unmappedCurveCount<<','<<steps<<'\n';
        if(a.tracks.empty())continue;
        // CAST permits single-key static reference/hold poses with a zero range.
        if(!a.durationFrames){report<<"STATIC_REFERENCE,"<<quoted(lane.name)<<','<<i<<','<<quoted(a.sourceName)<<'\n';continue;}
        require(std::isfinite(a.framerate)&&a.framerate>0,"Invalid clip duration/rate: "+a.name);
        for(const auto& track:a.tracks)require(track.boneIndex<s.skeleton.bones.size(),"Track bone outside skeleton");
        if(std::find(lane.schedule.begin(),lane.schedule.end(),i)==lane.schedule.end())continue;
        const auto initial=s.samplePose(i,0);
        double clipMotion{};std::size_t motionBone{};float motionFrame{};
        for(unsigned k=0;k<=steps;++k){const float f=float(a.durationFrames)*float(k)/steps;const auto local=s.sampleLocalPose(i,f);const auto pose=s.globalPose(local);require(finite(pose),"Nonfinite animated pose: "+a.name);
            for(std::size_t b=0;b<pose.size();++b){const double motion=scene::length(scene::transformPoint(pose[b],{})-scene::transformPoint(initial[b],{}));lane.maxMotion=std::max(lane.maxMotion,motion);
                if(motion>clipMotion){clipMotion=motion;motionBone=b;motionFrame=f;}
                // Only retargeted rigid articulated segments have a length invariant.
                if(lane.name=="body"&&a.coldWarWorldPose&&s.skeleton.bones[b].parent>=0&&b<a.coldWarWorldPose->globalTranslation.size()&&!a.coldWarWorldPose->globalTranslation[b]){
                    const auto n=lowerText(s.skeleton.bones[b].name);if(n.find("elbow")!=std::string::npos||n.find("forearm")!=std::string::npos||n.find("knee")!=std::string::npos||n.find("calf")!=std::string::npos){
                        const double rest=scene::length(s.skeleton.bones[b].restLocal.position);const double delta=std::abs(scene::length(local[b].position)-rest);lane.maxLengthError=std::max(lane.maxLengthError,delta);require(delta<=std::max(.0001,rest*.001),"Rigid segment length changed: "+s.skeleton.bones[b].name);}
                }
            }
        }
        report<<"MOTION_PEAK,"<<quoted(lane.name)<<','<<i<<','<<quoted(a.sourceName)<<','<<clipMotion<<','<<motionFrame<<','<<motionBone<<','<<quoted(s.skeleton.bones[motionBone].name)<<'\n';
    }
    require(lane.maxMotion>.0001,"All exported animation clips are motionless");
}
scene::Mat4 viewCamera(const scene::CastScene& s,const std::vector<scene::Mat4>& p,scene::Vec3& eye){
    scene::Vec3 forward{1,0,0},up{0,0,1};eye={};const auto it=s.skeleton.boneByCanonicalName.find("tag_camera");
    if(it!=s.skeleton.boneByCanonicalName.end()&&it->second<p.size()){const auto& c=p[it->second];eye={c.v[12],c.v[13],c.v[14]};forward={c.v[0],c.v[1],c.v[2]};up={c.v[8],c.v[9],c.v[10]};}
    float fov=75;
#ifdef CADENCE_CROSS_CURRENT_REVIEW
    if(const char* value=std::getenv("CADENCE_REVIEW_FOV")){fov=std::stof(value);require(std::isfinite(fov)&&fov>1&&fov<175,"Invalid review FOV");}
#endif
    return scene::perspective(fov*scene::kPi/180,float(Width)/Height,.02f,10000)*scene::lookAtDirection(eye,forward,up);
}
scene::Mat4 bodyCamera(const scene::CastScene& s,scene::Vec3& eye){
    auto center=s.bounds.center();float extent=std::max(30.f,s.bounds.radius()*2);const auto forward=scene::imported::bodyOrCodLayout(s.skeleton).authoredForward;
#ifdef CADENCE_CROSS_CURRENT_REVIEW
    // Fixed wider diagnostic framing, not per-frame tracking: retain visible
    // root drift/jitter while including fallen bodies below standing bounds.
    // Opt-in keeps the original comparison images and camera unchanged.
    if(std::getenv("CADENCE_BODY_WIDE")){center.z-=s.bounds.radius()*.35f;extent*=1.65f;}
#endif
    eye=center+(forward+scene::cross(forward,scene::Vec3{0,0,1})*.5f)*extent+scene::Vec3{0,0,extent*.13f};
    return scene::perspective(50*scene::kPi/180,float(Width)/Height,.1f,10000)*scene::lookAt(eye,center,{0,0,1});
}
#ifdef CADENCE_CROSS_CURRENT_REVIEW
// Separate from raw-clip evidence: sparse ADS channels require gameplay's
// idle layer and camera anchoring. An isolated state avoids altering the take.
void captureGameplayAds(AppState& app,const scene::CastScene& s,std::size_t clip,float fraction,const std::string& action,const std::filesystem::path& file){
    const auto idle=findViewmodelClip(app,"idle");if(!idle||*idle>=s.animations.size())return;
    auto preview=std::make_unique<AppState>();preview->scene=s;preview->weaponProfile=app.weaponProfile;preview->weaponTiming=app.weaponTiming;
    preview->assetFirstPerson=true;preview->actorMode=true;preview->animationIndex=*idle;preview->animationFrame=0;
    preview->viewmodelAdsBaseAnimation=clip;preview->viewmodelAdsBaseFrame=s.animations[clip].durationFrames*fraction;
    preview->viewmodelAdsEngaged=true;preview->viewmodelAdsExiting=action=="ads_down";preview->gameplayAds=action!="ads_down";
    preview->adsCameraBlend=action=="ads_down"?1-fraction:fraction;preview->experimentalWeaponTurning=false;
    if(const auto retained=std::getenv("CADENCE_ADS_RETAIN_FIRE"))if(const auto fire=findViewmodelClip(app,std::string_view(retained)=="ads"?"ads_fire":"fire")){
        const auto& shot=s.animations[*fire];const float duration=shot.durationFrames/std::max(1.f,shot.framerate);
        // Controlled retained-layer composition, not input-clock simulation.
        const auto mode=std::getenv("CADENCE_ADS_RETAIN_FIRE_ADDITIVE")?scene::LayerMode::Additive:animationLayerMode(shot);
        const float phase=std::getenv("CADENCE_ADS_FIRE_SWEEP")?fraction:.5f;
        preview->runtimeLayers.push_back({scene::ActionRole::Fire,*fire,shot.durationFrames*phase,duration*phase,duration,1.f,0.f,.08f,mode,false});
        if(std::getenv("CADENCE_ADS_ACTIVE_FIRE")){
            preview->runtimeLayers.clear();preview->actionActive=true;preview->actionOverlay=true;
            preview->activeAction=scene::ActionRole::Fire;preview->actionAnimationIndex=*fire;
            preview->actionFrame=shot.durationFrames*phase;preview->actionElapsed=duration*phase;preview->actionDurationOverride=duration;
        }else if(std::getenv("CADENCE_ADS_FIRE_FADE")){
            // Explicit weight sweep: checks composition at the fade boundary,
            // not the live controller's clock or input dispatch.
            preview->runtimeLayers.back().weight=1-fraction;
        }
    }
    const auto pose=evaluateCurrentPose(*preview);require(preview->actorViewCameraValid,"Missing gameplay ADS camera");
    const auto& camera=preview->actorViewCamera;const scene::Vec3 eye{camera.v[12],camera.v[13],camera.v[14]},forward{camera.v[0],camera.v[1],camera.v[2]},up{camera.v[8],camera.v[9],camera.v[10]};
    const auto vp=scene::perspective(75*scene::kPi/180,float(Width)/Height,.02f,10000)*scene::lookAtDirection(eye,forward,up);
    app.renderer.setCameraPosition(eye);app.renderer.setFirstPersonProjection(true);configureReviewLighting(app);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
    app.renderer.render(s,pose,vp,Width,Height,false,false,false);std::string error;require(app.renderer.saveColorPng(file,error),error);
}
#endif
// Opt-in CPU reconstruction of this diagnostic draw, not visibility or visual
// certification. Keep the actual capture scene/geometry completely unchanged.
#ifdef CADENCE_CROSS_CURRENT_REVIEW
void auditViewVertices(const scene::CastScene& s,const std::vector<scene::Mat4>& pose,const scene::Mat4& vp,const std::filesystem::path& file){
    if(!std::getenv("CADENCE_VIEW_VERTEX_AUDIT"))return;
    const char* filter=std::getenv("CADENCE_VIEW_VERTEX_BONE");if(!filter)filter=std::getenv("CADENCE_ISOLATE_VIEW_BONE");
    const char* endpoint=std::getenv("CADENCE_VIEW_VERTEX_AXIS_END");
    const auto find=[&](const char* name){if(!name)return s.skeleton.bones.size();auto it=s.skeleton.boneByName.find(name);require(it!=s.skeleton.boneByName.end(),"Unknown vertex audit bone");return it->second;};
    const auto selected=find(filter),end=find(endpoint);const bool axisValid=selected<pose.size()&&end<pose.size();
    std::vector<scene::Mat4> skin;for(std::size_t b=0;b<pose.size();++b)skin.push_back(pose[b]*s.skeleton.bones[b].inverseBind);
    render::nativeViewhandSkinning(s,pose,skin);
    auto path=file;path.replace_extension("vertices.csv");std::ofstream rows(path);rows<<std::setprecision(9);
    rows<<"mesh_id,mesh,vertex,first_triangle,skinned,local_x,local_y,local_z,bind_x,bind_y,bind_z,world_x,world_y,world_z,clip_x,clip_y,clip_z,clip_w,pixel_x,pixel_y,selected_weight,weights,bind_axial,bind_radial,posed_axial,posed_radial\n";
    path=file;path.replace_extension("vertex-transforms.csv");std::ofstream matrices(path);matrices<<std::setprecision(9)<<"kind,index,name";for(int k=0;k<16;++k)matrices<<",m"<<k;matrices<<'\n';
    const auto matrix=[&](const char* kind,std::size_t id,const std::string& name,const scene::Mat4& m){matrices<<kind<<','<<id<<','<<quoted(name);for(float v:m.v)matrices<<','<<v;matrices<<'\n';};
    matrix("view_projection",0,"capture",vp);
    for(std::size_t b=0;b<skin.size();++b){matrix("skin_after_native_correction",b,s.skeleton.bones[b].name,skin[b]);matrix("pose",b,s.skeleton.bones[b].name,pose[b]);matrix("rest",b,s.skeleton.bones[b].name,s.skeleton.bones[b].restGlobal);}
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    for(std::size_t mi=0;mi<s.meshes.size();++mi){const auto& mesh=s.meshes[mi];if(mesh.viewmodelWeapon)continue;
        auto model=mesh.modelTransform;if(mesh.attachmentIndex>=0&&static_cast<std::size_t>(mesh.attachmentIndex)<s.attachments.size()){const auto& a=s.attachments[mesh.attachmentIndex];if(a.boneIndex<pose.size())model=pose[a.boneIndex]*a.localMatrix()*model;}
        matrix("draw_model",mi,mesh.name,model);matrix("mesh_bind_model",mi,mesh.name,mesh.modelTransform);
        std::vector<std::size_t> first(mesh.vertices.size(),std::numeric_limits<std::size_t>::max());for(std::size_t i=0;i<mesh.indices.size();++i)if(first.at(mesh.indices[i])==std::numeric_limits<std::size_t>::max())first[mesh.indices[i]]=i/3;
        for(std::size_t vi=0;vi<mesh.vertices.size();++vi){if(first[vi]==std::numeric_limits<std::size_t>::max())continue;const auto& v=mesh.vertices[vi];float selectedWeight=0,sum=0;scene::Vec3 p{};std::string weights;
            for(int k=0;k<4;++k)if(v.weights[k]>0){require(v.bones[k]<skin.size(),"Invalid vertex audit palette");if(v.bones[k]==selected)selectedWeight+=v.weights[k];sum+=v.weights[k];p+=scene::transformPoint(skin[v.bones[k]],v.position)*v.weights[k];weights+=std::to_string(v.bones[k])+":"+s.skeleton.bones[v.bones[k]].name+":"+std::to_string(v.weights[k])+";";}
            if(v.armSkinning>.5f&&mesh.skinned)p=scene::skinVertexPosition(v,skin);
            if(filter&&selectedWeight<=.01f)continue;
            if(!mesh.skinned){p=v.position;sum=1;}
            // GLSL uModel * skin * vec4(position,1): preserve homogeneous
            // weight sum rather than assuming normalized weights silently.
            const auto world=scene::volume_fit::vector(model,p)+scene::Vec3{model.v[12],model.v[13],model.v[14]}*sum;
            const auto clip=scene::volume_fit::vector(vp,world)+scene::Vec3{vp.v[12],vp.v[13],vp.v[14]}*sum;
            const float cw=vp.v[3]*world.x+vp.v[7]*world.y+vp.v[11]*world.z+vp.v[15]*sum;
            const auto bind=scene::transformPoint(mesh.modelTransform,v.position);
            float ba=nan,br=nan,pa=nan,pr=nan;if(axisValid){const auto project=[&](scene::Vec3 point,const scene::Mat4& origin,const scene::Mat4& tip,float& axial,float& radial){const auto o=scene::transformPoint(origin,{}),delta=point-o,dir=scene::transformPoint(tip,{})-o;const auto len=scene::length(dir);if(len>1e-6f){const auto unit=dir/len;axial=scene::dot(delta,unit);radial=scene::length(delta-unit*axial);}};project(bind,s.skeleton.bones[selected].restGlobal,s.skeleton.bones[end].restGlobal,ba,br);project(world,pose[selected],pose[end],pa,pr);}
            rows<<mi<<','<<quoted(mesh.name)<<','<<vi<<','<<first[vi]<<','<<mesh.skinned<<','<<v.position.x<<','<<v.position.y<<','<<v.position.z<<','<<bind.x<<','<<bind.y<<','<<bind.z<<','<<world.x<<','<<world.y<<','<<world.z<<','<<clip.x<<','<<clip.y<<','<<clip.z<<','<<cw<<','<<(std::abs(cw)>1e-8f?(clip.x/cw*.5f+.5f)*Width:nan)<<','<<(std::abs(cw)>1e-8f?(.5f-clip.y/cw*.5f)*Height:nan)<<','<<selectedWeight<<','<<quoted(weights)<<','<<ba<<','<<br<<','<<pa<<','<<pr<<'\n';
        }
    }
    require(rows.good()&&matrices.good(),"Unable to write vertex audit");
}
#endif
// Capture-only experiment: fixed donor palm and lengths, native shoulder anchor.
// Never part of normal pose evaluation or serialized replay data.
void diagnosticFixedPalm(const scene::CastScene& s,std::vector<scene::Mat4>& pose){
    require(s.codmRigAdapter&&!s.codmRigAdapter->positionLinks.empty(),"Fixed palm probe requires pose-only adapter");
    const auto original=pose;
    const auto position=[&](int b){return scene::transformPoint(original.at(b),{});};
    const auto descendant=[&](int b,int ancestor){for(size_t n=0;b>=0&&size_t(b)<s.skeleton.bones.size()&&n<s.skeleton.bones.size();++n){if(b==ancestor)return true;b=s.skeleton.bones[b].parent;}return false;};
    const auto rigid=[](scene::Vec3 from,scene::Vec3 to,scene::Vec3 origin,scene::Vec3 destination){
        from=scene::normalize(from);to=scene::normalize(to);const float d=scene::dot(from,to);
        require(d>-.9999f,"Antiparallel fixed-palm rotation requires stable pole");
        const auto c=scene::cross(from,to);const auto q=scene::normalize(scene::Quat{c.x,c.y,c.z,1+d});
        return scene::translation(destination)*scene::rotation(q)*scene::translation(origin*-1.f);
    };
    for(const auto* side:{"le","ri"}){
        const auto bone=[&](const std::string& name){const auto it=s.skeleton.boneByName.find(name);require(it!=s.skeleton.boneByName.end(),"Missing fixed-palm probe joint "+name);return int(it->second);};
        const int shoulder=bone(std::string("pose_only|j_shoulder_")+side),elbow=bone(std::string("pose_only|j_elbow_")+side),wrist=bone(std::string("pose_only|j_wrist_")+side);
        const auto oldShoulder=position(shoulder),oldElbow=position(elbow),end=position(wrist),start=position(bone(std::string("j_shoulder_")+side));
        const float upper=scene::length(oldElbow-oldShoulder),fore=scene::length(end-oldElbow),distance=scene::length(end-start);
        // Keep unreachable poses unchanged; never stretch a donor to reach.
        if(distance<=std::abs(upper-fore)+1e-5f||distance>=upper+fore-1e-5f)continue;
        const auto axis=(end-start)/distance;
        const float along=(upper*upper-fore*fore+distance*distance)/(2*distance);
        auto pole=oldElbow-start-axis*scene::dot(oldElbow-start,axis);
        if(scene::length(pole)<1e-5f)continue;
        const auto solved=start+axis*along+scene::normalize(pole)*std::sqrt(std::max(0.f,upper*upper-along*along));
        const auto upperMove=rigid(oldElbow-oldShoulder,solved-start,oldShoulder,start);
        const auto foreMove=rigid(end-oldElbow,end-solved,oldElbow,solved);
        for(size_t i=s.codmRigAdapter->firstBone;i<pose.size();++i){
            if(descendant(int(i),wrist))continue;
            if(descendant(int(i),elbow))pose[i]=foreMove*original[i];
            else if(descendant(int(i),shoulder))pose[i]=upperMove*original[i];
        }
        require(scene::length(scene::transformPoint(pose[wrist],{})-end)<1e-5f,"Probe moved wrist");
    }
}
// Capture-only sleeve-clearance probe. Rotate upper arm about fixed elbow;
// retain all donor dimensions and every forearm/palm/finger transform.
void diagnosticSleeveClearance(const scene::CastScene& s,std::vector<scene::Mat4>& pose,const std::filesystem::path& file){
    require(s.codmRigAdapter&&!s.codmRigAdapter->positionLinks.empty(),"Sleeve probe requires pose-only adapter");
    scene::Vec3 eye;const auto vp=viewCamera(s,pose,eye);
    auto tracePath=file;tracePath.replace_extension("sleeve.csv");std::ofstream trace(tracePath);
    trace<<"side,status,angle_radians,support_vertices\n";
    const auto descendant=[&](int b,int ancestor){for(size_t n=0;b>=0&&size_t(b)<s.skeleton.bones.size()&&n<s.skeleton.bones.size();++n){if(b==ancestor)return true;b=s.skeleton.bones[b].parent;}return false;};
    for(const auto* side:{"le","ri"}){
        const auto find=[&](const std::string& n){const auto i=s.skeleton.boneByName.find(n);require(i!=s.skeleton.boneByName.end(),"Missing sleeve probe joint");return int(i->second);};
        const auto shoulder=find(std::string("pose_only|j_shoulder_")+side),elbow=find(std::string("pose_only|j_elbow_")+side);
        const auto start=scene::transformPoint(pose[shoulder],{}),pivot=scene::transformPoint(pose[elbow],{});
        const auto restStart=scene::transformPoint(s.skeleton.bones[shoulder].restGlobal,{}),restEnd=scene::transformPoint(s.skeleton.bones[elbow].restGlobal,{});
        const auto restAxis=scene::normalize(restEnd-restStart);
        const float sign=std::string_view(side)=="ri"?-1.f:1.f;
        const scene::Vec3 inward{vp.v[3]+sign*vp.v[0],vp.v[7]+sign*vp.v[4],vp.v[11]+sign*vp.v[8]};
        const float planeLength=scene::length(inward);
        const auto direction=scene::normalize(start-pivot),outward=scene::normalize(inward)*-1.f;
        const auto tangent=outward-direction*scene::dot(outward,direction);
        if(scene::length(tangent)<1e-5f)continue;
        const auto axis=scene::normalize(scene::cross(direction,tangent));
        std::vector<scene::Vec3> rim;
        const auto skin=pose[shoulder]*s.skeleton.bones[shoulder].inverseBind;
        for(const auto& mesh:s.meshes)if(!mesh.viewmodelWeapon&&mesh.skinned&&mesh.attachmentIndex<0){
            std::vector<bool> rigid(mesh.vertices.size()),proximal(mesh.vertices.size()),support(mesh.vertices.size());
            for(size_t i=0;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];
                float weight=0;for(int k=0;k<4;++k)if(v.bones[k]==uint32_t(shoulder))weight+=v.weights[k];
                const auto bind=scene::transformPoint(mesh.modelTransform,v.position);
                rigid[i]=weight>.999f;
                proximal[i]=rigid[i]&&scene::dot(bind-restStart,restAxis)<scene::length(restEnd-restStart)*.1f;
            }
            // A triangle can cross a screen corner with all vertices outside.
            // Include the whole rigid face touching the proximal region, not
            // just its proximal vertices. Never propagate into the forearm.
            for(size_t i=0;i+2<mesh.indices.size();i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
                if(rigid.at(a)&&rigid.at(b)&&rigid.at(c)&&(proximal[a]||proximal[b]||proximal[c]))support[a]=support[b]=support[c]=true;
            }
            for(size_t i=0;i<mesh.vertices.size();++i)if(support[i])
                rim.push_back(scene::transformPoint(mesh.modelTransform,scene::transformPoint(skin,mesh.vertices[i].position)));
        }
        if(rim.empty())continue;
        const auto transform=[&](float angle){const float h=angle*.5f;return scene::translation(pivot)*scene::rotation(scene::Quat{axis.x*std::sin(h),axis.y*std::sin(h),axis.z*std::sin(h),std::cos(h)})*scene::translation(pivot*-1.f);};
        const auto clearance=[&](float angle){const auto move=transform(angle);float worst=-1e30f;for(auto p:rim){p=scene::transformPoint(move,p);worst=std::max(worst,(scene::dot(inward,p)+vp.v[15]+sign*vp.v[12])/planeLength);}return worst;};
        if(clearance(0)<=-.25f){trace<<side<<",clear,0,"<<rim.size()<<'\n';continue;}
        // Saturate at the permitted rotation when full clearance is not
        // reachable. Returning to zero here caused an abrupt pose jump at
        // the feasibility boundary. Never stretch/cull to satisfy the plane.
        constexpr float limit=.35f;const bool limited=clearance(limit)>-.25f;
        float low=0,high=limit;if(!limited)for(int n=0;n<18;++n){const float mid=(low+high)*.5f;if(clearance(mid)>-.25f)low=mid;else high=mid;}
        const auto move=transform(high);
        trace<<side<<','<<(limited?"limited":"corrected")<<','<<high<<','<<rim.size()<<'\n';
        for(size_t i=s.codmRigAdapter->firstBone;i<pose.size();++i)if(descendant(int(i),shoulder)&&!descendant(int(i),elbow))pose[i]=move*pose[i];
    }
}
// Capture-only feasibility probe: move proximal donor arm about a fixed
// wrist towards the bottom frustum plane. No mesh edits or runtime defaults.
void diagnosticForearmClearance(const scene::CastScene& s,std::vector<scene::Mat4>& pose,const std::filesystem::path& file){
    const char* value=std::getenv("CADENCE_FOREARM_CLEARANCE_PROBE");if(!value)return;
    require(s.codmRigAdapter!=nullptr,"Forearm probe requires donor adapter");
    if(std::string_view(value)=="rim"){
        scene::Vec3 eye;const auto vp=viewCamera(s,pose,eye);
        const scene::Vec3 inward{vp.v[3]+vp.v[1],vp.v[7]+vp.v[5],vp.v[11]+vp.v[9]};
        const float planeLength=scene::length(inward);require(planeLength>1e-5f,"Invalid rim plane");
        auto tracePath=file;tracePath.replace_extension("rim.csv");std::ofstream trace(tracePath);
        trace<<"side,points,angle,before,after\n";
        for(const char* side:{"L","R"}){
            const std::string prefix=std::string("pose_only|cs_hand_")+side;
            const auto ei=s.skeleton.boneByName.find(prefix+".forearm"),wi=s.skeleton.boneByName.find(prefix+".palm");
            require(ei!=s.skeleton.boneByName.end()&&wi!=s.skeleton.boneByName.end(),"Missing rim probe arm");
            const auto elbow=ei->second,wrist=wi->second;
            const auto bind=scene::transformPoint(s.skeleton.bones[elbow].restGlobal,{});
            const auto bindDelta=scene::transformPoint(s.skeleton.bones[wrist].restGlobal,{})-bind;
            const float armLength=scene::length(bindDelta);require(armLength>1e-5f,"Empty rim arm");
            const auto pivot=scene::transformPoint(pose[wrist],{});
            const auto direction=scene::normalize(scene::transformPoint(pose[elbow],{})-pivot);
            const auto axis=scene::cross(direction,scene::normalize(inward)*-1.f);
            if(scene::length(axis)<1e-5f)continue;
            std::vector<scene::Vec3> rim;
            for(const auto& mesh:s.meshes)if(!mesh.viewmodelWeapon&&mesh.skinned&&mesh.attachmentIndex<0)
                for(const auto& v:mesh.vertices){
                    float weight=0;for(int k=0;k<4;++k)if(v.bones[k]==elbow)weight+=v.weights[k];
                    const auto p=scene::transformPoint(mesh.modelTransform,v.position);
                    if(weight>.999f&&scene::dot(p-bind,bindDelta/armLength)<armLength*.005f)
                        rim.push_back(scene::transformPoint(pose[elbow]*s.skeleton.bones[elbow].inverseBind,p));
                }
            if(rim.empty())continue;
            const auto move=[&](float a){return scene::translation(pivot)*scene::rotation(scene::fromAxisAngle(scene::normalize(axis),a))*scene::translation(pivot*-1.f);};
            const auto clearance=[&](float a){float worst=-1e30f;const auto m=move(a);for(auto p:rim)worst=std::max(worst,(scene::dot(inward,scene::transformPoint(m,p))+vp.v[15]+vp.v[13])/planeLength);return worst;};
            const float before=clearance(0),target=-armLength*.01f;
            float angle=0;
            if(before>target){
                float best=before;for(int k=1;k<=60;++k){const float a=k*.01f,c=clearance(a);if(c<best){angle=a;best=c;}if(c<=target)break;}
                if(best<=target){float low=0,high=angle;for(int k=0;k<20;++k){const float mid=(low+high)*.5f;if(clearance(mid)>target)low=mid;else high=mid;}angle=high;}
            }
            trace<<side<<','<<rim.size()<<','<<angle<<','<<before<<','<<clearance(angle)<<'\n';
            pose[elbow]=move(angle)*pose[elbow];
        }
        return;
    }
    const bool automatic=std::string_view(value)=="auto";
    float angle=automatic?0.f:std::strtof(value,nullptr);require(std::isfinite(angle)&&angle>=0&&angle<=1.2f,"Invalid forearm probe angle");
    const auto find=[&](const char* name){auto i=s.skeleton.boneByName.find(name);require(i!=s.skeleton.boneByName.end(),"Missing forearm probe bone");return int(i->second);};
    const char* probeRoot=std::getenv("CADENCE_FOREARM_PROBE_ROOT");
    const char* probeElbow=std::getenv("CADENCE_FOREARM_PROBE_ELBOW");
    const char* probeWrist=std::getenv("CADENCE_FOREARM_PROBE_WRIST");
    const int shoulder=find(probeRoot?probeRoot:"pose_only|j_shoulder_ri"),elbow=find(probeElbow?probeElbow:"pose_only|j_elbow_ri"),wrist=find(probeWrist?probeWrist:"pose_only|j_wrist_ri");
    scene::Vec3 eye;const auto vp=viewCamera(s,pose,eye);
    const auto pivot=scene::transformPoint(pose[wrist],{}),direction=scene::normalize(scene::transformPoint(pose[elbow],{})-pivot);
    const scene::Vec3 inward{vp.v[3]+vp.v[1],vp.v[7]+vp.v[5],vp.v[11]+vp.v[9]};
    const auto axis=scene::cross(direction,scene::normalize(inward)*-1.f);if(scene::length(axis)<1e-5f)return;
    const auto transform=[&](float a){return scene::translation(pivot)*scene::rotation(scene::fromAxisAngle(scene::normalize(axis),a))*scene::translation(pivot*-1.f);};
    float radius=0,distance=0;
    if(automatic){
        const auto bindElbow=scene::transformPoint(s.skeleton.bones[elbow].restGlobal,{}),bindWrist=scene::transformPoint(s.skeleton.bones[wrist].restGlobal,{});
        const auto bindAxis=scene::normalize(bindWrist-bindElbow);
        for(const auto& mesh:s.meshes)if(!mesh.viewmodelWeapon&&mesh.skinned&&mesh.attachmentIndex<0)for(const auto& v:mesh.vertices){
            float weight=0;for(int k=0;k<4;++k)if(v.bones[k]==uint32_t(elbow))weight+=v.weights[k];if(weight<=.5f)continue;
            const auto p=scene::transformPoint(mesh.modelTransform,v.position)-bindElbow;
            radius=std::max(radius,scene::length(p-bindAxis*scene::dot(p,bindAxis)));
        }
        const auto proximal=scene::transformPoint(pose[shoulder],{});
        const auto clearance=[&](float a){const auto p=scene::transformPoint(transform(a),proximal),segment=p-pivot;
            const float t=std::clamp(scene::dot(eye-pivot,segment)/scene::dot(segment,segment),0.f,1.f);
            return scene::length(pivot+segment*t-eye);
        };
        distance=clearance(0);const float target=radius+.75f;
        if(radius>0&&distance<target){
            float low=0,high=.35f;
            if(clearance(high)>=target)for(int i=0;i<20;++i){const float mid=(low+high)*.5f;if(clearance(mid)<target)low=mid;else high=mid;}
            const float onset=std::clamp((target-distance)/.5f,0.f,1.f);
            angle=high*onset*onset*(3-2*onset);
        }
    }
    auto tracePath=file;tracePath.replace_extension("forearm-clearance.csv");std::ofstream trace(tracePath);
    trace<<"automatic,radius,camera_distance,angle\n"<<automatic<<','<<radius<<','<<distance<<','<<angle<<'\n';
    const auto move=scene::translation(pivot)*scene::rotation(scene::fromAxisAngle(scene::normalize(axis),angle))*scene::translation(pivot*-1.f);
    const auto descendant=[&](int b,int parent){for(size_t n=0;b>=0&&size_t(b)<s.skeleton.bones.size()&&n<s.skeleton.bones.size();++n){if(b==parent)return true;b=s.skeleton.bones[b].parent;}return false;};
    for(size_t b=s.codmRigAdapter->firstBone;b<pose.size();++b)if(descendant(int(b),shoulder)&&!descendant(int(b),wrist))pose[b]=move*pose[b];
}
void capture(AppState& app,const scene::CastScene& s,std::size_t clip,float fraction,bool body,const std::filesystem::path& file){
    std::string error;configureReviewLighting(app);app.renderer.setFirstPersonProjection(!body);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
    auto pose=body?evaluatedBodyPose(s,clip,s.animations[clip].durationFrames*fraction):s.samplePose(clip,s.animations[clip].durationFrames*fraction);
    if(!body&&std::getenv("CADENCE_FIXED_PALM_IK"))diagnosticFixedPalm(s,pose);
    if(!body&&std::getenv("CADENCE_SLEEVE_CLEARANCE"))diagnosticSleeveClearance(s,pose,file);
    if(!body)diagnosticForearmClearance(s,pose,file);
    scene::Vec3 eye;const auto vp=body?bodyCamera(s,eye):viewCamera(s,pose,eye);app.renderer.setCameraPosition(eye);app.renderer.render(s,pose,vp,Width,Height,false,false,false);
    // Isolate rotational skin collapse without changing the runtime renderer.
    // This CPU-only diagnostic is deliberately excluded from performance claims.
    if(!body&&std::getenv("CADENCE_DIAGNOSTIC_DQ_SKIN")){
        auto candidate=s;std::vector<scene::volume_fit::Bone> transforms;std::vector<scene::Mat4> matrices;
        for(std::size_t b=0;b<pose.size();++b){matrices.push_back(pose[b]*s.skeleton.bones[b].inverseBind);transforms.push_back(scene::volume_fit::prepare(matrices.back(),{}));}
        std::set<uint32_t> armBones;const auto anatomy=scene::imported::viewLayout(s);
        for(const auto& arm:anatomy.arms)if(arm.valid()){if(arm.shoulder>=0)armBones.insert(arm.shoulder);armBones.insert(arm.elbow);armBones.insert(arm.wrist);}
        std::size_t dqVertices{},linearVertices{};
        for(auto& mesh:candidate.meshes)if(!mesh.viewmodelWeapon&&mesh.skinned){
            for(auto& vertex:mesh.vertices){bool eligible=true;for(int k=0;k<4;++k)if(vertex.weights[k]>0&&!armBones.contains(vertex.bones[k]))eligible=false;
                if(!std::getenv("CADENCE_DQ_ARM_ONLY")||eligible){scene::Mat4 differential;vertex.position=scene::volume_fit::position(transforms,vertex.position,vertex.bones,vertex.weights,&differential);vertex.normal=scene::normalize(scene::volume_fit::vector(differential,vertex.normal));++dqVertices;}
                else{scene::Vec3 p{},n{};for(int k=0;k<4;++k)if(vertex.weights[k]>0){p+=scene::transformPoint(matrices[vertex.bones[k]],vertex.position)*vertex.weights[k];n+=scene::volume_fit::vector(matrices[vertex.bones[k]],vertex.normal)*vertex.weights[k];}vertex.position=p;vertex.normal=scene::normalize(n);++linearVertices;}
            }
            mesh.skinned=false;
        }
        if(std::getenv("CADENCE_DQ_ARM_ONLY")){auto counts=file;counts.replace_extension("skin-counts.txt");std::ofstream report(counts);report<<"diagnostic_only arm_dq="<<dqVertices<<" unchanged_linear="<<linearVertices<<'\n';}
        require(app.renderer.loadScene(candidate,error),error);app.renderer.render(candidate,pose,vp,Width,Height,false,false,false);
        require(app.renderer.saveColorPng(file,error),error);require(app.renderer.loadScene(s,error),error);
    }else require(app.renderer.saveColorPng(file,error),error);require(glGetError()==GL_NO_ERROR,"GL error while capturing");
#ifdef CADENCE_CROSS_CURRENT_REVIEW
    if(!body)auditViewVertices(s,pose,vp,file);
    if(!body&&std::getenv("CADENCE_VIEW_BONES")){
        auto path=file;path.replace_extension("bones.csv");std::ofstream bones(path);
        bones<<"bone,parent,rest_x,rest_y,rest_z,x,y,z\n";
        for(std::size_t b=0;b<pose.size();++b){const auto& bone=s.skeleton.bones[b];const auto rest=scene::transformPoint(bone.restGlobal,{}),p=scene::transformPoint(pose[b],{});
            bones<<quoted(bone.name)<<','<<bone.parent<<','<<rest.x<<','<<rest.y<<','<<rest.z<<','<<p.x<<','<<p.y<<','<<p.z<<'\n';}
    }
    if(!body&&std::getenv("CADENCE_VIEW_EDGES")){
        auto path=file;path.replace_extension("edges.csv");std::ofstream edges(path);
        edges<<"mesh,vertex_a,vertex_b,rest_length,posed_length,ratio,bones_a,bones_b\n";
        std::vector<scene::Mat4> skin;for(std::size_t b=0;b<pose.size();++b)skin.push_back(pose[b]*s.skeleton.bones[b].inverseBind);
        render::nativeViewhandSkinning(s,pose,skin);
        for(const auto& mesh:s.meshes){if(mesh.viewmodelWeapon)continue;
            std::vector<scene::Vec3> points;points.reserve(mesh.vertices.size());
            for(const auto& v:mesh.vertices){scene::Vec3 p{};for(int k=0;k<4;++k)if(v.weights[k]>0)
                p+=scene::transformPoint(skin.at(v.bones[k]),v.position)*v.weights[k];points.push_back(scene::transformPoint(mesh.modelTransform,p));}
            struct Edge{unsigned a,b;float rest,posed;};std::vector<Edge> ranked;
            for(std::size_t t=0;t+2<mesh.indices.size();t+=3)for(int k=0;k<3;++k){const auto a=mesh.indices[t+k],b=mesh.indices[t+(k+1)%3];
                const float rest=scene::length(scene::transformPoint(mesh.modelTransform,mesh.vertices[a].position)-scene::transformPoint(mesh.modelTransform,mesh.vertices[b].position));
                if(rest>.01f)ranked.push_back({a,b,rest,scene::length(points[a]-points[b])});}
            std::sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){return a.posed/a.rest>b.posed/b.rest;});
            for(std::size_t i=0;i<std::min<std::size_t>(5,ranked.size());++i){const auto& e=ranked[i];edges<<quoted(mesh.name)<<','<<e.a<<','<<e.b<<','<<e.rest<<','<<e.posed<<','<<e.posed/e.rest;
                for(auto vi:{e.a,e.b}){std::string weights;const auto& v=mesh.vertices[vi];for(int k=0;k<4;++k)if(v.weights[k]>0)weights+=s.skeleton.bones[v.bones[k]].name+":"+std::to_string(v.weights[k])+";";edges<<','<<quoted(weights);}edges<<'\n';}
        }
    }
    if(body){
        if(std::getenv("CADENCE_BODY_BONES")){
            auto path=file;path.replace_extension("bones.csv");std::ofstream bones(path);
            bones<<"bone,parent,source_bone,weighted_vertices,rest_x,rest_y,rest_z,x,y,z\n";
            std::vector<std::size_t> weighted(pose.size());
            for(const auto& mesh:s.meshes)for(const auto& v:mesh.vertices)for(std::size_t k=0;k<v.bones.size();++k)if(v.weights[k]>0&&v.bones[k]<weighted.size())++weighted[v.bones[k]];
            const auto& adapter=s.animations[clip].coldWarWorldPose;
            for(std::size_t b=0;b<pose.size();++b){const auto& bone=s.skeleton.bones[b];const auto rest=scene::transformPoint(bone.restGlobal,{}),p=scene::transformPoint(pose[b],{});
                bones<<quoted(bone.name)<<','<<bone.parent<<','<<(adapter&&b<adapter->sourceBones.size()?adapter->sourceBones[b]:-1)<<','<<weighted[b]<<','<<rest.x<<','<<rest.y<<','<<rest.z<<','<<p.x<<','<<p.y<<','<<p.z<<'\n';}
        }
        if(std::getenv("CADENCE_BODY_DETAIL")){
            const auto layout=scene::imported::bodyOrCodLayout(s.skeleton);
            if(layout.hand[0]>=0&&layout.hand[1]>=0){
                const auto left=scene::transformPoint(pose[layout.hand[0]],{}),right=scene::transformPoint(pose[layout.hand[1]],{});
                const auto center=(left+right)*.5f;const float extent=std::max(35.f,scene::length(left-right)*2.5f);
                const auto forward=layout.authoredForward,side=scene::cross(forward,scene::Vec3{0,0,1});
                // Diagnostic control only: retain the evaluated body/wrists but
                // remove finger rotation deltas to isolate skin/bind vs curl.
                auto restDigits=pose;std::vector<bool> digitBone(pose.size());
                for(const auto& hand:scene::imported::bodyFingerChains(s.skeleton))for(const auto& chain:hand)for(int b:chain)if(b>=0)digitBone[b]=true;
                for(std::size_t b=0;b<s.skeleton.bones.size();++b){const auto& bone=s.skeleton.bones[b];
                    if(bone.parent>=0&&(digitBone[b]||digitBone[bone.parent])){digitBone[b]=true;const auto& t=bone.restLocal;restDigits[b]=restDigits[bone.parent]*scene::trs(t.position,t.rotation,t.scale);}}
                for(int view=0;view<2;++view){const auto detailEye=center+(forward+side*(view?-1.4f:1.4f))*extent+scene::Vec3{0,0,extent*.4f};
                    const auto detailVp=scene::perspective(40*scene::kPi/180,float(Width)/Height,.1f,10000)*scene::lookAt(detailEye,center,{0,0,1});
                    app.renderer.setCameraPosition(detailEye);app.renderer.render(s,pose,detailVp,Width,Height,false,false,false);
                    const auto detailPath=file.parent_path()/(file.stem().string()+"_hands"+std::to_string(view)+".png");
                    require(app.renderer.saveColorPng(detailPath,error),error);require(glGetError()==GL_NO_ERROR,"GL error capturing body hand detail");
                    app.renderer.render(s,restDigits,detailVp,Width,Height,false,false,false);
                    require(app.renderer.saveColorPng(file.parent_path()/(file.stem().string()+"_rest_digits"+std::to_string(view)+".png"),error),error);
                }
                app.renderer.setCameraPosition(eye);
            }
        }
        auto path=file;path.replace_extension("pose.csv");std::ofstream joints(path);
        joints<<"rig,joint,x,y,z\n";
        const auto write=[&](const char* rig,const scene::Skeleton& skeleton,const std::vector<scene::Mat4>& p){
            const auto layout=scene::imported::bodyOrCodLayout(skeleton);
            const auto row=[&](const char* name,int b){if(b>=0&&std::size_t(b)<p.size()){const auto v=scene::transformPoint(p[b],{});joints<<rig<<','<<name<<','<<v.x<<','<<v.y<<','<<v.z<<'\n';}};
            row("pelvis",layout.pelvis);row("head",layout.head);
            for(int side=0;side<2;++side){row(side?"right_shoulder":"left_shoulder",layout.shoulder[side]);row(side?"right_elbow":"left_elbow",layout.elbow[side]);row(side?"right_wrist":"left_wrist",layout.hand[side]);}
        };
        write("target",s.skeleton,pose);
        if(const auto& a=s.animations[clip].coldWarWorldPose;a&&a->source)
            write("donor_scaled",a->source->skeleton,a->source->samplePose(a->animation,s.animations[clip].durationFrames*fraction));
    }
#endif
}
void actions(AppState& app,const scene::CastScene& s,Lane& lane,const std::filesystem::path& out,std::ofstream& report){
    if(lane.name=="viewmodel")if(const auto* flip=std::getenv("CADENCE_REVIEW_FLIP"))app.weaponProfile.flipViewmodel=std::string_view(flip)!="0";
    std::string uploadError;const auto uploadStarted=Clock::now();require(app.renderer.loadScene(s,uploadError),uploadError);lane.uploadMs=elapsed(uploadStarted);app.renderer.setViewmodelProjection(app.weaponProfile.viewmodelFovMultiplier,app.weaponProfile.flipViewmodel);
#ifdef CADENCE_CROSS_CURRENT_REVIEW
    if(lane.name=="viewmodel"&&std::getenv("CADENCE_LIVE_ADS_CLOCK")){
        const auto idle=findViewmodelClip(app,"idle");require(idle.has_value(),"Live clock requires idle");
        auto state=std::make_unique<AppState>();auto& p=*state;p.scene=s;p.weaponProfile=app.weaponProfile;p.weaponTiming=app.weaponTiming;
        p.assetCatalog=app.assetCatalog;p.selectedWeaponAsset=app.selectedWeaponAsset;p.assetFirstPerson=true;p.actorMode=true;p.gameplayLogic=true;p.playing=true;p.animationIndex=*idle;p.experimentalWeaponTurning=false;
        const bool pulloutSequence=std::getenv("CADENCE_LIVE_PULLOUT")!=nullptr;
        if(std::getenv("CADENCE_LIVE_CACHED_ADS"))if(const auto down=pairedAdsDown(p)){
            p.viewmodelAdsBaseAnimation=*down;p.viewmodelAdsBaseFrame=p.scene.animations[*down].durationFrames;
            p.viewmodelAdsTransitionElapsed=p.weaponTiming.adsOut;
        }
        if(pulloutSequence&&std::getenv("CADENCE_LIVE_SWITCH_HANDOFF"))p.weaponSwitchStage=2;
        p.gameplayAds=!pulloutSequence;p.gameplayAction=pulloutSequence?scene::ActionRole::Equip:scene::ActionRole::Aim;triggerGameplayAction(p);
        std::ofstream trace(out/"live-ads-clock.csv");trace<<"frame,time,action,action_frame,engaged,exiting,ads_frame,layers\n";
        const int lastFrame=std::getenv("CADENCE_LIVE_ADS_COMPLETE")?600:180;
        const int captureEvery=std::getenv("CADENCE_LIVE_EVERY_FRAME")?1:lastFrame>180?12:6;
        for(int frame=0;frame<=lastFrame;++frame){
            if(frame==(pulloutSequence?240:48)){p.gameplayAction=scene::ActionRole::Fire;triggerGameplayAction(p);}
            if(frame==50&&!pulloutSequence){p.gameplayAds=false;releaseViewmodelAim(p);}
            if(frame==(pulloutSequence?300:96)){p.gameplayAction=scene::ActionRole::Reload;triggerGameplayAction(p);}
            const float dt=1.f/120;p.gameplayClock+=dt;updateGameplay(p,dt);advanceLiveAnimationPlayback(p,dt);
            const auto pose=evaluateCurrentPose(p);require(p.actorViewCameraValid,"Live ADS camera unavailable");
            trace<<frame<<','<<p.gameplayClock<<','<<int(p.activeAction)<<','<<p.actionFrame<<','<<p.viewmodelAdsEngaged<<','<<p.viewmodelAdsExiting<<','<<p.viewmodelAdsBaseFrame<<','<<p.runtimeLayers.size()<<'\n';
            if(frame%captureEvery==0){const auto& c=p.actorViewCamera;const scene::Vec3 eye{c.v[12],c.v[13],c.v[14]},forward{c.v[0],c.v[1],c.v[2]},up{c.v[8],c.v[9],c.v[10]};
                app.renderer.setCameraPosition(eye);app.renderer.setFirstPersonProjection(true);configureReviewLighting(app);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
                app.renderer.render(s,pose,scene::perspective(75*scene::kPi/180,float(Width)/Height,.02f,10000)*scene::lookAtDirection(eye,forward,up),Width,Height,false,false,false);
                std::string error;require(app.renderer.saveColorPng(out/("live_ads_"+std::to_string(frame)+".png"),error),error);
                if(std::getenv("CADENCE_LIVE_ARM_AUDIT")&&frame<=8&&s.codmRigAdapter){
                    std::vector<scene::Mat4> skin;for(std::size_t b=0;b<pose.size();++b)skin.push_back(pose[b]*s.skeleton.bones[b].inverseBind);
                    render::nativeViewhandSkinning(s,pose,skin);
                    const auto vp=scene::perspective(75*scene::kPi/180,float(Width)/Height,.02f,10000)*scene::lookAtDirection(eye,forward,up);
                    scene::applySleeveClearance(s,pose,render::viewmodel_projection::adjusted(vp,75,p.weaponProfile.viewmodelFovMultiplier,p.weaponProfile.flipViewmodel),p.weaponProfile.flipViewmodel,skin);
                    std::ofstream audit(out/("live_arm_"+std::to_string(frame)+".csv"));audit<<"bone,rest_length,pose_length,corrected_length,scale_x,scale_y,scale_z,camera_distance,radius\n";
                    for(const auto& support:s.codmRigAdapter->forearmClearance)for(auto b:{support.elbow,support.wrist}){
                        const auto parent=b==support.elbow?support.shoulder:support.elbow;
                        const auto rest=scene::transformPoint(s.skeleton.bones[b].restGlobal,{}),restParent=scene::transformPoint(s.skeleton.bones[parent].restGlobal,{});
                        const auto at=scene::transformPoint(pose[b],{}),pa=scene::transformPoint(pose[parent],{});
                        const auto corrected=scene::transformPoint(skin[b],rest),correctedParent=scene::transformPoint(skin[parent],restParent);
                        const auto& m=skin[b];audit<<quoted(s.skeleton.bones[b].name)<<','<<scene::length(rest-restParent)<<','<<scene::length(at-pa)<<','<<scene::length(corrected-correctedParent)<<','<<scene::length(scene::Vec3{m.v[0],m.v[1],m.v[2]})<<','<<scene::length(scene::Vec3{m.v[4],m.v[5],m.v[6]})<<','<<scene::length(scene::Vec3{m.v[8],m.v[9],m.v[10]})<<','<<scene::length(corrected-eye)<<','<<support.radius<<'\n';
                    }
                }
            }
        }
    }
    // Diagnostic only: test authored hip/aim endpoints before adopting a
    // runtime fallback. This deliberately does not claim successful dispatch.
    if(lane.name=="viewmodel"&&std::getenv("CADENCE_AIM_ENDPOINT_CANDIDATE")){
        const auto idle=findViewmodelClip(app,"idle"),aim=findViewmodelClip(app,"ads_idle");
        require(idle&&aim,"Endpoint candidate requires authored idle and ads_idle");
        auto candidate=s;
        const auto hip=s.sampleLocalPose(*idle,0),ads=s.sampleLocalPose(*aim,0);
        require(hip.size()==ads.size(),"Endpoint pose sizes differ");
        for(bool down:{false,true}){
            scene::Animation a;a.name=down?"candidate_ads_down":"candidate_ads_up";
            a.sourceName=a.name;a.domain=scene::AnimationDomain::ViewModel;a.action=scene::ActionRole::Aim;
            a.durationFrames=30;a.framerate=30;a.viewmodelCameraReference=s.animations[*aim].viewmodelCameraReference;
            for(std::size_t bone=0;bone<hip.size();++bone){
                const auto& from=down?ads[bone]:hip[bone];const auto& to=down?hip[bone]:ads[bone];
                const auto& rest=s.skeleton.bones[bone].restLocal;
                const scene::Quat inverseRest{-rest.rotation.x,-rest.rotation.y,-rest.rotation.z,rest.rotation.w};
                scene::Track r;r.boneIndex=bone;r.property=scene::TrackProperty::Rotation;r.mode=scene::TrackMode::Relative;r.frames={0,30};r.rotationValues={scene::multiply(inverseRest,from.rotation),scene::multiply(inverseRest,to.rotation)};a.tracks.push_back(r);
                for(int axis=0;axis<3;++axis){
                    scene::Track t;t.boneIndex=bone;t.property=static_cast<scene::TrackProperty>(axis);t.mode=scene::TrackMode::Relative;t.frames={0,30};
                    const float origin=axis==0?rest.position.x:axis==1?rest.position.y:rest.position.z;
                    t.scalarValues={(axis==0?from.position.x:axis==1?from.position.y:from.position.z)-origin,(axis==0?to.position.x:axis==1?to.position.y:to.position.z)-origin};a.tracks.push_back(t);
                }
            }
            candidate.animations.push_back(std::move(a));
            for(int step=0;step<=20;++step)captureGameplayAds(app,candidate,candidate.animations.size()-1,step/20.f,down?"ads_down":"ads_up",out/(std::string("candidate_gameplay_")+(down?"ads_down_":"ads_up_")+std::to_string(step*5)+".png"));
        }
    }
    if(lane.name=="viewmodel"&&std::getenv("CADENCE_AIM_DISPATCH")){
        const auto idle=findViewmodelClip(app,"idle");require(idle&&*idle<s.animations.size(),"Aim dispatch requires idle");
        auto preview=std::make_unique<AppState>();preview->scene=s;preview->weaponProfile=app.weaponProfile;preview->weaponTiming=app.weaponTiming;
        preview->assetCatalog=app.assetCatalog;preview->selectedWeaponAsset=app.selectedWeaponAsset;
        preview->assetFirstPerson=true;preview->actorMode=true;preview->gameplayLogic=true;
        preview->animationIndex=*idle;preview->animationFrame=0;preview->gameplayAds=true;
        preview->gameplayAction=scene::ActionRole::Aim;triggerGameplayAction(*preview);
        std::ofstream dispatch(out/"aim-dispatch.txt");
        dispatch<<"Production triggerGameplayAction(Aim), initial dispatch only; not a full input/controller simulation.\n"
            <<"camera_scope="<<importedCameraScope(*preview)<<"\nengaged="<<preview->viewmodelAdsEngaged
            <<"\naction_active="<<preview->actionActive<<"\nbase="<<preview->viewmodelAdsBaseAnimation
            <<"\nstatus="<<preview->gameplayStatus<<'\n';
        if(std::getenv("CADENCE_FIRE_REFERENCE_AUDIT")){
            std::ofstream refs(out/"fire-reference.csv");refs<<"action,clip,reference,bone,position_error,angle_degrees\n";
            for(const auto* key:{"fire","ads_fire"}){
                const auto fire=findViewmodelClip(app,key),reference=findViewmodelClip(app,std::string_view(key)=="fire"?"idle":"ads_idle");
                if(!fire||!reference)continue;
                const auto first=s.sampleLocalPose(*fire,0),neutral=s.sampleLocalPose(*reference,0);
                for(std::size_t b=0;b<first.size();++b){
                    const auto& a=first[b].rotation;const auto& n=neutral[b].rotation;
                    const auto angle=2*std::acos(std::clamp(std::abs(a.x*n.x+a.y*n.y+a.z*n.z+a.w*n.w),0.f,1.f))*180/scene::kPi;
                    const auto position=scene::length(first[b].position-neutral[b].position);
                    if(position>.0001f||angle>.01f)refs<<key<<','<<quoted(s.animations[*fire].sourceName)<<','<<quoted(s.animations[*reference].sourceName)<<','<<quoted(s.skeleton.bones[b].name)<<','<<position<<','<<angle<<'\n';
                }
            }
        }
        if(std::getenv("CADENCE_AIM_INTERRUPTS")){
            const auto up=findViewmodelClip(*preview,"ads_up"),down=findViewmodelClip(*preview,"ads_down");
            require(up&&down,"ADS interruption diagnostic requires both transitions");
            for(const auto action:{scene::ActionRole::Fire,scene::ActionRole::Reload}){
                resetWeaponAimState(*preview);preview->actionActive=false;preview->animationIndex=*idle;
                startViewmodelAimClip(*preview,*up);preview->viewmodelAdsBaseFrame=preview->scene.animations[*up].durationFrames;
                preview->gameplayAction=action;triggerGameplayAction(*preview);
                const auto index=preview->actionAnimationIndex;const auto frame=preview->actionFrame;
                preview->gameplayAds=false;releaseViewmodelAim(*preview);
                const bool kept=preview->actionActive&&preview->activeAction==action&&preview->actionAnimationIndex==index&&preview->actionFrame==frame;
                dispatch<<"release_during_"<<(action==scene::ActionRole::Fire?"fire":"reload")<<"_kept="<<kept<<" down="<<(preview->viewmodelAdsBaseAnimation==*down)<<'\n';
            }
            // A fresh rig restores source clips first, then activates its saved
            // weapon profile. Remove generated clips to model that boundary.
            auto restored=std::make_unique<AppState>();restored->scene=s;
            std::erase_if(restored->scene.animations,[](const auto& a){return a.sourceName.find("__cadence_ads_")!=std::string::npos;});
            restored->assetCatalog=app.assetCatalog;restored->selectedWeaponAsset=app.selectedWeaponAsset;
            activateWeaponProfile(*restored,app.weaponProfile);
            dispatch<<"profile_activation_restores_up="<<bool(findViewmodelClip(*restored,"ads_up"))
                <<" down="<<bool(findViewmodelClip(*restored,"ads_down"))<<'\n';
        }
    }
#endif
    std::vector<std::pair<std::string,std::size_t>> choices;
    if(lane.name=="viewmodel"){
        for(const auto* key:{"idle","pullout","first_raise","putaway","ads_up","ads_idle","ads_down","fire","ads_fire","reload","reload_empty","sprint_in","sprint_loop","sprint_out","melee"}){
            const auto clip=findViewmodelClip(app,key);report<<"ACTION,viewmodel,"<<key<<','<<(clip?std::to_string(*clip):"MISSING")<<'\n';
            if(clip&&*clip<s.animations.size())choices.emplace_back(key,*clip);else if(std::string_view(key)=="idle"||std::string_view(key)=="pullout"||(!app.weaponProfile.meleeWeapon&&(std::string_view(key)=="fire"||std::string_view(key)=="reload")))fail(lane,std::string("Missing required view action ")+key);
        }
    }else{
        for(const auto motion:{scene::MotionRole::Idle,scene::MotionRole::Walk,scene::MotionRole::Run,scene::MotionRole::Sprint})for(const auto direction:{scene::Direction::Forward,scene::Direction::Backward,scene::Direction::Left,scene::Direction::Right}){
            scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=motion;q.stance=scene::Stance::Stand;q.weapon=app.botWeaponClass;q.direction=direction;
            const auto clip=botLocomotionAnimation(s,q,&app);const auto label="motion"+std::to_string(int(motion))+"_dir"+std::to_string(int(direction));report<<"ACTION,body,"<<label<<','<<(clip?std::to_string(*clip):"MISSING")<<'\n';if(clip&&*clip<s.animations.size())choices.emplace_back(label,*clip);
        }
        for(const auto stance:{scene::Stance::Crouch,scene::Stance::Prone}){scene::AnimationQuery q;q.motion=scene::MotionRole::Walk;q.stance=stance;q.weapon=app.botWeaponClass;q.direction=scene::Direction::Forward;const auto c=botLocomotionAnimation(s,q,&app);const auto label="stance"+std::to_string(int(stance));report<<"ACTION,body,"<<label<<','<<(c?std::to_string(*c):"MISSING")<<'\n';if(c)choices.emplace_back(label,*c);}
        for(const auto action:{scene::ActionRole::Death,scene::ActionRole::Fire,scene::ActionRole::Reload}){scene::AnimationQuery q;q.action=action;q.motion=scene::MotionRole::Unknown;q.weapon=app.botWeaponClass;q.allowContextual=action==scene::ActionRole::Death;
#ifdef CADENCE_CROSS_CURRENT_REVIEW
            if(action!=scene::ActionRole::Death)q=bot_animation::actionQuery(action,app.botWeaponClass,scene::Stance::Stand);
#endif
            auto c=cachedBotAction(app,s,q);const auto label="action"+std::to_string(int(action));
#ifdef CADENCE_CROSS_CURRENT_REVIEW
            if(action==scene::ActionRole::Death&&std::getenv("CADENCE_DEFAULT_DEATH")){
                const auto pool=cadence::prepareBotDeaths(s);
                cadence::BotDeathHistory history;cadence::BotDeathRequest request;
                request.motion=scene::MotionRole::Idle;request.stance=scene::Stance::Stand;
                request.direction=scene::Direction::Forward;request.weapon=app.botWeaponClass;
                c=cadence::selectBotDeath(pool,history,request);
                report<<"ACTION_SCOPE,body,"<<label<<",PRODUCTION_DEFAULT_DEATH_SELECTION\n";
                if(!c)fail(lane,"No eligible default standing death");
            }
#endif
            report<<"ACTION,body,"<<label<<','<<(c?std::to_string(*c):"MISSING")<<'\n';
            if(action==scene::ActionRole::Death){
                const auto livePool=cadence::prepareBotDeaths(s);
                const bool eligible=c&&std::any_of(livePool.begin(),livePool.end(),[&](const auto& choice){return choice.animation==*c;});
                report<<"ACTION_SCOPE,body,"<<label<<','<<(eligible?"DEFAULT_DEATH_POOL_MEMBER":"RAW_CLIP_ONLY_NOT_DEFAULT_DEATH")<<'\n';
                report<<"DEFAULT_DEATH_POOL,body,count,"<<livePool.size()<<'\n';
            }
            if(c)choices.emplace_back(label,*c);}
        for(std::size_t i=0;i<s.animations.size();++i)if(s.animations[i].motion==scene::MotionRole::Jump||s.animations[i].motion==scene::MotionRole::Climb){choices.emplace_back("traversal",i);break;}
        if(choices.empty())fail(lane,"No selected body actions");
    }
    for(const auto& [name,index]:choices)if(std::find(lane.schedule.begin(),lane.schedule.end(),index)==lane.schedule.end())lane.schedule.push_back(index);
    // Inspect every clip's metadata and sample complete selected actions at 120 Hz.
    std::ofstream muzzles(out/(lane.name+"_muzzles.csv"));muzzles<<"action,phase,x,y,z,prepared_anchors\n";
    for(const auto& [name,index]:choices){if(name.find("_dir")!=std::string::npos&&!name.ends_with("_dir1"))continue;
        for(float fraction:{0.f,.5f,.95f}){const auto p=lane.name=="body"?evaluatedBodyPose(s,index,s.animations[index].durationFrames*fraction):s.samplePose(index,s.animations[index].durationFrames*fraction);const auto muzzle=scene::resolveMuzzlePosition(s,p);if(muzzle){require(std::isfinite(muzzle->x)&&std::isfinite(muzzle->y)&&std::isfinite(muzzle->z),"Nonfinite muzzle");muzzles<<name<<','<<fraction<<','<<muzzle->x<<','<<muzzle->y<<','<<muzzle->z<<','<<s.muzzleAnchors.size()<<'\n';}}
        for(float fraction:{0.f,.5f,.95f})capture(app,s,index,fraction,lane.name=="body",out/(lane.name+"_"+name+"_"+std::to_string(int(fraction*100))+".png"));}
#ifdef CADENCE_CROSS_CURRENT_REVIEW
    // Separate filenames preserve the original three-phase evidence. Opt in
    // to a 21-pose action strip; this is still pose sampling, not controller
    // transition playback or a claim of visual approval.
    if(const char* dense=std::getenv("CADENCE_DENSE_ACTION")){
        const int percentStep=std::getenv("CADENCE_DENSE_EVERY_PERCENT")?1:5;
        for(const auto& [name,index]:choices)if(std::string_view(dense)=="all"||name==dense){
            for(int percent=0;percent<=100;percent+=percentStep){
                const float fraction=percent/100.f;
                const auto suffix=std::to_string(percent)+".png";
                capture(app,s,index,fraction,lane.name=="body",out/("dense_"+lane.name+"_"+name+"_"+suffix));
                if(lane.name=="viewmodel"&&(name=="ads_up"||name=="ads_down"))
                    captureGameplayAds(app,s,index,fraction,name,out/("dense_gameplay_"+name+"_"+suffix));
            }
        }
    }
    if(lane.name=="viewmodel"&&std::getenv("CADENCE_GAMEPLAY_ADS"))for(const auto& [name,index]:choices)if(name=="ads_up"||name=="ads_down")for(float fraction:{0.f,.5f,.95f})
        captureGameplayAds(app,s,index,fraction,name,out/("gameplay_"+name+"_"+std::to_string(int(fraction*100))+".png"));
    if(lane.name=="body"&&std::getenv("CADENCE_BODY_FIRE_GAIT")){
        std::ofstream audit(out/"fire_gait_layers.csv");audit<<"direction,phase,unowned_bones,max_local_delta\n";
        const auto fire=cachedBotAction(app,s,bot_animation::actionQuery(scene::ActionRole::Fire,app.botWeaponClass,scene::Stance::Stand));
        require(fire&&s.animations[*fire].durationFrames>0,"Missing nonstatic torso fire");
        std::vector<bool> owned(s.skeleton.bones.size());for(const auto& track:s.animations[*fire].tracks)if(track.ownsLayer&&track.boneIndex<owned.size())owned[track.boneIndex]=true;
        for(const auto direction:{scene::Direction::Forward,scene::Direction::Backward,scene::Direction::Left,scene::Direction::Right}){
            scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Run;q.stance=scene::Stance::Stand;q.weapon=app.botWeaponClass;q.direction=direction;
            const auto base=botLocomotionAnimation(s,q,&app);require(base.has_value(),"Missing gait for fire-layer check");
            for(int step=0;step<=20;++step){const float t=step/20.f;
                std::vector<scene::PoseSlot> slots(2);slots[1].nodes.push_back({*fire,t*s.animations[*fire].durationFrames,1.f,scene::LayerMode::Override,true});
                const float frame=t*s.animations[*base].durationFrames;
                const auto plain=s.sampleLocalPose(*base,frame),layered=s.sampleLocalPoseSlots(*base,frame,slots);
                float delta=0;size_t unowned=0;
                for(size_t b=0;b<owned.size();++b)if(!owned[b]){++unowned;const auto a=scene::trs(plain[b].position,plain[b].rotation,plain[b].scale),c=scene::trs(layered[b].position,layered[b].rotation,layered[b].scale);for(int k=0;k<16;++k)delta=std::max(delta,std::abs(a.v[k]-c.v[k]));}
                audit<<int(direction)<<','<<step*5<<','<<unowned<<','<<delta<<'\n';require(unowned>0&&delta<1e-4f,"Torso fire changed unowned local bones");
                auto pose=s.samplePoseSlots(*base,frame,slots);scene::imported::suppressBodyRootMotion(s.skeleton,pose,s.animations[*base].sourceGame=="pointblank");
                scene::Vec3 eye;const auto vp=bodyCamera(s,eye);app.renderer.setCameraPosition(eye);app.renderer.render(s,pose,vp,Width,Height,false,false,false);
                std::string error;require(app.renderer.saveColorPng(out/("fire_gait_dir"+std::to_string(int(direction))+"_"+std::to_string(step*5)+".png"),error),error);
            }
        }
    }
#endif
    if(lane.schedule.empty())lane.schedule.push_back(0);
}
void replay(AppState& app,const scene::CastScene& live,Lane& lane,const std::filesystem::path& out){
    take::Take recorded;recorded.sampleRate=60;
    if(lane.name=="viewmodel"){captureTakeActorManifest(app);recorded.actor=app.recordedTake.actor;recorded.boneCount=live.skeleton.bones.size();}
    else{captureTakeBotManifest(app);recorded.botActor=app.recordedTake.botActor;recorded.botBoneCount=live.skeleton.bones.size();recorded.botCount=1;recorded.boneCount=1;}
    for(int i=0;i<=120;++i){take::Sample sample;sample.time=i/60.f;const auto c=lane.schedule[(i/30)%lane.schedule.size()];const auto& a=live.animations[c];const auto frame=std::fmod(i/60.f*a.framerate,std::max(1.f,float(a.durationFrames)));const auto pose=lane.name=="body"?evaluatedBodyPose(live,c,frame):live.samplePose(c,frame);
        if(lane.name=="viewmodel")sample.pose=pose;else{sample.pose={scene::Mat4::identity()};take::RecordedActorState b;b.id=1;b.modelVariant=0;b.pose=pose;b.primaryClip=a.sourceName;b.primaryFrame=i/60.f*a.framerate;sample.bots.push_back(std::move(b));}recorded.samples.push_back(std::move(sample));}
    std::vector<std::size_t> reviewSamples;
    if(std::getenv("CADENCE_REPLAY_ALL_ACTIONS"))for(const auto clip:lane.schedule)for(float fraction:{0.f,.15f,.5f,.95f}){
        const auto& animation=live.animations[clip];const float frame=animation.durationFrames*fraction;
        take::Sample sample;sample.time=float(recorded.samples.size())/60.f;
        const auto pose=lane.name=="body"?evaluatedBodyPose(live,clip,frame):live.samplePose(clip,frame);
        if(lane.name=="viewmodel")sample.pose=pose;
        else{sample.pose={scene::Mat4::identity()};take::RecordedActorState bot;bot.id=1;bot.modelVariant=0;bot.pose=pose;bot.primaryClip=animation.sourceName;bot.primaryFrame=frame;sample.bots.push_back(std::move(bot));}
        reviewSamples.push_back(recorded.samples.size());recorded.samples.push_back(std::move(sample));
    }
    std::string error;take::Take loaded;require(take::save(recorded,out/(lane.name+".c_dm"),error),error);require(take::load(out/(lane.name+".c_dm"),loaded,error),error);
    for(std::size_t i=0;i<recorded.samples.size();++i){const auto& a=lane.name=="viewmodel"?recorded.samples[i].pose:recorded.samples[i].bots.front().pose;const auto& b=lane.name=="viewmodel"?loaded.samples[i].pose:loaded.samples[i].bots.front().pose;require(a.size()==b.size(),"Recorded pose count differs");for(std::size_t j=0;j<a.size();++j)for(int k=0;k<16;++k)require(a[j].v[k]==b[j].v[k],"Recorded poses changed on disk");}
    for(float t:{0.f,.137f,.713f,1.511f,1.99f}){const auto sample=loaded.interpolatedSample(t);require(finite(lane.name=="viewmodel"?sample.pose:sample.bots.front().pose),"Nonfinite fractional replay");}
    // Immediate, disk-loaded, then disabled-catalog reconstruction use production routes.
    std::ofstream restoreStages(out/(lane.name+"_restore_stages.csv"));
    restoreStages<<"stage,status,detail\n";
    const auto restore=[&](const take::ActorManifest& manifest,const char* stage){
        try{
        const bool restored=lane.name=="viewmodel"?restoreTakeActor(app,manifest,live.skeleton.bones.size(),error):restoreTakeBotActor(app,manifest,live.skeleton.bones.size(),error);
        require(restored,"Replay restore: "+error);
        const auto& rebuilt=lane.name=="viewmodel"?app.scene:*app.botActorScene;lane.maxReplayError=std::max(lane.maxReplayError,geometryDifference(live,rebuilt));
        restoreStages<<stage<<",PASS,geometry_matches\n";restoreStages.flush();
        }catch(const std::exception& failure){
            restoreStages<<stage<<",FAIL,"<<quoted(failure.what())<<'\n';restoreStages.flush();
            throw std::runtime_error(std::string("Replay stage ")+stage+": "+failure.what());
        }
    };
    restore(lane.name=="viewmodel"?recorded.actor:recorded.botActor,"immediate");restore(lane.name=="viewmodel"?loaded.actor:loaded.botActor,"disk_loaded");
    auto catalog=std::move(app.assetCatalog);try{
        restore(lane.name=="viewmodel"?loaded.actor:loaded.botActor,"catalog_disabled");
        if(std::getenv("CADENCE_REPLAY_COLD_SOURCE_CACHE")){
            // Explicitly test reference rediscovery, not just a warmed lookup.
            // This is not a claim that every application cache is cold.
            app.animationSourceSkeletons.clear();
            restore(lane.name=="viewmodel"?loaded.actor:loaded.botActor,"catalog_disabled_cold_source_cache");
        }
    }catch(...){app.assetCatalog=std::move(catalog);throw;}app.assetCatalog=std::move(catalog);
    // Reconstructed takes need recorded matrices, not reselected live animation clips.
    const auto& rebuilt=lane.name=="viewmodel"?app.scene:*app.botActorScene;
    const auto recordedPose=lane.name=="viewmodel"?loaded.samples.front().pose:loaded.samples.front().bots.front().pose;
    scene::Vec3 eye;const auto vp=lane.name=="body"?bodyCamera(live,eye):viewCamera(live,recordedPose,eye);
    const auto pixels=[&](const scene::CastScene& s,const char* name){require(app.renderer.loadScene(s,error),error);app.renderer.setCameraPosition(eye);app.renderer.setFirstPersonProjection(lane.name=="viewmodel");app.renderer.render(s,recordedPose,vp,Width,Height,false,false,false);std::vector<std::uint8_t> p;require(app.renderer.readColorRgba(p,error),error);require(app.renderer.saveColorPng(out/(lane.name+"_"+name+".png"),error),error);return p;};
    const auto a=pixels(live,"live"),b=pixels(rebuilt,"restored");require(a.size()==b.size()&&!a.empty(),"Replay pixel count differs");int maximum{};std::size_t visible{};for(std::size_t i=0;i<a.size();++i)maximum=std::max(maximum,std::abs(int(a[i])-int(b[i])));for(std::size_t i=4;i<a.size();i+=4)visible+=a[i]!=a[0]||a[i+1]!=a[1]||a[i+2]!=a[2];require(visible>100,"Actor has no visible rendered coverage");require(maximum<=1,"Replay capture pixels differ: "+std::to_string(maximum));
    // Optional broad reconstruction check: all selected clips, including poses
    // where generated proximal geometry is visible. This is still discrete pose
    // parity, not a substitute for controller-transition visual review.
    std::ofstream parity;if(!reviewSamples.empty()){parity.open(out/(lane.name+"_replay_pose_parity.csv"));parity<<"sample,clip,phase,max_pixel_error\n";}
    for(std::size_t i=0;i<reviewSamples.size();++i){const auto& sample=loaded.samples.at(reviewSamples[i]);const auto& pose=lane.name=="viewmodel"?sample.pose:sample.bots.front().pose;
        scene::Vec3 poseEye;const auto poseVp=lane.name=="body"?bodyCamera(live,poseEye):viewCamera(live,pose,poseEye);
        const auto renderPose=[&](const scene::CastScene& s,const char* label){require(app.renderer.loadScene(s,error),error);app.renderer.setCameraPosition(poseEye);app.renderer.setFirstPersonProjection(lane.name=="viewmodel");app.renderer.render(s,pose,poseVp,Width,Height,false,false,false);std::vector<std::uint8_t> rgba;require(app.renderer.readColorRgba(rgba,error),error);require(app.renderer.saveColorPng(out/(lane.name+"_replay_"+std::to_string(i)+"_"+label+".png"),error),error);return rgba;};
        const auto livePixels=renderPose(live,"live"),restoredPixels=renderPose(rebuilt,"restored");require(livePixels.size()==restoredPixels.size()&&!livePixels.empty(),"Replay action pixel count differs");int delta=0;for(std::size_t k=0;k<livePixels.size();++k)delta=std::max(delta,std::abs(int(livePixels[k])-int(restoredPixels[k])));
        constexpr float phases[]{0.f,.15f,.5f,.95f};parity<<reviewSamples[i]<<','<<quoted(live.animations[lane.schedule[i/4]].sourceName)<<','<<phases[i%4]<<','<<delta<<'\n';parity.flush();require(delta<=1,"Replay action pixels differ: "+std::to_string(delta));
    }
}
struct Metrics {std::vector<double> total,pose,submit,gpu;std::size_t hitches{};double seconds{},memoryStart{},memoryEnd{};};
double percentile(std::vector<double> v,double p){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[std::min(v.size()-1,std::size_t((v.size()-1)*p))];}
void timing(AppState& app,const scene::CastScene* body,const scene::CastScene* view,const Lane& bl,const Lane& vl,double seconds,int repeats,const std::filesystem::path& out){
    std::string error;scene::CastScene empty;const bool combined=body&&view;const std::string name=combined?"combined":body?"body":"viewmodel";
    require(app.renderer.loadScene(view?*view:empty,error),error);require(app.renderer.loadAuxiliaryScenes(nullptr,nullptr,body,error),error);
    configureReviewLighting(app);app.renderer.setFirstPersonProjection(view!=nullptr);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
    scene::Vec3 bodyEye;const auto bodyVp=body?bodyCamera(*body,bodyEye):scene::Mat4::identity();std::vector<int> variants{0};
    std::ofstream stats(out/(name+"_timings.csv"));stats<<"repetition,wall_seconds,frames,completed_p50_ms,completed_p95_ms,completed_p99_ms,completed_max_ms,pose_p50_ms,pose_p95_ms,submit_p50_ms,gpu_delayed_p50_ms,over_5ms,memory_start_bytes,memory_end_bytes\n";
    std::ofstream frames(out/(name+"_frames.csv"));frames<<"repetition,frame,simulated_seconds,pose_ms,submit_ms,completed_ms,gpu_delayed_ms\n";
    const auto draw=[&](std::uint64_t frame,Metrics* m){const auto start=Clock::now();std::vector<scene::Mat4> vpPose;std::vector<std::vector<scene::Mat4>> bodyPoses;
        const double simulated=double(frame)/60;const auto sample=[&](const scene::CastScene& s,const Lane& lane){const auto c=lane.schedule[(frame/120)%lane.schedule.size()];const auto& a=s.animations[c];return s.samplePose(c,float(std::fmod(simulated*a.framerate,std::max(1.,double(a.durationFrames)))));};
        if(view)vpPose=sample(*view,vl);if(body){const auto c=bl.schedule[(frame/120)%bl.schedule.size()];const auto& a=body->animations[c];bodyPoses.push_back(evaluatedBodyPose(*body,c,float(std::fmod(simulated*a.framerate,std::max(1.,double(a.durationFrames))))));}
        require(finite(vpPose)&&(!body||finite(bodyPoses.front())),"Nonfinite measured pose");
        scene::Vec3 eye;auto vp=view?viewCamera(*view,vpPose,eye):bodyVp;if(!view)eye=bodyEye;
        if(combined){const auto layout=scene::imported::bodyOrCodLayout(body->skeleton);const auto direction=scene::normalize(scene::Vec3{vp.v[0],vp.v[4],vp.v[8]});(void)direction;
            // Put one actor at a fixed offset in front of the native view camera.
            const auto cameraBone=view->skeleton.boneByCanonicalName.find("tag_camera");scene::Vec3 forward{1,0,0};if(cameraBone!=view->skeleton.boneByCanonicalName.end()){const auto& c=vpPose[cameraBone->second];forward=scene::normalize(scene::Vec3{c.v[0],c.v[1],c.v[2]});}
            const auto offset=eye+forward*280-scene::Vec3{0,0,75}-body->bounds.center();for(auto& p:bodyPoses.front()){p.v[12]+=offset.x;p.v[13]+=offset.y;p.v[14]+=offset.z;}
        }
        const double poseMs=elapsed(start);app.renderer.setCameraPosition(eye);const auto submit=Clock::now();app.renderer.render(view?*view:empty,vpPose,vp,Width,Height,false,false,false,body,&bodyPoses,&variants,nullptr,nullptr,view!=nullptr);const double submitMs=elapsed(submit);glFinish();const double completed=elapsed(start);require(glGetError()==GL_NO_ERROR,"Measured GL error");
        if(m){m->pose.push_back(poseMs);m->submit.push_back(submitMs);m->total.push_back(completed);m->gpu.push_back(app.renderer.gpuFrameMilliseconds());m->hitches+=completed>5;if(frame%60==0)frames<<m->seconds<<','<<frame<<','<<simulated<<','<<poseMs<<','<<submitMs<<','<<completed<<','<<app.renderer.gpuFrameMilliseconds()<<'\n';}
    };
    for(int repetition=0;repetition<repeats;++repetition){
        for(std::uint64_t f=0;f<120;++f)draw(f,nullptr);
        Metrics m;m.seconds=repetition;m.memoryStart=privateBytes();const auto start=Clock::now();std::uint64_t f{};
        do{draw(f++,&m);if(f%120==0)glfwPollEvents();}while(elapsed(start)<seconds*1000);
        m.seconds=elapsed(start)/1000;m.memoryEnd=privateBytes();
        stats<<repetition<<','<<m.seconds<<','<<f<<','<<percentile(m.total,.5)<<','<<percentile(m.total,.95)<<','<<percentile(m.total,.99)<<','<<*std::max_element(m.total.begin(),m.total.end())<<','<<percentile(m.pose,.5)<<','<<percentile(m.pose,.95)<<','<<percentile(m.submit,.5)<<','<<percentile(m.gpu,.5)<<','<<m.hitches<<','<<m.memoryStart<<','<<m.memoryEnd<<std::endl;
        std::cout<<"TIMING "<<name<<" repetition="<<repetition<<" p50="<<percentile(m.total,.5)<<" p99="<<percentile(m.total,.99)<<" ms"<<std::endl;
    }
}
void row(std::ofstream& out,const std::string& model,const std::string& driver,const Lane& lane){
    out<<quoted(model)<<','<<quoted(driver)<<','<<lane.name<<','<<quoted(lane.status)<<','<<quoted(lane.reason)<<','<<quoted(lane.actualGame)<<','<<lane.loadMs<<','<<lane.uploadMs<<','<<lane.maxMotion<<','<<lane.maxLengthError<<','<<lane.maxReplayError<<",PENDING,PENDING_BASELINE\n";out.flush();
}
void run(AppState& app,const Fixture& model,const Fixture& driver,const std::filesystem::path& out,double seconds,int repeats){
    std::filesystem::create_directories(out);std::ofstream detail(out/"clips.csv"),result(out/"result.csv"),metadata(out/"setup.txt");
    result<<"model_game,driver_game,lane,functional_status,reason,actual_driver_game,load_ms,upload_ms,max_motion,max_rigid_length_error,replay_error,visual_status,performance_status\n";
#ifdef CADENCE_CROSS_CURRENT_REVIEW
    metadata<<"Cadence current-source diagnostic build; production version must be identified from the accompanying build/review record.\n";
#else
    metadata<<"Cadence v274 benchmark; production application source unchanged from v272.\n";
#endif
    metadata<<"GPU: "<<glGetString(GL_RENDERER)<<"\nGL: "<<glGetString(GL_VERSION)<<"\n960x540, VSync off, fullbright studio, no weather, one body and/or one viewmodel.\nFixed 1/60 animation step; repetitions measured in actual wall seconds. Synchronous glFinish completed-frame times; GPU timestamps are delayed, not additive.\nFresh process per pairing; OS file cache is not flushed. Visual/material/effect/transition review and v272 comparative timing remain separate gates.\n";
    if(std::getenv("CADENCE_CROSS_LIT"))metadata<<"Override: lit beauty review, directional sun plus ambient; shadows disabled.\n";
    if(const auto* flip=std::getenv("CADENCE_REVIEW_FLIP"))metadata<<"Override: viewmodel flip="<<(std::string_view(flip)!="0")<<" (diagnostic only).\n";
    if(std::getenv("CADENCE_REVIEW_UNCACHED_POSES"))metadata<<"Override: pose upload cache disabled (diagnostic only).\n";
    const auto path=[&](auto i){return i<app.assetCatalog.entries.size()?app.assetCatalog.entries[i].path.string():std::string{};};metadata<<"Model: "<<path(model.body)<<"\nRequested hands: "<<path(model.hand)<<"\nWeapon: "<<path(driver.weapon)<<"\nRequested driver: "<<driver.game<<"\n";
    app.autoPlayerModel=false;app.deferSceneUpload=true;app.enemyBotCount=1;app.botTeamSide=0;app.botFactionModelVariety=false;app.botUseSalukiWeaponPool=false;app.botGame=model.game;app.botAnimationGame=driver.game;app.botSystemMode=0;
    Lane vl;vl.name="viewmodel";Lane bl;bl.name="body";std::optional<scene::CastScene> view,body;
    if(model.hand==SIZE_MAX||driver.weapon==SIZE_MAX)vl.reason=model.hand==SIZE_MAX?model.handError:driver.weaponError;
    else try{
        const auto start=Clock::now();app.selectedBaseAsset=model.hand;app.selectedWeaponAsset=driver.weapon;equipViewWeapon(app,driver.weapon);vl.loadMs=elapsed(start);
        if(std::getenv("CADENCE_SLEEVE_BIND_CANDIDATE")&&model.game=="iw_sp"&&driver.game=="bocw_sp"){
            auto& rig=app.scene;const auto& names=rig.skeleton.boneByName;
            const auto spine=names.at("j_spineupper"),left=names.at("j_clavicle_le"),right=names.at("j_clavicle_ri");
            for(auto& mesh:rig.meshes){bool shoulder=false,bodySkin=false;
                for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0){const auto& name=rig.skeleton.bones[v.bones[k]].name;
                    shoulder=shoulder||name.starts_with("j_shouldertwist");bodySkin=bodySkin||name.starts_with("j_hip")||name.starts_with("j_knee");}
                if(!shoulder||bodySkin||mesh.viewmodelWeapon)continue;
                const auto lp=scene::transformPoint(rig.skeleton.bones[left].restGlobal,{}),rp=scene::transformPoint(rig.skeleton.bones[right].restGlobal,{});
                for(auto& v:mesh.vertices){const auto side=scene::length(v.position-lp)<scene::length(v.position-rp)?left:right;
                    for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]==spine)v.bones[k]=side;}
            }
            metadata<<"DIAGNOSTIC ONLY: sleeve spine weights follow nearest clavicle; not a production fix or replay certification.\n";
        }
        require(!app.scene.skeleton.bones.empty(),"Viewmodel assembly produced no skeleton");vl.actualGame=app.scene.viewHandsDriverGame.empty()?driver.game:app.scene.viewHandsDriverGame;view=app.scene;vl.ready=true;vl.status="CHECKED_PENDING_VISUAL";metadata<<"Actual hands after native selection: "<<path(app.selectedBaseAsset)<<"\n";
#ifdef CADENCE_CROSS_CURRENT_REVIEW
        if(const char* skinPath=std::getenv("CADENCE_FIT_VIEW_SKIN")){
            auto skin=scene::buildScene(cast::Document::load(skinPath),false);
            if(std::getenv("CADENCE_COMPONENT_PROXIMAL_SKIN"))metadata<<"DIAGNOSTIC ONLY: isolated arm ancestor slots retained in memory: "<<scene::imported::retainArmComponentSupport(skin)<<"; original export unchanged.\n";
            if(std::getenv("CADENCE_MINOR_ANCESTOR_SKIN"))metadata<<"DIAGNOSTIC ONLY: minor shared ancestor residual removed from "<<scene::imported::diagnosticMinorAncestorWeights(skin)<<" arm vertices; no production change.\n";
            if(std::getenv("CADENCE_CODM_TWIST_NAMES")){
                for(auto& bone:skin.skeleton.bones){
                    if(bone.name=="b_LeftForeArmRoll")bone.name="L ForeTwist";
                    else if(bone.name=="b_RightForeArmRoll")bone.name="R ForeTwist";
                }
                metadata<<"DIAGNOSTIC ONLY: CODM forearm-roll helper labels recognized by skin-only fitting; exports unchanged.\n";
            }
            if(std::getenv("CADENCE_DB_TWIST_NAMES")){
                for(auto& bone:skin.skeleton.bones)for(const auto* side:{"L","R"}){
                    const std::string prefix=std::string("SkelBone_")+side+"_ForeTwist";
                    if(bone.name.starts_with(prefix))bone.name=std::string(side)+" ForeTwist"+bone.name.substr(prefix.size());
                }
                metadata<<"DIAGNOSTIC ONLY: Dirty Bomb foretwist helper names normalized for fitting.\n";
            }
            if(std::getenv("CADENCE_KEEP_PROXIMAL_SKIN")){
                const auto layout=scene::imported::viewLayout(skin);
                std::set<uint32_t> ancestors;
                for(const auto& arm:layout.arms)if(arm.shoulder>=0){int p=skin.skeleton.bones[arm.shoulder].parent;
                    for(std::size_t n=0;p>=0&&n<skin.skeleton.bones.size();++n){ancestors.insert(static_cast<uint32_t>(p));p=skin.skeleton.bones[p].parent;}}
                std::size_t changed{};
                for(auto& mesh:skin.meshes)if(!mesh.viewmodelWeapon)for(auto& v:mesh.vertices){
                    const auto point=scene::transformPoint(mesh.modelTransform,v.position);int nearest=-1;float distance=std::numeric_limits<float>::max();
                    for(const auto& arm:layout.arms)if(arm.shoulder>=0){const float d=scene::length(point-scene::transformPoint(skin.skeleton.bones[arm.shoulder].restGlobal,{}));if(d<distance){distance=d;nearest=arm.shoulder;}}
                    if(nearest>=0)for(int k=0;k<4;++k)if(v.weights[k]>0&&ancestors.contains(v.bones[k])){v.bones[k]=static_cast<uint32_t>(nearest);++changed;}
                }
                metadata<<"DIAGNOSTIC ONLY: proximal ancestor weights retained on nearest shoulder: "<<changed<<"; not a production fix.\n";
            }
            const auto nativeRollReference=std::getenv("CADENCE_NATIVE_SHOULDER_ROLL")?std::optional<scene::CastScene>(*view):std::nullopt;
            const auto nativeWristReference=std::getenv("CADENCE_NATIVE_WRIST_ROLL")?std::optional<scene::CastScene>(*view):std::nullopt;
            const auto nativeProximalReference=(std::getenv("CADENCE_NATIVE_PROXIMAL_SPATIAL")||std::getenv("CADENCE_NATIVE_PROXIMAL_EXTENT")||std::getenv("CADENCE_SHOULDERLESS_EXTENT_FIT"))?std::optional<scene::CastScene>(*view):std::nullopt;
            const auto foretwistReference=std::getenv("CADENCE_FORETWIST_SPATIAL")?std::optional<scene::CastScene>(*view):std::nullopt;
            std::string fitError;require(scene::imported::fitForeignViewSkin(*view,skin,fitError,std::getenv("CADENCE_CLAVICLE_SKIN")!=nullptr,std::getenv("CADENCE_UPPER_HELPER_SKIN")!=nullptr,std::getenv("CADENCE_ARM_TRANSPORT")!=nullptr,std::getenv("CADENCE_SPATIAL_ELBOW")!=nullptr,std::getenv("CADENCE_FIT_ONLY_ELBOW")!=nullptr,std::getenv("CADENCE_BEND_PLANE")!=nullptr,std::getenv("CADENCE_NATIVE_UPPER_WEIGHTS")!=nullptr,std::getenv("CADENCE_UNIFORM_ARM_RADII")!=nullptr,std::getenv("CADENCE_STRAIGHT_PROXIMAL_CARRIER")!=nullptr,std::getenv("CADENCE_PROXIMAL_CONTINUITY")!=nullptr,true,std::getenv("CADENCE_NATIVE_ARM_ENVELOPE")!=nullptr),fitError);
            if(std::getenv("CADENCE_NATIVE_ARM_ENVELOPE"))metadata<<"DIAGNOSTIC ONLY: native arm radial envelope; palm/finger fit unchanged; not production/replay certification.\n";
            if(nativeWristReference){
                const auto layout=scene::imported::viewLayout(*view);
                for(int side=0;side<2;++side){const auto& arm=layout.arms[side];
                    const auto helper=view->skeleton.boneByName.find(side?"j_wristfronttwist1_ri":"j_wristfronttwist1_le");
                    if(!arm.valid()||helper==view->skeleton.boneByName.end())continue;
                    const auto weight=[&](){double sum=0;for(const auto& mesh:view->meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.bones[k]==helper->second)sum+=v.weights[k];return sum;};
                    const auto before=weight();scene::imported::transferNativeForearmRoll(*nativeWristReference,view->meshes,arm.elbow,arm.wrist,int(helper->second));
                    metadata<<"DIAGNOSTIC ONLY: native wrist roll side="<<side<<" before="<<before<<" after="<<weight()<<"; original native distribution, no production/replay approval.\n";
                }
            }
            if(nativeProximalReference&&std::getenv("CADENCE_NATIVE_PROXIMAL_SPATIAL")){
                for(const auto& arm:scene::imported::viewLayout(*view).arms)if(arm.valid())
                    metadata<<"DIAGNOSTIC ONLY native proximal changed="<<scene::imported::nativeProximalDiagnostic(*nativeProximalReference,view->meshes,arm.shoulder,arm.elbow,true,std::getenv("CADENCE_NATIVE_PROXIMAL_COHERENT")!=nullptr,std::getenv("CADENCE_NATIVE_PROXIMAL_TRANSITION")!=nullptr,std::getenv("CADENCE_NATIVE_PROXIMAL_LOCAL")!=nullptr)<<'\n';
            }
            if(nativeProximalReference&&std::getenv("CADENCE_NATIVE_PROXIMAL_EXTENT")){
                for(const auto& arm:scene::imported::viewLayout(*view).arms)if(arm.valid()){
                    const auto extent=scene::imported::nativeProximalExtent(*nativeProximalReference,arm.shoulder,arm.elbow);
                    if(!extent){metadata<<"DIAGNOSTIC ONLY native extent unavailable\n";continue;}
                    const auto result=scene::imported::continueShoulderRimDiagnostic(view->meshes,view->skeleton,{arm.shoulder,arm.elbow,arm.wrist,false},true,true,*extent);
                    metadata<<"DIAGNOSTIC ONLY native extent="<<*extent<<" added="<<result.addedVertices<<" reason="<<result.reason<<'\n';
                }
            }
            if(nativeProximalReference&&std::getenv("CADENCE_SHOULDERLESS_EXTENT_FIT")){
                // Diagnostic only: fit a retained donor sleeve to the actual
                // native arm extent instead of projecting an extra upper arm
                // behind a shoulderless view rig. No topology/UV/weight edits.
                const auto from=scene::imported::viewLayout(skin),to=scene::imported::viewLayout(*view);
                for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
                    if(!a.valid()||a.shoulder<0||!b.valid()||b.shoulder>=0)continue;
                    const auto origin=scene::transformPoint(view->skeleton.bones[b.elbow].restGlobal,{});
                    const auto fore=scene::transformPoint(view->skeleton.bones[b.wrist].restGlobal,{})-origin;
                    const float len=scene::length(fore);if(len<1e-4f)continue;const auto axis=fore/len;
                    const auto station=[&](scene::Vec3 p){return scene::dot(p-origin,axis)/len;};
                    const auto armOnly=[&](const scene::Vertex& v){float total=0;for(int k=0;k<4;++k)if(v.weights[k]>0){if(v.bones[k]!=uint32_t(b.elbow)&&v.bones[k]!=uint32_t(b.wrist))return false;total+=v.weights[k];}return std::abs(total-1.f)<1e-4f;};
                    const auto minimum=[&](const scene::CastScene& model){float result=std::numeric_limits<float>::max();for(const auto& mesh:model.meshes)if(!mesh.viewmodelWeapon)for(auto index:mesh.indices){const auto& v=mesh.vertices[index];if(armOnly(v))result=std::min(result,station(scene::transformPoint(mesh.modelTransform,v.position)));}return result;};
                    const float sourceMin=minimum(*view),targetMin=minimum(*nativeProximalReference),cuff=.8f;
                    if(!std::isfinite(sourceMin)||!std::isfinite(targetMin)||sourceMin>=targetMin-.05f||targetMin>=cuff-.05f)continue;
                    const float sourceSpan=cuff-sourceMin,targetSpan=cuff-targetMin,ratio=targetSpan/sourceSpan,m1=1.f/ratio;
                    // Monotone Hermite segment, matching identity derivative
                    // at the cuff. Reject extreme compression rather than fold.
                    if(ratio<=0||ratio>=1||m1>3)continue;
                    std::size_t changed=0;
                    for(auto& mesh:view->meshes)if(!mesh.viewmodelWeapon){const auto inverse=scene::inverseAffine(mesh.modelTransform);
                        for(auto& v:mesh.vertices)if(armOnly(v)){const auto point=scene::transformPoint(mesh.modelTransform,v.position);const float t=station(point);if(t>=cuff)continue;
                            const float u=std::clamp((t-sourceMin)/sourceSpan,0.f,1.f);
                            const float mapped=targetMin+targetSpan*((m1-1)*u*u*u+(1-m1)*u*u+u);
                            const float derivative=ratio*(3*(m1-1)*u*u+2*(1-m1)*u+1);
                            require(derivative>0&&std::isfinite(mapped),"Shoulderless extent diagnostic folded");
                            v.position=scene::transformPoint(inverse,point+axis*((mapped-t)*len));
                            auto normal=scene::volume_fit::vector(mesh.modelTransform,v.normal);
                            normal=normal+axis*(scene::dot(normal,axis)*(1/derivative-1));
                            v.normal=scene::normalize(scene::volume_fit::vector(inverse,normal));++changed;
                        }
                    }
                    metadata<<"DIAGNOSTIC ONLY shoulderless extent side="<<side<<" source="<<sourceMin<<" target="<<targetMin<<" changed="<<changed<<"; cuff/weights/UV/topology unchanged, visual review required.\n";
                }
            }
            if(std::getenv("CADENCE_NATIVE_FORETWIST_CANDIDATE")){
                // Diagnostic only: test native helper transport independently
                // of bind fitting. Never alter source assets or animation tracks.
                const auto from=scene::imported::viewLayout(skin),to=scene::imported::viewLayout(*view);
                std::size_t changed{};
                for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];if(!a.valid()||!b.valid())continue;
                    const auto position=[](const scene::Skeleton& rig,int bone){return scene::transformPoint(rig.bones[bone].restGlobal,{});};
                    const auto axis=position(skin.skeleton,a.wrist)-position(skin.skeleton,a.elbow),nativeAxis=position(view->skeleton,b.wrist)-position(view->skeleton,b.elbow);
                    std::array<std::map<uint32_t,float>,32> nativeField;std::array<float,32> nativeTotal{};
                    const auto axial=[&](scene::Vec3 p){return std::clamp(scene::dot(p-position(view->skeleton,b.elbow),nativeAxis)/scene::dot(nativeAxis,nativeAxis),0.f,1.f)*31.f;};
                    if(foretwistReference)for(const auto& mesh:foretwistReference->meshes)if(!mesh.viewmodelWeapon)for(const auto& vertex:mesh.vertices){
                        const float x=axial(scene::transformPoint(mesh.modelTransform,vertex.position));const int lo=int(x),hi=std::min(lo+1,31);const float f=x-lo;
                        for(int k=0;k<4;++k){const auto bone=vertex.bones[k];if(vertex.weights[k]<=0||bone>=view->skeleton.bones.size())continue;
                            const auto& joint=view->skeleton.bones[bone];if(bone!=b.elbow&&bone!=b.wrist&&!(joint.parent==b.elbow&&assets::imported::lower(joint.name).find("foretwist")!=std::string::npos))continue;
                            nativeField[lo][bone]+=vertex.weights[k]*(1-f);nativeTotal[lo]+=vertex.weights[k]*(1-f);nativeField[hi][bone]+=vertex.weights[k]*f;nativeTotal[hi]+=vertex.weights[k]*f;
                        }
                    }
                    for(std::size_t bone=0;bone<skin.skeleton.bones.size();++bone){if(!scene::imported::viewForearmTwist(assets::imported::lower(skin.skeleton.bones[bone].name),side))continue;
                        const float t=std::clamp(scene::dot(position(skin.skeleton,int(bone))-position(skin.skeleton,a.elbow),axis)/scene::dot(axis,axis),0.f,1.f);
                        int helper=-1;float best=2.f;
                        std::vector<std::pair<float,int>> anchors{{0.f,b.elbow},{1.f,b.wrist}};
                        for(std::size_t n=0;n<view->skeleton.bones.size();++n){const auto& candidate=view->skeleton.bones[n];if(candidate.parent!=b.elbow||assets::imported::lower(candidate.name).find("foretwist")==std::string::npos)continue;
                            const float nt=scene::dot(position(view->skeleton,int(n))-position(view->skeleton,b.elbow),nativeAxis)/scene::dot(nativeAxis,nativeAxis);
                            if(nt>=0.f&&nt<=1.f)anchors.emplace_back(nt,int(n));
                            if(std::abs(nt-t)<best){best=std::abs(nt-t);helper=int(n);}}
                        if(helper<0)continue;
                        int lower=helper,upper=helper;float mix=0.f;
                        if(std::getenv("CADENCE_FORETWIST_INTERPOLATE")){
                            std::sort(anchors.begin(),anchors.end());
                            for(std::size_t n=1;n<anchors.size();++n)if(t<=anchors[n].first&&anchors[n].first-anchors[n-1].first>1e-5f){lower=anchors[n-1].second;upper=anchors[n].second;mix=std::clamp((t-anchors[n-1].first)/(anchors[n].first-anchors[n-1].first),0.f,1.f);break;}
                        }
                        for(const auto& original:skin.meshes)for(auto& fitted:view->meshes)if(original.name==fitted.name&&original.vertices.size()==fitted.vertices.size())for(std::size_t v=0;v<original.vertices.size();++v){float weight{};for(int k=0;k<4;++k)if(original.vertices[v].bones[k]==bone)weight+=original.vertices[v].weights[k];if(weight<=0)continue;
                            auto& vertex=fitted.vertices[v];std::map<uint32_t,float> weights;for(int k=0;k<4;++k)weights[vertex.bones[k]]+=vertex.weights[k];
                            const float takenElbow=std::min(weights[b.elbow],weight*(1-t)),takenHand=std::min(weights[b.wrist],weight*t);
                            weights[b.elbow]-=takenElbow;weights[b.wrist]-=takenHand;
                            const float x=axial(scene::transformPoint(fitted.modelTransform,vertex.position));int lo=int(x),hi=std::min(lo+1,31);while(lo>=0&&nativeTotal[lo]<=0)--lo;while(hi<32&&nativeTotal[hi]<=0)++hi;
                            if(foretwistReference&&(lo>=0||hi<32)){
                                if(lo<0)lo=hi;if(hi>=32)hi=lo;const float f=lo==hi?0.f:std::clamp((x-lo)/(hi-lo),0.f,1.f);
                                for(const auto& [joint,w]:nativeField[lo])weights[joint]+=(takenElbow+takenHand)*(1-f)*w/nativeTotal[lo];
                                for(const auto& [joint,w]:nativeField[hi])weights[joint]+=(takenElbow+takenHand)*f*w/nativeTotal[hi];
                            }else{weights[lower]+=(takenElbow+takenHand)*(1-mix);weights[upper]+=(takenElbow+takenHand)*mix;}
                            std::vector<std::pair<uint32_t,float>> sorted(weights.begin(),weights.end());std::sort(sorted.begin(),sorted.end(),[](auto x,auto y){return x.second>y.second;});vertex.bones={};vertex.weights={};float sum{};
                            for(std::size_t k=0;k<std::min<std::size_t>(4,sorted.size());++k){vertex.bones[k]=sorted[k].first;vertex.weights[k]=std::max(0.f,sorted[k].second);sum+=vertex.weights[k];}if(sum>0)for(auto& w:vertex.weights)w/=sum;++changed;
                        }
                    }
                }
                metadata<<"DIAGNOSTIC ONLY: native foretwist candidate vertices="<<changed<<" interpolated="<<bool(std::getenv("CADENCE_FORETWIST_INTERPOLATE"))<<" spatial="<<bool(foretwistReference)<<"; not production/replay certification.\n";
            }
            if(std::getenv("CADENCE_FIT_VERTICES")){
                std::ofstream vertices(out/"fit-vertices.csv");
                vertices<<"mesh,vertex,sx,sy,sz,fx,fy,fz,source_weights,fitted_weights\n";
                for(const auto& original:skin.meshes)for(const auto& fitted:view->meshes)if(original.name==fitted.name){
                    require(original.vertices.size()==fitted.vertices.size(),"Diagnostic vertex correspondence changed");
                    for(std::size_t i=0;i<original.vertices.size();++i){
                        const auto a=scene::transformPoint(original.modelTransform,original.vertices[i].position),b=scene::transformPoint(fitted.modelTransform,fitted.vertices[i].position);
                        vertices<<quoted(original.name)<<','<<i<<','<<a.x<<','<<a.y<<','<<a.z<<','<<b.x<<','<<b.y<<','<<b.z;
                        for(int kind=0;kind<2;++kind){const auto& v=kind?fitted.vertices[i]:original.vertices[i];const auto& rig=kind?view->skeleton:skin.skeleton;std::string weights;
                            for(int k=0;k<4;++k)if(v.weights[k]>0)weights+=rig.bones.at(v.bones[k]).name+":"+std::to_string(v.weights[k])+";";
                            vertices<<','<<quoted(weights);
                        }vertices<<'\n';
                    }
                }
            }
            if(nativeRollReference){
                const auto layout=scene::imported::viewLayout(*nativeRollReference);
                for(int side=0;side<2;++side){const auto& arm=layout.arms[side];
                    const auto roll=nativeRollReference->skeleton.boneByName.find(side?"j_shouldertwist_ri":"j_shouldertwist_le");
                    if(roll!=nativeRollReference->skeleton.boneByName.end()){
                        const auto weight=[&](){double total{};for(const auto& m:view->meshes)if(!m.viewmodelWeapon)for(const auto& v:m.vertices)for(int k=0;k<4;++k)if(v.bones[k]==roll->second)total+=v.weights[k];return total;};
                        const double before=weight();
                        const auto origin=scene::transformPoint(nativeRollReference->skeleton.bones[arm.shoulder].restGlobal,{});
                        const auto axis=scene::transformPoint(nativeRollReference->skeleton.bones[arm.elbow].restGlobal,{})-origin;
                        for(int model=0;model<2;++model){
                            const auto& sample=model?*view:*nativeRollReference;
                            std::array<double,8> shoulderWeights{},rollWeights{};
                            for(const auto& mesh:sample.meshes)if(!mesh.viewmodelWeapon)for(const auto& vertex:mesh.vertices){
                                const auto point=scene::transformPoint(mesh.modelTransform,vertex.position);
                                const auto bin=std::clamp(int(scene::dot(point-origin,axis)/scene::dot(axis,axis)*8),0,7);
                                for(int k=0;k<4;++k){auto bone=vertex.bones[k];
                                    for(const auto& [follower,target]:sample.nativePoseFollowers)if(bone==follower){bone=target;break;}
                                    if(bone==static_cast<unsigned>(arm.shoulder))shoulderWeights[bin]+=vertex.weights[k];
                                    if(bone==roll->second)rollWeights[bin]+=vertex.weights[k];
                                }
                            }
                            for(int bin=0;bin<8;++bin)metadata<<"Shoulder envelope side="<<side<<" model="<<(model?"fitted":"native")<<" bin="<<bin<<" shoulder="<<shoulderWeights[bin]<<" roll="<<rollWeights[bin]<<'\n';
                        }
                        scene::imported::transferNativeForearmRoll(*nativeRollReference,view->meshes,arm.shoulder,arm.elbow,static_cast<int>(roll->second));
                        metadata<<"Native shoulder roll "<<side<<" before="<<before<<" after="<<weight()<<'\n';
                    }
                }
                metadata<<"DIAGNOSTIC ONLY: shoulder roll fractions transferred from original native skin.\n";
            }
            metadata<<"DIAGNOSTIC ONLY: replacement skin "<<skinPath<<" fitted to assembled native driver; replay mismatch expected.\n";
        }
        if(std::getenv("CADENCE_DISABLE_SKIN_CLEARANCE")){
            require(view->codmRigAdapter!=nullptr,"Clearance isolation requires donor adapter");
            auto adapter=std::make_shared<scene::CodmRigAdapter>(*view->codmRigAdapter);
            adapter->sleeves.clear();adapter->forearmClearance.clear();view->codmRigAdapter=std::move(adapter);
            metadata<<"DIAGNOSTIC ONLY: skin clearance disabled; bindings and donor geometry unchanged.\n";
        }
        if(std::getenv("CADENCE_POSE_ONLY_STRAIGHT_UPPER")){
            require(view->codmRigAdapter&&!view->codmRigAdapter->positionLinks.empty(),"Straight upper probe requires pose-only donor");
            const auto donor=scene::buildScene(cast::Document::load(path(model.hand)),false);
            const auto from=scene::imported::viewLayout(donor),to=scene::imported::viewLayout(*view);
            auto adapter=std::make_shared<scene::CodmRigAdapter>(*view->codmRigAdapter);
            const auto pos=[](const scene::Skeleton& s,int i){return scene::transformPoint(s.bones.at(i).restGlobal,{});};
            for(int side=0;side<2;++side){
                const auto a=from.arms[side],b=to.arms[side];
                require(a.valid()&&b.valid()&&a.shoulder>=0&&b.shoulder>=0,"Straight upper probe requires full arms");
                const auto se=pos(donor.skeleton,a.elbow),sw=pos(donor.skeleton,a.wrist),de=pos(view->skeleton,b.elbow),dw=pos(view->skeleton,b.wrist);
                const auto sn=scene::normalize(scene::cross(pos(donor.skeleton,a.fingers[1][0])-sw,pos(donor.skeleton,a.fingers[4][0])-sw));
                const auto dn=scene::normalize(scene::cross(pos(view->skeleton,b.fingers[1][0])-dw,pos(view->skeleton,b.fingers[4][0])-dw));
                const auto fit=scene::imported::viewAnatomicalFrame(de,dw,dn)*scene::inverseAffine(scene::imported::viewAnatomicalFrame(se,se+(se-pos(donor.skeleton,a.shoulder)),sn));
                scene::Vec3 p,scale;scene::Quat q;scene::decomposeAffine(fit,p,q,scale);
                const auto desired=scene::rotation(q)*view->skeleton.bones[adapter->firstBone+a.shoulder].restGlobal;
                const auto old=view->skeleton.bones[adapter->bindings[a.shoulder].source].restGlobal*adapter->bindings[a.shoulder].offset;
                const auto correction=desired*scene::inverseAffine(old);
                for(auto& binding:adapter->bindings)if(binding.source==static_cast<size_t>(b.shoulder)){
                    binding.offset=scene::inverseAffine(view->skeleton.bones[b.elbow].restGlobal)*correction*view->skeleton.bones[b.shoulder].restGlobal*binding.offset;
                    binding.source=b.elbow;binding.rollWeight=0;
                }
            }
            view->codmRigAdapter=std::move(adapter);
            metadata<<"DIAGNOSTIC ONLY: donor upper arm follows forearm; unchanged donor geometry/lengths and wrist bindings. Replay mismatch expected.\n";
        }
        if(const char* referencePath=std::getenv("CADENCE_SHOULDER_LENGTH_CANDIDATE")){
            const bool proximalOnly=std::getenv("CADENCE_PROXIMAL_EXTENT_CANDIDATE")!=nullptr;
            const auto reference=scene::buildScene(cast::Document::load(referencePath),false);
            const auto referenceLayout=scene::imported::viewLayout(reference),layout=scene::imported::viewLayout(*view);
            const auto pos=[](const scene::Skeleton& s,int b){return scene::transformPoint(s.bones.at(b).restGlobal,{});};
            for(int side=0;side<2;++side){const auto a=referenceLayout.arms[side],b=layout.arms[side];
                require(a.valid()&&b.valid()&&a.shoulder>=0&&b.shoulder>=0,"Length candidate needs complete arms");
                const auto origin=pos(view->skeleton,b.shoulder),axis=scene::normalize(pos(view->skeleton,b.elbow)-origin);
                const float length=scene::length(pos(view->skeleton,b.elbow)-origin);
                const auto minimum=[&](const scene::CastScene& model,int bone,scene::Vec3 o,scene::Vec3 direction){float result=std::numeric_limits<float>::max();
                    for(const auto& mesh:model.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices){float w=0;for(int k=0;k<4;++k)if(v.bones[k]==static_cast<uint32_t>(bone))w+=v.weights[k];if(w>=.5f)result=std::min(result,scene::dot(scene::transformPoint(mesh.modelTransform,v.position)-o,direction));}return result;};
                const auto ro=pos(reference.skeleton,a.shoulder),ra=pos(reference.skeleton,a.elbow)-ro;
                const float nativeMin=minimum(reference,a.shoulder,ro,scene::normalize(ra))*length/scene::length(ra),fittedMin=minimum(*view,b.shoulder,origin,axis);
                require(std::isfinite(nativeMin)&&std::isfinite(fittedMin)&&length>nativeMin&&length>fittedMin,"Invalid shoulder envelope");
                metadata<<"Shoulder envelope "<<side<<": fitted minimum "<<fittedMin<<", native minimum "<<nativeMin<<", elbow "<<length<<'\n';
                if(nativeMin<=fittedMin)continue;
                const float axialScale=(length-nativeMin)/(length-fittedMin);
                const float join=length*.7f;
                if(proximalOnly)require(join>nativeMin&&join>fittedMin,"Proximal extent has no room before elbow preservation zone");
                for(auto& mesh:view->meshes)if(!mesh.viewmodelWeapon)for(auto& v:mesh.vertices){float w=0;for(int k=0;k<4;++k)if(v.bones[k]==static_cast<uint32_t>(b.shoulder))w+=v.weights[k];if(w<=0)continue;
                    const auto p=scene::transformPoint(mesh.modelTransform,v.position);const float x=scene::dot(p-origin,axis);if(x>=length)continue;
                    float shift=(length-x)*(1-axialScale)*std::min(1.f,w*2.f);
                    if(proximalOnly){
                        // Monotone spatial fit, with unit derivative at the join.
                        // Leave the distal 30% and its elbow weights untouched.
                        if(x>=join)continue;
                        const float t=std::clamp((x-fittedMin)/(join-fittedMin),0.f,1.f);
                        const float exponent=(join-fittedMin)/(join-nativeMin);
                        shift=nativeMin+(join-nativeMin)*std::pow(t,exponent)-x;
                        require(scene::proximal_support::matrixError(mesh.modelTransform,scene::Mat4::identity())<.00001f,"Proximal normal transport requires scene-space skin");
                        const float derivative=std::pow(t,exponent-1.f);
                        v.normal=scene::proximal_support::axialNormal(v.normal,axis,derivative);
                    }
                    v.position=scene::transformPoint(scene::inverseAffine(mesh.modelTransform),p+axis*shift);
                }
            }
            metadata<<"DIAGNOSTIC ONLY: proximal sleeve compressed to native shoulder extent; replay mesh mismatch expected.\n";
            if(proximalOnly)metadata<<"DIAGNOSTIC ONLY: proximal spatial power fit with cofactor normal transport, distal30% preserved.\n";
            else metadata<<"DIAGNOSTIC ONLY: legacy non-proximal length candidate normals not corrected.\n";
        }
        if(std::getenv("CADENCE_SHOULDER_BEND_CANDIDATE")){
            // Test humerus roll derived from the elbow bend plane instead of
            // the palm plane. Do not alter animation, hands or weapon bones.
            const auto donor=scene::buildScene(cast::Document::load(path(model.hand)),false);
            const auto from=scene::imported::viewLayout(donor),to=scene::imported::viewLayout(*view);
            const auto pos=[](const scene::Skeleton& s,int b){return scene::transformPoint(s.bones.at(b).restGlobal,{});};
            for(int side=0;side<2;++side){
                const auto a=from.arms[side],b=to.arms[side];
                require(a.valid()&&b.valid()&&a.shoulder>=0&&b.shoulder>=0,"Bend candidate needs complete arms");
                const auto sp=pos(donor.skeleton,a.shoulder),se=pos(donor.skeleton,a.elbow),sw=pos(donor.skeleton,a.wrist);
                const auto dp=pos(view->skeleton,b.shoulder),de=pos(view->skeleton,b.elbow),dw=pos(view->skeleton,b.wrist);
                const auto sn=scene::normalize(scene::cross(pos(donor.skeleton,a.fingers[1][0])-sw,pos(donor.skeleton,a.fingers[4][0])-sw));
                const auto dn=scene::normalize(scene::cross(pos(view->skeleton,b.fingers[1][0])-dw,pos(view->skeleton,b.fingers[4][0])-dw));
                const auto sb=scene::cross(scene::normalize(se-sp),scene::normalize(sw-se));
                const auto db=scene::cross(scene::normalize(de-dp),scene::normalize(dw-de));
                require(scene::length(sb)>.05f&&scene::length(db)>.05f,"Bend plane is degenerate");
                const auto oldFit=scene::imported::viewAnatomicalFrame(dp,de,dn)*scene::inverseAffine(scene::imported::viewAnatomicalFrame(sp,se,sn));
                const auto newFit=scene::imported::viewAnatomicalFrame(dp,de,scene::normalize(db))*scene::inverseAffine(scene::imported::viewAnatomicalFrame(sp,se,scene::normalize(sb)));
                const auto correction=newFit*scene::inverseAffine(oldFit);
                for(auto& mesh:view->meshes)if(!mesh.viewmodelWeapon)for(auto& v:mesh.vertices){
                    float weight=0;for(int k=0;k<4;++k)if(v.bones[k]==static_cast<uint32_t>(b.shoulder))weight+=v.weights[k];
                    if(weight<=0)continue;
                    const auto p=scene::transformPoint(mesh.modelTransform,v.position);
                    v.position=scene::transformPoint(scene::inverseAffine(mesh.modelTransform),p*(1-weight)+scene::transformPoint(correction,p)*weight);
                    const auto local=scene::inverseAffine(mesh.modelTransform)*correction*mesh.modelTransform;
                    v.normal=scene::normalize(v.normal*(1-weight)+scene::volume_fit::vector(local,v.normal)*weight);
                }
            }
            metadata<<"DIAGNOSTIC ONLY: shoulder bind roll follows elbow bend plane; replay mesh mismatch expected.\n";
        }
        if(std::getenv("CADENCE_SHOULDER_TWIST_CANDIDATE")){
            for(const auto* side:{"le","ri"}){
                const auto shoulder=view->skeleton.boneByName.find(std::string("j_shoulder_")+side);
                const auto twist=view->skeleton.boneByName.find(std::string("j_shouldertwist_")+side);
                require(shoulder!=view->skeleton.boneByName.end()&&twist!=view->skeleton.boneByName.end(),"Shoulder twist diagnostic requires both bones");
                for(auto& mesh:view->meshes)if(!mesh.viewmodelWeapon)for(auto& v:mesh.vertices)
                    for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]==shoulder->second)v.bones[k]=twist->second;
            }
            metadata<<"DIAGNOSTIC ONLY: full shoulder weight transferred to shoulder twist; not a production fix or replay certification.\n";
        }
        if(const char* reference=std::getenv("CADENCE_PROXIMAL_SUPPORT_REFERENCE")){
            const auto native=scene::buildScene(cast::Document::load(reference),false);
            scene::proximal_support::Options options;options.enabled=true;
            options.conformSurface=std::getenv("CADENCE_PROXIMAL_SURFACE_CANDIDATE")!=nullptr;
            scene::proximal_support::Report report;std::string error;
            const bool direct=std::getenv("CADENCE_DIRECT_NATIVE_SUPPORT")!=nullptr;
            const bool coherent=std::getenv("CADENCE_COHERENT_NATIVE_SUPPORT")!=nullptr;
            const bool holes=std::getenv("CADENCE_NATIVE_SUPPORT_HOLES")!=nullptr;
            const bool fullMatched=std::getenv("CADENCE_NATIVE_SUPPORT_FULL_MATCHED")!=nullptr;
            require(direct?scene::proximal_support::applyDirectNative(*view,native,report,error,coherent,holes,fullMatched):scene::proximal_support::apply(*view,native,options,report,error),error);
            if(direct&&coherent)metadata<<"DIAGNOSTIC ONLY: harmonic confidence over validated connected native matches.\n";
            if(direct)metadata<<"DIAGNOSTIC ONLY: direct native proximal support weights; no geometry compression.\n";
            metadata<<"DIAGNOSTIC ONLY: proximal support candidates="<<report.candidates<<" edited="<<report.edited<<" rejected="<<report.rejected<<" bad_palette="<<report.badPalette<<" native_triangles="<<report.eligibleTriangles<<"; not production/replay certification.\n";
        }
        if(std::getenv("CADENCE_PROXIMAL_CONTINUATION")){
            const auto sourcePath=std::getenv("CADENCE_FIT_VIEW_SKIN")?std::string(std::getenv("CADENCE_FIT_VIEW_SKIN")):path(model.hand);
            const auto source=scene::buildScene(cast::Document::load(sourcePath),false);
            const auto from=scene::imported::viewLayout(source),to=scene::imported::viewLayout(*view);
            for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
                bool weightedUpper=false;if(a.shoulder>=0)for(const auto& mesh:source.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.weights[k]>.001f&&v.bones[k]==static_cast<uint32_t>(a.shoulder))weightedUpper=true;
                if(!a.valid()||!b.valid())continue;
                scene::imported::ProximalArm arm{b.shoulder,b.elbow,b.wrist,!weightedUpper};
                const auto result=scene::imported::continueProximalDiagnostic(view->meshes,view->skeleton,arm,true,std::getenv("CADENCE_PROXIMAL_CONTINUATION_UV")!=nullptr);
                metadata<<"DIAGNOSTIC ONLY: proximal continuation side="<<side<<" meshes="<<result.modifiedMeshes<<" vertices="<<result.addedVertices<<" texture_review="<<result.requiresTextureReview<<" reason="<<result.reason<<"; not production/replay certification.\n";
            }
        }
        if(std::getenv("CADENCE_WRIST_TRANSITION")){
            const auto layout=scene::imported::viewLayout(*view);
            for(const auto& a:layout.arms)metadata<<"DIAGNOSTIC ONLY: wrist transition vertices="<<scene::imported::wristTransitionDiagnostic(view->meshes,view->skeleton,a.elbow,a.wrist,true,std::getenv("CADENCE_WRIST_REFINE")!=nullptr)<<"; not production/replay certification.\n";
        }
        if(std::getenv("CADENCE_SHOULDER_RIM_CONTINUATION")){
            const auto layout=scene::imported::viewLayout(*view);
            for(int side=0;side<2;++side){const auto& a=layout.arms[side];
                const auto r=scene::imported::continueShoulderRimDiagnostic(view->meshes,view->skeleton,{a.shoulder,a.elbow,a.wrist,false},true,std::getenv("CADENCE_PROXIMAL_CONTINUATION_UV")!=nullptr);
                metadata<<"DIAGNOSTIC ONLY: shoulder rim side="<<side<<" meshes="<<r.modifiedMeshes<<" vertices="<<r.addedVertices<<" reason="<<r.reason<<"; original skin retained, no production/replay certification.\n";
            }
        }
        if(const char* meshPart=std::getenv("CADENCE_ISOLATE_VIEW_MESH")){
            bool found=false;
            for(auto& mesh:view->meshes){
                if(!mesh.viewmodelWeapon&&mesh.name.find(meshPart)!=std::string::npos){found=true;metadata<<"Isolated mesh "<<mesh.name<<'\n';}
                else mesh.indices.clear();
            }
            require(found,"No viewhand mesh matched isolation filter");
            metadata<<"DIAGNOSTIC ONLY: mesh isolation; not a production or replay pass.\n";
        }
        if(const char* isolate=std::getenv("CADENCE_ISOLATE_VIEW_BONE")){
            const auto bone=view->skeleton.boneByName.find(isolate);
            require(bone!=view->skeleton.boneByName.end(),"Unknown isolation bone");
            for(auto& mesh:view->meshes){
                std::vector<uint32_t> selected;
                if(!mesh.viewmodelWeapon)for(std::size_t i=0;i+2<mesh.indices.size();i+=3){
                    bool influenced=false;
                    for(int corner=0;corner<3;++corner){const auto& v=mesh.vertices.at(mesh.indices[i+corner]);
                        for(int k=0;k<4;++k)if(v.weights[k]>.01f&&v.bones[k]==bone->second)influenced=true;}
                    if(influenced)selected.insert(selected.end(),mesh.indices.begin()+i,mesh.indices.begin()+i+3);
                }
                mesh.indices=std::move(selected);
            }
            metadata<<"DIAGNOSTIC ONLY: triangles influenced by "<<isolate<<" isolated; weapon hidden. Not replay or production certification.\n";
        }
#endif
        require(app.selectedBaseAsset<app.assetCatalog.entries.size()&&app.assetCatalog.entries[app.selectedBaseAsset].game==model.game,"Requested viewhands library was replaced");
        require(app.loadedBaseModelPath==app.assetCatalog.entries[app.selectedBaseAsset].path,"Loaded viewhands do not match selected fixture");
        for(const auto& warning:app.scene.warnings)metadata<<"View warning: "<<warning<<'\n';
        if(view->codmRigAdapter)metadata<<"Weapon grip fit scale: "<<view->codmRigAdapter->weaponFitScale<<'\n';
        if(std::getenv("CADENCE_PROFILE_TRACE")){
            metadata<<"Resolved animation prefix: "<<app.viewmodelAnimationPrefix<<'\n';
            for(const auto& [slot,name]:app.weaponProfile.animations)metadata<<"Profile slot "<<slot<<": "<<name<<'\n';
            for(const auto& [slot,names]:app.weaponProfile.animationVariants)for(const auto& name:names)metadata<<"Profile variant "<<slot<<": "<<name<<'\n';
            for(std::size_t i=0;i<view->animations.size();++i){const auto& a=view->animations[i];metadata<<"Clip domain "<<i<<": "<<int(a.domain)<<" "<<a.sourceName<<'\n';}
        }
        actions(app,*view,vl,out,detail);inspectClips(*view,vl,detail);replay(app,*view,vl,out);app.scene=*view;
    }catch(const std::exception& e){fail(vl,e.what());if(vl.schedule.empty())vl.ready=false;}
    app.scene=view?*view:scene::CastScene{};app.selectedWeaponAsset=driver.weapon;
    if(std::getenv("CADENCE_VIEW_ONLY")){bl.status="SKIPPED";bl.reason="Explicit viewmodel-only diagnostic; body not tested";}
    else if(model.body==SIZE_MAX||driver.weapon==SIZE_MAX)bl.reason=model.body==SIZE_MAX?model.bodyError:driver.weaponError;
    else try{
        app.botModelAsset=model.body;const auto start=Clock::now();rebuildBotActors(app);bl.loadMs=elapsed(start);require(app.botActorScene&&app.bots.size()==1,"One-body assembly failed: "+app.status);body=*app.botActorScene;bl.ready=true;bl.status="CHECKED_PENDING_VISUAL";
        for(const auto& warning:body->warnings)metadata<<"Body warning: "<<warning<<'\n';
#ifdef CADENCE_CROSS_CURRENT_REVIEW
        if(const char* clipPath=std::getenv("CADENCE_DIAGNOSTIC_BODY_CLIP")){
            const char* modelPath=std::getenv("CADENCE_DIAGNOSTIC_BODY_SOURCE");
            require(modelPath,"Diagnostic body clip requires explicit source model");
            auto native=std::make_shared<scene::CastScene>(scene::buildScene(cast::Document::load(modelPath),false));
            scene::appendAnimations(cast::Document::load(clipPath),*native);
            require(native->animations.size()==1,"Diagnostic body source must contain exactly one clip");
            if(const char* basePath=std::getenv("CADENCE_DIAGNOSTIC_BODY_BASE")){
                scene::appendAnimations(cast::Document::load(basePath),*native);
                require(native->animations.size()==2,"Diagnostic holding source must contain exactly one clip");
                const auto anatomy=scene::imported::bodyOrCodLayout(native->skeleton);
                require(anatomy.spine>=0,"Diagnostic holding composition requires native spine");
                auto composed=scene::imported::composeBindBakedPostDelta(*native,1,0,static_cast<std::size_t>(anatomy.spine));
                native->animations={std::move(composed)};
                metadata<<"DIAGNOSTIC ONLY: bind-baked POST delta composed on actual holding base "<<basePath<<" frame0 before retarget; native spine-descendant ownership, no lower-body delta. Provenance must be independently established; not production admission.\n";
            }
            native->animations[0].domain=scene::AnimationDomain::PlayerBody;
            native->animations[0].motion=scene::MotionRole::Unknown;
            native->animations[0].action=scene::ActionRole::Fire;
            if(std::getenv("CADENCE_DIAGNOSTIC_BODY_TORSO")||std::getenv("CADENCE_DIAGNOSTIC_BODY_BASE")){
                auto& clip=native->animations[0];clip.domain=scene::AnimationDomain::PlayerTorso;
                clip.weapon=scene::WeaponClass::Rifle;clip.stance=std::getenv("CADENCE_DIAGNOSTIC_BODY_BASE")?scene::Stance::Stand:scene::Stance::Crouch;
            }
            auto candidate=*body;std::string issue;
            require(scene::imported::appendRetargetedBody(candidate,native,0,issue),issue);
            const auto index=candidate.animations.size()-1;
            require(app.renderer.loadScene(candidate,issue),issue);
            for(int phase=0;phase<=100;phase+=5)capture(app,candidate,index,phase/100.f,true,out/("diagnostic_body_clip_"+std::to_string(phase)+".png"));
            metadata<<"DIAGNOSTIC ONLY: explicit native body clip "<<clipPath<<" decoded against "<<modelPath<<" torso="<<bool(std::getenv("CADENCE_DIAGNOSTIC_BODY_TORSO"))<<" run_base="<<bool(std::getenv("CADENCE_DIAGNOSTIC_BODY_RUN"))<<"; explicit diagnostic metadata, not runtime selection approval.\n";
        }
#endif
        std::set<std::string> sourceGames;for(const auto& a:body->animations)if(!a.tracks.empty())sourceGames.insert(a.sourceGame);for(const auto& g:sourceGames){if(!bl.actualGame.empty())bl.actualGame+='+';bl.actualGame+=g;}
        if(!sourceGames.contains(driver.game)){bl.status="CHECKED_FALLBACK_PENDING_VISUAL";bl.reason="Requested body library absent/unavailable; production fallback is "+bl.actualGame;}
        actions(app,*body,bl,out,detail);inspectClips(*body,bl,detail);if(body->attachments.empty())fail(bl,"No world weapon attachment");
        const auto liveBots=app.bots;
        replay(app,*body,bl,out);app.botActorScene=*body;app.bots=liveBots;
    }catch(const std::exception& e){fail(bl,e.what());if(bl.schedule.empty())bl.ready=false;}
#ifdef CADENCE_CROSS_CURRENT_REVIEW
    if(body&&bl.ready&&std::getenv("CADENCE_LIVE_BODY_CONTROLLER")){
        // Exercise production AI, physics, clip selection and pose blending.
        // Script only the player's target position, never bone poses or clips.
        app.botActorScene=*body;app.botFoundationMovement=true;
        app.activeBotSystemMode=0;app.botAutoRespawn=false;
        app.botAllowJumpDrop=false;app.takePreview=false;app.takeRecording=false;
        app.actorPosition={100000,100000,10000};
        require(app.bots.size()==1,"Live controller needs the original single bot");
        const bool combatFixture=std::getenv("CADENCE_LIVE_BODY_COMBAT")!=nullptr;
        if(combatFixture){
            auto& bot=app.bots.front();
            bot.position={-2000,-2000,0};bot.previousPhysicsPosition=bot.position;
            bot.velocity={};bot.yaw=0;bot.grounded=true;bot.ammo=3;
            bot.visibilityCheckTime=0;bot.physicsPresentationValid=false;
            metadata<<"Diagnostic combat fixture: open course spawn,3 initial rounds, target in current facing cone; no forced clip or pose.\n";
        }
        std::string error;require(app.renderer.loadScene(*body,error),error);
        configureReviewLighting(app);
        std::ofstream trace(out/"live-body-controller.csv");
        trace<<"frame,phase,x,y,z,grounded,speed,base,base_frame,action,action_frame,action_weight,yaw,visible,behavior,ammo,reload_time\n";
        const float extent=std::max(100.f,body->bounds.radius()*3.f);
        for(int frame=0;frame<=960;++frame){
            auto& bot=app.bots.front();const int phase=frame/240;
            const float distance=gameplay::iw::worldUnits(phase==0?1800.f:phase==1?450.f:phase==2?110.f:1800.f);
            const float angle=combatFixture?bot.yaw:(phase>=3?scene::kPi*.5f:0.f);
            app.actorPosition=bot.position+scene::Vec3{std::cos(angle)*distance,std::sin(angle)*distance,0};
            updateBotActors(app,1.f/120.f);
            require(!app.botActorPoses.empty()&&finite(app.botActorPoses.front()),"Invalid live bot pose");
            trace<<frame<<','<<phase<<','<<bot.position.x<<','<<bot.position.y<<','<<bot.position.z<<','<<bot.grounded<<','<<gameplay::bot::horizontalSpeed(bot)<<','<<bot.animation<<','<<bot.animationFrame<<','<<bot.actionAnimation<<','<<bot.actionFrame<<','<<bot.actionBlendWeight<<','<<bot.yaw<<','<<bot.targetVisible<<','<<int(bot.behavior)<<','<<bot.ammo<<','<<bot.reloadTime<<'\n';
            if(frame%12==0){
                // Translation-follow camera retains pose/yaw and foot movement;
                // this is not evidence of map-specific navigation or foot contact.
                const auto center=gameplay::bot::presentationPosition(bot)+scene::Vec3{0,0,body->bounds.radius()*.65f};
                const auto eye=center+scene::Vec3{extent,extent*.5f,extent*.25f};
                const auto vp=scene::perspective(50*scene::kPi/180,float(Width)/Height,.1f,10000)*scene::lookAt(eye,center,{0,0,1});
                app.renderer.setCameraPosition(eye);
                app.renderer.render(*body,app.botActorPoses.front(),vp,Width,Height,false,false,false);
                require(app.renderer.saveColorPng(out/("live_body_"+std::to_string(frame)+".png"),error),error);
                if(combatFixture&&(frame==420||frame==552)&&bot.actionAnimation<body->animations.size()){
                    const auto& active=*app.botActorScene;
                    const auto direct=active.sampleLocalPose(bot.actionAnimation,bot.actionFrame);
                    std::ofstream audit(out/("live_action_"+std::to_string(frame)+".csv"));
                    audit<<"bone,parent,owns_rotation,live_qx,live_qy,live_qz,live_qw,direct_qx,direct_qy,direct_qz,direct_qw\n";
                    for(std::size_t b=0;b<direct.size()&&b<app.botLocalPoseScratch.size();++b){
                        bool owns=false;for(const auto& t:active.animations[bot.actionAnimation].tracks)owns|=t.boneIndex==b&&t.property==scene::TrackProperty::Rotation&&t.ownsLayer;
                        const auto a=app.botLocalPoseScratch[b].rotation,d=direct[b].rotation;
                        audit<<quoted(active.skeleton.bones[b].name)<<','<<active.skeleton.bones[b].parent<<','<<owns<<','<<a.x<<','<<a.y<<','<<a.z<<','<<a.w<<','<<d.x<<','<<d.y<<','<<d.z<<','<<d.w<<'\n';
                    }
                    metadata<<"Live action audit "<<frame<<": "<<active.animations[bot.actionAnimation].sourceName<<", frame="<<bot.actionFrame<<", stance="<<int(bot.stance)<<", clip_stance="<<int(active.animations[bot.actionAnimation].stance)<<"\n";
                }
            }
        }
        metadata<<"Diagnostic live body controller:961 updates at120Hz,81rendered samples; target-distance/direction phases only. No map ground-contact certification.\n";
    }
#endif
    row(result,model.game,driver.game,vl);row(result,model.game,driver.game,bl);
    std::set<std::filesystem::path> inputs;for(const auto i:{model.hand,model.body,driver.weapon})if(i<app.assetCatalog.entries.size())inputs.insert(app.assetCatalog.entries[i].path);
    for(const auto& p:app.animationSources)inputs.insert(p);
    for(const auto& [game,documents]:app.botAnimationCache)for(const auto& doc:documents)inputs.insert(std::filesystem::u8path(doc.sourceName()));
    for(const auto* rig:{view?&*view:nullptr,body?&*body:nullptr})if(rig)for(const auto& mesh:rig->meshes)for(const auto& p:{mesh.albedoPath,mesh.normalPath,mesh.specularPath,mesh.metalnessPath,mesh.roughnessPath})if(!p.empty())inputs.insert(p);
    std::ofstream inputLog(out/"inputs.csv");inputLog<<"path,bytes,modified\n";for(const auto& p:inputs){std::error_code ec;if(std::filesystem::is_regular_file(p,ec))inputLog<<quoted(p.string())<<','<<std::filesystem::file_size(p,ec)<<','<<std::filesystem::last_write_time(p,ec).time_since_epoch().count()<<'\n';}
    if(seconds>0){
        for(int mode=0;mode<3;++mode){if((mode==0&&!vl.ready)||(mode==1&&!bl.ready)||(mode==2&&(!vl.ready||!bl.ready)))continue;
            const auto* b=mode==0?nullptr:&*body;const auto* v=mode==1?nullptr:&*view;
            try{timing(app,b,v,bl,vl,seconds,repeats,out);}catch(const std::exception& e){std::ofstream issue(out/"timing_errors.txt",std::ios::app);issue<<mode<<": "<<e.what()<<'\n';}
        }
    }
    std::ofstream completion(out/"completed.txt");completion<<"Pairing execution complete. Functional statuses in result.csv; visual and comparative performance certification pending.\n";
}
}

#ifndef CADENCE_CROSS_BENCH_LIBRARY_ONLY
int main(int argc,char** argv)try{
    if(argc<4){std::cerr<<"--inventory ROOT OUT | --case ROOT OUT MODEL_GAME DRIVER_GAME [wall_seconds=60] [repetitions=3] [fixtures.tsv]\n";return 2;}
    const std::filesystem::path root=argv[2],out=argv[3];std::filesystem::create_directories(out);
    crossbench::require(glfwInit()!=0,"GLFW init failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(crossbench::Width,crossbench::Height,"Cadence cross-game benchmark",nullptr,nullptr);crossbench::require(window,"GL window failed");glfwMakeContextCurrent(window);glfwSwapInterval(0);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;app.defaultSalukiDirectory=root;std::string error;crossbench::require(app.renderer.initialize(error),error);
    std::vector<std::string> selected;
    if(std::getenv("CADENCE_FOCUSED_SCAN")&&std::string_view(argv[1])=="--case"&&argc>=6){
        selected={argv[4],argv[5]};
        for(const auto& game:std::vector<std::string>(selected))if(game=="pointblank"||game=="bocw_sp"||game=="bo2"||game=="bo2_sp"){selected.push_back("bo2");selected.push_back("bo2_sp");}
    }
    const auto games=crossbench::scan(app,selected);
    if(std::string_view(argv[1])=="--inventory")crossbench::inventory(app,out,games);
    else{crossbench::require(argc>=6,"Missing model/driver game");const auto select=[&](const char* game){return argc>8?crossbench::frozenFixture(app,game,argv[8]):crossbench::fixture(app,game);};crossbench::run(app,select(argv[4]),select(argv[5]),out,argc>6?std::stod(argv[6]):60,argc>7?std::stoi(argv[7]):3);}
    app.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}
#endif
