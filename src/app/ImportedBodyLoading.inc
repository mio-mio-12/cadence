// Include after AppState and ImportedBodyRetarget.h, before the legacy body
// loader. Imported body routing shares the native assembly transaction.
// Scope ImportedBodyPreparationBatch beside each PointBlankPreparationBatch.
std::string bodyDonorContextKey(const AppState&,const std::string&);
struct ImportedBodyPreparationCache {
    struct Selection { std::filesystem::path path;std::string error; };
    std::map<std::filesystem::path,std::shared_ptr<const scene::CastScene>> bodies;
    std::map<std::filesystem::path,std::vector<bool>> requiredBones;
    std::map<std::string,Selection> selections;
};
thread_local ImportedBodyPreparationCache* activeImportedBodyPreparation=nullptr;
struct ImportedBodyPreparationBatch {
    ImportedBodyPreparationCache cache;
    ImportedBodyPreparationCache* previous=activeImportedBodyPreparation;
    ImportedBodyPreparationBatch(){if(!previous)activeImportedBodyPreparation=&cache;}
    ~ImportedBodyPreparationBatch(){activeImportedBodyPreparation=previous;}
};

static std::shared_ptr<const scene::CastScene> importedBodyReference(ImportedBodyPreparationCache& cache,const std::filesystem::path& path){
    const auto physical=cadence::content::resolve(path);
    if(const auto found=cache.bodies.find(physical);found!=cache.bodies.end()){
        if(found->second)cadence::content::observeDocument(physical);
        return found->second;
    }
    std::shared_ptr<const scene::CastScene> result;
    try{const auto doc=cast::Document::load(path);if(doc.valid()){
        auto body=std::make_shared<scene::CastScene>(scene::buildScene(doc,false));
        if(scene::imported::completeBodyLayout(scene::imported::bodyOrCodLayout(body->skeleton))){
            cache.requiredBones[physical]=cadence::body_donor::requiredBones(*body);
            body->meshes.clear();body->attachments.clear();body->rigParts.clear();body->animations.clear();body->runtimePoseAdapters.clear();
            result=std::move(body);
        }
    }}catch(const std::exception&){}
    cache.bodies.emplace(physical,result);return result;
}

static bool importedBodySameBind(const scene::Skeleton& a,const scene::Skeleton& b){
    if(a.bones.size()!=b.bones.size())return false;
    for(std::size_t i=0;i<a.bones.size();++i){const auto& ab=a.bones[i];const auto found=b.boneByName.find(ab.name);if(found==b.boneByName.end()||found->second!=i)return false;
        const auto& bb=b.bones[found->second];
        const auto parentName=[](const scene::Skeleton& s,int p){return p>=0&&static_cast<std::size_t>(p)<s.bones.size()?s.bones[p].name:std::string{};};
        if(parentName(a,ab.parent)!=parentName(b,bb.parent))return false;
        for(std::size_t k=0;k<16;++k)if(std::abs(ab.restGlobal.v[k]-bb.restGlobal.v[k])>1e-4f)return false;
    }return true;
}

