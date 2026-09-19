bool equipNativePointBlank(AppState& app,std::size_t weaponIndex){
 if(weaponIndex>=app.assetCatalog.entries.size()||lowerText(app.assetCatalog.entries[weaponIndex].game)!="pointblank")return false;
 if(app.selectedBaseAsset>=app.assetCatalog.entries.size()){app.status="Select viewhands first";return true;}
 const auto weapon=app.assetCatalog.entries[weaponIndex];const auto handPath=app.assetCatalog.entries[app.selectedBaseAsset].path;
 const bool legacy=lowerText(app.assetCatalog.entries[app.selectedBaseAsset].game)!="pointblank";
 const bool adapt=legacy||lowerText(handPath.stem().string())!="viewmodel_swat_male_hands";
 auto nativeHand=handPath;
 if(adapt){auto it=std::find_if(app.assetCatalog.entries.begin(),app.assetCatalog.entries.end(),[](const auto&a){return lowerText(a.game)=="pointblank"&&lowerText(a.name)=="viewmodel_swat_male_hands";});if(it==app.assetCatalog.entries.end()){app.status="Point Blank adapter requires SWAT Male native hands in the catalog";return true;}nativeHand=it->path;}
 const auto wd=cast::Document::load(weapon.path),hd=cast::Document::load(nativeHand);scene::CastScene composite;std::string error;
 if(!scene::pointblank::assemble(wd,hd,composite,error)){app.status=error;return true;}
 if(adapt&&!scene::pointblank::fitHands(composite,hd,cast::Document::load(handPath),error)){app.status="Unsupported Point Blank hands: "+error;return true;}
 const auto folder=animationFolderFor(weapon);std::vector<std::filesystem::path> files;std::error_code ec;const auto prefix=lowerText(weapon.name)+"_";
 if(folder)for(std::filesystem::recursive_directory_iterator it(*folder,ec),end;it!=end;it.increment(ec)){if(ec){ec.clear();continue;}auto name=lowerText(it->path().filename().string());if(it->is_regular_file(ec)&&name.starts_with(prefix)&&name.ends_with(".cast")&&name.find("_weapon _ ")==std::string::npos)files.push_back(it->path());}
 std::sort(files.begin(),files.end());std::vector<cast::Document> documents;std::vector<std::filesystem::path> validFiles;
 for(auto&p:files){auto d=cast::Document::load(p);if(!d.valid()){composite.warnings.push_back("Invalid Point Blank clip: "+p.string());continue;}auto first=composite.animations.size();scene::appendAnimations(d,composite);tagAnimationGame(composite,first,p);documents.push_back(std::move(d));validFiles.push_back(p);}files=std::move(validFiles);
 cadence::pointblank_actions::prepare(composite);
 if(composite.animations.empty()){app.status="No matching Point Blank viewmodel clips";return true;}
 loadFile(app,handPath);app.scene=std::move(composite);app.animationSources=std::move(files);app.animationDocuments=std::move(documents);app.loadedRigModelPaths={weapon.path};app.equippedRigAssets={weaponIndex};app.selectedWeaponAsset=weaponIndex;
 activateWeaponProfile(app,generatedWeaponProfile(app,weapon));app.viewmodelAnimationPrefix=prefix;app.assetFirstPerson=app.viewmodelCamera=true;
 if(!app.weaponProfile.meleeWeapon&&cadence::dual::sideClip(app.scene,prefix,"attackidle",0,true)&&cadence::dual::sideClip(app.scene,prefix,"attackidle",1,true)){
  if(!cadence::dual::prepare(app.scene,app.weaponProfile,prefix,false,error)){app.status=error;return true;}app.weaponTiming=app.weaponProfile.stats;
 }
 app.dualLeftNextFireTime=0;app.previousDualLeft=false;app.pendingDualShots.clear();app.dualMuzzleFlashTime={};
 app.actionActive=app.actionOverlay=app.transitioning=false;app.jumpFeedbackAnimation=static_cast<std::size_t>(-1);app.runtimeLayers.clear();
 selectViewmodelIdle(app,weapon);if(!app.deferSceneUpload)uploadMainScene(app);
 app.status="Point Blank: "+std::to_string(app.scene.animations.size())+" exact action clips; "+(legacy?"native-space anatomical hand adapter":"native hands");return true;
}
