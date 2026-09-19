// Frozen pre-v171 profile search and bot assembly, diagnostic only.
void populateDefaultDiscoveredAnimationsReference(const AppState& app,weapon::Profile& profile,const assets::Asset& asset){
    const bool hadAdsFireSelection=(profile.animations.contains("ads_fire")&&!profile.animations.at("ads_fire").empty())||(profile.animationVariants.contains("ads_fire")&&!profile.animationVariants.at("ads_fire").empty());
    if(lowerText(asset.game)=="codm"&&app.scene.codmNativeCentimetres){cadence::codm_actions::populate(app.scene,profile,asset.name);return;}
    if(lowerText(asset.game)=="pointblank"){cadence::pointblank_actions::populate(app.scene,profile,asset.name);return;}
    const auto nameLower=lowerText(asset.name);
    const auto gameLower=lowerText(asset.game);
    const bool authoredColdWar=(gameLower=="bocw_sp"||gameLower=="bocw"||gameLower=="t9")&&profile.source.starts_with("base profile: ");
    if(!authoredColdWar&&(gameLower=="bocw_sp"||gameLower=="bocw"||gameLower=="t9")&&nameLower.starts_with("wpn_t9_sniper_standard_")){
        profile.archetype=weapon::Archetype::BoltSniper;profile.stats.boltAction=true;
    }
    auto prefix=profile.animationPrefix.empty()?animationPrefixForWeapon(asset):profile.animationPrefix;
    if(!authoredColdWar&&(gameLower=="bocw_sp"||gameLower=="bocw"||gameLower=="t9"))profile.animationPrefix=prefix=animationPrefixForWeapon(asset);
    if(prefix.empty()&&gameLower!="cs2"){
        const auto keys=viewmodelKeys(asset);
        if(!keys.empty()) prefix="vm_"+keys.back()+"_";
    }
    if(nameLower.find("m40a3")!=std::string::npos||nameLower.find("r700")!=std::string::npos||nameLower.find("w1200")!=std::string::npos||nameLower.find("dsr50")!=std::string::npos||nameLower.find("ballista")!=std::string::npos||nameLower.find("tac330")!=std::string::npos||nameLower.find("ksg")!=std::string::npos||nameLower.find("870")!=std::string::npos||nameLower.find("awp")!=std::string::npos||nameLower.find("ssg08")!=std::string::npos||profile.archetype==weapon::Archetype::Shotgun||profile.archetype==weapon::Archetype::BoltSniper||profile.archetype==weapon::Archetype::BoltIndividualSniper){
        if(!authoredColdWar)profile.stats.boltAction=true;
    }

    std::vector<std::string> prefixes;
    if(!prefix.empty()) prefixes.push_back(prefix);
    for(const auto& key:viewmodelKeys(asset)){
        prefixes.push_back("vm_"+key+"_");
        prefixes.push_back("viewmodel_"+key+"_");
        prefixes.push_back("va_"+key+"_");
        prefixes.push_back("weapon_"+key+"_");
    }
    if(prefix.starts_with("viewmodel_")) prefixes.push_back("vm_"+prefix.substr(10));
    if(prefix.starts_with("vm_")) prefixes.push_back("viewmodel_"+prefix.substr(3));

    static constexpr const char* slots[]={"idle","fire","ads_fire","reload","reload_empty","rechamber","ads_rechamber","ads_up","ads_down","sprint_in","sprint_loop","sprint_out","pullout","first_raise","putaway","inspect","melee"};
    const bool cs2Knife=gameLower=="cs2"&&nameLower.find("knife_")!=std::string::npos;std::string cs2KnifeFamily;
    if(cs2Knife){const auto begin=nameLower.find("knife_")+6;cs2KnifeFamily=nameLower.substr(begin);for(const auto suffix:{std::string_view{"_model"},std::string_view{"_lod0"}})if(cs2KnifeFamily.ends_with(suffix))cs2KnifeFamily.resize(cs2KnifeFamily.size()-suffix.size());}
    for(const char* slot:slots){
        if(gameLower=="cs2"&&std::string_view(slot)=="ads_fire"&&hadAdsFireSelection)continue;
        // Old generated T9 profiles contained guessed filenames. Replace only
        // unresolved mappings once this weapon's native clips are loaded.
        if(!authoredColdWar&&(gameLower=="bocw_sp"||gameLower=="bocw"||gameLower=="t9")){
            if(const auto mapped=profile.animations.find(slot);mapped!=profile.animations.end()){
                const auto wanted=lowerText(std::filesystem::path(mapped->second).filename().string());
                bool nativeLoaded=false,resolved=false;
                for(const auto& clip:app.scene.animations){
                    if(clip.domain!=scene::AnimationDomain::ViewModel||clip.tracks.empty())continue;
                    const auto source=lowerText(std::filesystem::path(clip.sourceName).filename().string());
                    nativeLoaded|=source.starts_with(lowerText(prefix));resolved|=source==wanted;
                }
                if(nativeLoaded&&!resolved)profile.animations.erase(mapped);
            }
        }
        if(const auto mapped=profile.animations.find(slot);mapped!=profile.animations.end()&&!validIwSprintSlot(gameLower,nameLower,slot,mapped->second))profile.animations.erase(mapped);
        if(gameLower=="cs2"&&(std::string_view(slot)=="ads_fire"||std::string_view(slot)=="ads_rechamber"||std::string_view(slot)=="ads_up"||std::string_view(slot)=="ads_down")){profile.animations.erase(slot);continue;}
        // Saved defaults from builds that treated every CS2 knife as the generic
        // knife must not override the selected knife's authored animation family.
        if(cs2Knife&&profile.animations.contains(slot)&&!profile.animations[slot].empty()){const auto mapped=lowerText(std::filesystem::path(profile.animations[slot]).filename().string());if(!mapped.ends_with("_"+cs2KnifeFamily+".cast"))profile.animations.erase(slot);}
        if(profile.animations.contains(slot)&&!profile.animations[slot].empty())continue;
        std::vector<std::string> actions;
        if(std::string_view(slot)=="fire") actions=cs2Knife?std::vector<std::string>{"light_hit1.cast","light_miss1.cast","light_hit2.cast","heavy_hit1.cast"}:std::vector<std::string>{"shoot1.cast","shoot.cast","fire_recoil.cast","recoil.cast","fire.cast","fire_01.cast","fire_last.cast","single.cast","heavy_hit1.cast","light_hit1.cast"};
        else if(std::string_view(slot)=="ads_fire") actions={"fire_ads.cast","ads_fire.cast","ads_fire_thermal.cast","shoot1.cast","shoot.cast"};
        else if(std::string_view(slot)=="melee"&&cs2Knife) actions={"heavy_hit1.cast","heavy_miss1.cast","heavy_backstab.cast","light_hit1.cast"};
        else if(std::string_view(slot)=="pullout") actions={"draw.cast","draw2.cast","pullout.cast","raise.cast","raise_first.cast","raise_quick.cast","first_raise.cast","pullout_quick.cast"};
        else if(std::string_view(slot)=="first_raise") actions={"first_time_pullout.cast","first_raise.cast","raise_first.cast","first_pullout.cast","pullout_first.cast"};
        else if(std::string_view(slot)=="putaway") actions={"putaway.cast","drop.cast","drop_quick.cast","putaway_quick.cast","holster.cast"};
        else if(std::string_view(slot)=="sprint_loop") actions={"sprint_loop.cast","sprint_offset.cast","sprint.cast","run.cast"};
        else if(std::string_view(slot)=="sprint_in") actions={"sprint_in.cast","sprint_offset.cast"};
        else if(std::string_view(slot)=="sprint_out") actions={"sprint_out.cast","sprint_offset.cast"};
        else if(std::string_view(slot)=="reload_empty") actions={"reload.cast","reload_empty.cast","reload_empty_fast.cast","reload_empty_start.cast","reload2.cast"};
        else if(std::string_view(slot)=="reload") actions={"reload.cast","reload_start.cast","reload_fast.cast","reload_in.cast","reload2.cast"};
        else if(std::string_view(slot)=="inspect") actions={"lookat01.cast","lookat02.cast","lookat03.cast","lookat.cast","inspect.cast","gesture.cast","akimbo_gesture.cast"};
        else if(std::string_view(slot)=="idle") actions={"idle.cast","idle1.cast","idle2.cast","idle_slide_back.cast"};
        else actions={std::string(slot)+".cast"};

        if(auditedWeaponGame(gameLower)){
            if(std::string_view(slot)=="fire")actions={"fire.cast","fire_01.cast","fire_last.cast","shoot.cast","single.cast","fire_recoil.cast","recoil.cast"};
            if(std::string_view(slot)=="reload_empty")actions={"reload_empty.cast","reload_empty_fast.cast","reload_empty_start.cast","reload.cast","reload2.cast"};
        }

        if(gameLower=="bocw_sp"||gameLower=="bocw"||gameLower=="t9"){
            if(std::string_view(slot)=="ads_up")actions={"ads_base_up.cast"};
            else if(std::string_view(slot)=="ads_down")actions={"ads_base_down.cast"};
            else if(std::string_view(slot)=="ads_rechamber")actions={"rechamber_ads.cast"};
            else if(std::string_view(slot)=="reload_empty")actions={"reload_empty.cast","reload.cast"};
            else if(std::string_view(slot)=="melee")actions={"gunbutt_swipe.cast"};
            else if(std::string_view(slot)=="idle")actions={"idle.cast","static_idle.cast","additive_idle.cast"};
        }

        // 1. Check prefix + action
        for(const auto& p:prefixes){
            for(const auto& act:actions){
                const auto wanted=lowerText(p+act);
                for(const auto& clip:app.scene.animations){
                    if(clip.domain!=scene::AnimationDomain::ViewModel||clip.tracks.empty())continue;
                    const auto source=lowerText(std::filesystem::path(clip.sourceName).filename().string());
                    if((source==wanted||source.ends_with("_"+wanted))&&validIwSprintSlot(gameLower,nameLower,slot,source)){
                        profile.animations[slot]=clip.sourceName;
                        if(profile.animationPrefix.empty()) profile.animationPrefix=p;
                        break;
                    }
                }
                if(profile.animations.contains(slot)&&!profile.animations[slot].empty())break;
            }
            if(profile.animations.contains(slot)&&!profile.animations[slot].empty())break;
        }

        // 2. Check action + "_" + key (suffix-based clips like shoot1_ak.cast, idle_ak.cast, reload_m4a4.cast)
        if(!profile.animations.contains(slot)||profile.animations[slot].empty()){
            for(const auto& key:viewmodelKeys(asset)){
                for(const auto& act:actions){
                    const auto actStem=std::filesystem::path(act).stem().string();
                    const auto wanted=lowerText(actStem+"_"+key+".cast");
                    for(const auto& clip:app.scene.animations){
                        if(clip.domain!=scene::AnimationDomain::ViewModel||clip.tracks.empty())continue;
                        const auto source=lowerText(std::filesystem::path(clip.sourceName).filename().string());
                        if(source==wanted||source.ends_with("_"+wanted)){
                            profile.animations[slot]=clip.sourceName;
                            break;
                        }
                    }
                    if(profile.animations.contains(slot)&&!profile.animations[slot].empty())break;
                }
                if(profile.animations.contains(slot)&&!profile.animations[slot].empty())break;
            }
        }

        // 2b. Check variant actions where attachment tokens are embedded (e.g. viewmodel_benellim4_foregrip_idle.cast)
        if(!profile.animations.contains(slot)||profile.animations[slot].empty()){
            for(const auto& p:prefixes){
                if(p.empty())continue;
                for(const auto& act:actions){
                    const auto actStem=lowerText(std::filesystem::path(act).stem().string());
                    const auto token="_"+actStem;
                    for(const auto& clip:app.scene.animations){
                        if(clip.domain!=scene::AnimationDomain::ViewModel||clip.tracks.empty())continue;
                        const auto source=lowerText(std::filesystem::path(clip.sourceName).filename().string());
                        if(source.starts_with(p)&&(source.find(token)!=std::string::npos||source.find(actStem)!=std::string::npos)){
                            profile.animations[slot]=clip.sourceName;
                            if(profile.animationPrefix.empty()) profile.animationPrefix=p;
                            break;
                        }
                    }
                    if(profile.animations.contains(slot)&&!profile.animations[slot].empty())break;
                }
                if(profile.animations.contains(slot)&&!profile.animations[slot].empty())break;
            }
        }

        // 3. Fallback: ads_fire -> fire
        if(std::string_view(slot)=="ads_fire"&&(!profile.animations.contains(slot)||profile.animations[slot].empty())){
            if(profile.animations.contains("fire")&&!profile.animations["fire"].empty()){
                profile.animations[slot]=profile.animations["fire"];
            }
        }
        // Fallback: reload_empty -> reload
        if(std::string_view(slot)=="reload_empty"&&(!profile.animations.contains(slot)||profile.animations[slot].empty())){
            if(profile.animations.contains("reload")&&!profile.animations["reload"].empty()){
                profile.animations[slot]=profile.animations["reload"];
            }
        }
    }
    if(gameLower=="cs2"){
        // Source 2 uses action-first names (lookat01_ak, light_hit1_knife)
        // rather than CoD's one-clip-per-slot convention. Keep every authored
        // family clip in a deterministic list; runtime cursors choose among
        // these without changing old single-string weaponfiles.
        const auto families=viewmodelKeys(asset);
        auto belongsToFamily=[&](const std::string& source){
            if(families.empty())return true;
            for(const auto& family:families){
                const auto token="_"+lowerText(family)+".cast";
                if(source.ends_with(token))return true;
            }
            return false;
        };
        auto naturalLess=[](const std::string& left,const std::string& right){
            std::size_t a=0,b=0;while(a<left.size()&&b<right.size()){
                const bool ad=std::isdigit(static_cast<unsigned char>(left[a]))!=0,bd=std::isdigit(static_cast<unsigned char>(right[b]))!=0;
                if(ad&&bd){std::size_t ae=a,be=b;while(ae<left.size()&&std::isdigit(static_cast<unsigned char>(left[ae])))++ae;while(be<right.size()&&std::isdigit(static_cast<unsigned char>(right[be])))++be;const auto av=std::stoull(left.substr(a,ae-a)),bv=std::stoull(right.substr(b,be-b));if(av!=bv)return av<bv;a=ae;b=be;continue;}
                if(left[a]!=right[b])return left[a]<right[b];++a;++b;
            }
            return left.size()<right.size();
        };
        auto collect=[&](const char* slot,const auto& predicate){
            auto& existing=profile.animationVariants[slot];
            existing.erase(std::remove_if(existing.begin(),existing.end(),[&](const std::string& value){
                const auto filename=lowerText(std::filesystem::path(value).filename().string());
                return !belongsToFamily(filename)||std::none_of(app.scene.animations.begin(),app.scene.animations.end(),[&](const auto& clip){return !clip.tracks.empty()&&lowerText(std::filesystem::path(clip.sourceName).filename().string())==filename;});
            }),existing.end());
            if(!existing.empty())return;
            std::vector<std::pair<std::string,std::string>> found;
            for(const auto& clip:app.scene.animations){
                if(clip.domain!=scene::AnimationDomain::ViewModel||clip.tracks.empty())continue;
                const auto source=lowerText(std::filesystem::path(clip.sourceName).filename().string());
                if(!belongsToFamily(source)||!predicate(source))continue;
                if(std::none_of(found.begin(),found.end(),[&](const auto& entry){return entry.first==source;}))found.push_back({source,clip.sourceName});
            }
            std::sort(found.begin(),found.end(),[&](const auto& l,const auto& r){return naturalLess(l.first,r.first);});
            auto& variants=profile.animationVariants[slot];
            for(const auto& entry:found)variants.push_back(entry.second);
            if(variants.empty()){
                if(const auto legacy=profile.animations.find(slot);legacy!=profile.animations.end()&&!legacy->second.empty())variants.push_back(legacy->second);
            }
            if(!variants.empty())profile.animations[slot]=variants.front();
        };
        collect("idle",[](const std::string& s){return s.starts_with("idle")||s.find("_idle")!=std::string::npos;});
        collect("pullout",[](const std::string& s){return s.starts_with("draw")||s.starts_with("equip")||s.starts_with("pullout")||s.starts_with("raise");});
        collect("first_raise",[](const std::string& s){return s.starts_with("first_time_pullout")||s.starts_with("first_raise")||s.starts_with("firstpullout");});
        collect("reload",[](const std::string& s){return s.starts_with("reload")&&s.find("empty")==std::string::npos;});
        collect("reload_empty",[](const std::string& s){return (s.starts_with("reload")||s.starts_with("empty_reload"))&&s.find("empty")!=std::string::npos;});
        collect("inspect",[](const std::string& s){return (s.starts_with("lookat")||s.starts_with("inspect"))&&s.find("draw")==std::string::npos&&s.find("transfix")==std::string::npos;});
        collect("fire",[](const std::string& s){return (s.starts_with("shoot")||s.starts_with("fire"))&&s.find("empty")==std::string::npos;});
        // Fire discovery above may fill the CS2 slot after the generic fallback.
        // Preserve authored ADS choices, otherwise use the same discovered shot.
        if(!hadAdsFireSelection&&profile.animations.contains("fire")&&!profile.animations.at("fire").empty())profile.animations["ads_fire"]=profile.animations.at("fire");
        collect("melee",[](const std::string& s){return s.starts_with("light_")||s.starts_with("heavy_");});
        collect("sprint_in",[](const std::string& s){return s.starts_with("sprint_in");});
        collect("sprint_loop",[](const std::string& s){return s.starts_with("sprint_loop");});
        collect("sprint_out",[](const std::string& s){return s.starts_with("sprint_out");});
        if(profile.animationVariants["first_raise"].empty()&&!profile.animationVariants["pullout"].empty())profile.animationVariants["first_raise"]=profile.animationVariants["pullout"],profile.animations["first_raise"]=profile.animations["pullout"];
        if(profile.animationVariants["reload_empty"].empty()&&!profile.animationVariants["reload"].empty())profile.animationVariants["reload_empty"]=profile.animationVariants["reload"],profile.animations["reload_empty"]=profile.animations["reload"];
    }
    if(gameLower!="cs2"){
        for(const char* slot:slots){
            if(profile.animations.contains(slot)&&!profile.animations[slot].empty())continue;
            if(prefix.ends_with("_")){
                profile.animations[slot]=prefix+slot+".cast";
            }
        }
    }
}