static ImportedBodyPreparationCache::Selection importedBodyDonor(const AppState& app,ImportedBodyPreparationCache& cache,const cast::Document& document,const std::string& game){
    const auto filename=assets::imported::lower(assets::imported::utf8(std::filesystem::path(document.sourceName()).filename()));
    std::vector<std::string> curves;
    const auto visit=[&](const auto& self,const cast::Node& node)->void{
        if(cast::nodeTypeName(node.identifier)=="Curve")if(const auto* name=node.findProperty("nn");name&&name->stringValue)curves.push_back(*name->stringValue);
        for(const auto& child:node.children)self(self,child);
    };
    for(const auto& root:document.roots())visit(visit,root);
    std::sort(curves.begin(),curves.end());
    std::string identity;
    if(game=="eldewrito"){
        const auto marker=filename.find("_combat_");
        if(marker==std::string::npos)return {{},"Unsupported Eldewrito body action identity"};
        identity=filename.substr(0,marker);
    }
    std::string key=bodyDonorContextKey(app,game)+":"+identity;
    for(const auto& name:curves){key+='\n';key+=name;}
    if(const auto found=cache.selections.find(key);found!=cache.selections.end())return found->second;
    ImportedBodyPreparationCache::Selection selected;
    std::shared_ptr<const scene::CastScene> best;std::size_t bestCoverage{};bool ambiguous=false;
    std::vector<const assets::Asset*> candidates;
    for(const auto& asset:app.assetCatalog.entries)if(asset.role==assets::Role::PlayerModel&&assets::imported::lower(asset.game)==game&&
        (identity.empty()||assets::imported::lower(asset.name)==identity))candidates.push_back(&asset);
    // An explicitly configured reference can resolve an otherwise ambiguous
    // same-name rig, but still must satisfy the actual clip-coverage threshold.
    // Halo's per-character export identity is stricter and remains authoritative.
    if(identity.empty())if(const auto configured=app.gameReferenceSetups.find(game);configured!=app.gameReferenceSetups.end()){
        const auto index=configured->second.playermodelAsset;
        if(index<app.assetCatalog.entries.size()){
            const auto* preferred=&app.assetCatalog.entries[index];
            if(std::find(candidates.begin(),candidates.end(),preferred)!=candidates.end())candidates={preferred};
        }
    }
    // BO2's shared body library has a known full-body reference. Different
    // soldier proportions are not ambiguous animation identities: use the
    // reference bind unless the user explicitly chose another donor above.
    // The same anatomy and curve-coverage checks below still apply.
    if(game=="bo2"&&candidates.size()>1){
        const auto reference=std::find_if(candidates.begin(),candidates.end(),[](const auto* asset){
            const auto name=assets::imported::lower(asset->name);
            return name=="c_usa_mp_isa_smg_fb"||name=="c_usa_mp_isa_smg_fb_lod0";
        });
        if(reference!=candidates.end())candidates={*reference};
    }
    std::sort(candidates.begin(),candidates.end(),[](const auto* a,const auto* b){return a->path.native()<b->path.native();});
    for(const auto* asset:candidates){const auto physical=cadence::content::resolve(asset->path);const auto body=importedBodyReference(cache,physical);if(!body)continue;
        std::size_t covered{};for(const auto& name:curves)covered+=body->skeleton.boneByName.contains(name)||body->skeleton.boneByCanonicalName.contains(scene::canonicalName(name));
        if(covered>bestCoverage){bestCoverage=covered;best=body;selected.path=physical;ambiguous=false;}
        else if(covered&&covered==bestCoverage&&best&&!cadence::body_donor::equivalent(best->skeleton,cache.requiredBones.at(selected.path),body->skeleton,cache.requiredBones.at(physical)))ambiguous=true;
    }
    if(curves.empty()||!best||bestCoverage*100<curves.size()*90){selected.path.clear();selected.error="No native body covers at least 90% of animation curves";}
    else if(ambiguous){selected.path.clear();selected.error="Ambiguous native body bind; matching curve names alone do not identify its rig";}
    cache.selections.emplace(std::move(key),selected);return selected;
}

