void prepareActionPreview(AppState& app,const std::string& game,std::size_t model){
    cadence::content::ObservationPause previewAssets;
    app.actionPreviewReady=false;app.actionPreviewClip=SIZE_MAX;
    if(model>=app.assetCatalog.entries.size()){app.status="No playermodel available for "+game;return;}
    try{
        struct RestoreReferences {AppState& app;decltype(app.gameReferenceSetups) saved;~RestoreReferences(){app.gameReferenceSetups=std::move(saved);}} restore{app,app.gameReferenceSetups};
        for(const auto& [source,name]:app.animationEditor.draft.models)for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game==source&&a.name==name&&a.role==assets::Role::PlayerModel){app.gameReferenceSetups[source].playermodelAsset=i;break;}}
        const auto& asset=app.assetCatalog.entries[model];auto doc=cast::Document::load(asset.path);if(!doc.valid()){app.status="Cannot load preview model";return;}
        auto preview=scene::buildScene(doc,false);
        for(const auto part:characterAssemblyParts(app,asset)){auto p=cast::Document::load(app.assetCatalog.entries[part].path);if(p.valid())scene::appendRigModel(p,preview,app.assetCatalog.entries[part].name);}
        ensureBotAnimationCache(app,game);PointBlankPreparationBatch pb;ImportedBodyPreparationBatch imported;
        for(const auto& animation:app.botAnimationCache[game]){
            const auto before=preview.animations.size();
            if(!appendPointBlankWorldAnimation(app,animation,preview,game,asset.game)){
                scene::appendAnimations(animation,preview);
                if(game!=asset.game)if(const auto donor=playerSkeletonForGame(app,game))scene::retargetAnimationRange(preview,before,*donor);
            }
            if(preview.animations.size()==before){
                scene::Animation unavailable;unavailable.sourceName=animation.sourceName();
                scene::classifyAnimationName(std::filesystem::path(unavailable.sourceName).filename().string(),unavailable);
                if(assets::imported::sourceFamily(game))scene::imported::classifySourceBodyAnimation(unavailable);
                if(unavailable.domain==scene::AnimationDomain::PlayerBody||unavailable.domain==scene::AnimationDomain::PlayerTorso)preview.animations.push_back(std::move(unavailable));
            }
            for(std::size_t i=before;i<preview.animations.size();++i){preview.animations[i].sourceGame=game;preview.animations[i].looping=false;}
        }
        scene::imported::repairDetachedBodyRootLoops(preview,0);
        std::string error;if(!app.actionPreviewRenderer.initialize(error)||!app.actionPreviewRenderer.loadScene(preview,error)){app.status=error;return;}
        app.actionPreviewRenderer.setPostEffects(0,0,0,false,0,false,0);
        app.actionPreviewRenderer.setTonemapping(0);
        app.actionPreviewScene=std::move(preview);app.actionPreviewGame=game;app.actionPreviewModel=model;app.actionPreviewReady=true;
        const auto bound=std::count_if(app.actionPreviewScene.animations.begin(),app.actionPreviewScene.animations.end(),[](const auto& a){return !a.tracks.empty();});
        app.status=app.actionPreviewScene.animations.empty()?"No matching exported body clips for "+asset.name:"Prepared "+std::to_string(bound)+" / "+std::to_string(app.actionPreviewScene.animations.size())+" body clips";
        if(app.actionPreviewScene.animations.empty()&&!app.actionPreviewScene.warnings.empty())app.status+=". "+app.actionPreviewScene.warnings.front();
    }catch(const std::exception& e){app.status=e.what();}
}

void drawAnimationSets(AppState& app){
    ImGui::TextUnformatted("Animation sets are available in the top view selector.");
}
void applyAnimationSetToLiveBots(AppState& app);
void drawAnimationSetPresets(AppState& app){
    const bool busy=app.takeRecording||app.takePreview||app.exportActive||app.pendingClassFuture.has_value()||app.pendingClassLoadStage>=0||app.pendingMapFuture.has_value()||app.catalogScanFuture.has_value();
    ImGui::BeginDisabled(busy);bool changed=false;
    if(ImGui::BeginCombo("Animation preset",app.animationSetPreset.empty()?"None":app.animationSetPreset.c_str())){
        if(ImGui::Selectable("None",app.animationSetPreset.empty())){app.animationSet={};app.animationSetPreset.clear();app.animationSetForBots=false;changed=true;}
        const auto files=uiFileLists().get(cadenceAssetsDirectory()/"animation_sets","|.cfg|");
        for(const auto& file:*files)if(ImGui::Selectable(file.stem().string().c_str(),app.animationSetPreset==file.stem().string())){cadence::AnimationSet set;std::string error;if(set.load(file,&error)){app.animationSet=std::move(set);app.animationSetPreset=file.stem().string();app.animationSetForBots=true;changed=true;}else app.status=error;}
        ImGui::EndCombo();
    }
    changed|=ImGui::Checkbox("Use animation set",&app.animationSetForBots);ImGui::EndDisabled();if(changed)applyAnimationSetToLiveBots(app);
}

void rebuildBotActors(AppState& app);