weapon::Profile generatedWeaponProfileReference(const AppState& app,const assets::Asset& asset,const std::filesystem::path& defaultsDirectory={},bool discoverAnimations=true){
    auto profile=weapon::makeGenerated(asset.name,weapon::inferArchetype(asset.name),animationPrefixForWeapon(asset));const auto game=lowerText(asset.game);const auto assetName=lowerText(asset.name);profile.meleeWeapon=game=="cs2"&&(assetName.find("knife_")!=std::string::npos||assetName.find("karambit")!=std::string::npos||assetName.find("butterfly")!=std::string::npos||assetName.find("talon")!=std::string::npos);bool loadedDefault=false;
    const auto relocated=[&](const std::filesystem::path& path){return defaultsDirectory.empty()||path.empty()?path:defaultsDirectory/path.parent_path().filename()/path.filename();};
    const auto defaultPath=relocated(weaponDefaultProfilePath(asset));if(std::filesystem::is_regular_file(defaultPath)){std::string error;weapon::Profile saved;if(weapon::load(defaultPath,saved,error)){profile=std::move(saved);loadedDefault=true;}}
    if(!loadedDefault)if(const auto source=legacyWeaponFileForAsset(app,asset)){std::string error;weapon::Profile imported;if(weapon::importLegacyWeaponFile(imported,*source,error)){imported.name=asset.name;profile=std::move(imported);}}
    else if(game=="bo2")profile.stats=loadT6WeaponTiming(asset);
    if(!loadedDefault&&(game=="bocw_sp"||game=="bocw"||game=="t9")){profile.stats.sprintLoopTime=0;profile.stats.sprintPlaybackScale=1.f;}
    if(game=="cs2"){profile.lockInspectRig=false;profile.lockInspectGrip=false;profile.meleeWeapon=profile.meleeWeapon||(assetName.find("knife_")!=std::string::npos||assetName.find("karambit")!=std::string::npos||assetName.find("butterfly")!=std::string::npos||assetName.find("talon")!=std::string::npos);if(!loadedDefault){profile.stats.dropTime=0;profile.stats.quickDropTime=0;profile.source="CS2 defaults + discovered weapon animations";}}
    const auto familyPath=relocated(weaponFamilyProfilePath(asset));
    if(!familyPath.empty()&&std::filesystem::is_regular_file(familyPath)){weapon::Profile family;std::string error;if(weapon::load(familyPath,family,error)){weapon::applyFamilyBehavior(profile,family);loadedDefault=true;}}
    // Existing installations already have the user's tuned base .iwweapon.
    // It is authoritative even if an older family cache or variant file exists.
    if(const auto base=coldWarBaseWeaponAsset(app,asset)){
        const auto basePath=relocated(weaponDefaultProfilePath(*base));weapon::Profile plate;std::string error;
        if(std::filesystem::is_regular_file(basePath)&&weapon::load(basePath,plate,error)){
            weapon::applyFamilyBehavior(profile,plate);profile.source="base profile: "+basePath.string();loadedDefault=true;
        }
    }
    if(discoverAnimations)populateDefaultDiscoveredAnimationsReference(app,profile,asset);
    if(!loadedDefault&&game=="codm"&&app.scene.codmNativeCentimetres)cadence::codm_actions::defaultTimings(app.scene,profile);
    if(game=="pointblank"){
        profile.meleeWeapon=profile.meleeWeapon||assetName.starts_with("viewmodel_knife_");
        if(!loadedDefault){if(assetName.starts_with("viewmodel_sniper_"))profile.archetype=weapon::Archetype::BoltSniper;cadence::pointblank_actions::populate(app.scene,profile,asset.name);cadence::pointblank_actions::defaults(app.scene,profile);}
    }
    if(profile.internalName.empty()){profile.internalName=profile.animationPrefix;if(profile.internalName.starts_with("viewmodel_"))profile.internalName.erase(0,10);if(!profile.internalName.empty()&&profile.internalName.back()=='_')profile.internalName.pop_back();}
    if(profile.source=="generated")profile.source=game=="bo2"?"BO2 weapon file + archetype defaults":"archetype defaults + discovered animations";
    return profile;
}

