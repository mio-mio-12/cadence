bool preparePointBlankCodHands(AppState& app,const cast::Document& hands,std::string game,scene::CastScene& output,std::string& error){
 if(game!="bo2"&&game!="bo"&&game!="mw"&&game!="mw2"&&game!="mw3"&&game!="ghosts"&&game!="aw"&&game!="iw_sp"&&game!="mwr")return false;
 const auto choose=[&](const assets::Catalog& catalog)->std::filesystem::path{
  const assets::Asset* best=nullptr;int score=-1;
  for(const auto& a:catalog.entries)if(a.game==game&&a.role==assets::Role::ViewHands){const auto name=lowerText(a.name);const int rank=name.starts_with("viewmodel_base_viewhands")?100:name.starts_with("c_usa_mp_seal6_longsleeve_viewhands")?100:1;if(rank>score){best=&a;score=rank;}}
  return best?best->path:std::filesystem::path{};
 };
 auto path=choose(app.assetCatalog);
 if(path.empty()&&!app.defaultSalukiDirectory.empty()){assets::Catalog catalog;assets::appendScan(salukiExportRoot(app.defaultSalukiDirectory)/game/"models",game,catalog,error);path=choose(catalog);}
 if(path.empty()){error="Missing native "+game+" reference viewhands";return false;}
 const auto doc=cast::Document::load(path);if(!doc.valid()){error="Cannot read native reference hands";return false;}
 auto driver=scene::buildScene(doc);
 if(!scene::pointblank::fitToCod(driver,hands,error))return false;
 driver.viewHandsDriverGame=game;output=std::move(driver);return true;
}
