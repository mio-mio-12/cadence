#define CADENCE_CROSS_BENCH_LIBRARY_ONLY
#include "CrossGameBenchmark.cpp"

// Supplementary failure diagnosis only. Never replaces the matrix's FAIL
// status, never writes exports/presets, and never supplies timing results.
namespace failureprobe {
void compare(const scene::CastScene& live,const scene::CastScene& restored,
             const std::filesystem::path& out,const std::string& stage){
    std::ofstream report(out/(stage+"_differences.txt"));
    try { report<<"Strict comparison: "<<crossbench::geometryDifference(live,restored)<<'\n'; }
    catch(const std::exception& e){report<<"Strict comparison FAIL: "<<e.what()<<'\n';}
    report<<"Skeletons: "<<live.skeleton.bones.size()<<" / "<<restored.skeleton.bones.size()
          <<"; meshes: "<<live.meshes.size()<<" / "<<restored.meshes.size()<<'\n';
    for(std::size_t b=0;b<std::min(live.skeleton.bones.size(),restored.skeleton.bones.size());++b){
        double maximum{};for(int k=0;k<16;++k)maximum=std::max(maximum,double(std::abs(live.skeleton.bones[b].inverseBind.v[k]-restored.skeleton.bones[b].inverseBind.v[k])));
        if(maximum<=.002)continue;
        std::size_t influenced{};for(const auto& mesh:live.meshes)if(mesh.skinned)for(const auto& vertex:mesh.vertices){bool weighted{};for(std::size_t k=0;k<4;++k)weighted|=vertex.weights[k]>0&&vertex.bones[k]==b;influenced+=weighted;}
        report<<"Bind delta "<<live.skeleton.bones[b].name<<": "<<maximum<<", weighted vertices="<<influenced<<'\n';
    }
    for(const auto* s:{&live,&restored}){
        report<<(s==&live?"LIVE":"RESTORED")<<" mesh inventory\n";
        for(const auto& m:s->meshes)report<<m.name<<", vertices="<<m.vertices.size()<<", indices="<<m.indices.size()<<", variant="<<m.actorVariant<<", attachment="<<m.attachmentIndex<<'\n';
        for(const auto& a:s->attachments)report<<"ATTACH "<<a.name<<", source="<<a.sourceModelPath.string()<<", bone="<<a.boneIndex<<'\n';
    }
}
void pixels(AppState& app,const scene::CastScene& live,const scene::CastScene& restored,
            const std::vector<scene::Mat4>& pose,bool body,const std::filesystem::path& out,const std::string& stage){
    std::string error;scene::Vec3 eye;const auto vp=body?crossbench::bodyCamera(live,eye):crossbench::viewCamera(live,pose,eye);
    app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});app.renderer.setFirstPersonProjection(!body);
    const auto draw=[&](const scene::CastScene& s,const std::string& name){
        crossbench::require(app.renderer.loadScene(s,error),"Probe upload failed: "+error);app.renderer.setCameraPosition(eye);
        app.renderer.render(s,pose,vp,crossbench::Width,crossbench::Height,false,false,false);
        std::vector<std::uint8_t> bytes;crossbench::require(app.renderer.readColorRgba(bytes,error),error);
        crossbench::require(app.renderer.saveColorPng(out/(name+".png"),error),error);return bytes;
    };
    const auto a=draw(live,stage+"_live"),b=draw(restored,stage+"_restored");
    crossbench::require(a.size()==b.size(),"Pixel array sizes differ");int maximum{};std::size_t changed{};
    for(std::size_t i=0;i<a.size();++i){const int d=std::abs(int(a[i])-int(b[i]));maximum=std::max(maximum,d);changed+=d>1;}
    std::ofstream report(out/(stage+"_pixels.txt"));report<<"Maximum channel difference="<<maximum<<"; channels differing by more than 1="<<changed<<'\n';
}
}
int main(int argc,char** argv)try{
    crossbench::require(argc>=6,"ROOT OUT MODEL_GAME DRIVER_GAME fixtures.tsv [body|viewmodel]");
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    crossbench::require(glfwInit()!=0,"GLFW init failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(crossbench::Width,crossbench::Height,"Cadence failure probe",nullptr,nullptr);crossbench::require(window,"Window failed");
    glfwMakeContextCurrent(window);glfwSwapInterval(0);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;app.defaultSalukiDirectory=root;std::string error;
    crossbench::require(app.renderer.initialize(error),error);crossbench::scan(app);
    const auto model=crossbench::frozenFixture(app,argv[3],argv[5]),driver=crossbench::frozenFixture(app,argv[4],argv[5]);
    const bool body=argc>6&&std::string_view(argv[6])=="body";
    crossbench::require(driver.weapon!=SIZE_MAX,"No frozen weapon");app.autoPlayerModel=false;app.deferSceneUpload=true;
    app.selectedBaseAsset=model.hand;app.selectedWeaponAsset=driver.weapon;
    if(model.hand!=SIZE_MAX)equipViewWeapon(app,driver.weapon);
    scene::CastScene live;take::ActorManifest manifest;
    if(body){
        app.enemyBotCount=1;app.botTeamSide=0;app.botFactionModelVariety=false;app.botUseSalukiWeaponPool=false;
        app.botGame=model.game;app.botAnimationGame=driver.game;app.botSystemMode=0;app.botModelAsset=model.body;
        rebuildBotActors(app);crossbench::require(app.botActorScene&&app.bots.size()==1,"Body assembly failed");live=*app.botActorScene;
        captureTakeBotManifest(app);manifest=app.recordedTake.botActor;
    }else{live=app.scene;captureTakeActorManifest(app);manifest=app.recordedTake.actor;}
    std::ofstream manifestLog(out/"manifest.txt");manifestLog<<"Base="<<manifest.baseModel<<'\n';
    for(const auto& r:manifest.rigModels)manifestLog<<"Rig="<<r<<'\n';
    for(const auto& a:manifest.attachedModels)manifestLog<<"Attachment="<<a.path<<", socket="<<a.bone<<'\n';
    crossbench::require(!live.animations.empty(),"No clips to inspect");
    auto clip=body?std::optional<std::size_t>{}:findViewmodelClip(app,"idle");
    if(body){scene::AnimationQuery q;q.motion=scene::MotionRole::Idle;q.stance=scene::Stance::Stand;q.weapon=app.botWeaponClass;clip=botLocomotionAnimation(live,q,&app);}
    const auto pose=body?crossbench::evaluatedBodyPose(live,clip.value_or(0),0):live.samplePose(clip.value_or(0),0);
    for(const bool disabled:{false,true}){
        const std::string stage=disabled?"disabled_catalog":"immediate";auto saved=disabled?std::move(app.assetCatalog):assets::Catalog{};
        const bool ok=body?restoreTakeBotActor(app,manifest,live.skeleton.bones.size(),error):restoreTakeActor(app,manifest,live.skeleton.bones.size(),error);
        if(disabled)app.assetCatalog=std::move(saved);
        std::ofstream status(out/(stage+"_status.txt"));status<<(ok?"RESTORED":"FAIL")<<": "<<error<<'\n';
        if(!ok)continue;const auto& restored=body?*app.botActorScene:app.scene;
        failureprobe::compare(live,restored,out,stage);failureprobe::pixels(app,live,restored,pose,body,out,stage);
    }
    app.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