// true means consumed OR intentionally rejected. false is only for unrelated
// existing games, allowing their current path to continue. A rejected imported
// animation must never fall through to raw local-curve binding.
bool appendImportedWorldAnimation(AppState& app,const cast::Document& document,scene::CastScene& target,const std::string& sourceGame,const std::string& targetGame){
    const auto game=assets::imported::lower(sourceGame),destination=assets::imported::lower(targetGame);
    const bool importedTarget=static_cast<bool>(scene::imported::bodyLayout(target.skeleton));
    if(!importedTarget&&!assets::imported::supported(game)&&!assets::imported::supported(destination))return false;
    const auto reject=[&](const std::string& reason){const auto warning="Imported body animation: "+reason;
        if(std::find(target.warnings.begin(),target.warnings.end(),warning)==target.warnings.end())target.warnings.push_back(warning);return true;};
    if(!scene::imported::completeBodyLayout(scene::imported::bodyOrCodLayout(target.skeleton)))return reject("unsupported target anatomy");
    // Foreign conventional rigs need an explicit reference unless a verified
    // shared BO2 donor exists. Never bind their local tracks directly.
    const auto& donorGame=game;
    const auto configured=app.gameReferenceSetups.find(donorGame);
    const bool explicitDonor=configured!=app.gameReferenceSetups.end()&&configured->second.playermodelAsset<app.assetCatalog.entries.size();
    if(donorGame!="bo2"&&!assets::imported::supported(game)&&!explicitDonor)return reject("select a native reference body for donor game "+game);
    if(importedTarget&&game==destination){
        // A selected native character is itself the authoritative reference.
        // Shared pb_* labels need not redundantly encode the character name.
        // Halo's graph exports do carry identity: mismatched graph clips are
        // intentionally skipped while scanning that game's complete library.
        if(game=="eldewrito"){
            const auto name=assets::imported::lower(assets::imported::utf8(std::filesystem::path(document.sourceName()).filename()));
            if(target.importedBodyIdentity.empty()||!name.starts_with(target.importedBodyIdentity+"_"))return true;
        }
        scene::CastScene native;native.skeleton=target.skeleton;native.importedTranslationScale=target.importedTranslationScale;native.importedBodyIdentity=target.importedBodyIdentity;
        try{scene::appendAnimations(document,native);}catch(const std::exception& e){return reject(e.what());}
        for(auto& clip:native.animations){
            if(assets::imported::sourceFamily(game))scene::imported::classifySourceBodyAnimation(clip);
            if((clip.domain!=scene::AnimationDomain::PlayerBody&&clip.domain!=scene::AnimationDomain::PlayerTorso)||clip.contextual){reject("only non-contextual native body clips are enabled");continue;}
            if(clip.tracks.empty()||!clip.sourceCurveCount||clip.unmappedCurveCount*10>clip.sourceCurveCount){reject("selected native body does not cover animation curves");continue;}
            if(clip.domain==scene::AnimationDomain::PlayerTorso){const auto mask=scene::imported::nativeBodyTorsoMask(target.skeleton);for(auto& track:clip.tracks)if(track.boneIndex>=mask.size()||!mask[track.boneIndex])track.ownsLayer=false;}
            clip.sourceGame=sourceGame;target.animations.push_back(std::move(clip));
        }
        return true;
    }
    ImportedBodyPreparationCache localCache;auto& cache=activeImportedBodyPreparation?*activeImportedBodyPreparation:localCache;
    const auto selection=importedBodyDonor(app,cache,document,donorGame);
    if(selection.path.empty())return reject(selection.error);
    const auto reference=importedBodyReference(cache,selection.path);if(!reference)return reject("native donor unavailable");
    auto donor=std::make_shared<scene::CastScene>(*reference);
    try{scene::appendAnimations(document,*donor);}catch(const std::exception& e){return reject(e.what());}
    if(donor->animations.empty())return reject("no native body clip bound");
    for(std::size_t i=0;i<donor->animations.size();++i){auto& clip=donor->animations[i];
        if(assets::imported::sourceFamily(game))scene::imported::classifySourceBodyAnimation(clip);
        if((clip.domain!=scene::AnimationDomain::PlayerBody&&clip.domain!=scene::AnimationDomain::PlayerTorso)||clip.contextual){reject("only non-contextual body clips are enabled");continue;}
        clip.sourceGame=sourceGame;
        // Keep native clips native when both model identity and bind match.
        // Attachment bones can differ, so use the existing exact path only
        // when complete skeleton equality is established.
        if(importedBodySameBind(target.skeleton,donor->skeleton)&&
           (game!="eldewrito"||target.importedBodyIdentity==donor->importedBodyIdentity)){
            target.animations.push_back(clip);
        }else{
            std::string error;if(!scene::imported::appendRetargetedBody(target,donor,i,error))reject(error);
        }
    }
    return true;
}
