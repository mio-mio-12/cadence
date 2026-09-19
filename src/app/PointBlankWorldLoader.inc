// One synchronous assembly transaction. No cache survives into a later load,
// so changed/re-exported assets are always reread without timestamp assumptions.
struct PointBlankPreparationCache {
    assets::AnimationFileIndex animationIndex;
    std::map<std::filesystem::path,cast::Document> documents;
    std::map<std::pair<std::filesystem::path,std::string>,std::filesystem::path> clips;
    std::map<std::string,scene::CastScene> weapons;
    std::map<std::string,std::optional<scene::Skeleton>> donors;
};
thread_local PointBlankPreparationCache* activePointBlankPreparation=nullptr;
struct PointBlankPreparationBatch {
    PointBlankPreparationCache cache;
    PointBlankPreparationCache* previous=activePointBlankPreparation;
    PointBlankPreparationBatch(){activePointBlankPreparation=&cache;}
    ~PointBlankPreparationBatch(){activePointBlankPreparation=previous;}
};
cast::Document pointBlankReferenceDocument(const std::filesystem::path& path){
    if(!activePointBlankPreparation)return cast::Document::load(path);
    auto& docs=activePointBlankPreparation->documents;
    auto it=docs.find(path);if(it==docs.end())it=docs.emplace(path,cast::Document::load(path)).first;
    return it->second;
}
std::filesystem::path pointBlankReferenceClip(const std::filesystem::path& folder,const std::string& desired,bool insensitive){
    const auto key=std::make_pair(folder,(insensitive?"i:":"s:")+desired);
    if(activePointBlankPreparation)if(auto it=activePointBlankPreparation->clips.find(key);it!=activePointBlankPreparation->clips.end())return it->second;
    std::filesystem::path result;std::error_code ec;
    for(std::filesystem::recursive_directory_iterator it(folder,ec),end;it!=end;it.increment(ec)){
        if(ec){ec.clear();continue;}
        const auto name=it->path().filename().string();
        if((insensitive?lowerText(name):name)==desired){result=it->path();break;}
    }
    if(activePointBlankPreparation)activePointBlankPreparation->clips.emplace(key,result);
    return result;
}
std::optional<scene::Skeleton> playerSkeletonForGame(const AppState&,const std::string&);
std::optional<scene::Skeleton> pointBlankDonor(const AppState& app,const std::string& game){
    if(!activePointBlankPreparation)return playerSkeletonForGame(app,game);
    auto& donors=activePointBlankPreparation->donors;auto it=donors.find(game);
    if(it==donors.end())it=donors.emplace(game,playerSkeletonForGame(app,game)).first;
    return it->second;
}
void configureLegacyWeaponOnPointBlank(AppState& app,scene::CastScene& actor,size_t attachment,const std::string& game,std::string poseGame,weapon::Archetype archetype){
    if(!scene::pointblank::body(actor.skeleton)||attachment>=actor.attachments.size()||game=="pointblank"||game=="codm"||game=="cs2")return;
    poseGame=lowerText(poseGame);if(poseGame=="bo2+bo2_sp")poseGame="bo2";
    if(poseGame.empty()||poseGame=="pointblank"||poseGame=="*")poseGame=game;
    const auto donor=pointBlankDonor(app,poseGame);if(!donor)return;
    const auto wrist=donor->boneByCanonicalName.find("j_wrist_ri"),socket=donor->boneByCanonicalName.find("tag_weapon_right");
    if(wrist==donor->boneByCanonicalName.end()||socket==donor->boneByCanonicalName.end())return;
    const auto target=actor.skeleton.boneByName.at("R Hand");
    const auto sourceWrist=scene::transformPoint(donor->bones[wrist->second].restGlobal,{});
    const auto targetWrist=scene::transformPoint(actor.skeleton.bones[target].restGlobal,{});
    // Bind sockets are not grip sockets (T6 keeps its bind weapon near the hip).
    // Sample an actual source hold, then express its socket in the target wrist.
    if(archetype==weapon::Archetype::Unknown)archetype=weapon::inferArchetype(actor.attachments[attachment].name);
    const bool pistol=archetype==weapon::Archetype::Pistol||archetype==weapon::Archetype::DualWield;
    const std::string desired=pistol?"pb_stand_alert_pistol.cast":"pb_stand_alert.cast";
    std::filesystem::path clip;
    for(const auto& asset:app.assetCatalog.entries)if(lowerText(asset.game)==poseGame){if(auto folder=animationFolderFor(asset)){
        clip=pointBlankReferenceClip(*folder,desired,true);
    }break;}
    if(clip.empty()){actor.warnings.push_back("PB world grip reference missing for "+poseGame+": "+desired);return;}
    scene::CastScene reference;reference.skeleton=*donor;scene::appendAnimations(pointBlankReferenceDocument(clip),reference);
    if(reference.animations.empty())return;const auto pose=reference.samplePose(0,0);
    const auto basis=scene::inverseAffine(actor.skeleton.bones[target].restGlobal)*scene::translation(targetWrist-sourceWrist)*donor->bones[wrist->second].restGlobal;
    const auto mount=basis*scene::inverseAffine(pose[wrist->second])*pose[socket->second];
    auto& a=actor.attachments[attachment];scene::Vec3 scale;scene::Quat rotation;
    scene::decomposeAffine(mount,a.position,rotation,scale);
    a.rotationDegrees=scene::Vec3{std::atan2(2*(rotation.w*rotation.x+rotation.y*rotation.z),1-2*(rotation.x*rotation.x+rotation.y*rotation.y)),std::asin(std::clamp(2*(rotation.w*rotation.y-rotation.z*rotation.x),-1.f,1.f)),std::atan2(2*(rotation.w*rotation.z+rotation.x*rotation.y),1-2*(rotation.y*rotation.y+rotation.z*rotation.z))}*(180.f/scene::kPi);a.scale=scale;
}
bool appendPointBlankWorldAnimation(AppState& app,const cast::Document& document,scene::CastScene& target,const std::string& game,const std::string& targetGame={}){
    const bool native=lowerText(game)=="pointblank",pbTarget=scene::pointblank::body(target.skeleton)||scene::codm::worldBody(target.skeleton);
    const bool ghostsCross=lowerText(targetGame)=="ghosts"&&lowerText(game)!="ghosts"&&lowerText(game)!="cs2";
    if(!native&&!pbTarget&&!ghostsCross)return false;
    const auto filename=lowerText(std::filesystem::path(document.sourceName()).filename().string());
    // UI ceremony/preview poses are not combat locomotion.
    if(native&&filename.find("_ui_")!=std::string::npos)return true;
    std::optional<scene::Skeleton> skeleton;
    if(native){
        const auto wanted=filename.find("female")!=std::string::npos?"playermode_swat_female_swat-sniper_fb":"playermode_swat_male_fb";
        for(const auto& asset:app.assetCatalog.entries)if(lowerText(asset.game)=="pointblank"&&lowerText(asset.name)==wanted){
            static std::map<std::filesystem::path,scene::Skeleton> references;auto it=references.find(asset.path);if(it==references.end()){auto doc=cast::Document::load(asset.path);if(doc.valid())it=references.emplace(asset.path,scene::buildScene(doc,false).skeleton).first;}
            if(it!=references.end())skeleton=it->second;break;
        }
    }else {static std::map<std::string,scene::Skeleton> references;auto it=references.find(game);if(it==references.end())if(auto found=playerSkeletonForGame(app,game))it=references.emplace(game,*found).first;if(it!=references.end())skeleton=it->second;}
    if(!skeleton){target.warnings.push_back("Missing native world reference for "+game+": "+filename);return true;}
    auto source=std::make_shared<scene::CastScene>();source->skeleton=*skeleton;source->pointBlankNativeCentimetres=native;
    scene::appendAnimations(document,*source);if(source->animations.empty())return true;
    // PB exports dual-handgun idle overlays separately. Each contains the
    // other arm's parked pose; selecting one as a complete torso drops that arm.
    // Pair only matching idle/move-idle files, preserving each arm's own tracks.
    if(native&&filename.find("dualhandgun")!=std::string::npos&&filename.find("attackidle")!=std::string::npos){
        auto path=std::filesystem::path(document.sourceName());auto stem=path.stem().string();
        const bool isLeft=filename.ends_with("_left.cast"),isRight=filename.ends_with("_right.cast");
        if(isLeft||isRight){stem.resize(stem.size()-(isLeft?4:5));stem+=isLeft?"Right":"Left";const auto paired=path.parent_path()/(stem+".cast");
            if(std::filesystem::is_regular_file(paired)){
                if(isLeft)return true;
                scene::appendAnimations(pointBlankReferenceDocument(paired),*source);
                if(source->animations.size()>1){auto& right=source->animations[0];const auto& left=source->animations[1];
                    std::erase_if(right.tracks,[&](const auto& t){return source->skeleton.bones[t.boneIndex].name.starts_with("L ");});
                    for(auto track:left.tracks)if(source->skeleton.bones[track.boneIndex].name.starts_with("L ")){
                        for(auto& frame:track.frames)frame=static_cast<std::uint32_t>(std::lround(frame*right.framerate/std::max(1.f,left.framerate)));
                        right.tracks.push_back(std::move(track));
                    }
                    right.durationFrames=std::max(right.durationFrames,static_cast<std::uint32_t>(std::lround(left.durationFrames*right.framerate/std::max(1.f,left.framerate))));source->animations.resize(1);
                }
            }
        }
    }
    // One-sided dual-pistol shots also contain the other arm's parked pose.
    // Keep that arm in its authored aim idle rather than dropping its gun on
    // every shot. This is composed in native space before any rig conversion.
    if(native&&filename.find("dualhandgun")!=std::string::npos&&source->animations[0].action==scene::ActionRole::Fire){
        const bool left=filename.ends_with("_left.cast"),right=filename.ends_with("_right.cast");
        const auto marker=filename.rfind("_attack_");
        if((left||right)&&marker!=std::string::npos){
            const auto path=std::filesystem::path(document.sourceName());
            const auto idleName=path.stem().string().substr(0,marker)+"_AttackIdle_"+(left?"Right":"Left")+".cast";
            auto idle=path.parent_path()/idleName;
            if(!std::filesystem::is_regular_file(idle))idle=pointBlankReferenceClip(salukiExportRoot(app.defaultSalukiDirectory)/"pointblank/animations/pt",idleName,false);
            if(std::filesystem::is_regular_file(idle)){
                scene::appendAnimations(pointBlankReferenceDocument(idle),*source);
                if(source->animations.size()>1){const auto pose=source->sampleLocalPose(1,0);auto& action=source->animations[0];const std::string prefix=left?"R ":"L ";
                    std::erase_if(action.tracks,[&](const auto& t){return source->skeleton.bones[t.boneIndex].name.starts_with(prefix);});
                    for(size_t b=0;b<pose.size();++b)if(source->skeleton.bones[b].name.starts_with(prefix)){
                        scene::Track rotation;rotation.boneIndex=b;rotation.mode=scene::TrackMode::Absolute;rotation.property=scene::TrackProperty::Rotation;rotation.frames={0};rotation.rotationValues={pose[b].rotation};action.tracks.push_back(std::move(rotation));
                        for(auto [property,value]:{std::pair{scene::TrackProperty::TranslationX,pose[b].position.x},std::pair{scene::TrackProperty::TranslationY,pose[b].position.y},std::pair{scene::TrackProperty::TranslationZ,pose[b].position.z}}){scene::Track t;t.boneIndex=b;t.mode=scene::TrackMode::Absolute;t.property=property;t.frames={0};t.scalarValues={value};action.tracks.push_back(std::move(t));}
                    }
                    source->animations.resize(1);
                }
            }
        }
    }
    const auto first=target.animations.size();target.animations.resize(first+source->animations.size());
    for(auto& a:source->animations)a.sourceGame=game;
    scene::pointblank::bridgeWorld(target,first,source,0);return true;
}

