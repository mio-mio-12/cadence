// One part list for player previews, gameplay, benchmarks and bot instances.
// Only invoked while assembling; there is no per-frame model parsing/fitting.
std::vector<std::size_t> characterAssemblyParts(const AppState& app,const assets::Asset& body,bool forBot=false){
    using assets::character::Part;
    constexpr auto absent=static_cast<std::size_t>(-1);
    std::vector<std::size_t> result;
    const auto setupIt=app.gameReferenceSetups.find(body.game);
    const AppState::GameReferenceSetup empty;
    const auto& setup=setupIt==app.gameReferenceSetups.end()?empty:setupIt->second;
    const auto add=[&](std::size_t index){
        if(index>=app.assetCatalog.entries.size())return;
        const auto& a=app.assetCatalog.entries[index];
        if(a.game!=body.game||a.path==body.path||std::find(result.begin(),result.end(),index)!=result.end())return;
        if(lowerText(body.game)=="mw3"&&lowerText(a.name).find("head")!=std::string::npos&&!assets::character::headCompatible(body.game,body.name,a.name))return;
        result.push_back(index);
    };
    if(lowerText(body.game)!="aw"){
        if(setup.headAsset!=static_cast<std::size_t>(-2))add(findGenericHeadForPlayermodel(app,body.game,body));
        for(auto index:{setup.helmetAsset,setup.eyewearAsset,setup.glovesAsset,setup.pantsAsset,setup.kneepadAsset,setup.bootsAsset,setup.exoAsset,setup.loadoutAsset,setup.shinguardAsset})add(index);
        for(auto index:setup.customModularParts)if(index<app.assetCatalog.entries.size()){
            const auto& a=app.assetCatalog.entries[index];
            if(a.name.find("head")!=std::string::npos&&!assets::character::headCompatible(body.game,body.name,a.name))continue;
            add(index);
        }
        return result;
    }
    if(assets::character::awPart(body.name)!=Part::Torso)return result;
    std::string headName,pantsName;
    const auto choose=[&](Part type,std::size_t explicitIndex,const char* preferred){
        if(explicitIndex==static_cast<std::size_t>(-2))return absent;
        const auto compatible=[&](const assets::Asset& a){return a.game==body.game&&assets::character::awPart(a.name)==type&&assets::character::genderCompatible(body.name,a.name);};
        if(explicitIndex<app.assetCatalog.entries.size()&&compatible(app.assetCatalog.entries[explicitIndex]))return explicitIndex;
        std::size_t best=absent;int bestScore=-1;
        for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(!compatible(a))continue;
            const auto n=lowerText(a.name);int score=n==preferred?50:0;
            if(type==Part::Headgear||type==Part::Eyewear){
                bool identity=false;
                for(const auto* owner:{"cormack","hanks","martin","waters","basara","coyle","gots","chen","young","barton"})
                    if(n.find(owner)!=std::string::npos){if(headName.find(owner)==std::string::npos){score=-100;break;}identity=true;}
                if(score<0)continue;if(identity)score+=100;
            }
            if(score>bestScore){best=i;bestScore=score;}
        }
        return best;
    };
    const auto headChoice=forBot&&setup.headAsset==static_cast<std::size_t>(-2)?absent:setup.headAsset;
    const auto head=choose(Part::Head,headChoice,"mp_head_cormack_lod0");add(head);if(head<app.assetCatalog.entries.size())headName=lowerText(app.assetCatalog.entries[head].name);
    const auto pants=choose(Part::Pants,setup.pantsAsset,"mp_pants_m_01b_k_lod0");add(pants);if(pants<app.assetCatalog.entries.size())pantsName=lowerText(app.assetCatalog.entries[pants].name);
    add(choose(Part::Gloves,setup.glovesAsset,"mp_glove_03a_lod0"));
    add(choose(Part::Boots,setup.bootsAsset,"mp_boot_01a_lod0"));
    add(choose(Part::Loadout,setup.loadoutAsset,"mp_loadouts_m_01a_lod0"));
    add(choose(Part::Exo,setup.exoAsset,"mp_exo_01a_lod0"));
    if(pantsName.find("_k_")!=std::string::npos||setup.kneepadAsset!=absent)add(choose(Part::Kneepads,setup.kneepadAsset,"mp_kneepad_03a_lod0"));
    if(pantsName.find("_s_")!=std::string::npos||setup.shinguardAsset!=absent)add(choose(Part::Shinguards,setup.shinguardAsset,"mp_shinguard_01a_lod0"));
    if(head<app.assetCatalog.entries.size()||setup.helmetAsset!=absent)add(choose(Part::Headgear,setup.helmetAsset,"mp_headgear_01a_cormack_lod0"));
    if(setup.eyewearAsset!=absent)add(choose(Part::Eyewear,setup.eyewearAsset,""));
    for(auto index:setup.customModularParts)if(index<app.assetCatalog.entries.size()){
        const auto& a=app.assetCatalog.entries[index];const auto type=assets::character::awPart(a.name);
        if(type==Part::None||type==Part::Torso||!assets::character::genderCompatible(body.name,a.name))continue;
        if(type==Part::Head&&headChoice==static_cast<std::size_t>(-2))continue;
        const bool duplicate=std::any_of(result.begin(),result.end(),[&](auto i){return assets::character::awPart(app.assetCatalog.entries[i].name)==type;});
        if(!duplicate)add(index);
    }
    return result;
}