void rebuildBotActorsReference(AppState &app) {
  PointBlankPreparationBatch preparationBatch;
  app.botAreaPing.clear();
  diagnostic::event("BOT SPAWN/REBUILD BEGIN");
  app.botSpContextStates.clear();
  if(app.loadedMap&&app.loadedMap->authoredCollision.present&&(!app.loadedMap->authoredCollision.physicsReady||!app.loadedMap->hasDefaultSpawnPoint)){
    app.bots.clear();app.botActorPoses.clear();app.status="Bots need validated walkable authored collision; this map is preview-only.";return;
  }
  app.botSpRoutes.clear();
  // optional<CastScene> can reuse its address and clip count across rebuilds.
  resetBotLocomotionCache();
  app.botActionQueryCache.clear();
  app.bots.clear();
  app.botActorPoses.clear();
  app.botActorScene.reset();
  app.botActorGpuReady = false;
  app.botWeaponChoices.clear();
  app.enemyBotCount = std::clamp(app.enemyBotCount, 0, 18);
  auto teamModels = botTeamModels(app);
  if (teamModels.empty()) {
    app.status = "No complete player classes match this game / team";
    return;
  }
  if (app.botTeamSide == 3) {
    // Shuffle once per build so every eligible body can be chosen even when
    // the pool exceeds the existing assembled-variant budget.
    static std::mt19937 modelSelectionRandom{std::random_device{}()};
    std::shuffle(teamModels.begin(), teamModels.end(), modelSelectionRandom);
  }
  if (app.botUseSalukiWeaponPool) {
    const auto appendPool = [&](const std::vector<std::size_t> &candidates) {
      for (const auto index : candidates)
        if (std::find(app.botWeaponChoices.begin(), app.botWeaponChoices.end(),
                      index) == app.botWeaponChoices.end())
          app.botWeaponChoices.push_back(index);
    };
    if (app.botPrimaryPoolEnabled)
      appendPool(
          botPoolWeapons(app, app.botPrimaryGame, app.botPrimaryCategory));
    if (app.botSecondaryPoolEnabled)
      appendPool(
          botPoolWeapons(app, app.botSecondaryGame, app.botSecondaryCategory));
    if (app.botWeaponChoices.empty()) {
      app.status =
          "Enable a Saluki weapon category containing at least one weapon";
      return;
    }
  } else {
    const int slot = std::clamp(app.botWeaponSlot, 0, 1);
    if (app.classSlotRigs[slot])
      app.botWeaponChoices.push_back(app.classSlotRigs[slot]->weaponAsset);
    else if (app.selectedWeaponAsset < app.assetCatalog.entries.size())
      app.botWeaponChoices.push_back(app.selectedWeaponAsset);
    if (app.botWeaponChoices.empty()) {
      app.status =
          "Load a Create-a-Class weapon or enable the Saluki bot weapon pool";
      return;
    }
  }
  const auto &weaponChoices = app.botWeaponChoices;
  app.botModelAsset = teamModels.front();
  const auto &model = app.assetCatalog.entries[app.botModelAsset];
  auto document = cast::Document::load(model.path);
  if (!document.valid()) {
    app.status = "Enemy team model could not be loaded";
    return;
  }
  scene::CastScene actorScene = scene::buildScene(document);
  actorScene.meshes.clear();
  actorScene.rigParts.clear();
  actorScene.attachments.clear();
  std::vector<scene::WeaponClass> variantWeaponClasses;
  std::vector<std::size_t> variantWorldWeapons, variantViewWeapons,
      variantBodyModels;
  const std::size_t requestedVariants =
      std::max(teamModels.size(), weaponChoices.size());
  const std::size_t variantCount = std::max<std::size_t>(
      1, std::min<std::size_t>({static_cast<std::size_t>(std::max(1, app.enemyBotCount)),
                                requestedVariants, 8}));
  for (std::size_t instance = 0; instance < variantCount; ++instance) {
    const auto bodyIndex = teamModels[std::min(
        teamModels.size() - 1, instance * teamModels.size() / variantCount)];
    const auto weaponIndex = weaponChoices[instance % weaponChoices.size()];
    const auto variant = static_cast<std::int32_t>(variantWeaponClasses.size());
    const auto &body = app.assetCatalog.entries[bodyIndex];
    auto bodyDocument = cast::Document::load(body.path);
    const auto first = actorScene.meshes.size();
    if (bodyDocument.valid())
      scene::appendRigModel(bodyDocument, actorScene, body.name);
    auto bodyParts=characterAssemblyParts(app,body);
    if(lowerText(body.game)!="aw"){
      std::erase_if(bodyParts,[&](auto index){return assets::character::headCompatible(body.game,body.name,app.assetCatalog.entries[index].name);});
      if(const auto head=botHeadForBody(app,body))bodyParts.insert(bodyParts.begin(),*head);
    }
    for(const auto part:bodyParts){
      const auto& asset=app.assetCatalog.entries[part];const auto partDocument=cast::Document::load(asset.path);
      if(partDocument.valid())scene::appendRigModel(partDocument,actorScene,asset.name);
    }
    scene::WeaponClass weaponClass = scene::WeaponClass::Rifle;
    std::size_t worldIndex = static_cast<std::size_t>(-1);
    if (weaponIndex < app.assetCatalog.entries.size()) {
      const auto profile = generatedWeaponProfileReference(app, app.assetCatalog.entries[weaponIndex]);
      weaponClass =
          gameplayWeaponClass(profile, app.assetCatalog.entries[weaponIndex]);
      if(lowerText(app.assetCatalog.entries[weaponIndex].game)=="pointblank"&&profile.meleeWeapon)weaponClass=scene::WeaponClass::Knife;
      worldIndex = matchingWorldWeapon(app, weaponIndex);
      if (worldIndex < app.assetCatalog.entries.size()) {
        auto weaponDocument =
            cast::Document::load(app.assetCatalog.entries[worldIndex].path);
        if (weaponDocument.valid()&&!attachCodmWorldWeapon(app,weaponDocument,weaponIndex,actorScene)) {
          const auto beforeAttachments=actorScene.attachments.size();
          const int worldGripBone=lowerText(app.assetCatalog.entries[worldIndex].game)=="pointblank"?pointBlankWorldGripBone(actorScene.skeleton):preferredAttachmentBone(actorScene.skeleton);
          auto preparedWeapon=lowerText(app.assetCatalog.entries[worldIndex].game)=="pointblank"?preparePointBlankWorldWeapon(app,weaponDocument,actorScene.skeleton,worldGripBone):scene::buildScene(weaponDocument);
          scene::appendPreparedAttachment(
              std::move(preparedWeapon), actorScene,
              static_cast<std::size_t>(
                  worldGripBone),
              app.assetCatalog.entries[worldIndex].name);
          if(lowerText(app.assetCatalog.entries[worldIndex].game)=="pointblank")appendPointBlankLeftWorldWeapon(app,weaponDocument,actorScene,app.assetCatalog.entries[worldIndex].name);
          configureLegacyWeaponOnPointBlank(app,actorScene,beforeAttachments,lowerText(app.assetCatalog.entries[worldIndex].game),app.botAnimationGame,profile.archetype);
          appendWorldWeaponParts(app,weaponIndex,worldIndex,preparedWeapon,actorScene,beforeAttachments);
          if(lowerText(app.assetCatalog.entries[worldIndex].game)=="cs2")
            configureCs2WorldWeaponAttachment(actorScene,weaponDocument,beforeAttachments);
        }
      }
    }
    for (std::size_t mesh = first; mesh < actorScene.meshes.size(); ++mesh)
      actorScene.meshes[mesh].actorVariant = variant;
    variantWeaponClasses.push_back(weaponClass);
    variantWorldWeapons.push_back(worldIndex);
    variantViewWeapons.push_back(weaponIndex);
    variantBodyModels.push_back(bodyIndex);
  }
  std::vector<std::string> animationGames;
  if (app.botAnimationGame == "*") {
    for (const auto &asset : app.assetCatalog.entries)
      if (!asset.game.empty() &&
          std::find(animationGames.begin(), animationGames.end(), asset.game) ==
              animationGames.end())
        animationGames.push_back(asset.game);
  } else
    animationGames.push_back(
        app.botAnimationGame.empty() ? model.game : app.botAnimationGame);
  // BO2 and BO2_SP are a single bot library; legacy saved selections remain valid.
  const bool pbMovementFallback=std::find(animationGames.begin(),animationGames.end(),"pointblank")!=animationGames.end()&&std::find(animationGames.begin(),animationGames.end(),"bo2")==animationGames.end();
  if(pbMovementFallback)animationGames.push_back("bo2");
  for(auto& game:animationGames)if(lowerText(game)=="bo2_sp"||lowerText(game)=="bo2+bo2_sp")game="bo2";
  // The SP rip's ai_ scripts are scenario clips, not a complete MP locomotion
  // library. Keep a native movement foundation when SP is selected explicitly.
  {
    std::vector<std::string> uniqueGames;
    for(const auto& game:animationGames)if(std::find(uniqueGames.begin(),uniqueGames.end(),game)==uniqueGames.end())uniqueGames.push_back(game);
    animationGames=std::move(uniqueGames);
  }
  for (const auto &animationGame : animationGames) {
    ensureBotAnimationCache(app, animationGame);
    if(lowerText(animationGame)=="bo2"&&app.botAnimationCache["bo2"].empty()){
      auto root=gameRootDirectoryForGame(app,"bo2");
      if(root.empty()){const auto sp=gameRootDirectoryForGame(app,"bo2_sp");if(!sp.empty())root=sp.parent_path()/"bo2";}
      // Resolve only the absent native foundation; don't alter catalog choices.
      for(const auto& folder:animationFoldersForGameRoot(root)){
        std::error_code error;
        for(std::filesystem::recursive_directory_iterator it(folder,std::filesystem::directory_options::skip_permission_denied,error),end;it!=end;it.increment(error)){
          if(error){error.clear();continue;}
          if(!it->is_regular_file(error)||lowerText(it->path().extension().string())!=".cast")continue;
          const auto name=lowerText(it->path().filename().string());
          if(!name.starts_with("pb_")&&!name.starts_with("pt_")&&!name.starts_with("mp_"))continue;
          if(name.find("dog")!=std::string::npos||name.find("shepherd")!=std::string::npos||name.find("vehicle")!=std::string::npos||name.find("turret")!=std::string::npos)continue;
          scene::Animation metadata;scene::classifyAnimationName(name,metadata);
          if(metadata.domain!=scene::AnimationDomain::PlayerBody&&metadata.domain!=scene::AnimationDomain::PlayerTorso)continue;
          auto document=cast::Document::load(it->path());if(document.valid())app.botAnimationCache["bo2"].push_back(std::move(document));
        }
      }
    }
    const auto before = actorScene.animations.size();
    for (const auto &animation : app.botAnimationCache[lowerText(animationGame)]){
      if(pbMovementFallback&&animationGame=="bo2"){scene::Animation meta;scene::classifyAnimationName(std::filesystem::path(animation.sourceName()).filename().string(),meta);if(meta.domain!=scene::AnimationDomain::PlayerBody||meta.action!=scene::ActionRole::None||meta.contextual)continue;}
      if(!appendPointBlankWorldAnimation(app,animation,actorScene,animationGame,model.game))scene::appendAnimations(animation, actorScene);
    }
    for (std::size_t i = before; i < actorScene.animations.size(); ++i)
      actorScene.animations[i].sourceGame = animationGame;
    if (lowerText(animationGame) != lowerText(model.game))
      if (const auto sourceSkeleton = playerSkeletonForGame(app, animationGame))
        scene::retargetAnimationRange(actorScene, before, *sourceSkeleton);
    appendMissingWorldHolds(app,actorScene,animationGame,model.game);
  }
  if (actorScene.animations.empty())
    for (std::size_t i = 0; i < app.animationDocuments.size(); ++i) {
      if (!app.animationDocuments[i].valid())
        continue;
      const auto before = actorScene.animations.size();
      scene::appendAnimations(app.animationDocuments[i], actorScene);
      if (i < app.animationSources.size())
        tagAnimationGame(actorScene, before, app.animationSources[i]);
    }
  loadSpBotScenarios(app,actorScene,model.game);
  gameplay::bot::configureBotOneShots(actorScene);
  app.botDeathChoices=cadence::prepareBotDeaths(actorScene);
  app.botActionQueryCache.clear();
  app.botAnimationClipCount = actorScene.animations.size();
  app.botClipAuthoredSpeeds.assign(actorScene.animations.size(), 0.0f);
  if (const auto root =
          actorScene.skeleton.boneByCanonicalName.find("tag_origin");
      root != actorScene.skeleton.boneByCanonicalName.end())
    for (std::size_t i = 0; i < actorScene.animations.size(); ++i) {
      const auto &clip = actorScene.animations[i];
      const float seconds = static_cast<float>(clip.durationFrames) /
                            std::max(1.0f, clip.framerate);
      if (seconds <= 0.001f)
        continue;
      const auto first = actorScene.sampleLocalPose(i, 0),
                 last = actorScene.sampleLocalPose(
                     i, static_cast<float>(clip.durationFrames));
      if (root->second < first.size() && root->second < last.size()) {
        const auto travel =
            last[root->second].position - first[root->second].position;
        const float speed =
            std::sqrt(travel.x * travel.x + travel.y * travel.y) / seconds;
        if (speed > gameplay::iw::worldUnits(1.0f) &&
            speed < gameplay::iw::worldUnits(1000.0f))
          app.botClipAuthoredSpeeds[i] = speed;
      }
    }
  const int botSlot = std::clamp(app.botWeaponSlot, 0, 1);
  std::size_t botViewWeapon = app.selectedWeaponAsset;
  if (app.classSlotRigs[botSlot])
    botViewWeapon = app.classSlotRigs[botSlot]->weaponAsset;
  if (botViewWeapon < app.assetCatalog.entries.size()) {
    const auto profile =
        app.classSlotRigs[botSlot]
            ? app.classSlotRigs[botSlot]->profile
            : generatedWeaponProfileReference(app,app.assetCatalog.entries[botViewWeapon]);
    app.botWeaponClass =
        gameplayWeaponClass(profile, app.assetCatalog.entries[botViewWeapon]);
  }
  float authoredHead = -std::numeric_limits<float>::max();
  for (const auto &bone : actorScene.skeleton.bones) {
    const auto name = lowerText(bone.name);
    if (name == "j_head" || name == "tag_eye" || name == "j_helmet")
      authoredHead = std::max(authoredHead, bone.restGlobal.v[14]);
  }
  if (authoredHead <= -std::numeric_limits<float>::max() / 2)
    for (const auto &bone : actorScene.skeleton.bones)
      authoredHead = std::max(authoredHead, bone.restGlobal.v[14]);
  const float authoredFloor =
      actorScene.bounds.valid ? actorScene.bounds.minimum.z : 0.0f;
  app.botHeadHeight = std::max(0.01f, authoredHead - authoredFloor);
  app.botModelScale = 1.0f;
  app.botActorScene = std::move(actorScene);
  prepareBotSpawnCandidates(app);
  for (int i = 0; i < app.enemyBotCount; ++i) {
    gameplay::bot::Actor bot;
    bot.id = static_cast<std::uint32_t>(i + 1);
    const auto variant=static_cast<std::size_t>(i)%variantCount;
    bot.modelVariant = static_cast<int>(variant);
    bot.weaponSlot = static_cast<int>(variant);
    bot.weaponClass = variantWeaponClasses[variant];
    bot.viewWeaponAsset = variantViewWeapons[variant];
    bot.worldWeaponAsset = variantWorldWeapons[variant];
    if(bot.viewWeaponAsset<app.assetCatalog.entries.size())
        bot.cachedWeaponStats=generatedWeaponProfileReference(app,app.assetCatalog.entries[bot.viewWeaponAsset]).stats;
    else if(app.classSlotRigs[std::clamp(app.botWeaponSlot,0,1)])
        bot.cachedWeaponStats=app.classSlotRigs[std::clamp(app.botWeaponSlot,0,1)]->profile.stats;
    else
        bot.cachedWeaponStats=app.weaponTiming;
    bot.team = botFactionFor(app.assetCatalog.entries[variantBodyModels[variant]]);
    bot.equipmentOpportunityTime =
        app.botEquipmentMinInterval + static_cast<float>(i % 7) * 2.3f;
    if (!app.botSpawnPoints.empty())
      bot.position = app.botSpawnPoints[static_cast<std::size_t>(i) %
                                        app.botSpawnPoints.size()];
    else {
      const int column = i % 6, row = i / 6;
      bot.position = {gameplay::iw::worldUnits(420.0f + row * 96.0f),
                      gameplay::iw::worldUnits(-300.0f + column * 120.0f), 0};
      bot.position.z =
          actorGroundHeight(app, bot.position.x, bot.position.y,
                            std::numeric_limits<float>::max() / 4.0f);
    }
    bot.position=chooseBotSpawn(app,bot.position);
    bot.spawnPosition = bot.position;
    bot.yaw = scene::kPi;
    bot.waypoint =
        activeBotNavigation(app).nodes.empty()
            ? static_cast<std::size_t>((i * 3) %
                                       gameplay::bot::kWaypoints.size())
            : static_cast<std::size_t>(i) % activeBotNavigation(app).nodes.size();
    bot.navigation =
        app.botUseAuthoredNavigation ? &activeBotNavigation(app) : nullptr;
    app.bots.push_back(std::move(bot));
  }
  app.rendererError.clear();
  const bool auxiliaryReady=(!app.loadedMap||app.mapGpuReady)&&(!app.hiddenWorldActor||app.worldActorGpuReady);
  const bool actorsReady = (auxiliaryReady&&app.renderer.replaceBotScene(&*app.botActorScene,app.rendererError)) || (
      app.classSlotRigs[0] && app.classSlotRigs[1]
          ? uploadResidentClassScenes(app)
          : app.renderer.loadAuxiliaryScenes(
                app.loadedMap ? &app.loadedMap->scene : nullptr,
                app.hiddenWorldActor ? &*app.hiddenWorldActor : nullptr,
                &*app.botActorScene, app.rendererError));
  app.mapGpuReady = actorsReady && app.loadedMap.has_value();
  app.worldActorGpuReady = actorsReady && app.hiddenWorldActor.has_value();
  app.botActorGpuReady = actorsReady;
  diagnostic::event("BOT SPAWN/REBUILD END: count="+std::to_string(app.bots.size())+" gpu_ready="+std::to_string(actorsReady));
  app.status = app.botActorGpuReady
                   ? "Spawned " + std::to_string(app.bots.size()) +
                         " enemy actors with " +
                         std::to_string(app.botAnimationClipCount) +
                         " player clips from " + model.name
                   : "Bot actor GPU upload failed";
}


