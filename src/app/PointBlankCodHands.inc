bool preferredIwReferenceHands(const std::string& game,const std::string& name){
 // IW's generic base is a different legacy bind, not the IW7 animation driver.
 return (game=="iw_sp"||game=="iw7"||game=="iw")&&
        (name=="viewmodel_base_viewhands_iw7"||name.starts_with("viewmodel_base_viewhands_iw7_lod"));
}
// BO1/T5 and BO2 use the legacy CoD first-person driver. Existing BO1 loads
// already bind its clips to BO2 hands directly; use the same driver for foreign
// skins when the BO1 reference model has not been exported. Never substitute
// this for IW7/Source/other incompatible native animation palettes.
std::filesystem::path boCompatibleReferenceHands(const AppState& app,std::string& error){
    const auto choose=[](const assets::Catalog& catalog){
        const assets::Asset* best=nullptr;int rank=-1;
        for(const auto& a:catalog.entries)if(lowerText(a.game)=="bo2"&&a.role==assets::Role::ViewHands){
            const auto name=lowerText(a.name);const int score=name.starts_with("c_usa_mp_seal6_longsleeve_viewhands")?100:name.starts_with("viewmodel_base_viewhands")?80:1;
            if(score>rank){rank=score;best=&a;}
        }
        return best?best->path:std::filesystem::path{};
    };
    auto path=choose(app.assetCatalog);if(!path.empty())return path;
    if(!app.defaultSalukiDirectory.empty()){assets::Catalog catalog;if(assets::appendScan(salukiExportRoot(app.defaultSalukiDirectory)/"bo2","bo2",catalog,error))return choose(catalog);}
    return {};
}
bool preparePointBlankCodHands(AppState& app,const cast::Document& hands,std::string game,scene::CastScene& output,std::string& error){
 if(game=="bocw_sp"||game=="cs2")game="bo2";
 if(game!="bo2"&&game!="bo"&&game!="mw"&&game!="mw2"&&game!="mw3"&&game!="ghosts"&&game!="aw"&&game!="iw_sp"&&game!="mwr")return false;
 const auto choose=[&](const assets::Catalog& catalog)->std::filesystem::path{
  const assets::Asset* best=nullptr;int score=-1;
  for(const auto& a:catalog.entries)if(a.game==game&&a.role==assets::Role::ViewHands){const auto name=lowerText(a.name);const int rank=preferredIwReferenceHands(game,name)?200:name.starts_with("viewmodel_base_viewhands")?100:name.starts_with("c_usa_mp_seal6_longsleeve_viewhands")?100:1;if(rank>score){best=&a;score=rank;}}
  return best?best->path:std::filesystem::path{};
 };
 auto path=choose(app.assetCatalog);
 if(path.empty()&&!app.defaultSalukiDirectory.empty()){assets::Catalog catalog;assets::appendScan(salukiExportRoot(app.defaultSalukiDirectory)/game/"models",game,catalog,error);path=choose(catalog);}
 if(path.empty()&&(game=="bo"||game=="t5"))path=boCompatibleReferenceHands(app,error);
 if(path.empty()){error="Missing native "+game+" reference viewhands";return false;}
 const auto doc=cast::Document::load(path);if(!doc.valid()){error="Cannot read native reference hands";return false;}
 auto driver=scene::buildScene(doc);
 if(!scene::pointblank::fitToCod(driver,hands,error))return false;
 driver.viewHandsDriverGame=game;output=std::move(driver);return true;
}