namespace animationui {
std::filesystem::path directory(){return cadenceAssetsDirectory()/"animation_sets";}
void dirty(AppState& app){auto& e=app.animationEditor;e.dirty=e.draft!=e.saved;}
bool save(AppState& app,const std::string& name){
    auto& e=app.animationEditor;
    if(name.empty()||name=="."||name==".."||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ")!=std::string::npos||name.back()==' '){e.status="Use letters, numbers, spaces, - or _ for the preset name.";return false;}
    const auto folded=lowerText(name);if(folded=="con"||folded=="prn"||folded=="aux"||folded=="nul"||(folded.size()==4&&(folded.starts_with("com")||folded.starts_with("lpt"))&&folded[3]>='0'&&folded[3]<='9')){e.status="That name is reserved.";return false;}
    std::string error;
    if(!e.draft.save(directory()/(name+".cfg"),&error)){e.status="Could not save preset: "+error;return false;}
    uiFileLists().get(directory(),"|.cfg|",false,true);
    e.preset=name;e.saved=e.draft;e.dirty=false;e.status="Saved "+name;return true;
}
void performPending(AppState& app,const std::filesystem::path& presetDirectory=directory()){
    auto& e=app.animationEditor;
    const auto currentReference=[&](){const auto it=e.draft.models.find(e.sourceGame);return it==e.draft.models.end()?std::string{}:it->second;};
    const auto previousReference=currentReference();
    if(e.pendingOperation==1){cadence::AnimationSet set;std::string error;if(!set.load(presetDirectory/(e.pendingPreset+".cfg"),&error)){e.status="Could not load preset: "+error;e.pendingOperation=0;e.pendingPreset.clear();return;}e.draft=std::move(set);e.preset=e.pendingPreset;e.saved=e.draft;e.dirty=false;e.status="Loaded "+e.preset;}
    else if(e.pendingOperation==2){e.draft={};e.saved={};e.preset.clear();e.dirty=false;e.status="New set";}
    else if(e.pendingOperation==3){e.draft=e.saved;e.dirty=false;e.status="Changes discarded";}
    if(previousReference!=currentReference()&&!e.sourceGame.empty()){
        // A preset can change the donor without changing the selected source.
        // Never leave its previously bound preview visible until the reload.
        e.pendingLoad=true;app.actionPreviewReady=false;app.actionPreviewPlaying=false;app.actionPreviewClip=SIZE_MAX;
    }
    e.pendingOperation=0;e.pendingPreset.clear();
}
void request(AppState& app,int operation,const std::string& preset={}){
    auto& e=app.animationEditor;e.pendingOperation=operation;e.pendingPreset=preset;
    if(e.dirty)e.confirmDiscard=true;else performPending(app);
}
void presets(AppState& app){
    auto& e=app.animationEditor;
    ImGui::SetNextItemWidth(220);
    const std::string label=(e.preset.empty()?"Unsaved set":e.preset)+(e.dirty?" *":"");
    if(ImGui::BeginCombo("##animation_preset",label.c_str())){const auto files=uiFileLists().get(directory(),"|.cfg|");for(const auto& file:*files)if(ImGui::Selectable(file.stem().string().c_str(),e.preset==file.stem().string()))request(app,1,file.stem().string());ImGui::EndCombo();}
    ImGui::SameLine();if(ImGui::Button("Save##editor")){if(e.preset.empty()){e.name.fill(0);e.saveAs=true;}else save(app,e.preset);}
    ImGui::SameLine();if(ImGui::Button("Save as##editor")){std::snprintf(e.name.data(),e.name.size(),"%s",e.preset.c_str());e.saveAs=true;}
    ImGui::SameLine();if(ImGui::Button("New##editor"))request(app,2);
    ImGui::SameLine();ImGui::BeginDisabled(e.preset.empty());if(ImGui::Button("Delete##editor"))e.confirmDelete=true;ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(!e.dirty);if(ImGui::Button("Discard changes"))request(app,3);ImGui::EndDisabled();
    if(e.confirmDiscard){ImGui::OpenPopup("Unsaved animation set");e.confirmDiscard=false;}
    if(ImGui::BeginPopupModal("Unsaved animation set",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted(e.pendingOperation==3?"Discard unsaved changes?":"Save changes before continuing?");
        if(e.pendingOperation!=3){if(ImGui::Button("Save")){if(e.preset.empty()){e.name.fill(0);e.saveAs=true;ImGui::CloseCurrentPopup();}else if(save(app,e.preset)){performPending(app);ImGui::CloseCurrentPopup();}}ImGui::SameLine();}
        if(ImGui::Button("Discard")){performPending(app);ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Cancel")){e.pendingOperation=0;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
    if(e.saveAs){ImGui::OpenPopup("Save animation set");e.saveAs=false;}
    if(ImGui::BeginPopupModal("Save animation set",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        bool closeSavedDialog=false;
        ImGui::InputText("Name",e.name.data(),e.name.size());
        if(ImGui::Button("Save")){std::error_code ec;const auto path=directory()/(std::string(e.name.data())+".cfg");if(std::filesystem::exists(path,ec)){e.confirmOverwrite=true;}else if(save(app,e.name.data())){if(e.pendingOperation)performPending(app);ImGui::CloseCurrentPopup();}}
        ImGui::SameLine();if(ImGui::Button("Cancel")){e.pendingOperation=0;ImGui::CloseCurrentPopup();}
        if(e.confirmOverwrite){ImGui::OpenPopup("Replace animation set?");e.confirmOverwrite=false;}
        if(ImGui::BeginPopupModal("Replace animation set?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::Text("Replace %s?",e.name.data());
            if(ImGui::Button("Replace")){if(save(app,e.name.data())){if(e.pendingOperation)performPending(app);ImGui::CloseCurrentPopup();closeSavedDialog=true;}}
            ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }
        if(closeSavedDialog)ImGui::CloseCurrentPopup();
        ImGui::TextUnformatted(e.status.c_str());ImGui::EndPopup();
    }
    if(e.confirmDelete){ImGui::OpenPopup("Delete animation set?");e.confirmDelete=false;}
    if(ImGui::BeginPopupModal("Delete animation set?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Delete saved preset %s?",e.preset.c_str());
        if(ImGui::Button("Delete")){std::error_code ec;if(std::filesystem::remove(directory()/(e.preset+".cfg"),ec)){uiFileLists().get(directory(),"|.cfg|",false,true);e.preset.clear();e.saved={};dirty(app);e.status="Saved preset deleted; current set retained.";ImGui::CloseCurrentPopup();}else e.status="Could not delete preset.";}
        ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
}
bool available(const scene::Animation& clip){return !clip.tracks.empty()&&(!clip.sourceCurveCount||clip.unmappedCurveCount<clip.sourceCurveCount);}
void select(AppState& app,std::size_t index){app.actionPreviewClip=index;app.actionPreviewFrame=0;app.actionPreviewPlaying=index<app.actionPreviewScene.animations.size()&&available(app.actionPreviewScene.animations[index]);if(index<app.actionPreviewScene.animations.size())app.animationEditor.assignment=cadence::AnimationSet::slot(app.actionPreviewScene.animations[index]);}
void assign(AppState& app,const scene::Animation& clip,bool enabled,bool custom=false){
    auto& e=app.animationEditor;if(!available(clip))return;
    const cadence::AnimationSet::Key key{clip.sourceGame,int(clip.action),cadence::AnimationSet::file(clip.sourceName)};
    if(clip.action==scene::ActionRole::Death){e.draft.curatedDeaths=true;if(enabled)e.draft.rules[key]=true;else e.draft.rules.erase(key);}
    else{const auto slot=custom?e.assignment:cadence::AnimationSet::slot(clip);if(cadence::AnimationSet::validSlot(slot)){if(enabled)e.draft.overrides[slot]=key;else std::erase_if(e.draft.overrides,[&](const auto& item){return item.second==key;});}}
    dirty(app);
}
bool assigned(const cadence::AnimationSet& set,const scene::Animation& clip){
    if(clip.action==scene::ActionRole::Death)return set.explicitEnabled(clip);
    const cadence::AnimationSet::Key key{clip.sourceGame,int(clip.action),cadence::AnimationSet::file(clip.sourceName)};
    return std::any_of(set.overrides.begin(),set.overrides.end(),[&](const auto& item){return item.second==key;});
}
std::vector<scene::Mat4> previewPose(AppState& app){
    const auto& preview=app.actionPreviewScene;std::vector<scene::Mat4> pose;
    const bool valid=app.actionPreviewClip<preview.animations.size()&&available(preview.animations[app.actionPreviewClip]);
    if(valid)pose=preview.samplePose(app.actionPreviewClip,app.actionPreviewFrame);else for(const auto& b:preview.skeleton.bones)pose.push_back(b.restGlobal);
    if(!app.animationEditor.rootMotion)scene::imported::suppressBodyRootMotion(preview.skeleton,pose,valid&&preview.animations[app.actionPreviewClip].sourceGame=="pointblank");
    const float lift=(preview.bounds.valid?-preview.bounds.minimum.z:0.f)+gameplay::iw::worldUnits(.25f);
    for(auto& bone:pose)bone.v[14]+=lift;
    return pose;
}
void framePose(AppState& app){
    if(!app.actionPreviewReady)return;auto& e=app.animationEditor;const auto& preview=app.actionPreviewScene;
    const auto pose=previewPose(app);
    scene::Bounds bounds;
    for(const auto& mesh:preview.meshes)for(const auto& vertex:mesh.vertices){
        auto point=vertex.position;
        if(mesh.skinned){scene::Vec3 skinned{};float weight{};for(std::size_t i=0;i<vertex.bones.size();++i)if(vertex.weights[i]>0&&vertex.bones[i]<pose.size()){skinned+=scene::transformPoint(pose[vertex.bones[i]]*preview.skeleton.bones[vertex.bones[i]].inverseBind,point)*vertex.weights[i];weight+=vertex.weights[i];}if(weight>0)point=skinned;}
        point=scene::transformPoint(mesh.modelTransform,point);if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))continue;
        if(!bounds.valid){bounds.minimum=bounds.maximum=point;bounds.valid=true;}else{bounds.minimum={std::min(bounds.minimum.x,point.x),std::min(bounds.minimum.y,point.y),std::min(bounds.minimum.z,point.z)};bounds.maximum={std::max(bounds.maximum.x,point.x),std::max(bounds.maximum.y,point.y),std::max(bounds.maximum.z,point.z)};}
    }
    if(bounds.valid){e.pan=bounds.center()-preview.bounds.center();e.zoom=std::clamp(bounds.radius()/std::max(.1f,preview.bounds.radius()),.03f,30.f);}
}
void load(AppState& app){
    auto& e=app.animationEditor;if(e.sourceGame.empty())return;
    if(e.matchSource)e.modelGame=e.sourceGame;
    if(e.model>=app.assetCatalog.entries.size()||app.assetCatalog.entries[e.model].game!=e.modelGame)e.model=findGenericPlayermodelForGame(app,e.modelGame);
    prepareActionPreview(app,e.sourceGame,e.model);e.status=app.status;e.pendingLoad=false;e.pan={};e.zoom=1;
}
void browser(AppState& app){
    auto& e=app.animationEditor;const auto games=rippedGames(app);
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##source_game",e.sourceGame.empty()?"Source game":e.sourceGame.c_str())){for(const auto& game:games)if(game!="cs2"&&ImGui::Selectable(game.c_str(),e.sourceGame==game)){e.sourceGame=game;e.pendingLoad=true;}ImGui::EndCombo();}
    const auto actionName=[](int i){return i?scene::actionRoleName(static_cast<scene::ActionRole>(i)):"Locomotion";};
    ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##action",actionName(e.action))){for(int i=0;i<=static_cast<int>(scene::ActionRole::Inspect);++i)if(ImGui::Selectable(actionName(i),e.action==i))e.action=i;ImGui::EndCombo();}
    if(e.action==0){ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##motion_filter",e.motionFilter<0?"All locomotion":scene::motionRoleName(static_cast<scene::MotionRole>(e.motionFilter)))){if(ImGui::Selectable("All locomotion",e.motionFilter<0))e.motionFilter=-1;for(int i=1;i<=int(scene::MotionRole::Ladder);++i)if(ImGui::Selectable(scene::motionRoleName(static_cast<scene::MotionRole>(i)),e.motionFilter==i))e.motionFilter=i;ImGui::EndCombo();}}
    ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##file_search","Search filename",e.search.data(),e.search.size());
    if(ImGui::Button("Reload library")){app.botAnimationCache.erase(e.sourceGame);e.pendingLoad=true;}
    std::vector<std::size_t> rows;const auto query=lowerText(e.search.data());
    if(app.actionPreviewReady&&app.actionPreviewGame==e.sourceGame)for(std::size_t i=0;i<app.actionPreviewScene.animations.size();++i){const auto& a=app.actionPreviewScene.animations[i];if(static_cast<int>(a.action)==e.action&&(e.action!=0||e.motionFilter<0||int(a.motion)==e.motionFilter)&&(query.empty()||lowerText(a.sourceName).find(query)!=std::string::npos))rows.push_back(i);}
    std::sort(rows.begin(),rows.end(),[&](auto a,auto b){return app.actionPreviewScene.animations[a].sourceName<app.actionPreviewScene.animations[b].sourceName;});
    ImGui::BeginDisabled(rows.empty());
    if(ImGui::Button(e.action==int(scene::ActionRole::Death)?"Add listed":"Fill empty slots")){for(const auto i:rows){const auto& a=app.actionPreviewScene.animations[i];if(a.action==scene::ActionRole::Death||!e.draft.overrides.contains(cadence::AnimationSet::slot(a)))assign(app,a,true);}dirty(app);}
    ImGui::SameLine();if(ImGui::Button("Remove listed")){for(const auto i:rows)assign(app,app.actionPreviewScene.animations[i],false);dirty(app);}ImGui::EndDisabled();
    const auto enabledCount=std::count_if(rows.begin(),rows.end(),[&](auto i){return assigned(e.draft,app.actionPreviewScene.animations[i]);});
    ImGui::Text("%zu clips / %zu assigned",rows.size(),static_cast<std::size_t>(enabledCount));
    const auto unavailableCount=std::count_if(rows.begin(),rows.end(),[&](auto i){return !available(app.actionPreviewScene.animations[i]);});if(unavailableCount)ImGui::TextColored({1,.65f,.3f,1},"%zu unavailable on this model",static_cast<std::size_t>(unavailableCount));
    if(!rows.empty()&&std::find(rows.begin(),rows.end(),app.actionPreviewClip)==rows.end())select(app,rows.front());
    if(rows.empty()){app.actionPreviewPlaying=false;app.actionPreviewClip=SIZE_MAX;}
    ImGui::BeginChild("animation_files",{0,0},true,ImGuiWindowFlags_HorizontalScrollbar);
    bool scroll=false;
    if(ImGui::IsWindowFocused()&&!ImGui::GetIO().WantTextInput&&!rows.empty()&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)){
        auto it=std::find(rows.begin(),rows.end(),app.actionPreviewClip);int row=it==rows.end()?0:static_cast<int>(it-rows.begin());
        if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)){row=std::min(row+1,static_cast<int>(rows.size())-1);select(app,rows[row]);scroll=true;}
        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)){row=std::max(row-1,0);select(app,rows[row]);scroll=true;}
        if(ImGui::IsKeyPressed(ImGuiKey_Space))select(app,rows[row]);
        if(ImGui::IsKeyPressed(ImGuiKey_Enter))assign(app,app.actionPreviewScene.animations[rows[row]],true);
        if(ImGui::IsKeyPressed(ImGuiKey_Delete))assign(app,app.actionPreviewScene.animations[rows[row]],false);
    }
    ImGuiListClipper clipper;clipper.Begin(static_cast<int>(rows.size()),ImGui::GetFrameHeightWithSpacing());
    if(scroll){const auto it=std::find(rows.begin(),rows.end(),app.actionPreviewClip);if(it!=rows.end()){clipper.IncludeItemByIndex(static_cast<int>(it-rows.begin()));}}
    while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row){const auto i=rows[row];const auto& a=app.actionPreviewScene.animations[i];ImGui::PushID(static_cast<int>(i));bool enabled=assigned(e.draft,a);ImGui::BeginDisabled(!available(a));if(ImGui::Checkbox("##enabled",&enabled))assign(app,a,enabled);ImGui::EndDisabled();ImGui::SameLine();const bool usable=available(a);const auto label=cadence::AnimationSet::file(a.sourceName)+(usable?"":" [unavailable]");if(!usable)ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,.65f,.3f,1));if(ImGui::Selectable(label.c_str(),app.actionPreviewClip==i,0,{0,ImGui::GetFrameHeight()}))select(app,i);if(!usable)ImGui::PopStyleColor();if(scroll&&app.actionPreviewClip==i)ImGui::SetScrollHereY(.5f);ImGui::PopID();}
    if(rows.empty())ImGui::TextWrapped("%s",e.sourceGame.empty()?"Choose a source game.":"No matching clips bound to this model.");ImGui::EndChild();
}
std::string slotLabel(const cadence::AnimationSet::Slot& s){
    const auto [action,motion,stance,direction,weapon,ads]=s;
    return std::string(action?scene::actionRoleName(static_cast<scene::ActionRole>(action)):scene::motionRoleName(static_cast<scene::MotionRole>(motion)))+" / "+scene::stanceName(static_cast<scene::Stance>(stance))+" / "+scene::directionName(static_cast<scene::Direction>(direction))+" / "+scene::weaponClassName(static_cast<scene::WeaponClass>(weapon))+(ads<0?" / Hip + ADS":ads?" / ADS":" / Hip");
}
void jumpTo(AppState& app,const cadence::AnimationSet::Key& key){auto& e=app.animationEditor;const bool reload=e.sourceGame!=std::get<0>(key);e.sourceGame=std::get<0>(key);e.action=std::get<1>(key);e.motionFilter=-1;std::snprintf(e.search.data(),e.search.size(),"%s",std::get<2>(key).c_str());if(reload)e.pendingLoad=true;else for(std::size_t i=0;i<app.actionPreviewScene.animations.size();++i)if(cadence::AnimationSet::file(app.actionPreviewScene.animations[i].sourceName)==std::get<2>(key)){select(app,i);break;}}
void assignments(AppState& app){
    auto& e=app.animationEditor;
    ImGui::TextUnformatted("Assignments");ImGui::TextDisabled("Unassigned: base bot set");
    const bool selected=app.actionPreviewReady&&app.actionPreviewClip<app.actionPreviewScene.animations.size()&&available(app.actionPreviewScene.animations[app.actionPreviewClip]);
    if(selected){const auto& clip=app.actionPreviewScene.animations[app.actionPreviewClip];
        ImGui::TextWrapped("%s",cadence::AnimationSet::file(clip.sourceName).c_str());
        if(clip.action==scene::ActionRole::Death){if(ImGui::Button("Add to death set"))assign(app,clip,true);}
        else {
            auto& [action,motion,stance,direction,weapon,ads]=e.assignment;
            const auto combo=[&](const char* label,int& value,int last,auto name){ImGui::TextUnformatted(label);ImGui::SameLine(82);ImGui::SetNextItemWidth(-1);const auto id=std::string("##assign_")+label;if(ImGui::BeginCombo(id.c_str(),name(value))){for(int i=0;i<=last;++i)if(ImGui::Selectable(name(i),i==value))value=i;ImGui::EndCombo();}};
            if(!action)combo("Movement",motion,int(scene::MotionRole::Ladder),[](int i){return scene::motionRoleName(static_cast<scene::MotionRole>(i));});
            combo("Stance",stance,int(scene::Stance::Prone),[](int i){return scene::stanceName(static_cast<scene::Stance>(i));});
            combo("Direction",direction,int(scene::Direction::BackwardRight),[](int i){return scene::directionName(static_cast<scene::Direction>(i));});
            combo("Weapon",weapon,int(scene::WeaponClass::RC),[](int i){return scene::weaponClassName(static_cast<scene::WeaponClass>(i));});
            int choice=ads+1;const char* options[]={"Hip + ADS","Hip","ADS"};ImGui::SetNextItemWidth(-1);if(ImGui::Combo("##assign_ads",&choice,options,3))ads=choice-1;
            ImGui::BeginDisabled(!cadence::AnimationSet::validSlot(e.assignment));if(ImGui::Button("Assign selected"))assign(app,clip,true,true);ImGui::EndDisabled();
        }
    }
    ImGui::Separator();
    if(ImGui::Checkbox("Override deaths",&e.draft.curatedDeaths))dirty(app);
    if(ImGui::Button("Use base deaths")){std::erase_if(e.draft.rules,[](const auto& item){return std::get<1>(item.first)==int(scene::ActionRole::Death);});e.draft.curatedDeaths=false;dirty(app);}
    const auto count=std::count_if(e.draft.rules.begin(),e.draft.rules.end(),[](const auto& item){return item.second&&std::get<1>(item.first)==int(scene::ActionRole::Death);});
    ImGui::Text("%zu deaths / %zu overrides",static_cast<std::size_t>(count),e.draft.overrides.size());
    ImGui::BeginChild("assignments_list",{0,0},true,ImGuiWindowFlags_HorizontalScrollbar);
    std::optional<cadence::AnimationSet::Key> removeDeath;std::optional<cadence::AnimationSet::Slot> removeSlot;
    for(const auto& [key,value]:e.draft.rules)if(value&&std::get<1>(key)==int(scene::ActionRole::Death)){
        const auto label="["+std::get<0>(key)+"] "+std::get<2>(key);ImGui::PushID(label.c_str());
        if(ImGui::SmallButton("Remove"))removeDeath=key;ImGui::SameLine();if(ImGui::Selectable(label.c_str(),false))jumpTo(app,key);ImGui::PopID();
    }
    for(const auto& [slot,key]:e.draft.overrides){const auto label=slotLabel(slot);ImGui::PushID(label.c_str());
        if(ImGui::SmallButton("Use base set"))removeSlot=slot;ImGui::SameLine();ImGui::TextUnformatted(label.c_str());
        const auto clip="["+std::get<0>(key)+"] "+std::get<2>(key);if(ImGui::Selectable(clip.c_str(),false))jumpTo(app,key);ImGui::Separator();ImGui::PopID();}
    if(removeDeath){e.draft.rules.erase(*removeDeath);dirty(app);}if(removeSlot){e.draft.overrides.erase(*removeSlot);dirty(app);}
    ImGui::EndChild();
}
void viewport(AppState& app){
    auto& e=app.animationEditor;const auto games=rippedGames(app);
    if(ImGui::Checkbox("Match source game",&e.matchSource))e.pendingLoad=true;
    if(e.model<app.assetCatalog.entries.size()&&app.assetCatalog.entries[e.model].game==e.sourceGame){if(ImGui::Button("Use as source reference")){e.draft.models[e.sourceGame]=app.assetCatalog.entries[e.model].name;dirty(app);e.pendingLoad=true;e.status="Source reference: "+app.assetCatalog.entries[e.model].name;}}
    if(!e.matchSource){ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##preview_game",e.modelGame.empty()?"Model game":e.modelGame.c_str())){for(const auto& g:games)if(g!="cs2"&&ImGui::Selectable(g.c_str(),e.modelGame==g)){e.modelGame=g;e.model=SIZE_MAX;e.pendingLoad=true;}ImGui::EndCombo();}}
    ImGui::SetNextItemWidth(-1);
    const char* model=e.model<app.assetCatalog.entries.size()?app.assetCatalog.entries[e.model].name.c_str():"Choose model";
    if(ImGui::BeginCombo("##preview_model",model)){for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game==e.modelGame&&a.role==assets::Role::PlayerModel&&ImGui::Selectable(a.name.c_str(),e.model==i)){e.model=i;e.pendingLoad=true;}}ImGui::EndCombo();}
    if(ImGui::Button("Reset view")){e.pan={};e.zoom=1;e.yaw=scene::kPi;e.pitch=-.15f;}
    ImGui::SameLine();if(ImGui::Button("Frame pose"))framePose(app);
    const float controls=5*ImGui::GetFrameHeightWithSpacing()+ImGui::GetTextLineHeightWithSpacing();
    const float width=std::max(32.f,ImGui::GetContentRegionAvail().x),height=std::max(32.f,ImGui::GetContentRegionAvail().y-controls);
    ImGui::BeginChild("animation_viewport",{width,height},false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    if(app.actionPreviewReady){
        auto& preview=app.actionPreviewScene;const float radius=std::max(.1f,scene::length(preview.bounds.maximum-preview.bounds.minimum)*.5f);
        const auto center=(preview.bounds.minimum+preview.bounds.maximum)*.5f+e.pan;const float tangent=std::tan(20.f*scene::kPi/180.f),aspect=width/height;
        const float distance=radius/std::sin(std::atan(tangent*std::min(1.f,aspect)))*1.12f*e.zoom;
        const scene::Vec3 forward{std::cos(e.pitch)*std::cos(e.yaw),std::cos(e.pitch)*std::sin(e.yaw),std::sin(e.pitch)};
        const auto eye=center-forward*distance;const auto vp=scene::perspective(40.f*scene::kPi/180.f,aspect,std::max(.001f,radius*.0001f),distance+radius*100.f)*scene::lookAt(eye,center,{0,0,1});
        const auto pose=previewPose(app);
        app.actionPreviewRenderer.setDebugView(1);app.actionPreviewRenderer.render(preview,pose,vp,static_cast<int>(width),static_cast<int>(height),false,false,false);
        const auto origin=ImGui::GetCursorScreenPos();ImGui::Image(static_cast<ImTextureID>(app.actionPreviewRenderer.colorTexture()),{width,height},{0,1},{1,0});ImGui::SetCursorScreenPos(origin);
        ImGui::InvisibleButton("navigate",{width,height},ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle);
        if(ImGui::IsItemHovered()||ImGui::IsItemActive()){
            const auto& io=ImGui::GetIO();ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetItemID());
            if(ImGui::IsItemHovered())e.zoom=std::clamp(e.zoom*std::exp(-io.MouseWheel*.12f),.03f,30.f);
            if(ImGui::IsItemActive()){if(io.KeyShift){const auto right=scene::normalize(scene::cross(forward,{0,0,1}));const auto up=scene::cross(right,forward);e.pan+=right*(-io.MouseDelta.x*2*distance*tangent/height)+up*(io.MouseDelta.y*2*distance*tangent/height);}else if(io.KeyCtrl)e.zoom=std::clamp(e.zoom*std::exp(io.MouseDelta.y*.01f),.03f,30.f);else{e.yaw-=io.MouseDelta.x*.01f;e.pitch=std::clamp(e.pitch+io.MouseDelta.y*.01f,-1.5f,1.5f);}}
        }
    }else ImGui::TextUnformatted(e.pendingLoad?"Loading preview...":"Choose a game and model.");ImGui::EndChild();
    const bool selected=app.actionPreviewReady&&app.actionPreviewClip<app.actionPreviewScene.animations.size();
    const bool clipValid=selected&&available(app.actionPreviewScene.animations[app.actionPreviewClip]);ImGui::BeginDisabled(!clipValid);
    if(ImGui::Button(app.actionPreviewPlaying?"Pause":"Play"))app.actionPreviewPlaying=!app.actionPreviewPlaying;
    ImGui::SameLine();if(ImGui::Button("Restart")){app.actionPreviewFrame=0;app.actionPreviewPlaying=true;}
    ImGui::SameLine();ImGui::Checkbox("Loop",&e.loop);ImGui::EndDisabled();ImGui::Checkbox("Root motion",&e.rootMotion);ImGui::BeginDisabled(!clipValid);ImGui::SetNextItemWidth(80);ImGui::DragFloat("Speed",&e.speed,.01f,0.f,4.f,"%.2fx");
    if(clipValid){const auto& clip=app.actionPreviewScene.animations[app.actionPreviewClip];const float end=static_cast<float>(clip.durationFrames);ImGui::SetNextItemWidth(-1);if(ImGui::SliderFloat("##frame",&app.actionPreviewFrame,0,end,"Frame %.1f",ImGuiSliderFlags_AlwaysClamp))app.actionPreviewPlaying=false;ImGui::TextUnformatted(cadence::AnimationSet::file(clip.sourceName).c_str());
        if(app.actionPreviewPlaying){const float delta=std::max(0.f,ImGui::GetIO().DeltaTime*std::max(0.f,e.speed)*clip.framerate);app.actionPreviewFrame+=delta;if(app.actionPreviewFrame>=end){if(e.loop&&end>0)app.actionPreviewFrame=std::fmod(app.actionPreviewFrame,end);else{app.actionPreviewFrame=end;app.actionPreviewPlaying=false;}}}
    }ImGui::EndDisabled();
    if(selected&&!clipValid){ImGui::TextColored({1,.65f,.3f,1},"Unavailable on this model");ImGui::TextUnformatted(cadence::AnimationSet::file(app.actionPreviewScene.animations[app.actionPreviewClip].sourceName).c_str());}
}
}

