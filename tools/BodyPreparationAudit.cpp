#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>

// CPU-only load-path evidence. Does not save application settings or exports.
int main(int argc,char** argv)try{
    if(argc!=2){std::cerr<<"output-directory\n";return 2;}
    const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    auto owner=std::make_unique<AppState>();auto& app=*owner;std::string error;
    app.defaultSalukiDirectory=cadence::local_assets::exportPath("");
    if(!assets::appendScan(app.defaultSalukiDirectory/"bo2"/"models"/"playermodels","bo2",app.assetCatalog,error)){std::cerr<<error;return 3;}
    const auto model=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[](const auto& a){return a.role==assets::Role::PlayerModel;});
    if(model==app.assetCatalog.entries.end())return 4;
    const auto base=scene::buildScene(cast::Document::load(model->path),false);
    if(base.skeleton.bones.empty())return 5;
    app.classWorldModelAsset=static_cast<std::size_t>(model-app.assetCatalog.entries.begin());
    app.botAnimationGame="bo2";app.botSystemMode=0;
    std::ofstream parses(out/"parses.csv"),summary(out/"summary.txt"),fingerprints(out/"fingerprints.txt"),stages(out/"stages.csv"),slots(out/"slots.csv");
    stages<<"iteration,workload,stage,inclusive_ms,identity\n";slots<<"iteration,workload,slot,preparation_seconds\n";
    parses<<"iteration,stage,parse_ms,valid,path\n";summary<<"Model "<<model->path.string()<<"\nFirst run is uncontrolled OS-cache state, not guaranteed cold. Second run is warm.\n";
    for(int iteration=0;iteration<2;++iteration){
        const auto run=[&](const char* label,int count,bool sp){
            BodyPreparationTrace trace;
            struct TraceScope {
                BodyPreparationTrace* previous=activeBodyPreparationTrace;
                explicit TraceScope(BodyPreparationTrace& value){activeBodyPreparationTrace=&value;}
                ~TraceScope(){activeBodyPreparationTrace=previous;}
            } traceScope(trace);
            PointBlankPreparationBatch preparation;ImportedBodyPreparationBatch imported;
            {std::lock_guard lock(cadence::content::pathMutex);cadence::content::observedDocuments.clear();}
            const auto start=std::chrono::steady_clock::now();double preparationSeconds{};
            for(int slot=0;slot<count;++slot){
                auto actor=base;
                const auto preparationStart=std::chrono::steady_clock::now();
                if(sp)loadSpBotScenarios(app,actor,"bo2");else appendSupplementalWorldMotion(app,actor);
                const auto slotSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-preparationStart).count();
                preparationSeconds+=slotSeconds;slots<<iteration<<','<<label<<','<<slot<<','<<slotSeconds<<'\n';
                std::uint64_t hash=1469598103934665603ull;
                const auto bytes=[&](const void* p,std::size_t n){const auto* b=static_cast<const unsigned char*>(p);for(std::size_t i=0;i<n;++i){hash^=b[i];hash*=1099511628211ull;}};
                const auto value=[&](const auto& v){bytes(&v,sizeof(v));};
                for(const auto& clip:actor.animations){
                    for(const auto* text:{&clip.name,&clip.sourceName,&clip.sourceGame}){bytes(text->data(),text->size());const unsigned char end=0;value(end);}
                    value(clip.framerate);value(clip.durationFrames);value(clip.domain);value(clip.action);value(clip.motion);value(clip.stance);value(clip.direction);value(clip.looping);value(clip.contextual);
                    for(const auto& track:clip.tracks){value(track.boneIndex);value(track.property);value(track.mode);value(track.ownsLayer);value(track.additiveWeight);for(auto f:track.frames)value(f);for(auto f:track.scalarValues)value(f);for(const auto& q:track.rotationValues){value(q.x);value(q.y);value(q.z);value(q.w);}}
                }
                for(std::size_t a=0;a<actor.animations.size();++a)for(float fraction:{0.f,.5f,1.f})for(const auto& matrix:actor.samplePose(a,actor.animations[a].durationFrames*fraction))for(float v:matrix.v)value(v);
                fingerprints<<iteration<<' '<<label<<' '<<slot<<' '<<actor.animations.size()<<' '<<hash<<'\n';
                if(actor.animations.empty())throw std::runtime_error(std::string(label)+" produced no clips");
            }
            const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            std::map<std::filesystem::path,int> counts;double parseMs{};
            for(const auto& p:trace.parses){++counts[p.path];parseMs+=p.milliseconds;parses<<iteration<<','<<label<<','<<p.milliseconds<<','<<p.valid<<','<<std::quoted(p.path.string())<<'\n';}
            for(const auto& stage:trace.stages)stages<<iteration<<','<<label<<','<<std::quoted(stage.name)<<','<<stage.milliseconds<<','<<std::quoted(stage.identity)<<'\n';
            std::size_t repeats{};for(const auto& [path,count]:counts)if(count>1)repeats+=count-1;
            summary<<iteration<<' '<<label<<" preparation_seconds="<<preparationSeconds<<" seconds_including_pose_checks="<<elapsed<<" parses="<<trace.parses.size()<<" unique="<<counts.size()<<" repeated="<<repeats<<" parse_ms="<<parseMs<<'\n';summary.flush();
            std::ofstream dependencies(out/(std::to_string(iteration)+"_"+label+"_dependencies.txt"));for(const auto& p:cadence::content::constructionDocuments())dependencies<<p.generic_string()<<'\n';
            std::cout<<iteration<<' '<<label<<" parses="<<trace.parses.size()<<" repeated="<<repeats<<std::endl;
        };
        // Three class slots share one transaction; SP preparation is a separate
        // bot rebuild and happens once, not artificially repeated per bot.
        run("supplemental",3,false);run("sp",1,true);
    }
    // Exact logical path, two pack owners inside one transaction. No exported
    // fixture files are rewritten; map aliases to existing distinct documents.
    const auto other=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[&](const auto& a){return a.role==assets::Role::PlayerModel&&a.path!=model->path;});
    if(other==app.assetCatalog.entries.end())throw std::runtime_error("Need two model documents for scope regression");
    const std::filesystem::path logical="audit/exported_files/bo2/models/body.cast";
    cadence::content::PathMount packA,packB;packA.id="audit-a";packB.id="audit-b";
    packA.paths[cadence::content::key(logical)]=model->path;
    packB.paths[cadence::content::key(logical)]=other->path;
    cadence::content::mountPaths({packA,packB});
    struct MountCleanup { ~MountCleanup(){cadence::content::mountPaths({});} } mounts;
    BodyPreparationTrace scopedTrace;
    struct ScopedTraceCleanup { ~ScopedTraceCleanup(){activeBodyPreparationTrace=nullptr;} } cleanup;
    activeBodyPreparationTrace=&scopedTrace;
    const auto clearObserved=[](){std::lock_guard lock(cadence::content::pathMutex);cadence::content::observedDocuments.clear();};
    const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    {
        PointBlankPreparationBatch batch;
        cadence::content::ResolutionScope scopeA("audit-a");clearObserved();
        {cadence::content::ObservationPause pause;require(pointBlankReferenceDocument(logical).sourceName()==model->path.string(),"pack A source mismatch");}
        require(cadence::content::constructionDocuments().empty(),"paused miss observed dependency");
        auto a=pointBlankReferenceDocument(logical);require(a.valid(),"pack A invalid");
        require(cadence::content::constructionDocuments()==std::vector<std::filesystem::path>{model->path},"valid hit did not reobserve");
        clearObserved();{cadence::content::ObservationPause pause;pointBlankReferenceDocument(logical);}
        require(cadence::content::constructionDocuments().empty(),"paused hit observed dependency");
        {cadence::content::ResolutionScope scopeB("audit-b");auto b=pointBlankReferenceDocument(logical);require(b.valid()&&b.sourceName()==other->path.string(),"pack B aliased pack A");}
        require(pointBlankReferenceDocument(logical).sourceName()==model->path.string(),"nested pack restoration mismatch");
        require(scopedTrace.parses.size()==2,"request cache reparsed a valid hit");
    }
    {PointBlankPreparationBatch nextRequest;cadence::content::ResolutionScope scopeA("audit-a");pointBlankReferenceDocument(logical);}
    require(scopedTrace.parses.size()==3,"document cache escaped request lifetime");
    summary<<"PASS pack scopes, valid-hit dependency reobservation, observation pause, request boundary\n";
    // Both aliases are existing
    // complete bodies, with different native MP/SP binds.
    const auto spBody=app.defaultSalukiDirectory/"bo2_sp/models/playermodels/seal6/c_usa_seal6_ass_sqrl_haper_wt_fb/c_usa_seal6_ass_sqrl_haper_wt_fb_LOD0.cast";
    require(std::filesystem::is_regular_file(spBody),"Missing different-bind donor fixture");
    packB.paths[cadence::content::key(logical)]=spBody;cadence::content::mountPaths({packA,packB});
    auto donorOwner=std::make_unique<AppState>();auto& donorApp=*donorOwner;
    donorApp.assetCatalog.entries.push_back(*model);donorApp.assetCatalog.entries[0].path=logical;
    std::optional<scene::Skeleton> firstDonor,cachedSecond,expectedSecond;
    {
        PointBlankPreparationBatch batch;
        {cadence::content::ResolutionScope scope("audit-a");firstDonor=pointBlankDonor(donorApp,"bo2");}
        {cadence::content::ResolutionScope scope("audit-b");cachedSecond=pointBlankDonor(donorApp,"bo2");expectedSecond=playerSkeletonForGame(donorApp,"bo2");}
    }
    require(firstDonor&&cachedSecond&&expectedSecond,"Missing donor fixture skeleton");
    const auto aKey=cadence::prepared_animation::skeletonKey("",*firstDonor),bKey=cadence::prepared_animation::skeletonKey("",*expectedSecond),cachedKey=cadence::prepared_animation::skeletonKey("",*cachedSecond);
    require(aKey!=bKey,"Donor fixtures do not have different binds");
    require(cachedKey==bKey,"donor cache ignored pack scope");
    summary<<"PASS donor pack A to B: cached_matches_uncached="<<(cachedKey==bKey)<<" cached_matches_A="<<(cachedKey==aKey)<<'\n';
    {
        PointBlankPreparationBatch batch;std::optional<scene::Skeleton> nativeA,nativeB;
        {cadence::content::ResolutionScope scope("audit-a");nativeA=pointBlankNativeBody(logical);}
        {cadence::content::ResolutionScope scope("audit-b");nativeB=pointBlankNativeBody(logical);}
        require(nativeA&&nativeB&&cadence::prepared_animation::skeletonKey("",*nativeA)==aKey&&cadence::prepared_animation::skeletonKey("",*nativeB)==bKey,"native reference cache ignored pack");
    }
    require(!app.botAnimationCache["bo2"].empty(),"missing imported donor clip fixture");
    const auto& donorClip=app.botAnimationCache["bo2"].front();
    {
        ImportedBodyPreparationCache cache;donorApp.gameReferenceSetups["bo2"].playermodelAsset=0;
        std::filesystem::path selectedA,selectedB;
        {cadence::content::ResolutionScope scope("audit-a");selectedA=importedBodyDonor(donorApp,cache,donorClip,"bo2").path;}
        {cadence::content::ResolutionScope scope("audit-b");selectedB=importedBodyDonor(donorApp,cache,donorClip,"bo2").path;}
        require(selectedA==model->path&&selectedB==spBody,"imported selection cache ignored pack");
    }
    // Same game/catalog and same outer transaction, changing only the explicit
    // source reference. Physical alias B deliberately has a distinct bind.
    donorApp.assetCatalog.entries.push_back(donorApp.assetCatalog.entries.front());
    donorApp.assetCatalog.entries[0].path=model->path;donorApp.assetCatalog.entries[1].path=spBody;
    {
        PointBlankPreparationBatch batch;
        const auto selected=[&](std::size_t index){donorApp.gameReferenceSetups["bo2"].playermodelAsset=index;auto actual=pointBlankDonor(donorApp,"bo2"),expected=playerSkeletonForGame(donorApp,"bo2");require(actual&&expected,"configured donor unavailable");const auto key=cadence::prepared_animation::skeletonKey("",*actual);require(key==cadence::prepared_animation::skeletonKey("",*expected),"configured donor differs from uncached");return key;};
        const auto first=selected(0),second=selected(1),again=selected(0);require(first!=second&&first==again,"configured donor A-B-A cache mismatch");
        clearObserved();{cadence::content::ObservationPause pause;pointBlankDonor(donorApp,"bo2");}require(cadence::content::constructionDocuments().empty(),"paused donor hit observed dependencies");
        pointBlankDonor(donorApp,"bo2");require(!cadence::content::constructionDocuments().empty(),"donor hit lost dependencies");
    }
    {
        ImportedBodyPreparationCache cache;
        for(std::size_t index:{0u,1u,0u}){donorApp.gameReferenceSetups["bo2"].playermodelAsset=index;
            const auto selected=importedBodyDonor(donorApp,cache,donorClip,"bo2");
            require(selected.path==donorApp.assetCatalog.entries[index].path,"imported selection ignored configured reference");
        }
    }
    const auto oldContext=cadence::content::resolverContextKey();
    cadence::content::mountPaths({packA,packB});require(oldContext!=cadence::content::resolverContextKey(),"mount revision unchanged");
    auto prior=cadence::content::resolverContextKey();cadence::content::preferPack("audit-b");require(prior!=cadence::content::resolverContextKey(),"preference revision unchanged");
    prior=cadence::content::resolverContextKey();cadence::content::setDisabledRoots({});require(prior!=cadence::content::resolverContextKey(),"disabled-root revision unchanged");
    prior=cadence::content::resolverContextKey();cadence::content::setFallbackExportRoot({});require(prior!=cadence::content::resolverContextKey(),"fallback revision unchanged");
    summary<<"PASS configured donor A-B-A, paused donor dependencies, resolver revisions, imported/native scoped caches\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 9;}
