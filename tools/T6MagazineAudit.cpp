#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main

int main(int argc,char**argv){
    if(argc<3||!glfwInit())return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,720,"Magazine audit",nullptr,nullptr);if(!window)return 2;
    glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& a=*state;a.window=window;a.defaultSalukiDirectory=argv[1];a.deferSceneUpload=true;
    const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);std::ofstream log(out/"results.txt");std::string error;
    if(!a.renderer.initialize(error))return 3;
    if(!assets::appendScan(a.defaultSalukiDirectory/"bo2","bo2",a.assetCatalog,error))return 4;
    const auto index=[&](std::string_view name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return SIZE_MAX;};
    int failures=0;
    std::vector<std::string> names={"t6_wpn_ar_an94_view_LOD0","t6_wpn_ar_hk416_view_LOD0","t6_wpn_smg_mp7_view_LOD0","t6_wpn_sniper_dsr50_view_LOD0","t6_wpn_sniper_ballista_view_LOD0","t6_wpn_ar_scarh_view_LOD0"};
    if(argc>3)names.assign(argv+3,argv+argc);
    for(const auto& name:names){
        const auto weapon=index(name);if(weapon==SIZE_MAX){log<<"MISSING "<<name<<std::endl;++failures;continue;}
        a.selectedBaseAsset=index("c_usa_mp_isa_smg_viewhands_LOD0");equipViewWeapon(a,weapon);
        log<<"SOURCE "<<a.assetCatalog.entries[weapon].path.string()<<std::endl;
        const auto mag=a.scene.skeleton.boneByCanonicalName.find("tag_clip");
        if(mag==a.scene.skeleton.boneByCanonicalName.end()){++failures;log<<"NO MAG "<<name<<std::endl;continue;}
        auto p=a.scene.skeleton.bones[mag->second].restLocal.position;
        log<<name<<" mount="<<p.x<<","<<p.y<<","<<p.z<<std::endl;
        if(scene::length(p)<.1f){log<<"FAIL missing calibration prefix="<<animationPrefixForWeapon(a.assetCatalog.entries[weapon])<<" documents="<<a.animationDocuments.size()<<std::endl;for(const auto& doc:a.animationDocuments)if(doc.sourceName().find("reload.cast")!=std::string::npos)log<<doc.sourceName()<<std::endl;++failures;}
        for(const auto& warning:a.scene.warnings)if(warning.find("T6 magazine")!=std::string::npos)log<<warning<<std::endl;
        if(!a.renderer.loadScene(a.scene,error))return 5;
        a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.1f,.12f,.15f,1});
        a.actorMode=true;a.actorPosition=a.actorRenderPosition={};a.actorYaw=a.cameraPitch=0;a.actorViewHeight=0;
        a.actionActive=a.actionOverlay=a.transitioning=false;a.interruptPoseDuration=0;a.runtimeLayers.clear();
        for(const auto* action:{"idle","reload","reload_empty"}){
            const auto clip=findViewmodelClip(a,action);if(!clip){++failures;log<<"MISSING "<<action<<std::endl;continue;}
            log<<"ACTION "<<action<<" -> "<<a.scene.animations[*clip].sourceName<<std::endl;
            for(float fraction:{0.f,.25f,.5f,.75f,1.f}){
                a.animationIndex=*clip;a.animationFrame=fraction*a.scene.animations[*clip].durationFrames;
                auto pose=a.scene.samplePose(*clip,a.animationFrame);
                for(const auto& m:pose)for(float value:m.v)if(!std::isfinite(value))++failures;
                for(int side=0;side<2;++side){
                    const auto vp=scene::perspective(65*scene::kPi/180,4.f/3,.1f,4000)*(side?scene::lookAt({30,-90,20},{30,0,-6},{0,0,1}):scene::lookAt({0,0,0},{1,0,0},{0,0,1}));
                    a.renderer.render(a.scene,pose,vp,960,720,false,false,false);
                    const auto file=out/(std::string(name)+"_"+action+"_"+std::to_string(int(fraction*100))+"_"+std::to_string(side)+".png");
                    if(!a.renderer.saveColorPng(file,error))return 6;
                }
            }
        }
        const auto first=p;calibrateT6Magazine(a);p=a.scene.skeleton.bones[mag->second].restLocal.position;
        if(scene::length(p-first)>.0001f){++failures;log<<"FAIL repeated calibration drift\n";}
    }
    log<<"failures="<<failures<<std::endl;return failures?7:0;
}