void drawAnimationSetWorkspace(AppState& app){
    auto& e=app.animationEditor;
    if(!e.initialized){e.initialized=true;e.draft=app.animationSet;e.saved=e.draft;e.preset=app.animationSetPreset;e.useOnBots=app.animationSetForBots;}
    // Loading happens once per requested source/model change, never during arrow browsing.
    if(e.pendingLoad)animationui::load(app);
    animationui::presets(app);
    ImGui::Checkbox("Use on bots",&e.useOnBots);
    ImGui::SameLine();const bool busy=app.takeRecording||app.takePreview||app.exportActive||app.pendingClassFuture.has_value()||app.pendingClassLoadStage>=0||app.pendingMapFuture.has_value()||app.catalogScanFuture.has_value();
    ImGui::BeginDisabled(busy);if(ImGui::Button("Apply to bots")){app.animationSet=e.draft;app.animationSetPreset=e.preset;app.animationSetForBots=e.useOnBots;applyAnimationSetToLiveBots(app);e.status=app.status;}ImGui::EndDisabled();
    if(!e.status.empty())ImGui::TextWrapped("%s",e.status.c_str());
    if(!app.animationSetMissing.empty()&&ImGui::TreeNode("Unavailable clips")){ImGui::BeginChild("missing_assignments",{0,80},true);for(const auto& missing:app.animationSetMissing)ImGui::TextColored({1,.45f,.3f,1},"%s",missing.c_str());ImGui::EndChild();ImGui::TreePop();}
    if(ImGui::BeginTable("animation_workspace_v262",3,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV)){
        ImGui::TableSetupColumn("Library",ImGuiTableColumnFlags_WidthStretch,4.f);ImGui::TableSetupColumn("Assignments",ImGuiTableColumnFlags_WidthStretch,3.5f);ImGui::TableSetupColumn("Preview",ImGuiTableColumnFlags_WidthStretch,2.5f);ImGui::TableHeadersRow();ImGui::TableNextRow();ImGui::TableNextColumn();
        ImGui::BeginChild("animation_browser",{0,0},false);animationui::browser(app);ImGui::EndChild();
        ImGui::TableNextColumn();ImGui::BeginChild("animation_assignments",{0,0},false);animationui::assignments(app);ImGui::EndChild();
        ImGui::TableNextColumn();ImGui::BeginChild("animation_preview",{0,0},false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);animationui::viewport(app);ImGui::EndChild();ImGui::EndTable();
    }
}

