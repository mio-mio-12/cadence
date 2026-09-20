// Load-time only: no catalog scans or document reads in the frame loop.
std::optional<cadence::world_weapon::Fields> t6WorldWeaponFields(const assets::Asset& world){
    using namespace cadence::world_weapon;
    static const auto index=[](){
        std::unordered_map<std::string,Fields> result;
        const std::filesystem::path root=cadence::portable::t6WeaponArchive();
        std::error_code error;
        for(std::filesystem::directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,error),end;it!=end;it.increment(error)){
            if(error){error.clear();continue;}if(!it->is_regular_file(error))continue;
            std::ifstream input(it->path(),std::ios::binary);const std::string data((std::istreambuf_iterator<char>(input)),{});
            auto fields=weaponFields(data);const auto model=fields.find("worldModel");
            if(model!=fields.end()&&!model->second.empty())result.emplace(modelIdentity(model->second),std::move(fields));
        }return result;
    }();
    const auto found=index.find(modelIdentity(world.name));if(found==index.end())return {};return found->second;
}

void appendWorldWeaponParts(const AppState& app,std::size_t viewIndex,std::size_t worldIndex,
                            const scene::CastScene& base,scene::CastScene& actor,std::size_t baseAttachment){
    using namespace cadence::world_weapon;
    if(viewIndex>=app.assetCatalog.entries.size()||worldIndex>=app.assetCatalog.entries.size()||baseAttachment>=actor.attachments.size())return;
    const auto& view=app.assetCatalog.entries[viewIndex];const auto& world=app.assetCatalog.entries[worldIndex];
    if(lowerText(world.game)=="cs2")return;
    const auto source=lowerText(world.game)=="bo2"?t6WorldWeaponFields(world):std::nullopt;
    const auto parent=actor.attachments[baseAttachment];
    std::unordered_set<std::string> added;
    for(const auto viewPartIndex:app.assetCatalog.compatibleAttachments(viewIndex)){
        const auto& viewPart=app.assetCatalog.entries[viewPartIndex];
        if(viewPart.role!=assets::Role::ViewAttachment||!shouldAutoLoadRigPart(app,view,viewPart))continue;
        const auto identity=modelIdentity(viewPart.name);
        if(added.contains(identity))continue;
        // Exact counterpart preserves weapon and cosmetic variant tokens. A
        // broad compatible-key match can borrow an unrelated 8bit/camo scope.
        const auto candidate=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[&](const auto& asset){
            return asset.role==assets::Role::WorldAttachment&&lowerText(asset.game)==lowerText(world.game)&&modelIdentity(asset.name)==identity;
        });
        if(candidate==app.assetCatalog.entries.end())continue;
        const auto document=cast::Document::load(candidate->path);if(!document.valid())continue;
        auto part=scene::buildScene(document);if(part.meshes.empty())continue;
        auto transform=socketTransform(base,part);
        if(!transform&&source){
            const auto root=std::find_if(part.skeleton.bones.begin(),part.skeleton.bones.end(),[](const auto& b){return b.parent<0;});
            if(root!=part.skeleton.bones.end()&&std::count_if(part.skeleton.bones.begin(),part.skeleton.bones.end(),[](const auto& b){return b.parent<0;})==1){
                const auto rootName=scene::canonicalName(root->name);
                // Magazine source must explicitly name this exact world model.
                const auto clip=source->find("worldClipModel");
                if(rootName!="tag_clip"||(clip!=source->end()&&modelIdentity(clip->second)==identity))
                    if(const auto station=t6Station(*source,rootName))transform=*station*scene::inverseAffine(root->restGlobal);
            }
        }
        if(!transform)continue;
        const auto before=actor.attachments.size();scene::appendPreparedAttachment(std::move(part),actor,parent.boneIndex,candidate->name);
        if(before<actor.attachments.size()){setTransform(actor.attachments[before],parent.localMatrix()**transform);added.insert(identity);}
    }
}