// Fill absent ordinary holds/reloads only. Native slots stay native; contextual
// last-stand or weapon-swap poses are never substitutes for normal actions.
void appendMissingWorldHolds(AppState& app,scene::CastScene& actor,const std::string& game,const std::string& targetGame){
    const auto key=lowerText(game);
    if(key!="aw"&&key!="mwr"&&key!="mw3"&&key!="ghosts"&&key!="mw")return;
    for(auto motion:{scene::MotionRole::Idle,scene::MotionRole::Run,scene::MotionRole::Unknown})for(auto stance:{scene::Stance::Stand,scene::Stance::Crouch})for(auto weapon:{scene::WeaponClass::Rifle,scene::WeaponClass::Pistol,scene::WeaponClass::DualWield,scene::WeaponClass::Knife}){
        // Missing pistol locomotion must not fall through to a two-handed rifle carry.
        if(motion==scene::MotionRole::Run&&(stance!=scene::Stance::Stand||weapon==scene::WeaponClass::Rifle))continue;
        if(motion==scene::MotionRole::Unknown&&weapon==scene::WeaponClass::Knife)continue;
        scene::AnimationQuery query;query.domain=scene::AnimationDomain::PlayerBody;query.motion=motion;query.stance=stance;query.weapon=weapon;
        const bool reload=motion==scene::MotionRole::Unknown;
        if(reload){query.domain=scene::AnimationDomain::PlayerTorso;query.action=scene::ActionRole::Reload;}
        if(const auto existing=scene::findBestAnimation(actor,query);existing&&actor.animations[*existing].weapon==weapon)continue;
        std::string name=reload?std::string("pt_reload_")+(stance==scene::Stance::Crouch?"crouch":"stand")+(weapon==scene::WeaponClass::Pistol?"_pistol.cast":"_rifle.cast"):motion==scene::MotionRole::Run?"pb_pistol_run_fast.cast":std::string(stance==scene::Stance::Crouch?"pb_crouch_alert":"pb_stand_alert")+(weapon==scene::WeaponClass::Pistol?"_pistol.cast":".cast");
        if(weapon==scene::WeaponClass::DualWield)name=reload?(stance==scene::Stance::Crouch?"pt_crouch_dw_botharms_reload.cast":"pt_dw_botharms_reload.cast"):motion==scene::MotionRole::Run?"pb_dw_run_forward.cast":stance==scene::Stance::Crouch?"pb_crouch_dw_idle_v1.cast":"pb_dw_botharms_idle_v1.cast";
        if(weapon==scene::WeaponClass::Knife)name=motion==scene::MotionRole::Run?"pb_knife_run_forward.cast":stance==scene::Stance::Crouch?"pb_knife_crouch_idle.cast":"pb_knife_stand_idle.cast";
        const auto path=pointBlankReferenceClip(salukiExportRoot(app.defaultSalukiDirectory)/(reload?"bo2/animations/pt/reload":"bo2/animations/pb/stand"),name,true);
        auto found=path;if(found.empty())found=pointBlankReferenceClip(salukiExportRoot(app.defaultSalukiDirectory)/"bo2/animations/pb",name,true);
        if(found.empty()){actor.warnings.push_back("Missing ordinary world hold: "+name);continue;}
        const auto first=actor.animations.size();const auto doc=pointBlankReferenceDocument(found);
        if(!appendPointBlankWorldAnimation(app,doc,actor,"bo2",targetGame))scene::appendAnimations(doc,actor);
        for(size_t i=first;i<actor.animations.size();++i)actor.animations[i].sourceGame="bo2";
        actor.warnings.push_back("Missing native hold/action supplemented with BO2: "+name);
    }
    for(auto weapon:{scene::WeaponClass::DualWield,scene::WeaponClass::Knife})for(auto stance:{scene::Stance::Stand,scene::Stance::Crouch}){
        scene::AnimationQuery query;query.domain=scene::AnimationDomain::PlayerTorso;query.stance=stance;query.weapon=weapon;
        query.action=weapon==scene::WeaponClass::Knife?scene::ActionRole::Melee:scene::ActionRole::Fire;
        if(const auto existing=scene::findBestAnimation(actor,query);existing&&actor.animations[*existing].weapon==weapon)continue;
        const std::string name=weapon==scene::WeaponClass::Knife?(stance==scene::Stance::Crouch?"pt_knife_crouch_melee_swipe.cast":"pt_knife_stand_melee_swipe.cast"):(stance==scene::Stance::Crouch?"pt_crouch_dw_botharms_fire.cast":"pt_dw_botharms_fire.cast");
        const auto path=pointBlankReferenceClip(salukiExportRoot(app.defaultSalukiDirectory)/"bo2/animations/pt",name,true);
        if(path.empty()){actor.warnings.push_back("Missing ordinary world action: "+name);continue;}
        const auto first=actor.animations.size();const auto doc=pointBlankReferenceDocument(path);
        if(!appendPointBlankWorldAnimation(app,doc,actor,"bo2",targetGame))scene::appendAnimations(doc,actor);
        for(size_t i=first;i<actor.animations.size();++i)actor.animations[i].sourceGame="bo2";
        actor.warnings.push_back("Missing native hold/action supplemented with BO2: "+name);
    }
}