void applyBotAnimationSet(AppState& app,scene::CastScene& actor,const std::string& modelGame){
    app.animationSetMissing.clear();
    auto& runtime=app.botSetRuntime;
    std::vector<std::size_t>* pools[]={&app.botSpAlertClips,&app.botSpPainClips,&app.botSpIdleClips,&app.botSpPistolPainClips,&app.botSpCrouchPainClips,&app.botSpRifleCrouchPainClips,&app.botSpPatrolClips,&app.botSpSlipClips,&app.botSpMovingPainClips,&app.botSpWalkClips};
    if(!runtime.initialized||runtime.owner!=&actor){for(std::size_t i=0;i<std::size(pools);++i)app.botSetBaseSpPools[i]=*pools[i];app.botSetBaseSpContexts=app.botSpContextClips;}
    const auto excluded=[&](std::size_t index){return index>=actor.animations.size()||(app.animationSetForBots&&!app.animationSet.enabled(actor.animations[index].sourceGame,actor.animations[index].action,actor.animations[index].sourceName));};
    for(std::size_t i=0;i<std::size(pools);++i){*pools[i]=app.botSetBaseSpPools[i];std::erase_if(*pools[i],excluded);}
    app.botSpContextClips=app.botSetBaseSpContexts;std::erase_if(app.botSpContextClips,[&](const auto& clip){return excluded(clip.animation);});
    runtime.begin(actor);runtime.bound.clear();
    if(!app.animationSetForBots){runtime.refresh(actor,app.animationSet,false);return;}
    struct RestoreReferences {AppState& app;decltype(app.gameReferenceSetups) saved;~RestoreReferences(){app.gameReferenceSetups=std::move(saved);}} restore{app,app.gameReferenceSetups};
    std::set<std::string> missingReferences;
    for(const auto& [game,name]:app.animationSet.models){bool found=false;for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& asset=app.assetCatalog.entries[i];if(asset.game==game&&asset.name==name&&asset.role==assets::Role::PlayerModel){app.gameReferenceSetups[game].playermodelAsset=i;found=true;break;}}if(!found)missingReferences.insert(game);}
    PointBlankPreparationBatch pb;ImportedBodyPreparationBatch imported;
    // Reference overrides are local to this application pass, including caches.
    ImportedBodyPreparationCache referenceCache;auto* previousCache=activeImportedBodyPreparation;activeImportedBodyPreparation=&referenceCache;
    struct RestoreCache {ImportedBodyPreparationCache* previous;~RestoreCache(){activeImportedBodyPreparation=previous;}} restoreCache{previousCache};
    for(const auto& [key,enabled]:app.animationSet.requested()){if(!enabled)continue;const auto& game=std::get<0>(key);const auto& file=std::get<2>(key);if(game=="cs2"||missingReferences.contains(game))continue;
        std::string reference;const auto setup=app.gameReferenceSetups.find(game);if(setup!=app.gameReferenceSetups.end()&&setup->second.playermodelAsset<app.assetCatalog.entries.size())reference=app.assetCatalog.entries[setup->second.playermodelAsset].path.string();
        const auto cacheKey=std::make_pair(key,reference);
        const auto matches=[&](const scene::Animation& a){return a.sourceGame==game&&int(a.action)==std::get<1>(key)&&cadence::AnimationSet::file(a.sourceName)==file&&animationui::available(a);};
        if(const auto it=runtime.prepared.find(cacheKey);it!=runtime.prepared.end()&&it->second<actor.animations.size()&&matches(actor.animations[it->second])){runtime.bound[key]=it->second;continue;}
        if(!app.animationSet.models.contains(game))for(std::size_t i=0;i<runtime.baseCount;++i)if(matches(actor.animations[i])){runtime.bound[key]=i;runtime.prepared[cacheKey]=i;break;}
        if(runtime.bound.contains(key))continue;
        ensureBotAnimationCache(app,game);
        for(const auto& doc:app.botAnimationCache[game])if(cadence::AnimationSet::file(doc.sourceName())==file){const auto before=actor.animations.size();if(!appendPointBlankWorldAnimation(app,doc,actor,game,modelGame)){scene::appendAnimations(doc,actor);if(game!=modelGame)if(const auto donor=playerSkeletonForGame(app,game))scene::retargetAnimationRange(actor,before,*donor);}for(std::size_t i=before;i<actor.animations.size();++i){actor.animations[i].sourceGame=game;if(matches(actor.animations[i]))runtime.prepared[cacheKey]=runtime.bound[key]=i;}break;}
    }
    for(const auto& [key,enabled]:app.animationSet.requested())if(enabled&&!runtime.bound.contains(key))app.animationSetMissing.push_back(std::get<0>(key)+" / "+std::get<2>(key));
    runtime.refresh(actor,app.animationSet,true);
}

