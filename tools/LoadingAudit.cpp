#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#include "LoadingReference.inc"
#include <bit>

// Read-only asset audit; outputs are confined to the supplied evidence directory.
int main(int argc,char** argv){
    if(argc<2||!glfwInit())return 1;
    const std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(640,480,"Loading audit",nullptr,nullptr);if(!window)return 2;
    glfwMakeContextCurrent(window);glfwSwapInterval(0);
    auto state=std::make_unique<AppState>();auto& app=*state;std::string error;
    app.window=window;app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
    if(!app.renderer.initialize(error))return 3;
    for(const auto* game:{"bo2","bo2_sp","codm","pointblank","bocw_sp"}){
        if(std::filesystem::exists(app.defaultSalukiDirectory/game)&&!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))return 4;
    }
    std::size_t secondary=static_cast<std::size_t>(-1);
    for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){
        const auto& a=app.assetCatalog.entries[i];
        if(a.name=="t6_wpn_ar_an94_view_LOD0")app.selectedWeaponAsset=i;
        if(a.name=="t6_wpn_sniper_ballista_view_LOD0")secondary=i;
        if(a.role==assets::Role::ViewHands&&a.game=="bo2"&&app.classViewhandsOverride>=app.assetCatalog.entries.size())app.classViewhandsOverride=i;
    }
    if(app.selectedWeaponAsset>=app.assetCatalog.entries.size())return 5;
    // Large loaded clip list reproduces profile discovery cost without changing
    // any selected bot assets. Every profile sees exactly the same scene.
    ensureBotAnimationCache(app,"bo2");
    for(const auto& doc:app.botAnimationCache["bo2"])scene::appendAnimations(doc,app.scene);
    const bool realClass=argc>2;
    if(realClass){
        if(secondary>=app.assetCatalog.entries.size())return 9;
        app.classPrimaryAsset=app.selectedWeaponAsset;app.classSecondaryAsset=secondary;
        loadBothClassSlots(app);
        while(app.pendingClassFuture){processPendingClassLoad(app);glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        if(!app.classGpuResident){std::cerr<<app.status;return 10;}
    }else for(int i=0;i<256;++i){scene::Animation a;a.sourceName="vm_unrelated_"+std::to_string(i)+"_idle.cast";a.domain=scene::AnimationDomain::ViewModel;a.tracks.resize(1);app.scene.animations.push_back(std::move(a));}
    std::ofstream report(output/"results.txt");
    report<<"real_class="<<realClass<<" view_clips="<<app.scene.animations.size()<<'\n';
    std::optional<std::uint64_t> expectedHash;
    const auto read=[](const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
    for(int round=0;round<4;++round){
        app.botGame="bo2";app.botTeamSide=1;app.enemyBotCount=5;app.botAnimationGame="bo2";app.botSystemMode=1;
        const bool reference=round==0||round==3;
        const auto start=std::chrono::steady_clock::now();
        if(reference)rebuildBotActorsReference(app);else rebuildBotActors(app);
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(!app.botActorScene||app.bots.size()!=5){std::cerr<<app.status;return 6;}
        const auto& s=*app.botActorScene;
        std::uint64_t hash=1469598103934665603ull;
        const auto bytes=[&](const void* p,std::size_t n){for(std::size_t i=0;i<n;++i){hash^=static_cast<const unsigned char*>(p)[i];hash*=1099511628211ull;}};
        for(const auto& mesh:s.meshes){for(const auto& v:mesh.vertices){bytes(&v.position,sizeof(v.position));bytes(&v.normal,sizeof(v.normal));bytes(&v.uv,sizeof(v.uv));bytes(v.bones.data(),sizeof(v.bones));bytes(v.weights.data(),sizeof(v.weights));}bytes(mesh.indices.data(),mesh.indices.size()*sizeof(std::uint32_t));}
        for(std::size_t i=0;i<s.animations.size();++i){const auto& a=s.animations[i];bytes(a.sourceName.data(),a.sourceName.size());for(float t:{0.f,.37f,1.f}){const auto pose=s.samplePose(i,t*a.durationFrames);for(const auto& m:pose)bytes(m.v.data(),sizeof(m.v));}}
        if(expectedHash&&hash!=*expectedHash){std::cerr<<"POSE/GEOMETRY MISMATCH";return 11;}expectedHash=hash;
        report<<"spawn_round="<<round<<" reference="<<reference<<" ms="<<ms<<" bones="<<s.skeleton.bones.size()<<" meshes="<<s.meshes.size()<<" clips="<<s.animations.size()<<" pose_geometry_hash="<<hash<<'\n';report.flush();
        auto profile=reference?generatedWeaponProfileReference(app,app.assetCatalog.entries[app.selectedWeaponAsset]):generatedWeaponProfile(app,app.assetCatalog.entries[app.selectedWeaponAsset]);
        if(!weapon::save(profile,output/("profile_"+std::to_string(round)+".iwweapon"),error))return 7;
        for(std::size_t i=0;i<app.bots.size();++i){profile.stats=app.bots[i].cachedWeaponStats;if(!weapon::save(profile,output/("bot_"+std::to_string(round)+"_"+std::to_string(i)+".iwweapon"),error))return 8;}
        if(round){
            if(read(output/"profile_0.iwweapon")!=read(output/("profile_"+std::to_string(round)+".iwweapon")))return 12;
            for(std::size_t i=0;i<app.bots.size();++i)if(read(output/("bot_0_"+std::to_string(i)+".iwweapon"))!=read(output/("bot_"+std::to_string(round)+"_"+std::to_string(i)+".iwweapon")))return 13;
        }
        std::cout<<"ROUND "<<round<<" ms="<<ms<<" hash="<<hash<<std::endl;
    }
    // Serialize complete generated profiles, not only the fields bots happen to
    // consume. Compare 8 actual weapons from each installed family.
    std::map<std::string,int> checked;
    for(const auto& asset:app.assetCatalog.entries){
        if(asset.role!=assets::Role::ViewWeapon||checked[asset.game]>=8)continue;
        auto a=generatedWeaponProfileReference(app,asset),b=generatedWeaponProfile(app,asset);
        if(!weapon::save(a,output/"reference.iwweapon",error)||!weapon::save(b,output/"candidate.iwweapon",error)||read(output/"reference.iwweapon")!=read(output/"candidate.iwweapon")){std::cerr<<"PROFILE MISMATCH "<<asset.name;return 14;}
        ++checked[asset.game];
    }
    for(const auto& [game,count]:checked)report<<"profile_parity "<<game<<' '<<count<<'\n';
    report<<"PASS exact geometry/pose hash and complete profile serialization\n";
    app.renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();return 0;
}
