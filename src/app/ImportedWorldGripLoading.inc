// Included after PointBlankWorldLoader.inc. Declare ImportedWorldGripBatch
// beside PointBlankPreparationBatch for each synchronous class/bot assembly.
// Caches are transaction-owned: re-exports are reread on the next load.
struct ImportedWorldGripReference {scene::CastScene actor;std::vector<scene::Mat4> pose;std::string error;};
struct ImportedWorldGripCache {std::map<std::string,ImportedWorldGripReference> holds;assets::AnimationFileIndex index;};
thread_local ImportedWorldGripCache* activeImportedWorldGrip=nullptr;
struct ImportedWorldGripBatch {
    ImportedWorldGripCache cache;ImportedWorldGripCache* previous=activeImportedWorldGrip;
    ImportedWorldGripBatch(){if(!previous)activeImportedWorldGrip=&cache;}
    ~ImportedWorldGripBatch(){activeImportedWorldGrip=previous;}
};
ImportedWorldGripReference prepareImportedWorldGripReference(AppState& app,const std::string& game,bool pistol){
    ImportedWorldGripReference result;
    if(game=="css"){result.error="CSS native held weapon registration has not been verified; no native player hold clips are exported";return result;}
    std::vector<const assets::Asset*> candidates;
    for(const auto& a:app.assetCatalog.entries)if(a.role==assets::Role::PlayerModel&&lowerText(a.game)==game)candidates.push_back(&a);
    std::sort(candidates.begin(),candidates.end(),[](const auto* a,const auto* b){return a->name<b->name;});
    assets::AnimationFileIndex localIndex;auto& index=activeImportedWorldGrip?activeImportedWorldGrip->index:localIndex;
    for(const auto* body:candidates){
        auto reference=scene::buildScene(pointBlankReferenceDocument(body->path),false);
        const auto socket=game=="eldewrito"?scene::imported::world_grip::bone(reference.skeleton,{"dew2cast_socket__right_hand__0"}):game=="css"?scene::imported::world_grip::bone(reference.skeleton,{"ValveBiped.weapon_bone"}):game=="cso2"?scene::imported::world_grip::bone(reference.skeleton,{"CSO2_W_bone"}):game=="bo2"?scene::imported::world_grip::bone(reference.skeleton,{"tag_weapon_right"}):-1;
        if(socket<0)continue;
        const auto folder=animationFolderFor(*body);if(!folder)continue;
        std::vector<std::filesystem::path> clips;
        for(const auto& e:index.files(*folder)){
            const auto& n=e.lowerFilename;
            const std::string family=pistol?"pistol":"rifle";
            const bool matches=game=="eldewrito"?n.starts_with(lowerText(body->name)+"_combat_"+family+"_")&&assets::imported::dewBodySemantic(n)=="pb_"+family+"_stand_idle":
                game=="bo2"?n==(pistol?"pb_stand_alert_pistol.cast":"pb_stand_alert.cast"):
                n.starts_with("pb_"+family+"_stand_idle_clip");
            if(matches)clips.push_back(e.path);
        }
        std::sort(clips.begin(),clips.end());
        for(const auto& clip:clips){scene::appendAnimations(pointBlankReferenceDocument(clip),reference);if(reference.animations.empty())continue;
            const auto& a=reference.animations.back();const auto wrist=scene::imported::world_grip::rightPalm(reference.skeleton).wrist;
            if(wrist<0||a.tracks.empty()||std::none_of(a.tracks.begin(),a.tracks.end(),[&](const auto& t){return t.boneIndex==static_cast<std::size_t>(wrist);})){reference.animations.clear();continue;}
            result.pose=reference.samplePose(reference.animations.size()-1,0);reference.animations.clear();reference.meshes.clear();result.actor=std::move(reference);return result;
        }
        if(game=="css"&&scene::imported::world_grip::bone(reference.skeleton,{"ValveBiped.weapon_bone_RHand"})>=0){result.pose=scene::imported::world_grip::restPose(reference.skeleton);reference.animations.clear();reference.meshes.clear();result.actor=std::move(reference);return result;}
    }
    result.error="No verified native holding reference for "+game;return result;
}
bool attachImportedViewFallback(AppState& app,const assets::Asset& view,scene::CastScene& actor){
    auto gun=scene::buildScene(pointBlankReferenceDocument(view.path));
    if(gun.meshes.empty())return false;
    if(const auto folder=animationFolderFor(view)){
        assets::AnimationFileIndex local;auto& index=activeImportedWorldGrip?activeImportedWorldGrip->index:local;
        for(const auto& file:index.files(*folder)){
            if(file.lowerFilename.find("idle")==std::string::npos||!animationSourceMatchesWeapon(file.path,view,&app.assetCatalog))continue;
            scene::appendAnimations(pointBlankReferenceDocument(file.path),gun);
            if(!gun.animations.empty())break;
        }
    }
    const auto pose=gun.animations.empty()?scene::imported::world_grip::restPose(gun.skeleton):gun.samplePose(gun.animations.size()-1,0);
    const float units=assets::imported::sourceFamily(lowerText(view.game))&&gun.importedTranslationScale==1.f?2.54f:1.f;
    const auto type=weapon::inferArchetype(view.name);const bool pistol=type==weapon::Archetype::Pistol||type==weapon::Archetype::DualWield;
    const auto key=std::string("bo2")+(pistol?":pistol":":rifle");
    ImportedWorldGripReference local;const ImportedWorldGripReference* reference{};
    if(activeImportedWorldGrip){auto& cache=activeImportedWorldGrip->holds;auto it=cache.find(key);if(it==cache.end())it=cache.emplace(key,prepareImportedWorldGripReference(app,"bo2",pistol)).first;reference=&it->second;}
    else{local=prepareImportedWorldGripReference(app,"bo2",pistol);reference=&local;}
    const bool longGun=assets::imported::sourceFamily(lowerText(view.game))&&
        (type==weapon::Archetype::Rifle||type==weapon::Archetype::Smg||type==weapon::Archetype::Lmg||
         type==weapon::Archetype::Shotgun||type==weapon::Archetype::BoltSniper||
         type==weapon::Archetype::BoltIndividualSniper||type==weapon::Archetype::SemiSniper);
    auto mount=scene::imported::world_grip::fromViewHold(gun,pose,actor.skeleton,units,reference->error.empty()?&reference->actor.skeleton:nullptr,reference->pose,longGun);
    if(!mount){
        // The calibrated preparer may have baked geometry before rejecting a
        // transform. Retry from the cached document, never bake that twice.
        gun=scene::buildScene(pointBlankReferenceDocument(view.path));
        std::string error;
        mount=scene::imported::world_grip::fromAuthoredAnchor(gun,pose,actor.skeleton,units,error);
        if(!mount){actor.warnings.push_back("Imported world placeholder: "+view.name+": "+error);return false;}
        actor.warnings.push_back("Imported world placeholder: "+view.name+": approximate authored-anchor mount (native grip unavailable)");
    }
    const auto before=actor.attachments.size();
    scene::appendPreparedAttachment(std::move(gun),actor,mount->bone,view.name);
    if(actor.attachments.size()==before)return false;
    cadence::world_weapon::setTransform(actor.attachments.back(),mount->transform);
    return true;
}
// true means this imported pairing was handled, including an explicit failure;
// do not fall back to an arbitrary CoD socket when no native reference exists.
bool attachImportedWorldWeapon(AppState& app,const cast::Document& world,std::size_t viewIndex,scene::CastScene& actor,const std::string& targetGame){
    if(viewIndex>=app.assetCatalog.entries.size())return false;
    const auto& view=app.assetCatalog.entries[viewIndex];const auto game=lowerText(view.game);
    const bool importedTarget=static_cast<bool>(scene::imported::bodyLayout(actor.skeleton));
    if(game=="cs2"||lowerText(targetGame)=="cs2"||(!assets::imported::supported(game)&&!(game=="bo2"&&importedTarget)))return false;
    if(assets::imported::supported(game)&&std::filesystem::path(world.sourceName())==view.path){
        // Failure is still handled: never send raw first-person geometry and
        // its arms to the legacy root/socket attachment path.
        if(!attachImportedViewFallback(app,view,actor))actor.warnings.push_back("Imported world grip: "+view.name+": no safe viewmodel placeholder");
        return true;
    }
    const auto archetype=weapon::inferArchetype(view.name);const bool pistol=archetype==weapon::Archetype::Pistol||archetype==weapon::Archetype::DualWield;
    const auto key=game+(pistol?":pistol":":rifle");
    ImportedWorldGripReference local;const ImportedWorldGripReference* reference{};
    if(activeImportedWorldGrip){auto& cache=activeImportedWorldGrip->holds;auto it=cache.find(key);if(it==cache.end()){if(cache.size()>=16)cache.clear();it=cache.emplace(key,prepareImportedWorldGripReference(app,game,pistol)).first;}reference=&it->second;}
    else {local=prepareImportedWorldGripReference(app,game,pistol);reference=&local;}
    std::string error=reference->error;std::optional<scene::imported::world_grip::Mount> mount;
    if(error.empty())mount=scene::imported::world_grip::fromNativeSocket(game,scene::buildScene(world,false),reference->actor.skeleton,reference->pose,actor.skeleton,error);
    if(!mount){if(assets::imported::supported(game)&&attachImportedViewFallback(app,view,actor))return true;actor.warnings.push_back("Imported world grip: "+view.name+": "+error);return true;}
    const auto before=actor.attachments.size();scene::appendAttachment(world,actor,mount->bone,assets::imported::utf8(std::filesystem::path(world.sourceName()).stem()));
    if(actor.attachments.size()>before)cadence::world_weapon::setTransform(actor.attachments.back(),mount->transform);
    return true;
}