void applyAnimationSetToLiveBots(AppState& app){
    app.animationSetMissing.clear();
    if(!app.botActorScene){app.status="Set selected for next spawn";return;}
    const auto game=app.botModelAsset<app.assetCatalog.entries.size()?app.assetCatalog.entries[app.botModelAsset].game:app.botGame;
    applyBotAnimationSet(app,*app.botActorScene,game);
    gameplay::bot::configureBotOneShots(*app.botActorScene);
    app.botSetRuntime.refresh(*app.botActorScene,app.animationSet,app.animationSetForBots);
    app.botDeathChoices=app.botSetRuntime.deaths(*app.botActorScene,app.animationSet,app.animationSetForBots);
    app.botDeathHistory={};app.botActionQueryCache.clear();resetBotLocomotionCache();
    app.botAnimationClipCount=app.botActorScene->animations.size();
    const auto previousSpeedCount=app.botClipAuthoredSpeeds.size();app.botClipAuthoredSpeeds.resize(app.botAnimationClipCount,0.f);
    const auto& actor=*app.botActorScene;
    if(scene::imported::bodyMotionRoot(actor.skeleton).bone>=0)for(std::size_t i=previousSpeedCount;i<actor.animations.size();++i){
        const auto& clip=actor.animations[i];const float end=clip.looping&&scene::imported::bodyLayout(actor.skeleton)?std::nextafter(float(clip.durationFrames),0.f):float(clip.durationFrames);
        const float seconds=end/std::max(1.f,clip.framerate);if(seconds<=.001f)continue;
        const auto first=actor.samplePose(i,0),last=actor.samplePose(i,end);const float speed=scene::imported::bodyRootTravelSpeed(actor.skeleton,first,last,seconds);
        if(speed>gameplay::iw::worldUnits(1.f)&&speed<gameplay::iw::worldUnits(1000.f))app.botClipAuthoredSpeeds[i]=speed;
    }
    app.status=app.animationSetMissing.empty()?"Applied to "+std::to_string(app.bots.size())+" bots":"Applied; some selected clips are unavailable";
}