int pointBlankWorldGripBone(const scene::Skeleton& skeleton){
    if(const auto it=skeleton.boneByCanonicalName.find("j_wrist_ri");it!=skeleton.boneByCanonicalName.end())return static_cast<int>(it->second);
    if(const auto it=skeleton.boneByName.find("R Hand");it!=skeleton.boneByName.end())return static_cast<int>(it->second);
    return preferredAttachmentBone(skeleton);
}
std::optional<scene::Mat4> fitPointBlankWorldGrip(AppState& app,const scene::CastScene& native,const std::vector<scene::Mat4>& nativePose,const scene::Skeleton& target,size_t targetBone,bool pistol){
    // Calibrate once at assembly against a real hold, not the relaxed bind pose.
    // Initial rollout is CODM bodies: these have the two reviewed reference grips.
    if(!scene::codm::worldBody(target)||targetBone>=target.bones.size())return {};
    const auto donor=pointBlankDonor(app,"bo2");if(!donor)return {};
    const auto folder=app.defaultSalukiDirectory/"bo2/animations/pb/stand";
    const auto path=pointBlankReferenceClip(folder,pistol?"pb_stand_alert_pistol.cast":"pb_stand_alert.cast",true);if(path.empty())return {};
    auto source=std::make_shared<scene::CastScene>();source->skeleton=*donor;scene::appendAnimations(pointBlankReferenceDocument(path),*source);if(source->animations.empty())return {};
    scene::CastScene reference;reference.skeleton=target;reference.animations.resize(1);scene::pointblank::bridgeWorld(reference,0,source,0);
    const auto pose=reference.samplePose(0,0);const auto tw=scene::inverseAffine(pose[targetBone]);
    const auto wrist=native.skeleton.boneByName.find("R Hand");if(wrist==native.skeleton.boneByName.end())return {};
    const auto sw=scene::inverseAffine(nativePose[wrist->second]);std::vector<scene::Vec3> from,to;
    for(auto finger:{std::pair{"Thumb","thumb"},std::pair{"Index","index"},std::pair{"Middle","mid"},std::pair{"Ring","ring"},std::pair{"Little","pinky"}})for(int level=1;level<=2;++level){
        auto s=native.skeleton.boneByName.find("R "+std::string(finger.first)+std::to_string(level));auto t=target.boneByCanonicalName.find("j_"+std::string(finger.second)+"_ri_"+std::to_string(level));
        if(s==native.skeleton.boneByName.end()||t==target.boneByCanonicalName.end())continue;
        from.push_back(scene::transformPoint(sw*nativePose[s->second],{}));to.push_back(scene::transformPoint(tw*pose[t->second],{}));
    }
    if(from.size()!=10)return {};auto fit=scene::fitGripLandmarks(from,to);if(!fit)return {};
    float error=0;for(size_t i=0;i<from.size();++i){auto d=scene::transformPoint(*fit,from[i])-to[i];error+=scene::dot(d,d);}if(std::sqrt(error/from.size())>4.f)return {};
    return *fit*scene::pointBlankGripSurfaceCalibration(pistol)*sw;
}
scene::CastScene preparePointBlankWorldWeaponUncached(AppState& app,const cast::Document& weapon,const scene::Skeleton& target,std::size_t targetBone){
    scene::CastScene assembled;std::string error;const auto hand=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[](const auto& a){return lowerText(a.game)=="pointblank"&&a.name=="viewmodel_SWAT_Male_hands";});
    if(hand==app.assetCatalog.entries.end()||!scene::pointblank::assemble(weapon,pointBlankReferenceDocument(hand->path),assembled,error)){assembled.warnings.push_back("PB world proxy unavailable: native hand reference missing");return assembled;}
    const auto stem=std::filesystem::path(weapon.sourceName()).stem().string();std::filesystem::path idle;
    const auto targetLeft=target.boneByCanonicalName.find("j_wrist_le");
    const bool left=targetBone<target.bones.size()&&((targetLeft!=target.boneByCanonicalName.end()&&targetLeft->second==targetBone)||target.bones[targetBone].name=="L Hand"||target.bones[targetBone].name=="b_LeftHand");
    const bool dual=assembled.skeleton.boneByName.contains("pb2cast_weapon__left__GunDummy");
    if(const auto folder=animationFolderFor(*hand))idle=pointBlankReferenceClip(*folder,stem+"_AttackIdle.cast",false);
    std::filesystem::path leftIdle;
    if(idle.empty()&&dual)if(const auto folder=animationFolderFor(*hand)){
        idle=pointBlankReferenceClip(*folder,stem+"_AttackIdle_Right.cast",false);
        leftIdle=pointBlankReferenceClip(*folder,stem+"_AttackIdle_Left.cast",false);
        if(leftIdle.empty())idle.clear();
    }
    if(idle.empty()){assembled.meshes.clear();assembled.warnings.push_back("PB world proxy unavailable: matching idle absent");return assembled;}
    scene::appendAnimations(pointBlankReferenceDocument(idle),assembled);if(assembled.animations.empty()){assembled.meshes.clear();return assembled;}
    if(!leftIdle.empty())scene::appendAnimations(pointBlankReferenceDocument(leftIdle),assembled);
    auto local=assembled.sampleLocalPose(0,0);
    if(!leftIdle.empty()&&assembled.animations.size()>1){const auto l=assembled.sampleLocalPose(1,0);for(size_t b=0;b<local.size();++b){const auto& name=assembled.skeleton.bones[b].name;if(name.starts_with("L ")||name.starts_with("pb2cast_weapon__left__"))local[b]=l[b];}}
    const auto pose=assembled.globalPose(local);const auto wrist=assembled.skeleton.boneByName.at(left?"L Hand":"R Hand");auto mount=scene::inverseAffine(pose[wrist]);
    if(targetBone<target.bones.size()&&!scene::pointblank::body(target)){
        // Use the body corresponding to the native hand reference, never the
        // alphabetically first exported character (which changes on new rips).
        const auto body=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[](const auto&a){return lowerText(a.game)=="pointblank"&&lowerText(a.name)=="playermode_swat_male_fb";});
        if(body!=app.assetCatalog.entries.end()){
            const auto reference=scene::buildScene(pointBlankReferenceDocument(body->path),false).skeleton;
            const auto from=reference.boneByName.find(left?"L Hand":"R Hand");
            if(from!=reference.boneByName.end()){scene::Vec3 p,s;scene::Quat q,t;scene::decomposeAffine(reference.bones[from->second].restGlobal,p,q,s);scene::decomposeAffine(target.bones[targetBone].restGlobal,p,t,s);mount=scene::trs({},scene::multiply(scene::Quat{-t.x,-t.y,-t.z,t.w},q),{1,1,1})*mount;}
        }else assembled.warnings.push_back("PB world grip reference body missing: SWAT Male");
    }
    const auto lowerStem=lowerText(stem);
    if(!dual&&weapon::inferArchetype(stem)!=weapon::Archetype::DualWield&&(lowerStem.starts_with("viewmodel_pistol_")||lowerStem.starts_with("viewmodel_ar_"))){
        if(auto fitted=fitPointBlankWorldGrip(app,assembled,pose,target,targetBone,lowerStem.starts_with("viewmodel_pistol_"))){mount=*fitted;assembled.warnings.push_back("PB/CODM world grip: rigid fit of ten posed finger landmarks (no weapon scaling)");}
        else if(scene::codm::worldBody(target))assembled.warnings.push_back("PB/CODM posed grip fit unavailable; retaining legacy wrist mount");
    }
    std::erase_if(assembled.meshes,[](const auto& m){return !m.viewmodelWeapon;});
    if(dual)std::erase_if(assembled.meshes,[&](const auto& m){
        float leftWeight=0,totalWeight=0;for(const auto& v:m.vertices)for(size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0&&v.bones[k]<assembled.skeleton.bones.size()){
            totalWeight+=v.weights[k];if(assembled.skeleton.bones[v.bones[k]].name.starts_with("pb2cast_weapon__left__"))leftWeight+=v.weights[k];
        }
        return (leftWeight>totalWeight*.5f)!=left;
    });
    std::vector<scene::Mat4> skinMatrices(pose.size());for(size_t b=0;b<pose.size();++b)skinMatrices[b]=mount*pose[b]*assembled.skeleton.bones[b].inverseBind;
    for(auto& m:assembled.meshes){for(auto& v:m.vertices){scene::Vec3 p{},n{};for(size_t k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){const auto& skin=skinMatrices[v.bones[k]];p+=scene::transformPoint(skin,v.position)*v.weights[k];auto rotated=skin;rotated.v[12]=rotated.v[13]=rotated.v[14]=0;n+=scene::transformPoint(rotated,v.normal)*v.weights[k];}v.position=p;v.normal=scene::normalize(n);}m.modelTransform=scene::Mat4::identity();m.viewmodelWeapon=false;}
    const auto barrel=dual?cadence::dual::muzzle(assembled,pose,left?1:0):scene::resolveMuzzlePosition(assembled,pose);
    assembled.skeleton={};assembled.animations.clear();
    if(barrel){scene::Bone b;b.name="tag_flash";b.restLocal.position=scene::transformPoint(mount,*barrel);b.restGlobal=scene::translation(b.restLocal.position);b.inverseBind=scene::inverseAffine(b.restGlobal);assembled.skeleton.bones.push_back(b);assembled.skeleton.boneByName[b.name]=0;assembled.skeleton.boneByCanonicalName[b.name]=0;}
    assembled.warnings.push_back("Point Blank shared world weapon: native idle grip fitted to the actor wrist");return assembled;
}
scene::CastScene preparePointBlankWorldWeapon(AppState& app,const cast::Document& weapon,const scene::Skeleton& target,std::size_t targetBone){
    if(!activePointBlankPreparation)return preparePointBlankWorldWeaponUncached(app,weapon,target,targetBone);
    // The grasp fit consumes the complete target hierarchy, including fingers.
    // Never reuse one body's fit merely because another wrist matrix matches.
    std::string key=weapon.sourceName();key.push_back('\0');
    const bool fitted=targetBone<target.bones.size()&&!scene::pointblank::body(target);
    key.push_back(fitted?'1':'0');
    key.append(reinterpret_cast<const char*>(&targetBone),sizeof(targetBone));
    if(fitted)for(const auto& b:target.bones){key+=b.name;key.push_back('\0');key.append(reinterpret_cast<const char*>(&b.parent),sizeof(b.parent));for(float v:b.restGlobal.v)key.append(reinterpret_cast<const char*>(&v),sizeof(v));}
    auto& cache=activePointBlankPreparation->weapons;
    auto it=cache.find(key);if(it!=cache.end())return it->second;
    auto result=preparePointBlankWorldWeaponUncached(app,weapon,target,targetBone);
    if(!result.meshes.empty())cache.emplace(std::move(key),result);
    return result;
}

void appendPointBlankLeftWorldWeapon(AppState& app,const cast::Document& weapon,scene::CastScene& actor,const std::string& name){
    // pb2cast supplies explicit left/right namespaces. Keep each gun on its own wrist.
    const auto source=scene::buildScene(weapon,false);
    if(!source.skeleton.boneByName.contains("pb2cast_weapon__left__GunDummy"))return;
    const auto left=actor.skeleton.boneByCanonicalName.find("j_wrist_le");
    if(left==actor.skeleton.boneByCanonicalName.end()){actor.warnings.push_back("PB dual world proxy needs a left wrist: "+name);return;}
    auto prepared=preparePointBlankWorldWeapon(app,weapon,actor.skeleton,left->second);
    scene::appendPreparedAttachment(std::move(prepared),actor,left->second,name);
}
