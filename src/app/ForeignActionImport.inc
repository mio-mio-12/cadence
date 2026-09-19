// Native CODM action -> existing legacy composite. Calibration is per load,
// with no source CAST rewrites and no per-frame filesystem work.
bool retargetForeignCodmAction(AppState& app,const std::filesystem::path& path,size_t first,const cast::Document* parsed){
    const auto* entries=&app.assetCatalog.entries;
    // A saved default may be restored with only BO2 selected. Discover the
    // required native reference models without loading a foreign clip library.
    if(std::none_of(entries->begin(),entries->end(),[](const auto& a){return lowerText(a.game)=="codm"&&a.role==assets::Role::ViewHands;})){
        auto root=path.parent_path();while(!root.empty()&&lowerText(root.filename().string())!="codm"){auto next=root.parent_path();if(next==root)break;root=next;}
        if(!root.empty()&&lowerText(root.filename().string())=="codm"){
            static std::map<std::filesystem::path,assets::Catalog> catalogs;auto found=catalogs.find(root);
            if(found==catalogs.end()){assets::Catalog catalog;std::string error;if(assets::appendScan(root,"codm",catalog,error))found=catalogs.emplace(root,std::move(catalog)).first;}
            if(found!=catalogs.end())entries=&found->second.entries;
        }
    }
    const assets::Asset* donor=nullptr;const auto filename=lowerText(path.filename().string());
    for(const auto& a:*entries)if(lowerText(a.game)=="codm"&&a.role==assets::Role::ViewWeapon&&filename.starts_with(lowerText(a.name)+"_")&&(!donor||a.name.size()>donor->name.size()))donor=&a;
    const auto fail=[&](const std::string& reason){for(size_t i=first;i<app.scene.animations.size();++i)app.scene.animations[i].tracks.clear();app.scene.warnings.push_back("CODM action not adapted: "+path.filename().string()+": "+reason);return true;};
    if(!donor)return fail("matching native weapon reference unavailable");
    const auto hand=std::find_if(entries->begin(),entries->end(),[](const auto&a){return lowerText(a.game)=="codm"&&lowerText(a.name).find("c_m_ghost")!=std::string::npos&&a.role==assets::Role::ViewHands;});
    if(hand==entries->end())return fail("native Ghost hands reference unavailable");
    size_t targetIdle=SIZE_MAX;const auto mapping=app.weaponProfile.animations.find("idle");
    for(size_t i=0;i<first;++i){const auto& a=app.scene.animations[i];if((mapping!=app.weaponProfile.animations.end()&&a.sourceName==mapping->second)||(!app.viewmodelAnimationPrefix.empty()&&a.sourceName==app.viewmodelAnimationPrefix+"idle.cast")){targetIdle=i;break;}}
    if(targetIdle==SIZE_MAX)return fail("load the recipient weapon idle first");
    auto idlePath=path.parent_path()/(donor->name+"_idle.cast");if(!std::filesystem::is_regular_file(idlePath))idlePath=path.parent_path()/(donor->name+"_pose.cast");
    std::string cacheKey;for(const auto& file:{donor->path,hand->path,idlePath}){std::error_code ec;const auto stamp=std::filesystem::last_write_time(file,ec);cacheKey+=file.string()+"#"+(ec?"missing":std::to_string(stamp.time_since_epoch().count()));}
    static std::map<std::string,std::shared_ptr<const scene::CastScene>> references;auto cached=references.find(cacheKey);
    if(cached==references.end()){auto reference=std::make_shared<scene::CastScene>();std::string error;
        if(!scene::codm::assemble(cast::Document::load(donor->path),cast::Document::load(hand->path),*reference,error))return fail(error);
        const auto idle=cast::Document::load(idlePath);if(!idle.valid()||!scene::appendAnimations(idle,*reference))return fail("native idle/pose reference missing");reference->meshes.clear();cached=references.emplace(cacheKey,std::move(reference)).first;
    }
    auto source=std::make_shared<scene::CastScene>(*cached->second);
    const auto sourceRef=source->samplePose(0,0);const auto targetLocal=app.scene.sampleLocalPose(targetIdle,0);const auto targetRef=app.scene.globalPose(targetLocal);
    const auto doc=parsed?*parsed:cast::Document::load(path);const auto begin=source->animations.size();if(!scene::appendAnimations(doc,*source))return fail("native action is empty");
    source->meshes.clear();
    std::map<std::string,std::string> names;
    for(const auto side:{std::pair{"le","Left"},std::pair{"ri","Right"}}){for(const auto joint:{std::pair{"clavicle","Clav"},std::pair{"shoulder","Arm"},std::pair{"elbow","ForeArm"},std::pair{"wrist","Hand"}})names["j_"+std::string(joint.first)+"_"+side.first]="b_"+std::string(side.second)+joint.second;
        for(const auto f:{std::pair{"index","Index"},std::pair{"mid","Middle"},std::pair{"ring","Ring"},std::pair{"pinky","Pinky"},std::pair{"thumb","Thumb"}})for(int n=0;n<3;++n)names["j_"+std::string(f.first)+"_"+side.first+"_"+std::to_string(n)]="b_"+std::string(side.second)+f.second+std::to_string(n+1);
    }
    names["tag_weapon"]=source->skeleton.boneByName.contains("Muzzle_point")?"Muzzle_point":"Bone_RightHand";
    if(!source->skeleton.boneByName.contains(names["tag_weapon"])||!app.scene.skeleton.boneByCanonicalName.contains("tag_weapon"))return fail("verified weapon carrier correspondence missing");
    const auto basis=scene::fromEulerRadians({0,0,scene::kPi*.5f});
    for(size_t i=first;i<app.scene.animations.size()&&begin+i-first<source->animations.size();++i){
        auto adapter=std::make_shared<scene::ColdWarWorldPose>();adapter->source=source;adapter->animation=begin+i-first;adapter->targetReference=targetLocal;adapter->sourceActionReference=sourceRef;adapter->targetActionReference=targetRef;adapter->actionBasis=basis;
        auto& clip=app.scene.animations[i];clip=source->animations[adapter->animation];clip.sourceGame="codm";clip.tracks.clear();size_t mapped=0;
        for(size_t b=0;b<app.scene.skeleton.bones.size();++b){const auto key=scene::canonicalName(app.scene.skeleton.bones[b].name);int driver=-1;if(const auto n=names.find(key);n!=names.end())if(const auto s=source->skeleton.boneByName.find(n->second);s!=source->skeleton.boneByName.end())driver=static_cast<int>(s->second);
            adapter->sourceBones.push_back(driver);scene::Quat offset;
            const bool position=key=="tag_weapon"||key.starts_with("j_shoulder_")||key.starts_with("j_elbow_")||key.starts_with("j_wrist_");adapter->globalTranslation.push_back(position);
            if(driver>=0){++mapped;scene::Vec3 p,s;scene::Quat from,to;scene::decomposeAffine(sourceRef[driver],p,from,s);scene::decomposeAffine(targetRef[b],p,to,s);from=scene::multiply(basis,from);offset=scene::multiply(scene::Quat{-from.x,-from.y,-from.z,from.w},to);
                for(auto property:{scene::TrackProperty::Rotation,scene::TrackProperty::TranslationX,scene::TrackProperty::TranslationY,scene::TrackProperty::TranslationZ}){if(property!=scene::TrackProperty::Rotation&&!position)continue;scene::Track t;t.boneIndex=b;t.property=property;t.frames={0};if(property==scene::TrackProperty::Rotation)t.rotationValues={{}};else t.scalarValues={0};clip.tracks.push_back(std::move(t));}
            }adapter->rotationOffsets.push_back(offset);
        }
        if(mapped<30)return fail("insufficient anatomical correspondence");adapter->prepareSampling(app.scene.skeleton);clip.coldWarWorldPose=std::move(adapter);
    }
    app.scene.warnings.push_back("CODM cross-weapon action uses native/recipient idle calibration; unmatched weapon mechanisms keep recipient idle. Validate grip before saving.");return true;
}
